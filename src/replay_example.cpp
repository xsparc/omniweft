// SPDX-License-Identifier: Apache-2.0
#include "replay_example.hpp"
#include "omniweft/replay.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string_view>
namespace {
using namespace ow::commands;using Json=nlohmann::json;
void require(bool ok){if(!ok)throw std::runtime_error("REPLAY_FIXTURE_FAILED");}
std::string hex(std::uint64_t n,unsigned width){std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(static_cast<int>(width))<<n;return s.str();}
std::string bytes(const std::vector<std::uint8_t>& v){std::string s;for(auto b:v)s+=hex(b,2);return s;}
EntityTarget id(unsigned n,std::uint64_t g=1){return {"workshop","00000007-0000-4000-8000-"+hex(n,12),g};}
Envelope request(unsigned n,std::uint64_t revision,std::vector<Operation> ops){
 Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-"+hex(0x130000U+n,12);
 e.idempotency={"private-replay-fixture",n};e.expected_world_revision=revision;e.apply_at={"next_tick",120};e.budget={4,0};e.operations=std::move(ops);return e;
}
TransformSet move(Target target,double x){return {std::move(target),{x,0,0}};}
Json trs(const ow::world::Transform& t) {return {{"position_m",t.position_m},{"rotation_xyzw",t.rotation_xyzw},{"scale",t.scale}};}
Json snapshot(const ow::world::Snapshot& s) {
  Json slots=Json::array();
  for(const auto& slot:s.slots) {
    Json e=nullptr;
    if(slot.entity) {const auto& v=*slot.entity;e={{"prefab",v.prefab},{"authoring_revision",v.authoring_revision},{"transform",trs(v.transform)},
      {"parent",nullptr},{"local_transform",nullptr},{"tags",v.tags}};
      if(v.parent) {e["parent"]={{"entity_uuid",v.parent->entity_uuid},{"generation",v.parent->generation}};e["local_transform"]=trs(v.local_transform);}}
    slots.push_back({{"entity_uuid",slot.entity_uuid},{"generation",slot.generation},{"retired",slot.retired},{"entity",e}});
  }
  return {{"format_version",s.format_version},{"world_id",s.world_id},{"seed",s.seed},{"max_slots",s.max_slots},{"world_revision",s.world_revision},{"slots",slots}};
}
Json receipt(const ow::transactions::Receipt& r) {
  Json errors=Json::array(),created=Json::array();
  for(const auto& e:r.errors) {Json v={{"code",e.code},{"path",e.path},{"message",e.message}};if(e.operation_index)v["operation_index"]=*e.operation_index;errors.push_back(v);}
  for(const auto& b:r.created)created.push_back({{"temporary_id",b.temporary_id},{"world_id",b.world_id},{"entity_uuid",b.entity_uuid},{"generation",b.generation}});
  return {{"status",r.status},{"durability",r.durability},{"transaction_id",r.transaction_id},{"world_revision",r.world_revision},{"created",created},{"errors",errors}};
}
Json log_json(const ow::replay::Log& log){
 Json records=Json::array(),assets=Json::array();
 for(const auto& a:log.assets)assets.push_back({{"prefab",a.prefab},{"revision",a.revision},{"manifest",a.manifest},{"content_sha256",a.content_sha256}});
 for(const auto& r:log.records){
  const auto parsed=serialize(request(static_cast<unsigned>(r.sequence),r.expected_revision,r.operations));require(parsed.json.has_value());
  Json bindings=Json::array();for(const auto& b:r.created)bindings.push_back({{"temporary_id",b.temporary_id},{"world_id",b.world_id},{"entity_uuid",b.entity_uuid},{"generation",b.generation}});
  records.push_back({{"sequence",r.sequence},{"boundary_tick",r.boundary_tick},{"expected_revision",r.expected_revision},
   {"budget",{{"max_operations",r.budget.max_operations},{"max_blob_bytes",r.budget.max_blob_bytes}}},
   {"operations",Json::parse(*parsed.json)["operations"]},{"created",bindings},{"checkpoint_hex",bytes(r.checkpoint)}});
 }
 return {{"schema",log.schema},{"command_schema",log.command_schema},{"checkpoint_schema",log.checkpoint_schema},{"world_id",log.world_id},
  {"seed",log.seed},{"max_slots",log.max_slots},{"assets",assets},{"records",records}};
}
}
int run_replay_example(int argc,char* argv[]){try{
 bool example=false,headless=false,seed=false,verify=false,output=false;std::filesystem::path directory;
 for(int i=1;i<argc;++i){const std::string_view arg(argv[i]);const auto value=[&]()->std::string_view{require(i+1<argc);return argv[++i];};
  if(arg=="--example" && !example){example=true;require(value()=="world.replay");}
  else if(arg=="--headless" && !headless)headless=true;
  else if(arg=="--seed" && !seed){seed=true;require(value()=="7");}
  else if(arg=="--verify" && !verify)verify=true;
  else if(arg=="--output" && !output){output=true;directory=value();require(!directory.empty());}
  else require(false);
 }
 require(example && headless && seed && output);
 ow::world::World world("workshop",7,8);ow::replay::Recorder recorder(world);
 const std::array<ow::replay::Asset,1> assets{ow::replay::Asset{}};
 Json originals=Json::array(),receipts=Json::array();
 const auto apply=[&](Envelope e,std::uint64_t tick){auto r=recorder.apply_at_boundary(e,tick);require(r.status=="committed");receipts.push_back(receipt(r));originals.push_back(snapshot(world.snapshot()));};
 apply(request(1,0,{EntityCreate{"A","builtin.unit_cube"},EntityCreate{"B","builtin.unit_cube"},move(TemporaryTarget{"A"},-2),move(TemporaryTarget{"B"},2)}),1);
 apply(request(2,1,{EntityTagsSet{id(1),{"red"}},move(id(1),-3)}),2);
 const auto before=world.canonical_bytes();auto invalid=move(id(1),9);invalid.scale[0]=0;
 const auto rejected=recorder.apply_at_boundary(request(3,2,{move(id(1),8),invalid}),2);
 const bool rejected_unchanged=rejected.status=="rejected" && rejected.errors.size()==1 && rejected.errors[0].code=="INVALID_SCHEMA" &&
  recorder.size()==2 && world.canonical_bytes()==before;require(rejected_unchanged);
 apply(request(4,2,{EntityDelete{id(2)},EntityCreate{"C","builtin.unit_cube"},move(TemporaryTarget{"C"},4)}),2);
 apply(request(5,3,{EntityDelete{id(1)},EntityCreate{"D","builtin.unit_cube"},move(TemporaryTarget{"D"},-5),EntityTagsSet{TemporaryTarget{"D"},{"blue"}}}),3);
 const auto log=recorder.export_log();Json replayed=Json::array();
 for(std::size_t n=0;n<log.records.size();++n){auto prefix=log;prefix.records.resize(n+1);auto result=ow::replay::reconstruct(prefix,assets);require(result.world && result.errors.empty());replayed.push_back(snapshot(result.world->snapshot()));}
 Json negatives=Json::array();
 for(unsigned kind=0;kind<6;++kind){auto bad=log;std::string label;
  if(kind==0){label="late-checkpoint";bad.records.back().checkpoint.back()^=1;}
  if(kind==1){label="required-schema";bad.command_schema="future";}
  if(kind==2){label="missing-asset";bad.assets.clear();}
  if(kind==3){label="wrong-generation";bad.records[2].created[0].generation=1;}
  if(kind==4){label="wrong-content";bad.assets[0].content_sha256[0]='x';}
  if(kind==5){label="accepted-order";std::swap(bad.records[1],bad.records[2]);}
  auto result=ow::replay::reconstruct(bad,assets);require(!result.world && result.errors.size()==1);
  if(verify){
    const std::array<const char*,6> codes{"CHECKPOINT_MISMATCH","UNSUPPORTED_SCHEMA","MISSING_ASSET","INVALID_REPLAY","MISSING_ASSET","INVALID_REPLAY"};
    require(result.errors[0].code==codes[kind]);
  }
  negatives.push_back({{"case",label},{"world_returned",static_cast<bool>(result.world)},{"code",result.errors[0].code},{"path",result.errors[0].path}});
 }
 auto recovered=ow::replay::reconstruct(log,assets);require(recovered.world && recovered.errors.empty());
 if(verify){require(replayed==originals && recovered.world->canonical_bytes()==world.canonical_bytes());require(recorder.size()==4 && world.snapshot().world_revision==4);
  const auto final=world.snapshot();require(final.slots.size()==2 && final.slots[0].generation==2 && final.slots[1].generation==2);
  require(final.slots[0].entity && final.slots[1].entity);
  require(final.slots[0].entity->transform.position_m[0]==-5 && final.slots[1].entity->transform.position_m[0]==4);
  require(final.slots[0].entity->authoring_revision==4 && final.slots[1].entity->authoring_revision==3);
  require(final.slots[0].entity->tags==std::vector<std::string>{"blue"} && final.slots[1].entity->tags.empty());}
 Json report={{"schema_version",1},{"example","world.replay"},{"seed",7},{"verified",verify},{"rejected_not_recorded",rejected_unchanged},
  {"log",log_json(log)},{"original_receipts",receipts},{"original_checkpoints",originals},{"replayed_checkpoints",replayed},
  {"negative",negatives},{"recovery_snapshot",snapshot(recovered.world->snapshot())}};
 if(!directory.parent_path().empty())std::filesystem::create_directories(directory.parent_path());require(std::filesystem::create_directory(directory));
 std::ofstream stream(directory/"result.json.tmp",std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);stream<<report.dump(2)<<'\n';stream.close();
 std::filesystem::rename(directory/"result.json.tmp",directory/"result.json");std::cout<<"world.replay: recorded authoring fixture completed\n";return 0;
}catch(...){std::cerr<<"world.replay failed: check fixture options and fresh writable output\n";return 2;}}
