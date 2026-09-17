// SPDX-License-Identifier: Apache-2.0
#include "omniweft/policy.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ow::policy {
namespace {
constexpr std::array<Grant, principal_count> grants{{
  {"west", {-8,-4,-4}, {-1,4,4}, 512, 98304, 512},
  {"east", {1,-4,-4}, {8,4,4}, 2048, 262144, 4096}
}};
[[noreturn]] void denied(const char* code, const char* path) { throw Denied{code, path}; }
[[noreturn]] void reject(const char* code, const char* path, const char* message,
                        std::optional<std::size_t> operation = std::nullopt) {
  throw transactions::Error{code, path, message, operation};
}
// Raw typed string storage is bounded before commands::serialize can allocate.
// JSON framing and fixed numeric arrays are charged by the fixed working base.
// Summation saturates above the body cap instead of wrapping.
std::size_t field_bytes(const commands::Envelope& envelope) {
  std::size_t bytes = 0;
  const auto add = [&](std::string_view value) {
    if (value.size() > body_limit || bytes > body_limit - value.size())
      reject("BUDGET_EXCEEDED", "/budget", "Typed input exceeds the policy body allowance.");
    bytes += value.size();
  };
  if (envelope.operations.size() > operation_limit ||
      envelope.operations.size() > envelope.budget.max_operations)
    reject("BUDGET_EXCEEDED", "/operations", "Operation allowance exceeded.");
  add(envelope.protocol_version); add(envelope.world_id); add(envelope.transaction_id);
  add(envelope.idempotency.epoch); add(envelope.apply_at.mode);
  for (const auto& item : envelope.operations) {
    std::visit([&](const auto& operation) {
      using T = std::decay_t<decltype(operation)>;
      if constexpr (std::is_same_v<T, commands::EntityCreate>) {
        add(operation.temporary_id); add(operation.prefab);
      } else {
        std::visit([&](const auto& target) {
          using Target = std::decay_t<decltype(target)>;
          if constexpr (std::is_same_v<Target, commands::TemporaryTarget>) add(target.temporary_id);
          else { add(target.world_id); add(target.entity_uuid); }
        }, operation.target);
        if constexpr (std::is_same_v<T, commands::EntityDelete>) add(operation.child_policy);
      }
    }, item);
  }
  return bytes;
}
}  // namespace

const Grant& grant(std::size_t principal) {
  if (principal >= principal_count) denied("NOT_AUTHORIZED", "/principal");
  return grants[principal];
}
std::size_t working_charge(std::size_t bytes) {
  if (bytes > body_limit) denied("BUDGET_EXCEEDED", "/body");
  return 65536 + 1024 * slot_count + 8 * bytes;
}
Ledger::Ledger(std::size_t global_limit) : global_limit_(global_limit) {
  if (global_limit < 1 || global_limit > 2) throw std::invalid_argument("INVALID_POLICY_LIMIT");
  sponsors_.fill(-1);
}
Lease::Lease(Ledger& ledger, std::size_t principal, std::size_t bytes) noexcept
  : ledger_(&ledger), principal_(principal), bytes_(bytes) {}
Lease::Lease(Lease&& other) noexcept
  : ledger_(std::exchange(other.ledger_, nullptr)), principal_(other.principal_), bytes_(other.bytes_), claimed_(other.claimed_) {}
Lease::~Lease() {
  if (!ledger_) return;
  std::lock_guard lock(ledger_->mutex_);
  ledger_->usage_.working[principal_] -= 65536 + 1024 * slot_count + 8 * bytes_;
  --ledger_->usage_.requests[principal_]; --ledger_->usage_.global_requests;
}
Lease Ledger::admit(std::size_t principal, std::size_t declared_bytes) {
  const auto& limits = grant(principal);
  const auto charge = working_charge(declared_bytes);
  std::lock_guard lock(mutex_);
  if (usage_.requests[principal] >= 1 || usage_.global_requests >= global_limit_)
    denied("BUDGET_EXCEEDED", "/queue");
  if (charge > limits.working_limit - usage_.working[principal])
    denied("BUDGET_EXCEEDED", "/memory/working");
  usage_.working[principal] += charge; ++usage_.requests[principal]; ++usage_.global_requests;
  return Lease(*this, principal, declared_bytes);
}
Usage Ledger::usage() const { std::lock_guard lock(mutex_); return usage_; }

Guard::Guard(Lease& lease) : lease_(lease) {}
void Guard::begin(const world::World& world, const world::Snapshot& snapshot,
                  const commands::Envelope& envelope) {
  if (begun_ || !lease_.ledger_ || lease_.claimed_)
    reject("NOT_AUTHORIZED", "/policy", "A live unused admission is required.");
  begun_ = true; lease_.claimed_ = true;
  const auto bytes = field_bytes(envelope);
  if (bytes > lease_.bytes_)
    reject("BUDGET_EXCEEDED", "/memory/working", "Typed fields exceed the reserved body allowance.");
  lock_ = std::unique_lock(lease_.ledger_->mutex_);
  auto& ledger = *lease_.ledger_;
  if ((ledger.world_ && ledger.world_ != &world) || snapshot.world_id != "workshop" ||
      snapshot.seed != 7 || snapshot.max_slots != slot_count || snapshot.slots.size() > slot_count ||
      snapshot.world_revision != ledger.usage_.world_revision)
    reject("REQUIRES_RESYNC", "/policy", "World and policy ledger must share the fixture boundary.");
  for (std::size_t i = 0; i < slot_count; ++i) {
    const bool present = i < snapshot.slots.size();
    if ((ledger.sponsors_[i] >= 0) != present ||
        ledger.live_[i] != (present && snapshot.slots[i].entity.has_value()))
      reject("REQUIRES_RESYNC", "/policy", "World and policy slot accounting differ.");
  }
  ledger.world_ = &world;
  sponsors_ = ledger.sponsors_; live_ = ledger.live_; retained_ = ledger.usage_.retained;
  begun_ = true;
}
void Guard::account(std::size_t operation) {
  retained_.fill(0);
  for (std::size_t i = 0; i < slot_count; ++i) {
    if (sponsors_[i] < 0) continue;
    retained_[static_cast<std::size_t>(sponsors_[i])] += skeleton_charge + (live_[i] ? cube_charge : 0);
  }
  for (std::size_t principal = 0; principal < principal_count; ++principal)
    if (retained_[principal] > grants[principal].retained_limit)
      reject("BUDGET_EXCEEDED", "/memory/retained", "Staging prefix exceeds retained resource allowance.", operation);
}
void Guard::scope(const world::Transform& transform, std::size_t operation) const {
  const auto& bounds = grants[lease_.principal_];
  const auto& q = transform.rotation_xyzw;
  const auto x=q[0], y=q[1], z=q[2], w=q[3];
  const std::array<std::array<double,3>,3> rotation{{
    {1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)},
    {2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)},
    {2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)}
  }};
  for (std::size_t axis=0; axis<3; ++axis) {
    double radius=0;
    for (std::size_t component=0; component<3; ++component)
      radius += std::abs(rotation[axis][component] * transform.scale[component]) * 0.5;
    const auto lower=transform.position_m[axis]-radius, upper=transform.position_m[axis]+radius;
    if (!std::isfinite(lower) || !std::isfinite(upper) ||
        lower<bounds.lower[axis] || upper>bounds.upper[axis])
      reject("NOT_AUTHORIZED", "/scope", "Complete cube bounds must fit the host-issued write region.", operation);
  }
}
void Guard::create(std::size_t slot, std::size_t operation) {
  if (slot >= slot_count) reject("BUDGET_EXCEEDED", "/slots", "Slot allowance exceeded.", operation);
  sponsors_[slot] = static_cast<int>(lease_.principal_); live_[slot] = true; unplaced_[slot] = true;
  account(operation);
}
void Guard::transform(std::size_t slot, const world::Transform& before,
                      const world::Transform& after, std::size_t operation) {
  if (!unplaced_[slot]) scope(before, operation);
  scope(after, operation); unplaced_[slot] = false;
}
void Guard::erase(std::size_t slot, const world::Transform& before, std::size_t operation) {
  if (!unplaced_[slot]) scope(before, operation);
  live_[slot] = false; unplaced_[slot] = false; account(operation);
}
void Guard::finish(const world::Snapshot&) {
  if (std::any_of(unplaced_.begin(), unplaced_.end(), [](bool value){ return value; }))
    reject("NOT_AUTHORIZED", "/scope", "Every surviving created cube must be explicitly placed.");
  finished_ = true;
}
void Guard::commit() noexcept {
  if (!begun_ || !finished_) std::terminate();
  auto& ledger = *lease_.ledger_;
  ledger.sponsors_ = sponsors_; ledger.live_ = live_; ledger.usage_.retained = retained_;
  ++ledger.usage_.world_revision;
}
transactions::Receipt apply(world::World& world, const commands::Envelope& envelope,
                            Ledger& ledger, std::size_t principal) {
  try {
    const auto bytes = field_bytes(envelope);
    auto lease = ledger.admit(principal, bytes);
    Guard guard(lease);
    return transactions::Coordinator::apply_at_boundary(world, envelope, &guard);
  } catch (const transactions::Error& error) {
    transactions::Receipt receipt; if (envelope.transaction_id.size() <= 128) receipt.transaction_id = envelope.transaction_id;
    receipt.world_revision = ledger.usage().world_revision; receipt.errors.push_back(error); return receipt;
  } catch (const Denied& error) {
    transactions::Receipt receipt; if (envelope.transaction_id.size() <= 128) receipt.transaction_id = envelope.transaction_id;
    receipt.world_revision = ledger.usage().world_revision;
    receipt.errors.push_back({error.code, error.path, "Policy admission denied.", std::nullopt}); return receipt;
  }
}
}  // namespace ow::policy
