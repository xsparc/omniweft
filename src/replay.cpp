// SPDX-License-Identifier: Apache-2.0
#include "omniweft/replay.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ow::replay {
namespace {
using namespace commands;
using transactions::Error;
[[noreturn]] void fail(const char* code,const char* path) {
  throw Error{code,path,"Recorded authoring replay rejected.",std::nullopt};
}
transactions::Receipt rejected(const Envelope& e,std::uint64_t revision,const char* code,const char* path) {
  transactions::Receipt r;r.world_revision=revision;
  if(e.transaction_id.size()<=36)r.transaction_id=e.transaction_id;
  r.errors.push_back({code,path,"Recorded authoring replay rejected.",std::nullopt});return r;
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

Envelope envelope(const Log& log,const Record& r) {
  Envelope e;e.world_id=log.world_id;e.transaction_id="018f7242-4387-7c98-a114-00000013000"+std::to_string(r.sequence);
  e.idempotency={"offline-replay-internal",r.sequence};e.expected_world_revision=r.expected_revision;
  e.apply_at={"next_tick",120};e.budget=r.budget;e.operations=r.operations;return e;
}
bool operations_bounded(const std::vector<Operation>& operations) {
  // Check every variable-size typed member before constructing/copying an envelope.
  if(operations.size()>operation_limit)return false;
  for(const auto& op:operations) {
    bool ok=std::visit([](const auto& v) {
      using T=std::decay_t<decltype(v)>;
      if constexpr(std::is_same_v<T,EntityCreate>)return v.temporary_id.size()<=64 && v.prefab.size()<=128;
      else {
        if(!target_bounded(v.target))return false;
        if constexpr(std::is_same_v<T,EntityTagsSet>) {
          if(v.tags.size()>8)return false;
          for(const auto& tag:v.tags)if(tag.size()>32)return false;
        } else if constexpr(std::is_same_v<T,EntityReparent>) {
          if(v.mode.size()>32 || (v.parent && !target_bounded(*v.parent)))return false;
        } else if constexpr(std::is_same_v<T,EntityDelete>) {if(v.child_policy.size()>32)return false;}
        return true;
      }
    },op);
    if(!ok)return false;
  }
  return true;
}
void preflight(const Log& log,std::span<const Asset> available) {
  if(log.schema!=1 || log.command_schema!="0.1" || log.checkpoint_schema!="OWOBJ001-003")fail("UNSUPPORTED_SCHEMA","/schema");
  if(log.world_id.empty() || log.world_id.size()>128 || log.max_slots==0 || log.max_slots>8 ||
     log.records.size()>entry_limit || available.size()>8)fail("BUDGET_EXCEEDED","/log");
  if(log.assets.size()!=1 || log.assets[0]!=Asset{})fail("MISSING_ASSET","/assets");
  if(std::none_of(available.begin(),available.end(),[](const auto& a){return a==Asset{};}))fail("MISSING_ASSET","/assets");
  std::uint64_t tick=0;
  for(std::size_t i=0;i<log.records.size();++i) {
    const auto& r=log.records[i];
    if(r.sequence!=i+1 || r.expected_revision!=i || r.boundary_tick<tick)fail("INVALID_REPLAY","/order");
    tick=r.boundary_tick;
    if(r.checkpoint.empty() || r.checkpoint.size()>checkpoint_limit || r.created.size()>operation_limit ||
       r.operations.empty() || !operations_bounded(r.operations))fail("BUDGET_EXCEEDED","/records");
    for(const auto& b:r.created)if(b.temporary_id.size()>64 || b.world_id.size()>128 || b.entity_uuid.size()!=36 || b.generation==0)
      fail("INVALID_REPLAY","/created");
    for(const auto& op:r.operations)if(const auto* c=std::get_if<EntityCreate>(&op);c && c->prefab!="builtin.unit_cube")
      fail("MISSING_ASSET","/operations/prefab");
    const auto e=envelope(log,r);const auto serialized=serialize(e);
    if(!serialized.json)fail("INVALID_REPLAY","/operations");
    if(serialized.json->size()>command_limit)fail("BUDGET_EXCEEDED","/command");
  }
}
class Observer final:public transactions::CommitObserver {
 public:
  Observer(Record& destination,Record& prepared,std::size_t& count,std::uint64_t& head):
    destination_(destination),prepared_(prepared),count_(count),head_(head) {}
  void prepare(const world::Snapshot& s,const transactions::Receipt& receipt) override {
    prepared_.created=receipt.created;prepared_.checkpoint=world::canonical_bytes(s);
    if(prepared_.checkpoint.size()>checkpoint_limit)fail("BUDGET_EXCEEDED","/checkpoint");
  }
  void publish() noexcept override {
    static_assert(std::is_nothrow_move_assignable_v<Record>);
    destination_=std::move(prepared_);++count_;++head_;
  }
 private:
  Record& destination_;Record& prepared_;std::size_t& count_;std::uint64_t& head_;
};
}
Recorder::Recorder(world::World& world):world_(world) {
  const auto s=world.snapshot();
  if(s.world_revision!=0 || !s.slots.empty() || s.max_slots>8)fail("INVALID_REPLAY","/initial_world");
}
transactions::Receipt Recorder::apply_at_boundary(const Envelope& e,std::uint64_t tick,transactions::StagingGuard* guard) {
  const auto live=world_.snapshot().world_revision;
  if(!bounded(e))return rejected(e,live,"BUDGET_EXCEEDED","/command");
  if(live!=head_ || e.expected_world_revision!=live)return rejected(e,live,"REVISION_CONFLICT","/expected_world_revision");
  if(count_==entry_limit)return rejected(e,live,"BUDGET_EXCEEDED","/records");
  if(count_ && tick<records_[count_-1].boundary_tick)return rejected(e,live,"INVALID_REPLAY","/boundary_tick");
  const auto serialized=serialize(e);
  if(!serialized.json)return rejected(e,live,"INVALID_SCHEMA","/command");
  if(serialized.json->size()>command_limit)return rejected(e,live,"BUDGET_EXCEEDED","/command");
  Record prepared;prepared.sequence=count_+1;prepared.boundary_tick=tick;prepared.expected_revision=head_;
  prepared.budget=e.budget;prepared.operations=e.operations;
  Observer observer(records_[count_],prepared,count_,head_);
  return transactions::Coordinator::apply_at_boundary(world_,e,guard,&observer);
}
Log Recorder::export_log() const {
  const auto s=world_.snapshot();if(s.world_revision!=head_)fail("REVISION_CONFLICT","/expected_world_revision");
  Log log;log.world_id=s.world_id;log.seed=s.seed;log.max_slots=s.max_slots;
  log.records.assign(records_.begin(),records_.begin()+static_cast<std::ptrdiff_t>(count_));return log;
}
Result reconstruct(const Log& log,std::span<const Asset> available_assets) {
  try {
    preflight(log,available_assets);
    auto world=std::make_unique<world::World>(log.world_id,log.seed,log.max_slots);
    for(const auto& r:log.records) {
      const auto receipt=transactions::Coordinator::replay_at_boundary(*world,envelope(log,r),r.created);
      if(receipt.status!="committed" || receipt.created!=r.created)fail("INVALID_REPLAY","/created");
      if(world->canonical_bytes()!=r.checkpoint)fail("CHECKPOINT_MISMATCH","/checkpoint");
    }
    return {std::move(world),{}};
  } catch(const Error& error) {return {nullptr,{error}};}
    catch(const std::invalid_argument&) {return {nullptr,{{"INVALID_REPLAY","/world_id","Recorded authoring replay rejected.",std::nullopt}}};}
}
} // namespace ow::replay
