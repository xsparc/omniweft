// SPDX-License-Identifier: Apache-2.0
#include "persistence_example.hpp"
#include "omniweft/persistence.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <string_view>
namespace {
using namespace ow::commands;using Json=nlohmann::json;using Store=ow::persistence::Store;
void require(bool ok){if(!ok)throw std::runtime_error("PERSISTENCE_FIXTURE_FAILED");}
std::string hex(std::uint64_t n,unsigned width){std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(static_cast<int>(width))<<n;return s.str();}
std::string bytes(const std::vector<std::uint8_t>& v){std::string s;for(auto b:v)s+=hex(b,2);return s;}
EntityTarget id(unsigned n,std::uint64_t g=1){return {"workshop","00000007-0000-4000-8000-"+hex(n,12),g};}
Envelope request(unsigned n,std::uint64_t revision,std::vector<Operation> ops){
 Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-"+hex(0x140000U+n,12);
 e.idempotency={"unused-native-fixture",n};e.expected_world_revision=revision;e.apply_at={"next_tick",120};e.budget={4,0};e.operations=std::move(ops);return e;
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

Json observation(const Store& store){return {{"snapshot",snapshot(store.snapshot())},{"canonical_hex",bytes(ow::world::canonical_bytes(store.snapshot()))},
 {"durable_revision",store.durable_revision()},{"high_water",store.high_water()},{"low_water",store.low_water()},{"recovery_required",store.recovery_required()}};}
Json result(const ow::persistence::Result& r){Json value=nullptr;if(r.receipt)value=receipt(*r.receipt);
 return {{"receipt",value},{"replayed",r.replayed},{"durable_revision",r.durable_revision},{"high_water",r.high_water},
 {"low_water",r.low_water},{"recovery_required",r.recovery_required},{"code",r.code}};}
Envelope edit(Store& store,unsigned n){std::vector<Operation> ops;
 if(n==1)ops={EntityCreate{"A","builtin.unit_cube"},move(TemporaryTarget{"A"},-2)};
 else if(n==2)ops={EntityDelete{id(1),"reject_if_children"},EntityCreate{"B","builtin.unit_cube"},move(TemporaryTarget{"B"},4)};
 else if(n==3)ops={EntityTagsSet{id(1,2),{"blue"}}};
 else ops={move(id(1,2),static_cast<double>(n))};
 auto e=request(n,n-1,std::move(ops));e.idempotency.epoch=store.epoch();return e;}
void accepted(const ow::persistence::Result& r){require(r.receipt && r.receipt->status=="committed" && r.receipt->durability=="durable" && !r.recovery_required);}
ow::persistence::Phase phase(std::string_view name){using P=ow::persistence::Phase;
 if(name=="before_blob_flush")return P::before_blob_flush;
 if(name=="before_journal_flush")return P::before_journal_flush;
 if(name=="after_journal_flush")return P::after_journal_flush;
 if(name=="before_ack")return P::before_ack;
 if(name=="after_ack")return P::after_ack;
 if(name=="before_checkpoint_flush")return P::before_checkpoint_flush;
 if(name=="after_checkpoint_flush")return P::after_checkpoint_flush;
 if(name=="before_checkpoint_replace")return P::before_checkpoint_replace;
 if(name=="after_checkpoint_replace")return P::after_checkpoint_replace;
 if(name=="before_journal_reclaim")return P::before_journal_reclaim;
 if(name=="after_journal_reclaim")return P::after_journal_reclaim;
 throw std::runtime_error("UNKNOWN_CRASH_PHASE");}
}
int run_persistence_example(int argc,char* argv[]){try{
 bool example=false,headless=false,seed=false,verify=false;std::filesystem::path output,store_path,witness;
 std::string action="baseline",crash;unsigned sequence=2;
 for(int i=1;i<argc;++i){const std::string_view arg(argv[i]);const auto value=[&]()->std::string{require(i+1<argc);return argv[++i];};
 if(arg=="--example" && !example){example=true;require(value()=="world.crash_recovery");}
 else if(arg=="--headless" && !headless)headless=true;
 else if(arg=="--seed" && !seed){seed=true;require(value()=="7");}
 else if(arg=="--verify" && !verify)verify=true;
 else if(arg=="--output" && output.empty())output=value();
 else if(arg=="--store" && store_path.empty())store_path=value();
 else if(arg=="--epoch-witness" && witness.empty())witness=value();
 else if(arg=="--action")action=value();
 else if(arg=="--crash-at")crash=value();
 else if(arg=="--sequence"){const auto v=value();require(v.size()==1 && v[0]>='1' && v[0]<='8');sequence=static_cast<unsigned>(v[0]-'0');}
 else require(false);}
 require(example && headless && seed && verify && !store_path.empty() && !output.empty() && !std::filesystem::exists(output));
 const auto outside=[&](const std::filesystem::path& path){const auto relative=std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::weakly_canonical(output));return relative.empty() || *relative.begin()=="..";};
 require(outside(store_path) && (witness.empty() || outside(witness)));
 std::filesystem::create_directories(output);
 ow::persistence::FaultHook hook;
 if(!crash.empty()){const auto selected=phase(crash);hook=[selected](auto p){if(p==selected)std::_Exit(86);};}
 Json report={{"schema_version",1},{"example","world.crash_recovery"},{"seed",7},{"action",action},{"verified",true}};
 if(action=="baseline"){
 auto store=Store::create(store_path,{});Json states=Json::array(),receipts=Json::array();
 for(unsigned n=1;n<=5;++n){auto r=store->apply_await_durable(edit(*store,n),n);accepted(r);receipts.push_back(result(r));states.push_back(observation(*store));}
 auto retry=store->apply_await_durable(edit(*store,5),5);accepted(retry);require(retry.replayed);
 auto changed=edit(*store,5);std::get<TransformSet>(changed.operations[0]).position_m[0]=99;
 auto mismatch=store->apply_await_durable(changed,5);auto expired=store->lookup(1);
 require(mismatch.code=="IDEMPOTENCY_CONFLICT" && !mismatch.receipt && expired.code=="REQUIRES_RESYNC" && !expired.receipt);
 const auto epoch=store->epoch();const auto before=observation(*store);const auto compacted=store->compact();
 store.reset();store=Store::open(store_path);const auto after=observation(*store);auto found=store->lookup(5);
 require(before==after && store->epoch()==epoch && store->high_water()==5 && store->snapshot().slots[0].generation==2);
 require(store->snapshot().slots[0].entity->transform.position_m[0]==5 && store->snapshot().slots[0].entity->tags==std::vector<std::string>{"blue"});
 report["states"]=states;report["receipts"]=receipts;report["retry"]=result(retry);report["mismatch"]=result(mismatch);report["expired"]=result(expired);
 report["compacted"]=result(compacted);report["recovered"]=after;report["lookup"]=result(found);report["epoch_preserved"]=store->epoch()==epoch;
 }else if(action=="create"){
 auto store=Store::create(store_path,{},hook);
 if(!witness.empty()){require(!std::filesystem::exists(witness));std::ofstream secret(witness,std::ios::binary);secret<<store->epoch();secret.close();require(secret.good());report["epoch_preserved"]=true;}
 auto r=store->apply_await_durable(edit(*store,1),1);accepted(r);std::cout<<Json{{"ack",result(r)}}.dump()<<std::endl;require(std::cout.good());store->notify_acknowledged();report["result"]=result(r);report["state"]=observation(*store);
 }else{
 auto store=Store::open(store_path,hook);
 if(!witness.empty()){std::ifstream secret(witness,std::ios::binary);std::string prior;secret>>prior;require(secret && prior==store->epoch());report["epoch_preserved"]=true;}
 if(action=="advance"){auto r=store->apply_await_durable(edit(*store,sequence),sequence);accepted(r);std::cout<<Json{{"ack",result(r)}}.dump()<<std::endl;require(std::cout.good());store->notify_acknowledged();report["result"]=result(r);}
 else if(action=="compact")report["result"]=result(store->compact());
 else if(action=="inspect")report["result"]=result(store->lookup(store->high_water()));
 else if(action=="retry")report["result"]=result(store->apply_await_durable(edit(*store,sequence),sequence));
 else require(false);
 report["state"]=observation(*store);
 }
 std::ofstream file(output/"result.json",std::ios::binary);file<<report.dump(2)<<'\n';file.close();require(file.good());
 return 0;
 }catch(const ow::persistence::Failure& e){std::cerr<<e.code()<<'\n';return 2;}
 catch(...){std::cerr<<"PERSISTENCE_EXAMPLE_FAILED\n";return 1;}}
