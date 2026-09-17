// SPDX-License-Identifier: Apache-2.0
#include <omniweft/policy.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
unsigned checks=0;
void require(bool value, const char* message) {
  ++checks; if(!value) throw std::runtime_error(message);
}
using namespace ow::commands;
Envelope batch(std::uint64_t revision, std::vector<Operation> operations) {
  Envelope e;
  e.world_id="workshop"; e.transaction_id="018f7242-4387-7c98-a114-67787915a399";
  e.idempotency={"fixture-epoch",1}; e.expected_world_revision=revision;
  e.apply_at={"next_tick",120}; e.budget={static_cast<std::uint64_t>(operations.size()),0};
  e.operations=std::move(operations); return e;
}
TransformSet move(Target target, double x) {
  TransformSet operation; operation.target=std::move(target); operation.position_m={x,0,0}; return operation;
}
void code(const ow::transactions::Receipt& receipt, const char* expected) {
  require(receipt.status=="rejected" && receipt.created.empty() && receipt.errors.size()==1 &&
          receipt.errors.front().code==expected, "expected typed rejection");
}
template<class Action> void denied(Action action, const char* path) {
  bool found=false;
  try { action(); } catch(const ow::policy::Denied& error) {
    found=std::string(error.code)=="BUDGET_EXCEEDED" && std::string(error.path)==path;
  }
  require(found,"expected pre-admission resource rejection");
}
}
int main() {
  try {
    using namespace ow::policy;
    Ledger quotas;
    require(working_charge(0)==73728 && working_charge(3072)==98304 &&
            working_charge(16384)==204800,"literal accounting boundaries");
    {
      auto west=quotas.admit(0,3072);
      require(quotas.usage().working[0]==98304 && quotas.usage().global_requests==1,"west exact working limit");
      denied([&]{auto excess=quotas.admit(0,0);},"/queue");
      {auto east=quotas.admit(1,16384); require(quotas.usage().global_requests==2,"east progresses beside west");}
      require(quotas.usage().requests[1]==0 && quotas.usage().working[1]==0,"east refund independent");
      auto moved=std::move(west);
      require(quotas.usage().global_requests==1,"move transfers refund once");
    }
    require(quotas.usage()==Usage{},"all admission resources returned");
    for(int i=0;i<100;++i) {
      denied([&]{auto excessive=quotas.admit(0,3073);},"/memory/working");
      denied([&]{auto excessive=quotas.admit(1,16385);},"/body");
      require(quotas.usage()==Usage{},"rejected admission does not leak");
    }
    Ledger global(1);
    {auto west=global.admit(0,0); denied([&]{auto east=global.admit(1,0);},"/queue");}
    {auto east=global.admit(1,0); require(global.usage().global_requests==1,"global refund recovery");}

    ow::world::World world("workshop",7,8);
    Ledger ledger;
    const auto apply=[&](std::size_t principal,std::vector<Operation> operations) {
      return ow::policy::apply(world,batch(world.snapshot().world_revision,std::move(operations)),ledger,principal);
    };
    const auto initial=world.canonical_bytes();
    code(apply(0,{EntityCreate{"A","builtin.unit_cube"}}),"NOT_AUTHORIZED");
    require(world.canonical_bytes()==initial && ledger.usage()==Usage{},"unplaced create rolls back");
    for(int i=0;i<100;++i) {
      code(apply(0,{EntityCreate{"A","builtin.unit_cube"},EntityCreate{"B","builtin.unit_cube"},
                   EntityDelete{TemporaryTarget{"A"}},EntityDelete{TemporaryTarget{"B"}}}),"BUDGET_EXCEEDED");
      require(world.canonical_bytes()==initial && ledger.usage()==Usage{},"peak amplification rejected without slots or charges");
    }
    const auto first=apply(0,{EntityCreate{"A","builtin.unit_cube"},move(TemporaryTarget{"A"},-3)});
    require(first.status=="committed" && first.world_revision==1 && first.created.size()==1,"scoped create");
    const EntityTarget west{"workshop","00000007-0000-4000-8000-000000000001",1};
    require(first.created.front().entity_uuid==west.entity_uuid && first.created.front().generation==1,"recovery preserves identity");
    require(ledger.usage().retained==std::array<std::size_t,2>{384,0},"skeleton and live charge");
    const auto before=world.canonical_bytes();
    for(const double x : {-8.0,-1.0,3.0}) {
      code(apply(0,{move(west,x)}),"NOT_AUTHORIZED");
      require(world.canonical_bytes()==before && ledger.usage().retained[0]==384,"full bounds rejection rollback");
    }
    code(apply(1,{move(west,3)}),"NOT_AUTHORIZED");
    code(apply(1,{EntityDelete{west}}),"NOT_AUTHORIZED");
    auto rotated=move(west,-1.6);
    rotated.rotation_xyzw={0,0,std::sin(3.14159265358979323846/8),std::cos(3.14159265358979323846/8)};
    code(apply(0,{rotated}),"NOT_AUTHORIZED");
    auto overflow=move(west,-3);
    overflow.scale.fill(std::numeric_limits<double>::max());
    code(apply(0,{overflow}),"NOT_AUTHORIZED");
    require(world.canonical_bytes()==before,"foreign and rotated/overflow failure stable");
    auto negative=move(west,-1.5);negative.scale={-1,-1,-1};
    require(apply(0,{negative}).status=="committed","inclusive bounds and negative scale permitted");
    const auto stable=world.canonical_bytes(); const auto quota_before=ledger.usage();
    for(int i=0;i<100;++i) {
      code(apply(0,{move(west,-4),move(TemporaryTarget{"missing"},-3)}),"NOT_FOUND");
      require(world.canonical_bytes()==stable && ledger.usage()==quota_before,"late prefix rolls back world and resources");
    }
    code(apply(0,{move(west,-4),move(west,-4),move(west,-4),move(west,-4),move(west,-4)}),"BUDGET_EXCEEDED");
    auto huge=batch(world.snapshot().world_revision,{move(west,-4)});
    huge.idempotency.epoch=std::string(20000,'x');
    code(ow::policy::apply(world,huge,ledger,0),"BUDGET_EXCEEDED");
    {
      auto lease=ledger.admit(0,0);Guard guard(lease);
      code(ow::transactions::Coordinator::apply_at_boundary(world,batch(world.snapshot().world_revision,{move(west,-4)}),&guard),"BUDGET_EXCEEDED");
      code(ow::transactions::Coordinator::apply_at_boundary(world,batch(world.snapshot().world_revision,{move(west,-4)}),&guard),"NOT_AUTHORIZED");
    }
    {
      auto lease=ledger.admit(0,1024);
      {
        Guard guard(lease);
        code(ow::transactions::Coordinator::apply_at_boundary(world,batch(world.snapshot().world_revision,{move(west,3)}),&guard),"NOT_AUTHORIZED");
      }
      Guard replacement(lease);
      const auto rejected=ow::transactions::Coordinator::apply_at_boundary(world,batch(world.snapshot().world_revision,{move(west,-4)}),&replacement);
      code(rejected,"NOT_AUTHORIZED");
      require(rejected.transaction_id=="018f7242-4387-7c98-a114-67787915a399","policy rejection preserves receipt correlation");
    }
    require(world.canonical_bytes()==stable && ledger.usage()==quota_before,"native undersized/reused reservation cannot bypass");

    require(apply(0,{EntityDelete{west}}).status=="committed","delete releases live payload");
    require(ledger.usage().retained[0]==128,"tombstone sponsor remains");
    const auto tombstone=world.canonical_bytes();const auto tombstone_usage=ledger.usage();
    for(int i=0;i<50;++i) {
      code(apply(1,{EntityCreate{"B","builtin.unit_cube"},move(TemporaryTarget{"B"},-3)}),"NOT_AUTHORIZED");
      require(world.canonical_bytes()==tombstone && ledger.usage()==tombstone_usage,"failed sponsor transfer rolls back");
    }
    const auto reused=apply(1,{EntityCreate{"B","builtin.unit_cube"},move(TemporaryTarget{"B"},3)});
    require(reused.status=="committed" && reused.created.front().entity_uuid==west.entity_uuid &&
            reused.created.front().generation==2,"slot reuse preserves UUID and advances generation");
    require(ledger.usage().retained==std::array<std::size_t,2>{0,384},"successful reuse atomically transfers sponsor");
    code(apply(0,{move(west,-3)}),"STALE_HANDLE");
    const EntityTarget east{"workshop",west.entity_uuid,2};
    code(apply(0,{EntityDelete{east}}),"NOT_AUTHORIZED");
    require(apply(0,{EntityCreate{"C","builtin.unit_cube"},move(TemporaryTarget{"C"},-3)}).status=="committed","west refunded sponsor can recover");

    ow::world::World another("workshop",7,8);
    code(ow::policy::apply(another,batch(0,{EntityCreate{"A","builtin.unit_cube"},move(TemporaryTarget{"A"},-3)}),ledger,0),"REQUIRES_RESYNC");
    require(another.snapshot().slots.empty(),"ledger cannot be reused for a different world");
    std::cout<<"policy.native passed "<<checks<<" assertions\n";
    return 0;
  } catch(const std::exception& error) {std::cerr<<"policy.native failed: "<<error.what()<<'\n';}
  catch(...) {std::cerr<<"policy.native failed: unexpected exception\n";}
  return 1;
}
