// SPDX-License-Identifier: Apache-2.0
#include "omniweft/retry.hpp"
#include <limits>
#include <type_traits>
#include <utility>

namespace ow::retry {
namespace {
bool bounded(const transactions::Receipt& r) {
  if(r.transaction_id.size()>36 || r.status.size()>16 || r.durability!="volatile" ||
     r.created.size()>4 || r.errors.size()>4) return false;
  std::size_t bytes=r.transaction_id.size()+r.status.size()+r.durability.size();
  for(const auto& b:r.created) {
    if(b.temporary_id.size()>64 || b.world_id.size()>64 || b.entity_uuid.size()!=36) return false;
    bytes+=b.temporary_id.size()+b.world_id.size()+b.entity_uuid.size();
  }
  for(const auto& e:r.errors) {
    if(e.code.size()>64 || e.path.size()>256 || e.message.size()>512) return false;
    bytes+=e.code.size()+e.path.size()+e.message.size();
  }
  return bytes<=receipt_field_limit;
}
}
void Window::reset() noexcept {
  for(auto& entry:entries_) entry=Entry{};
  next_=1;exhausted_=false;
}
Result Window::execute(std::uint64_t sequence,std::string_view canonical,
                       const std::function<transactions::Receipt()>& apply) {
  const auto now=Clock::now();
  for(auto& entry:entries_) if(entry.sequence && now>=entry.deadline) entry=Entry{};
  if(sequence==0 || (!exhausted_ && sequence>next_)) return {nullptr,"REQUIRES_RESYNC",false};
  if(sequence<next_ || exhausted_) {
    for(const auto& entry:entries_) if(entry.sequence==sequence) {
      if(entry.payload!=canonical) return {nullptr,"IDEMPOTENCY_MISMATCH",false};
      if(!entry.receipt) return {nullptr,"REQUIRES_RESYNC",false};
      return {&*entry.receipt,"",true};
    }
    return {nullptr,"REQUIRES_RESYNC",false};
  }
  if(canonical.empty() || canonical.size()>payload_limit || !apply)
    return {nullptr,"BUDGET_EXCEEDED",false};
  // The only retained payload allocation happens before consuming the key.
  Entry reserved;reserved.sequence=sequence;reserved.payload=canonical;
  reserved.deadline=now+receipt_lifetime;
  auto& entry=entries_[(sequence-1)%window_size];
  static_assert(std::is_nothrow_move_assignable_v<Entry>);
  static_assert(std::is_nothrow_move_constructible_v<transactions::Receipt>);
  entry=std::move(reserved);
  if(next_==std::numeric_limits<std::uint64_t>::max()) exhausted_=true;
  else ++next_;
  try {
    auto receipt=apply();
    // A broken trusted callback remains consumed, with no reusable receipt.
    if(!bounded(receipt)) return {nullptr,"REQUIRES_RESYNC",false};
    entry.receipt.emplace(std::move(receipt));
    return {&*entry.receipt,"",false};
  } catch(...) {return {nullptr,"REQUIRES_RESYNC",false};}
}
} // namespace ow::retry
