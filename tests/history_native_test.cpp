// SPDX-License-Identifier: Apache-2.0
#include "omniweft/history.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>

namespace {
using namespace ow::commands;
using ow::history::Action;
unsigned checks=0;
void check(bool v,const char* label) {++checks;if(!v)throw std::runtime_error(label);}
EntityTarget id(unsigned n=1) {return {"workshop",n==1?"00000007-0000-4000-8000-000000000001":"00000007-0000-4000-8000-000000000002",1};}
Envelope e(std::uint64_t revision,std::vector<Operation> ops={}) {
 Envelope r;r.world_id="workshop";r.transaction_id="018f7242-4387-7c98-a114-000000120001";r.idempotency={"native-fixture-epoch",1};
 r.expected_world_revision=revision;r.apply_at={"next_tick",120};r.budget={4,0};r.operations=std::move(ops);return r;
}
TransformSet move(double x,unsigned n=1) {return {id(n),{x,0,0}};}
void committed(const ow::transactions::Receipt& r) {check(r.status=="committed" && r.errors.empty(),"committed receipt");}
void denied(const ow::transactions::Receipt& r,const char* code) {check(r.status=="rejected" && r.created.empty() && r.errors.size()==1 && r.errors[0].code==code,"explicit rejection");}
void setup(ow::world::World& w) {committed(ow::transactions::Coordinator::apply_at_boundary(w,e(0,{EntityCreate{"A","builtin.unit_cube"},EntityCreate{"B","builtin.unit_cube"}})));}
struct Deny final:ow::transactions::StagingGuard {
 void begin(const ow::world::World&,const ow::world::Snapshot&,const Envelope&) override {}
 void create(std::size_t,std::size_t) override {}
 void transform(std::size_t,const ow::world::Transform&,const ow::world::Transform&,std::size_t) override {}
 void erase(std::size_t,const ow::world::Transform&,std::size_t) override {}
 void finish(const ow::world::Snapshot&) override {throw ow::transactions::Error{"NOT_AUTHORIZED","/fixture","Fixture denial.",std::nullopt};}
 void commit() noexcept override {std::terminate();}
};
}
int main() {
 try {
  ow::world::World w("workshop",7,8);setup(w);ow::history::History h(w);
  const auto baseline=w.snapshot();
  committed(h.apply_at_boundary(e(1,{move(2),move(4),EntityTagsSet{id(),{"z","a"}}})));
  check(w.snapshot().slots[0].entity->transform.position_m[0]==4,"duplicate forward property final value");
  auto bytes=w.canonical_bytes();auto state=h.state();
  auto small=e(2);small.budget.max_operations=2;
  denied(h.navigate_at_boundary(Action::undo,small),"BUDGET_EXCEEDED");
  check(w.canonical_bytes()==bytes && h.state()==state,"inverse budget failure atomic");
  Deny guard;denied(h.navigate_at_boundary(Action::undo,e(2),&guard),"NOT_AUTHORIZED");
  check(w.canonical_bytes()==bytes && h.state()==state,"staging hook denies inverse without publication");
  committed(h.navigate_at_boundary(Action::undo,e(2)));
  auto s=w.snapshot();check(s.world_revision==3 && s.slots[0].entity->authoring_revision==3,"undo monotonic revisions");
  check(s.slots[0].entity->transform==baseline.slots[0].entity->transform && s.slots[0].entity->tags.empty(),"exact duplicate inverse content");
  check(s.slots[0].generation==1 && s.slots.size()==2 && s.slots[1]==baseline.slots[1],"identity and untouched state preserved");
  bytes=w.canonical_bytes();state=h.state();
  auto invalid=move(9);invalid.scale[0]=0;
  denied(h.apply_at_boundary(e(3,{move(8),invalid})),"INVALID_SCHEMA");
  check(w.canonical_bytes()==bytes && h.state()==state,"late semantic failure preserves redo");
  denied(h.apply_at_boundary(e(3,{move(8)}),&guard),"NOT_AUTHORIZED");
  check(w.canonical_bytes()==bytes && h.state()==state,"guard failure preserves redo");
  committed(h.navigate_at_boundary(Action::redo,e(3)));
  check(w.snapshot().slots[0].entity->transform.position_m[0]==4,"redo restored forward value");
  committed(h.navigate_at_boundary(Action::undo,e(4)));
  committed(h.apply_at_boundary(e(5,{move(7)})));
  check(h.state().entries==1 && h.state().cursor==1,"successful branch truncates redo tail");
  denied(h.navigate_at_boundary(Action::redo,e(6)),"NOT_FOUND");
  bytes=w.canonical_bytes();state=h.state();
  denied(h.navigate_at_boundary(Action::rewind_simulation,e(6)),"UNSUPPORTED_OPERATION");
  denied(h.navigate_at_boundary(static_cast<Action>(99),e(6)),"UNSUPPORTED_OPERATION");
  denied(h.navigate_at_boundary(Action::undo,e(6,{move(0)})),"INVALID_SCHEMA");
  denied(h.navigate_at_boundary(Action::undo,e(5)),"REVISION_CONFLICT");
  check(w.canonical_bytes()==bytes && h.state()==state,"negative navigation leaves state");
  committed(ow::transactions::Coordinator::apply_at_boundary(w,e(6,{move(11,2)})));
  bytes=w.canonical_bytes();state=h.state();
  denied(h.navigate_at_boundary(Action::undo,e(7)),"REVISION_CONFLICT");
  denied(h.apply_at_boundary(e(7,{move(9)})),"REVISION_CONFLICT");
  check(w.canonical_bytes()==bytes && h.state()==state,"outside edit cannot silently retarget history");
  h.clear_at_boundary();check(w.canonical_bytes()==bytes && h.state()==ow::history::State{0,0,7},"explicit clear preserves world");
  for(unsigned n=0;n<8;++n) {
   committed(h.apply_at_boundary(e(7+n,{move(n)})));
   check(h.state().entries==n+1 && h.state().cursor==n+1,"bounded history depth");
  }
  bytes=w.canonical_bytes();state=h.state();denied(h.apply_at_boundary(e(15,{move(20)})),"BUDGET_EXCEEDED");
  check(w.canonical_bytes()==bytes && h.state()==state,"full history rejects before commit");
  for(unsigned n=0;n<8;++n)committed(h.navigate_at_boundary(Action::undo,e(15+n)));
  check(w.snapshot().slots[0].entity->transform.position_m[0]==7,"full undo returns authored baseline");
  denied(h.navigate_at_boundary(Action::undo,e(23)),"NOT_FOUND");
  for(unsigned n=0;n<8;++n)committed(h.navigate_at_boundary(Action::redo,e(23+n)));
  check(w.snapshot().slots[0].entity->transform.position_m[0]==7,"full redo final authored value");
  h.clear_at_boundary();bytes=w.canonical_bytes();state=h.state();
  for(unsigned kind=0;kind<7;++kind) {
   auto bad=e(31,{move(1)});
   if(kind==0)bad.transaction_id=std::string(100000,'x');
   if(kind==1)bad.idempotency.epoch=std::string(100000,'x');
   if(kind==2)bad.operations.resize(10000,move(1));
   if(kind==3)bad.operations={EntityTagsSet{id(),std::vector<std::string>(10000,"x")}};
   if(kind==4)bad.operations={EntityTagsSet{id(),{std::string(100000,'x')}}};
   if(kind==5)bad.operations={TransformSet{EntityTarget{std::string(100000,'x'),id().entity_uuid,1}}};
   if(kind==6)bad.budget.max_operations=0;
   denied(h.apply_at_boundary(bad),"BUDGET_EXCEEDED");
   check(w.canonical_bytes()==bytes && h.state()==state,"bounded typed input rejection unchanged");
  }
  for(const Operation& op:{Operation(EntityCreate{"X","builtin.unit_cube"}),Operation(EntityDelete{id()}),Operation(EntityReparent{id(),id(2),"preserve_world"})}) {
   denied(h.apply_at_boundary(e(31,{move(2),op})),"UNSUPPORTED_OPERATION");
   check(w.canonical_bytes()==bytes && h.state()==state,"unsupported late operation never applies prefix");
  }
  auto stale=id();stale.generation=2;denied(h.apply_at_boundary(e(31,{TransformSet{stale}})),"STALE_HANDLE");
  check(w.canonical_bytes()==bytes && h.state()==state,"generation mismatch atomic");
  committed(ow::transactions::Coordinator::apply_at_boundary(w,e(31,{EntityReparent{id(2),id(),"preserve_world"}})));
  h.clear_at_boundary();bytes=w.canonical_bytes();state=h.state();
  denied(h.apply_at_boundary(e(32,{move(1)})),"UNSUPPORTED_OPERATION");
  denied(h.apply_at_boundary(e(32,{move(1,2)})),"UNSUPPORTED_OPERATION");
  check(w.canonical_bytes()==bytes && h.state()==state,"parent and child transforms unsupported");
  committed(h.apply_at_boundary(e(32,{EntityTagsSet{id(2),{"tag"}}})));
  committed(h.navigate_at_boundary(Action::undo,e(33)));
  s=w.snapshot();check(s.slots[1].entity->parent && s.slots[1].entity->tags.empty(),"tag inverse retains hierarchy");
  check(s.slots[1].entity->transform.position_m[0]==11 && s.slots[1].entity->local_transform.position_m[0]==4,"tag inverse preserves local and world transforms");
  std::cout<<"{\"status\":\"passed\",\"assertions\":"<<checks<<"}\n";return 0;
 } catch(const std::exception& error) {std::cerr<<"history native failed: "<<error.what()<<'\n';return 1;}
}
