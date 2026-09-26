// SPDX-License-Identifier: Apache-2.0
#include "omniweft/history.hpp"
#include <algorithm>
#include <type_traits>
#include <utility>

namespace ow::history {
namespace {
using namespace commands;
using transactions::Receipt;
Receipt rejected(const Envelope& e,std::uint64_t revision,const char* code,const char* path) {
  Receipt r;r.world_revision=revision;
  if(e.transaction_id.size()<=36) r.transaction_id=e.transaction_id;
  r.errors.push_back({code,path,"Authoring history request rejected.",std::nullopt});return r;
}
bool target_bounded(const Target& t) {
  if(const auto* v=std::get_if<EntityTarget>(&t)) return v->world_id.size()<=128 && v->entity_uuid.size()<=36;
  return std::get<TemporaryTarget>(t).temporary_id.size()<=64;
}
bool bounded(const Envelope& e) {
  if(e.protocol_version.size()>16 || e.world_id.size()>128 || e.transaction_id.size()>36 ||
     e.idempotency.epoch.size()>128 || e.apply_at.mode.size()>32 || e.operations.size()>operation_limit) return false;
  for(const auto& op:e.operations) {
    const bool ok=std::visit([](const auto& value) {
      using T=std::decay_t<decltype(value)>;
      if constexpr(std::is_same_v<T,EntityCreate>) return value.temporary_id.size()<=64 && value.prefab.size()<=128;
      else {
        if(!target_bounded(value.target)) return false;
        if constexpr(std::is_same_v<T,EntityTagsSet>) {
          if(value.tags.size()>8) return false;
          for(const auto& tag:value.tags) if(tag.size()>32) return false;
        } else if constexpr(std::is_same_v<T,EntityReparent>) {
          if(value.mode.size()>32 || (value.parent && !target_bounded(*value.parent))) return false;
        } else if constexpr(std::is_same_v<T,EntityDelete>) {
          if(value.child_policy.size()>32) return false;
        }
        return true;
      }
    },op);
    if(!ok) return false;
  }
  return true;
}
const world::Slot* resolve(const world::Snapshot& s,const Target& target) {
  const auto* id=std::get_if<EntityTarget>(&target);
  if(!id || id->world_id!=s.world_id) return nullptr;
  const auto at=std::find_if(s.slots.begin(),s.slots.end(),[&](const auto& slot) {
    return slot.entity_uuid==id->entity_uuid && slot.generation==id->generation && slot.entity && !slot.retired;
  });
  return at==s.slots.end()?nullptr:&*at;
}
bool isolated(const world::Snapshot& s,const world::Slot& slot) {
  if(slot.entity->parent) return false;
  return std::none_of(s.slots.begin(),s.slots.end(),[&](const auto& child) {
    return child.entity && child.entity->parent && child.entity->parent->entity_uuid==slot.entity_uuid &&
           child.entity->parent->generation==slot.generation;
  });
}
}
History::History(world::World& world):world_(world) {state_.expected_revision=world_.snapshot().world_revision;}
void History::clear_at_boundary() {
  const auto revision=world_.snapshot().world_revision;
  for(auto& entry:entries_) entry=Entry{};
  state_={0,0,revision};
}
Receipt History::apply_at_boundary(const Envelope& e,transactions::StagingGuard* guard) {
  const auto before=world_.snapshot();
  const auto deny=[&](const char* code,const char* path) {return rejected(e,before.world_revision,code,path);};
  if(!bounded(e)) return deny("BUDGET_EXCEEDED","/history/budget");
  if(e.expected_world_revision!=before.world_revision || state_.expected_revision!=before.world_revision)
    return deny("REVISION_CONFLICT","/expected_world_revision");
  if(e.operations.empty()) return deny("INVALID_SCHEMA","/operations");
  if(e.operations.size()>e.budget.max_operations) return deny("BUDGET_EXCEEDED","/budget/max_operations");
  const auto serialized=serialize(e);
  if(!serialized.json) return deny("INVALID_SCHEMA","/history/envelope");
  if(serialized.json->size()>envelope_limit) return deny("BUDGET_EXCEEDED","/history/budget");
  if(state_.cursor==capacity) return deny("BUDGET_EXCEEDED","/history");
  Entry prepared;prepared.forward=e.operations;prepared.inverse.reserve(e.operations.size());
  for(const auto& operation:e.operations) {
    if(const auto* transform=std::get_if<TransformSet>(&operation)) {
      const auto* slot=resolve(before,transform->target);
      if(!slot) return deny("STALE_HANDLE","/operations/target");
      if(!isolated(before,*slot)) return deny("UNSUPPORTED_OPERATION","/operations");
      const auto& t=slot->entity->transform;
      prepared.inverse.emplace_back(TransformSet{transform->target,t.position_m,t.rotation_xyzw,t.scale});
    } else if(const auto* tags=std::get_if<EntityTagsSet>(&operation)) {
      const auto* slot=resolve(before,tags->target);
      if(!slot) return deny("STALE_HANDLE","/operations/target");
      prepared.inverse.emplace_back(EntityTagsSet{tags->target,slot->entity->tags});
    } else return deny("UNSUPPORTED_OPERATION","/operations");
  }
  std::reverse(prepared.inverse.begin(),prepared.inverse.end());
  auto inverse=e;inverse.operations=prepared.inverse;
  if(!bounded(inverse)) return deny("BUDGET_EXCEEDED","/history/budget");
  const auto inverse_json=serialize(inverse);
  if(!inverse_json.json || inverse_json.json->size()>envelope_limit) return deny("BUDGET_EXCEEDED","/history/budget");
  // Every allocation, including both history directions, precedes publication.
  auto receipt=transactions::Coordinator::apply_at_boundary(world_,e,guard);
  if(receipt.status=="committed") {
    static_assert(std::is_nothrow_move_assignable_v<Entry>);
    static_assert(std::is_nothrow_move_constructible_v<Receipt>);
    entries_[state_.cursor]=std::move(prepared);
    ++state_.cursor;
    for(std::size_t i=state_.cursor;i<state_.entries;++i) entries_[i]=Entry{};
    state_.entries=state_.cursor;state_.expected_revision=receipt.world_revision;
  }
  return receipt;
}
Receipt History::navigate_at_boundary(Action action,const Envelope& request,transactions::StagingGuard* guard) {
  const auto live=world_.snapshot();
  const auto deny=[&](const char* code,const char* path) {return rejected(request,live.world_revision,code,path);};
  if(!bounded(request)) return deny("BUDGET_EXCEEDED","/history/budget");
  if(!request.operations.empty()) return deny("INVALID_SCHEMA","/operations");
  if(action!=Action::undo && action!=Action::redo) return deny("UNSUPPORTED_OPERATION","/history/action");
  if(request.expected_world_revision!=live.world_revision || state_.expected_revision!=live.world_revision)
    return deny("REVISION_CONFLICT","/expected_world_revision");
  if((action==Action::undo && state_.cursor==0) || (action==Action::redo && state_.cursor==state_.entries))
    return deny("NOT_FOUND","/history");
  const auto& entry=entries_[action==Action::undo?state_.cursor-1:state_.cursor];
  const auto& operations=action==Action::undo?entry.inverse:entry.forward;
  if(operations.size()>request.budget.max_operations) return deny("BUDGET_EXCEEDED","/budget/max_operations");
  auto envelope=request;envelope.operations=operations;
  const auto serialized=serialize(envelope);
  if(!serialized.json) return deny("INVALID_SCHEMA","/history/envelope");
  if(serialized.json->size()>envelope_limit) return deny("BUDGET_EXCEEDED","/history/budget");
  auto receipt=transactions::Coordinator::apply_at_boundary(world_,envelope,guard);
  if(receipt.status=="committed") {
    if(action==Action::undo) --state_.cursor;else ++state_.cursor;
    state_.expected_revision=receipt.world_revision;
  }
  return receipt;
}
} // namespace ow::history
