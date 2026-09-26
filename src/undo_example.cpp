// SPDX-License-Identifier: Apache-2.0
#include "undo_example.hpp"
#include "omniweft/history.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string_view>

namespace {
using namespace ow::commands;
using Json=nlohmann::json;
void require(bool ok) {if(!ok) throw std::runtime_error("UNDO_FIXTURE_FAILED");}
std::string hex(std::uint64_t n,unsigned width) {std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(static_cast<int>(width))<<n;return s.str();}
EntityTarget target(unsigned n) {return {"workshop","00000007-0000-4000-8000-"+hex(n,12),1};}
Envelope request(unsigned n,std::uint64_t revision,std::vector<Operation> ops={}) {
  Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-"+hex(0x120000U+n,12);
  e.idempotency={"native-fixture-epoch",n};e.expected_world_revision=revision;e.apply_at={"next_tick",120};
  e.budget={ops.empty()?4:ops.size(),0};e.operations=std::move(ops);return e;
}
TransformSet move(unsigned n,double x,std::array<double,3> scale={1,1,1}) {return {target(n),{x,0,0},{0,0,0,1},scale};}
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
}
int run_undo_example(int argc,char* argv[]) {
  try {
    bool example=false,headless=false,seed=false,verify=false,output=false;std::filesystem::path directory;
    for(int i=1;i<argc;++i) {
      const std::string_view arg(argv[i]);const auto value=[&]() -> std::string_view {require(i+1<argc);return argv[++i];};
      if(arg=="--example" && !example) {example=true;require(value()=="world.undo_chain");}
      else if(arg=="--headless" && !headless)headless=true;
      else if(arg=="--seed" && !seed) {seed=true;require(value()=="7");}
      else if(arg=="--verify" && !verify)verify=true;
      else if(arg=="--output" && !output) {output=true;directory=value();require(!directory.empty());}
      else require(false);
    }
    require(example && headless && seed && output);
    ow::world::World world("workshop",7,8);
    auto initial=ow::transactions::Coordinator::apply_at_boundary(world,request(1,0,{
      EntityCreate{"A","builtin.unit_cube"},EntityCreate{"B","builtin.unit_cube"},
      TransformSet{TemporaryTarget{"A"},{-2,0,0}},TransformSet{TemporaryTarget{"B"},{2,0,0}},
      EntityTagsSet{TemporaryTarget{"A"},{"base"}},EntityTagsSet{TemporaryTarget{"B"},{"blue"}}}));
    require(initial.status=="committed");ow::history::History history(world);Json rows=Json::array();
    const auto retain=[&](const char* label,const ow::transactions::Receipt& r) {
      std::string bytes;for(const auto b:world.canonical_bytes())bytes+=hex(b,2);
      const auto state=history.state();rows.push_back({{"case",label},{"receipt",receipt(r)},{"after",snapshot(world.snapshot())},{"canonical_hex",bytes},
        {"history",{{"entries",state.entries},{"cursor",state.cursor},{"expected_revision",state.expected_revision}}}});
    };
    retain("setup",initial);
    retain("edit-one",history.apply_at_boundary(request(2,1,{move(1,-4),EntityTagsSet{target(1),{"red","chair"}}})));
    retain("edit-two",history.apply_at_boundary(request(3,2,{move(2,4,{-2,1,1}),EntityTagsSet{target(2),{"table"}}})));
    retain("edit-three",history.apply_at_boundary(request(4,3,{move(1,-5),move(1,-6),EntityTagsSet{target(1),{"green"}}})));
    unsigned n=5;
    for(const auto* label:{"undo-three","undo-two","undo-one"})retain(label,history.navigate_at_boundary(ow::history::Action::undo,request(n++,world.snapshot().world_revision)));
    for(const auto* label:{"redo-one","redo-two","redo-three"})retain(label,history.navigate_at_boundary(ow::history::Action::redo,request(n++,world.snapshot().world_revision)));
    retain("physical-rewind",history.navigate_at_boundary(ow::history::Action::rewind_simulation,request(n++,10)));
    retain("external-edit",ow::transactions::Coordinator::apply_at_boundary(world,request(n++,10,{move(2,7)})));
    retain("stale-undo",history.navigate_at_boundary(ow::history::Action::undo,request(n++,11)));
    const auto before_clear=world.canonical_bytes();history.clear_at_boundary();require(before_clear==world.canonical_bytes());
    retain("recovery-edit",history.apply_at_boundary(request(n++,11,{move(1,-8)})));
    retain("recovery-undo",history.navigate_at_boundary(ow::history::Action::undo,request(n++,12)));
    if(verify) {
      require(rows.size()==15);
      const std::array<std::uint64_t,15> revisions{1,2,3,4,5,6,7,8,9,10,10,11,11,12,13};
      for(std::size_t i=0;i<rows.size();++i) {
        const auto& r=rows[i]["receipt"];
        const bool negative=i==10 || i==12;
        require(r["status"]==(negative?"rejected":"committed"));
        require(r["world_revision"]==revisions[i] && rows[i]["after"]["world_revision"]==revisions[i]);
        require(r["errors"].size()==(negative?1U:0U));
        if(negative) {
          require(r["errors"][0]["code"]==(i==10?"UNSUPPORTED_OPERATION":"REVISION_CONFLICT"));
          require(rows[i]["after"]==rows[i-1]["after"] && rows[i]["history"]==rows[i-1]["history"]);
        }
      }
      const auto final=world.snapshot();
      require(history.state()==ow::history::State{1,0,13} && final.slots.size()==2);
      require(final.slots[0].entity && final.slots[1].entity);
      require(final.slots[0].entity->transform.position_m[0]==-6 && final.slots[1].entity->transform.position_m[0]==7);
      require(final.slots[0].entity->tags==std::vector<std::string>{"green"} && final.slots[1].entity->tags==std::vector<std::string>{"table"});
    }
    const Json report={{"schema_version",1},{"example","world.undo_chain"},{"seed",7},{"verified",verify},{"clear_preserved_world",true},{"records",rows}};
    if(!directory.parent_path().empty())std::filesystem::create_directories(directory.parent_path());
    require(std::filesystem::create_directory(directory));
    std::ofstream stream(directory/"result.json.tmp",std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);
    stream<<report.dump(2)<<'\n';stream.close();std::filesystem::rename(directory/"result.json.tmp",directory/"result.json");
    std::cout<<"world.undo_chain: inverse authoring fixture completed\n";return 0;
  } catch(...) {std::cerr<<"world.undo_chain failed: check fixture options and fresh writable output\n";return 2;}
}
