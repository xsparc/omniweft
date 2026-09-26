// SPDX-License-Identifier: Apache-2.0
#include "omniweft/replay.hpp"
#include "omniweft/presentation.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <new>

namespace {
using namespace ow::commands;
unsigned checks=0;
void check(bool v,const char* label){++checks;if(!v)throw std::runtime_error(label);}
Envelope request(std::uint64_t rev,std::vector<Operation> ops) {
 Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-000000130001";
 e.idempotency={"private-native-session",1};e.expected_world_revision=rev;e.apply_at={"next_tick",120};
 e.budget={4,0};e.operations=std::move(ops);return e;
}
EntityTarget id(unsigned n=1,std::uint64_t generation=1){return {"workshop",n==1?"00000007-0000-4000-8000-000000000001":"00000007-0000-4000-8000-000000000002",generation};}
TransformSet move(Target target,double x){return {std::move(target),{x,0,0}};}
void committed(const ow::transactions::Receipt& r){check(r.status=="committed" && r.errors.empty(),"commit");}
void denied(const ow::transactions::Receipt& r,const char* code){check(r.status=="rejected" && r.created.empty() && r.errors.size()==1 && r.errors[0].code==code,"rejection");}
struct FailObserver final:ow::transactions::CommitObserver {
 bool allocation=false;
 void prepare(const ow::world::Snapshot& s,const ow::transactions::Receipt& r) override {
  check(s.world_revision==r.world_revision && r.status=="committed" && !r.created.empty(),"observer receives final staged revision and mapping");
  if(allocation)throw std::bad_alloc();
  throw ow::transactions::Error{"BUDGET_EXCEEDED","/fixture","Fixture observer rejection.",std::nullopt};
 }
 void publish() noexcept override {std::terminate();}
};
struct Deny final:ow::transactions::StagingGuard {
 void begin(const ow::world::World&,const ow::world::Snapshot&,const Envelope&) override {}
 void create(std::size_t,std::size_t) override {}
 void transform(std::size_t,const ow::world::Transform&,const ow::world::Transform&,std::size_t) override {}
 void erase(std::size_t,const ow::world::Transform&,std::size_t) override {}
 void finish(const ow::world::Snapshot&) override {throw ow::transactions::Error{"NOT_AUTHORIZED","/fixture","Fixture rejection.",std::nullopt};}
 void commit() noexcept override {std::terminate();}
};
struct Allow final:ow::transactions::StagingGuard {
 bool published=false;
 void begin(const ow::world::World&,const ow::world::Snapshot&,const Envelope&) override {}
 void create(std::size_t,std::size_t) override {}
 void transform(std::size_t,const ow::world::Transform&,const ow::world::Transform&,std::size_t) override {}
 void erase(std::size_t,const ow::world::Transform&,std::size_t) override {}
 void finish(const ow::world::Snapshot&) override {}
 void commit() noexcept override {published=true;}
};
}
int main(){try{
 using namespace ow::replay;const std::array<Asset,1> assets{Asset{}};
 ow::world::World asset_world("workshop",7,8);
 committed(ow::transactions::Coordinator::apply_at_boundary(asset_world,request(0,{EntityCreate{"cube","builtin.unit_cube"}})));
 const auto packet=ow::presentation::make_packet(asset_world.snapshot());
 const auto manifest=nlohmann::json::parse(assets[0].manifest);
 check(packet.vertices.size()==24 && packet.indices.size()==36,"builtin mesh descriptor size");
 for(std::size_t face=0;face<6;++face){
  for(std::size_t corner=0;corner<4;++corner){
   const auto local=manifest["faces_m"][face][corner].get<std::array<double,3>>();
   const auto& c=packet.camera;const auto& v=packet.vertices[face*4+corner];
   const std::array<float,4> expected{
    static_cast<float>((2*(local[0]-c.position_m[0])-c.right-c.left)/(c.right-c.left)),
    static_cast<float>(-(2*(local[1]-c.position_m[1])-c.top-c.bottom)/(c.top-c.bottom)),
    static_cast<float>((c.position_m[2]-local[2]-c.near_m)/(c.far_m-c.near_m)),1};
   check(v.clip_position==expected,"compiled builtin positions match content descriptor");
   for(std::size_t channel=0;channel<4;++channel)
    check(v.color[channel]==static_cast<float>(manifest["face_rgba8"][face][channel].get<unsigned>())/255.0F,"compiled builtin colors match descriptor");
  }
  for(std::size_t i=0;i<6;++i)check(packet.indices[face*6+i]==face*4+manifest["face_triangles"][i].get<unsigned>(),"compiled triangles match descriptor");
 }
 ow::world::World w("workshop",7,8);Recorder recorder(w);const auto empty=w.canonical_bytes();
 FailObserver observer;
 auto first=request(0,{EntityCreate{"A","builtin.unit_cube"},EntityCreate{"B","builtin.unit_cube"},move(TemporaryTarget{"A"},-2),move(TemporaryTarget{"B"},2)});
 denied(ow::transactions::Coordinator::apply_at_boundary(w,first,nullptr,&observer),"BUDGET_EXCEEDED");
 check(w.canonical_bytes()==empty,"observer rejection keeps allocator/world");
 Allow allow;
 denied(ow::transactions::Coordinator::apply_at_boundary(w,first,&allow,&observer),"BUDGET_EXCEEDED");
 check(!allow.published && w.canonical_bytes()==empty && recorder.size()==0,"observer failure precedes guard and world publication");
 observer.allocation=true;bool threw=false;
 try{ow::transactions::Coordinator::apply_at_boundary(w,first,nullptr,&observer);}catch(const std::bad_alloc&){threw=true;}
 check(threw && w.canonical_bytes()==empty,"allocation exception before publication");
 Deny guard;denied(recorder.apply_at_boundary(first,1,&guard),"NOT_AUTHORIZED");
 check(recorder.size()==0 && w.canonical_bytes()==empty,"staging rejection keeps log and world");
 committed(recorder.apply_at_boundary(first,1));
 committed(recorder.apply_at_boundary(request(1,{EntityTagsSet{id(),{"red"}},move(id(),-3)}),2));
 committed(recorder.apply_at_boundary(request(2,{EntityDelete{id(2)},EntityCreate{"C","builtin.unit_cube"},move(TemporaryTarget{"C"},4)}),2));
 const auto log=recorder.export_log();const auto before=w.canonical_bytes();
 check(log.records.size()==3 && log.records[2].created.size()==1,"accepted count and mapping");
 check(log.records[2].created[0].entity_uuid==id(2).entity_uuid && log.records[2].created[0].generation==2,"same-batch reuse has recorded generation");
 auto rebuilt=reconstruct(log,assets);check(rebuilt.world && rebuilt.errors.empty(),"replay success");
 check(rebuilt.world->snapshot()==w.snapshot() && rebuilt.world->canonical_bytes()==before,"exact all state and canonical bytes");
 for(std::size_t n=0;n<log.records.size();++n){auto prefix=log;prefix.records.resize(n+1);auto r=reconstruct(prefix,assets);check(r.world && r.world->canonical_bytes()==log.records[n].checkpoint,"every checkpoint exact");}
 for(unsigned kind=0;kind<20;++kind){
  auto bad=log;
  switch(kind){
   case 0:bad.schema=2;break;
   case 1:bad.command_schema="future";break;
   case 2:bad.checkpoint_schema="future";break;
   case 3:bad.assets.clear();break;
   case 4:bad.assets[0].revision=2;break;
   case 5:bad.records.back().checkpoint.back()^=1;break;
   case 6:bad.records[0].created[0].generation=2;break;
   case 7:bad.records[0].created.clear();break;
   case 8:bad.records.back().created.push_back(bad.records[0].created[0]);break;
   case 9:std::swap(bad.records[0].created[0],bad.records[0].created[1]);break;
   case 10:bad.records[1].sequence=8;break;
   case 11:bad.records[2].boundary_tick=0;break;
   case 12:bad.records[1].expected_revision=0;break;
   case 13:bad.seed=8;break;
   case 14:bad.records[0].operations[0]=EntityCreate{"A","builtin.missing"};break;
   case 15:bad.records[1].operations[1]=move(id(),std::numeric_limits<double>::infinity());break;
   case 16:bad.records[0].created[0].world_id="other";break;
   case 17:bad.world_id="invalid/world";break;
   case 18:bad.assets[0].content_sha256[0]='x';break;
   default:bad.assets[0].manifest+=' ';break;
  }
  auto r=reconstruct(bad,assets);check(!r.world && r.errors.size()==1,"invalid log returns no partial world");
  check(w.canonical_bytes()==before && recorder.size()==3,"failed private replay leaves source unchanged");
 }
 check(!reconstruct(log,{}).world,"missing available asset fails");
 auto wrong_assets=assets;wrong_assets[0].content_sha256[0]='x';check(!reconstruct(log,wrong_assets).world,"wrong available asset content fails");
 check(reconstruct(log,assets).world->canonical_bytes()==before,"valid replay recovers after corrupt attempts");
 auto badmove=move(id(),9);badmove.scale[0]=0;
 denied(recorder.apply_at_boundary(request(3,{move(id(),8),badmove}),3),"INVALID_SCHEMA");
 check(w.canonical_bytes()==before && recorder.size()==3,"late command failure omitted atomically");
 denied(recorder.apply_at_boundary(request(3,{move(id(),0)}),1),"INVALID_REPLAY");
 for(unsigned n=3;n<8;++n)committed(recorder.apply_at_boundary(request(n,{move(id(),n)}),n));
 const auto full=w.canonical_bytes();denied(recorder.apply_at_boundary(request(8,{move(id(),10)}),9),"BUDGET_EXCEEDED");
 check(w.canonical_bytes()==full && recorder.size()==8,"full recorder rejects before publication");
 check(reconstruct(recorder.export_log(),assets).world->canonical_bytes()==full,"full log replay");
 for(unsigned kind=0;kind<7;++kind){auto bad=log;
  if(kind==0)bad.records.resize(10000);
  if(kind==1)bad.records[0].operations.resize(10000,EntityCreate{"A","builtin.unit_cube"});
  if(kind==2)bad.records[0].checkpoint.resize(100000);
  if(kind==3)bad.records[0].created.resize(10000);
  if(kind==4)bad.records[0].operations={EntityTagsSet{id(),std::vector<std::string>(10000,"tag")}};
  if(kind==5)bad.max_slots=1024;
  if(kind==6)bad.records[0].operations={move(EntityTarget{std::string(100000,'x'),id().entity_uuid,1},2)};
  check(!reconstruct(bad,assets).world,"bounded typed log rejects");
 }
 committed(ow::transactions::Coordinator::apply_at_boundary(w,request(8,{move(id(),11)})));
 denied(recorder.apply_at_boundary(request(9,{move(id(),12)}),10),"REVISION_CONFLICT");
 threw=false;try{recorder.export_log();}catch(const ow::transactions::Error& e){threw=e.code=="REVISION_CONFLICT";}
 check(threw,"outside edit invalidates export");
 threw=false;try{Recorder invalid(w);}catch(const ow::transactions::Error&){threw=true;}check(threw,"nonempty initial world rejected");
 ow::world::World fresh("workshop",7,8);Recorder bounded_recorder(fresh);
 for(unsigned kind=0;kind<4;++kind){auto bad=first;
  if(kind==0)bad.idempotency.epoch=std::string(100000,'x');
  if(kind==1)bad.operations.resize(10000,EntityCreate{"A","builtin.unit_cube"});
  if(kind==2)bad.transaction_id=std::string(100000,'x');
  if(kind==3)bad.operations={EntityTagsSet{id(),{std::string(100000,'x')}}};
  denied(bounded_recorder.apply_at_boundary(bad,1),"BUDGET_EXCEEDED");
  check(fresh.canonical_bytes()==empty && bounded_recorder.size()==0,"bounded request stays empty");
 }
 check(reconstruct(bounded_recorder.export_log(),assets).world->canonical_bytes()==empty,"empty replay supported");
 ow::world::World reuse("workshop",7,8);Recorder reuse_recorder(reuse);
 committed(reuse_recorder.apply_at_boundary(request(0,{EntityCreate{"first","builtin.unit_cube"},EntityDelete{TemporaryTarget{"first"}},EntityCreate{"second","builtin.unit_cube"}}),1));
 auto reuse_log=reuse_recorder.export_log();
 check(reuse_log.records[0].created.size()==2 && reuse_log.records[0].created[0].generation==1 &&
       reuse_log.records[0].created[1].generation==2 && reuse_log.records[0].created[0].entity_uuid==reuse_log.records[0].created[1].entity_uuid,"same-batch transient and surviving identities retained");
 check(reconstruct(reuse_log,assets).world->snapshot()==reuse.snapshot(),"same-batch transient identity replay");
 reuse_log.records[0].created[0].generation=2;check(!reconstruct(reuse_log,assets).world,"transient identity mismatch rejects despite matching final identity");
 ow::world::World hierarchy("workshop",7,8);Recorder hierarchy_recorder(hierarchy);
 committed(hierarchy_recorder.apply_at_boundary(request(0,{EntityCreate{"P","builtin.unit_cube"},EntityCreate{"C","builtin.unit_cube"},move(TemporaryTarget{"C"},1),EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_world"}}),1));
 const auto hierarchy_log=hierarchy_recorder.export_log();
 check(hierarchy.snapshot().format_version==2 && reconstruct(hierarchy_log,assets).world->snapshot()==hierarchy.snapshot(),"hierarchy schema2 exact replay");
 committed(hierarchy_recorder.apply_at_boundary(request(1,{EntityTagsSet{id(2),{"child"}}}),2));
 check(hierarchy.snapshot().format_version==3 && reconstruct(hierarchy_recorder.export_log(),assets).world->snapshot()==hierarchy.snapshot(),"hierarchy and tags schema3 exact replay");
 std::cout<<"{\"status\":\"passed\",\"assertions\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<"replay native failed: "<<e.what()<<'\n';return 1;}}
