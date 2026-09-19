// SPDX-License-Identifier: Apache-2.0
// Independent public-API oracle: literal rational rotation, graph and packet expectations.
#include <omniweft/transactions.hpp>
#include <omniweft/presentation.hpp>
#include <omniweft/policy.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using namespace ow::commands;
using ow::world::World;
using ow::transactions::Coordinator;
unsigned checks=0;
void require(bool test, const char* label) { ++checks; if (!test) throw std::runtime_error(label); }
Envelope batch(const World& world, std::vector<Operation> operations) {
  Envelope e;
  e.world_id="workshop"; e.transaction_id="018f7242-4387-7c98-a114-67787915a399";
  e.idempotency={"fixture-epoch",1}; e.expected_world_revision=world.snapshot().world_revision;
  e.apply_at={"next_tick",120}; e.budget={operations.size(),0}; e.operations=std::move(operations);
  return e;
}
EntityTarget id(unsigned n, std::uint64_t generation=1) {
  return {"workshop","00000007-0000-4000-8000-00000000000"+std::to_string(n),generation};
}
EntityCreate create(const char* name) { return {name,"builtin.unit_cube"}; }
void commit(World& world, std::vector<Operation> operations) {
  const auto revision=world.snapshot().world_revision;
  const auto result=Coordinator::apply_at_boundary(world,batch(world,std::move(operations)));
  require(result.status=="committed" && result.errors.empty() && result.world_revision==revision+1,"expected commit");
}
void reject(World& world, std::vector<Operation> operations, const char* code, std::size_t index) {
  const auto before=world.snapshot(); const auto bytes=world.canonical_bytes();
  const auto result=Coordinator::apply_at_boundary(world,batch(world,std::move(operations)));
  require(result.status=="rejected" && result.created.empty() && result.errors.size()==1 &&
    result.errors[0].code==code && result.errors[0].operation_index==index &&
    result.world_revision==before.world_revision,"literal rejected receipt");
  require(world.snapshot()==before && world.canonical_bytes()==bytes,"complete rejection rollback");
}
template<std::size_t N> void near(const std::array<double,N>& actual, const std::array<double,N>& expected) {
  for (std::size_t i=0;i<N;++i)
    require(std::isfinite(actual[i]) && std::abs(actual[i]-expected[i])<=1e-10,"independent derived value");
}
void value(const World& world, unsigned n, const ow::world::Transform& expected) {
  const auto actual=world.snapshot().slots[n-1].entity->transform;
  near(actual.position_m,expected.position_m); near(actual.rotation_xyzw,expected.rotation_xyzw); near(actual.scale,expected.scale);
}
}
int main() {
  try {
    World w("workshop",7,8);
    commit(w,{create("P"), TransformSet{TemporaryTarget{"P"},{10,-2,3},{0,0,.6,.8},{2,2,2}},
      create("C"), TransformSet{TemporaryTarget{"C"},{1,2,-1},{0,0,0,1},{-1,2,3}},
      EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_local"}});
    value(w,2,{{6.72,1.04,1},{0,0,.6,.8},{-2,4,6}});
    require(w.snapshot().format_version==2 && w.snapshot().slots[1].entity->parent->generation==1,"generation-bearing edge and format2");
    const auto packet=ow::presentation::make_packet(w.snapshot());
    require(packet.objects.size()==2 && packet.vertices.size()==48,"hierarchical packet shape");
    // First +X-face corner local(.5,-.5,-.5), signed world scale(-2,4,6),
    // rational Z rotation x'=.28x-.96y, y'=.96x+.28y, world position(6.72,1.04,1).
    const std::array<double,4> clip{8.36/4,.48/3,6.9/19.9,1};
    for (std::size_t i=0;i<4;++i)
      require(std::abs(static_cast<double>(packet.vertices[24].clip_position[i])-clip[i])<2e-6,"literal cached-world packet corner");
    const auto preserved=w.snapshot().slots[1].entity->transform;
    commit(w,{create("Q"),TransformSet{TemporaryTarget{"Q"},{-4,1,2},{1,0,0,0},{.5,.5,.5}},
      EntityReparent{id(2),TemporaryTarget{"Q"},"preserve_world"}});
    require(w.snapshot().slots[1].entity->transform==preserved,"preserve-world target is byte-exact");
    const auto local=w.snapshot().slots[1].entity->local_transform;
    near(local.position_m,std::array<double,3>{21.44,-.08,2});
    near(local.rotation_xyzw,std::array<double,4>{-.8,.6,0,0});
    near(local.scale,std::array<double,3>{-4,8,12});
    commit(w,{EntityReparent{id(2),std::nullopt,"preserve_local"}});
    value(w,2,local);
    require(w.snapshot().format_version==1 && !w.snapshot().slots[1].entity->parent &&
      w.snapshot().slots[1].entity->local_transform==ow::world::Transform{},"detach clears metadata and restores root format");
    commit(w,{EntityReparent{id(2),id(1),"preserve_world"}});
    const auto detached=w.snapshot().slots[1].entity->transform;
    commit(w,{EntityReparent{id(2),std::nullopt,"preserve_world"}});
    require(w.snapshot().slots[1].entity->transform==detached,"preserve-world detach byte-exact");
    reject(w,{EntityReparent{id(1),id(1),"preserve_world"}},"INVALID_SCHEMA",0);
    reject(w,{EntityReparent{id(1),id(2),"preserve_local"}},"INVALID_SCHEMA",0);
    auto foreign=id(1); foreign.world_id="elsewhere";
    reject(w,{EntityReparent{id(2),foreign,"preserve_world"}},"NOT_FOUND",0);
    reject(w,{EntityReparent{id(2),TemporaryTarget{"later"},"preserve_world"},create("later")},"NOT_FOUND",0);
    reject(w,{EntityReparent{id(2),std::nullopt,"bad_mode"}},"INVALID_SCHEMA",0);

    World chain("workshop",7,8), control("workshop",7,8);
    const std::vector<Operation> setup{create("P"),TransformSet{TemporaryTarget{"P"},{10,0,0},{0,0,1,0},{2,2,2}},
      create("C"),TransformSet{TemporaryTarget{"C"},{1,0,0}},create("G"),TransformSet{TemporaryTarget{"G"},{0,1,0}},
      EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_local"},
      EntityReparent{TemporaryTarget{"G"},TemporaryTarget{"C"},"preserve_local"}};
    commit(chain,setup); commit(control,setup);
    value(chain,3,{{8,-2,0},{0,0,1,0},{2,2,2}});
    reject(chain,{TransformSet{id(1),{20,0,0}},create("rollback"),EntityReparent{id(1),id(3),"preserve_world"}},"INVALID_SCHEMA",2);
    reject(chain,{EntityDelete{id(1)}},"INVALID_SCHEMA",0);
    reject(chain,{TransformSet{id(1),{0,0,0},{0,0,0,1},{1,2,1}}},"INVALID_SCHEMA",0);
    reject(chain,{TransformSet{id(2),{0,0,0},{0,0,0,1},{-1,-1,-1}}},"INVALID_SCHEMA",0);
    commit(chain,{create("recovery")}); commit(control,{create("recovery")});
    require(chain.snapshot()==control.snapshot() && chain.canonical_bytes()==control.canonical_bytes(),"failed allocation and graph prefix recover identically to control");
    commit(chain,{TransformSet{id(1),{20,0,0},{0,0,0,1},{3,3,3}}});
    value(chain,2,{{23,0,0},{0,0,0,1},{3,3,3}}); value(chain,3,{{23,3,0},{0,0,0,1},{3,3,3}});
    const auto revision=chain.snapshot().world_revision;
    require(chain.snapshot().slots[0].entity->authoring_revision==revision &&
      chain.snapshot().slots[1].entity->authoring_revision==revision &&
      chain.snapshot().slots[2].entity->authoring_revision==revision &&
      chain.snapshot().slots[3].entity->authoring_revision==2,"affected subtree revision policy");
    commit(chain,{TransformSet{id(2),{30,0,0},{0,0,0,1},{6,6,6}}});
    value(chain,3,{{30,6,0},{0,0,0,1},{6,6,6}});
    near(chain.snapshot().slots[1].entity->local_transform.position_m,std::array<double,3>{10.0/3,0,0});
    // Prefix order: detaching C permits P-under-C, then reattaching C-under-P must reject.
    reject(chain,{EntityReparent{id(2),std::nullopt,"preserve_world"},
      EntityReparent{id(1),id(2),"preserve_world"},EntityReparent{id(2),id(1),"preserve_world"}},"INVALID_SCHEMA",2);
    commit(chain,{EntityReparent{id(3),std::nullopt,"preserve_world"},EntityDelete{id(2)},create("reusedChild")});
    require(chain.snapshot().slots[1].generation==2 && !chain.snapshot().slots[1].entity->parent,"child slot reused without edge");
    reject(chain,{EntityReparent{id(2),id(1),"preserve_world"}},"STALE_HANDLE",0);
    reject(chain,{EntityReparent{id(3),id(2),"preserve_world"}},"STALE_HANDLE",0);
    commit(chain,{EntityDelete{id(1)},create("reusedParent")});
    reject(chain,{EntityReparent{id(3),id(1),"preserve_world"}},"STALE_HANDLE",0);
    commit(chain,{EntityReparent{id(3),id(1,2),"preserve_local"},EntityDelete{id(3)},EntityDelete{id(1,2)}});
    require(chain.snapshot().format_version==1,"children-first deletion restores format1");

    for (const auto scales : {std::array<double,3>{-1,-1,-1},std::array<double,3>{1,2,1}}) {
      World invalid("workshop",7,4);
      commit(invalid,{create("P"),TransformSet{TemporaryTarget{"P"},{0,0,0},{0,0,0,1},scales},create("C")});
      reject(invalid,{EntityReparent{id(2),id(1),"preserve_world"}},"INVALID_SCHEMA",0);
    }
    for (const double scale : {1e-300,1e300}) {
      World extremes("workshop",7,4);
      commit(extremes,{create("P"),TransformSet{TemporaryTarget{"P"},{0,0,0},{0,0,0,1},{scale,scale,scale}},
        create("C"),TransformSet{TemporaryTarget{"C"},{1e300,0,0},{0,0,0,1},{1e-300,1e-300,1e-300}}});
      reject(extremes,{EntityReparent{id(2),id(1),"preserve_world"}},"INVALID_SCHEMA",0);
    }
    World overflow("workshop",7,4);
    commit(overflow,{create("P"),TransformSet{TemporaryTarget{"P"},{0,0,0},{0,0,0,1},{1e300,1e300,1e300}},
      create("C"),TransformSet{TemporaryTarget{"C"},{1e300,0,0}}});
    reject(overflow,{EntityReparent{id(2),id(1),"preserve_local"}},"INVALID_SCHEMA",0);
    World underflow("workshop",7,4);
    commit(underflow,{create("P"),TransformSet{TemporaryTarget{"P"},{0,0,0},{0,0,0,1},{1e-300,1e-300,1e-300}},
      create("C"),TransformSet{TemporaryTarget{"C"},{0,0,0},{0,0,0,1},{1e-300,1e-300,1e-300}}});
    reject(underflow,{EntityReparent{id(2),id(1),"preserve_local"}},"INVALID_SCHEMA",0);

    World deep("workshop",7,128);
    std::vector<Operation> many;
    for (unsigned i=0;i<80;++i) {
      const auto name="node"+std::to_string(i);
      many.emplace_back(EntityCreate{name,"builtin.unit_cube"});
      if (i) many.emplace_back(EntityReparent{TemporaryTarget{name},TemporaryTarget{"node"+std::to_string(i-1)},"preserve_local"});
    }
    commit(deep,many);
    commit(deep,{TransformSet{id(1),{1,2,3}}});
    for (const auto& entry : deep.snapshot().slots)
      require(entry.entity->transform.position_m==std::array<double,3>{1,2,3} &&
        entry.entity->authoring_revision==2,"iterative deep subtree propagation");
    const auto last=deep.snapshot().slots.back();
    reject(deep,{EntityReparent{id(1),EntityTarget{"workshop",last.entity_uuid,1},"preserve_local"}},"INVALID_SCHEMA",0);
    auto typed=batch(deep,{EntityReparent{id(1),std::nullopt,"preserve_world"}});
    const auto serialized=serialize(typed);
    require(serialized.json.has_value() && parse(*serialized.json).envelope==typed,"native reparent round trip");
    const auto valid=*serialized.json;
    for (const std::string parent : {std::string("false"),std::string("{}"),std::string("{\"temporary_id\":\"P\",\"generation\":1}")}) {
      auto raw=valid; const auto start=raw.find("\"parent\":null");
      require(start!=std::string::npos,"literal parent wire field");
      raw.replace(start,13,"\"parent\":"+parent);
      const auto parsed=parse(raw);
      require(!parsed.envelope && parsed.errors.size()==1 && parsed.errors[0].code=="INVALID_SCHEMA","invalid parent structure denied");
    }

    World policy_world("workshop",7,8); ow::policy::Ledger ledger;
    auto forbidden=batch(policy_world,{create("C"),EntityReparent{TemporaryTarget{"C"},std::nullopt,"preserve_world"}});
    // Large parent data must not be copied/serialized before the bounded unsupported-operation scan.
    std::get<EntityReparent>(forbidden.operations[1]).parent=TemporaryTarget{std::string(20000,'x')};
    const auto zero=policy_world.canonical_bytes();
    auto denied=ow::policy::apply(policy_world,forbidden,ledger,1);
    require(denied.status=="rejected" && denied.errors.size()==1 && denied.errors[0].code=="UNSUPPORTED_OPERATION" &&
      policy_world.canonical_bytes()==zero && ledger.usage()==ow::policy::Usage{},"typed policy pre-admission gate and refund");
    {
      auto lease=ledger.admit(1,1024); ow::policy::Guard guard(lease);
      denied=Coordinator::apply_at_boundary(policy_world,forbidden,&guard);
      require(denied.errors.size()==1 && denied.errors[0].code=="UNSUPPORTED_OPERATION","direct policy guard gate");
    }
    require(ledger.usage()==ow::policy::Usage{},"direct denied lease refunds");
    commit(policy_world,{create("P"),create("C"),EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_world"}});
    const auto hierarchical=policy_world.canonical_bytes();
    denied=ow::policy::apply(policy_world,batch(policy_world,{TransformSet{id(1),{3,0,0}}}),ledger,1);
    require(denied.errors.size()==1 && denied.errors[0].code=="UNSUPPORTED_OPERATION" &&
      policy_world.canonical_bytes()==hierarchical && ledger.usage()==ow::policy::Usage{},"existing hierarchical world cannot enter old policy");
    std::cout << "{\"status\":\"passed\",\"assertions\":" << checks << "}\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "hierarchy_native_test failed: " << error.what() << '\n'; return 1;
  }
}
