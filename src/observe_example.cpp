// SPDX-License-Identifier: Apache-2.0
#include "observe_example.hpp"
#include "omniweft/observe.hpp"
#include "omniweft/transactions.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
using Json=nlohmann::json;
using namespace ow::commands;
using namespace ow::observe;
void require(bool condition) { if (!condition) throw std::runtime_error("QUERY_FIXTURE_FAILED"); }
EntityTarget id(unsigned n) { return {"workshop","00000007-0000-4000-8000-00000000000"+std::to_string(n),1}; }
void apply(ow::world::World& world,std::vector<Operation> operations) {
  Envelope e; e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-67787915a399";
  e.idempotency={"fixture-epoch",1}; e.apply_at={"next_tick",120};
  e.expected_world_revision=world.snapshot().world_revision;e.budget={operations.size(),0};e.operations=std::move(operations);
  require(ow::transactions::Coordinator::apply_at_boundary(world,e).status=="committed");
}
}
int run_observe_example(int argc,char* argv[]) {
  try {
    bool example=false,seed=false,headless=false,verify=false,output=false;
    std::filesystem::path directory;
    for (int i=1;i<argc;++i) {
      const std::string_view arg(argv[i]);
      const auto value=[&]() -> std::string_view { require(i+1<argc);return argv[++i]; };
      if (arg=="--example" && !example) { example=true;require(value()=="observe.semantic_query"); }
      else if (arg=="--seed" && !seed) {seed=true;require(value()=="7");}
      else if (arg=="--headless" && !headless) headless=true;
      else if (arg=="--verify" && !verify) verify=true;
      else if (arg=="--output" && !output) {output=true;directory=value();require(!directory.empty());}
      else require(false);
    }
    require(example && seed && headless && output);
    ow::world::World world("workshop",7,8);
    apply(world,{
      EntityCreate{"P","builtin.unit_cube"},TransformSet{TemporaryTarget{"P"},{10,0,0},{0,0,1,0},{2,2,2}},EntityTagsSet{TemporaryTarget{"P"},{"hidden-parent"}},
      EntityCreate{"C","builtin.unit_cube"},TransformSet{TemporaryTarget{"C"},{1,0,0}},EntityTagsSet{TemporaryTarget{"C"},{"red","chair"}},
      EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_local"},
      EntityCreate{"B","builtin.unit_cube"},TransformSet{TemporaryTarget{"B"},{-3,0,0},{0,0,0,1},{-2,1,1}},EntityTagsSet{TemporaryTarget{"B"},{"blue","chair"}},
      EntityCreate{"H","builtin.unit_cube"},EntityTagsSet{TemporaryTarget{"H"},{"red","chair"}},
      EntityCreate{"T","builtin.unit_cube"},TransformSet{TemporaryTarget{"T"},{1,0,0},{0,0,.6,.8},{2,1,1}},EntityTagsSet{TemporaryTarget{"T"},{"red","table"}},
      EntityCreate{"R","builtin.unit_cube"},TransformSet{TemporaryTarget{"R"},{5,0,0}},EntityTagsSet{TemporaryTarget{"R"},{"red","chair"}}});
    ReadGrant grant;
    for (unsigned n:{2U,3U,5U,6U}) grant.entities.push_back({id(n).entity_uuid,1});
    grant.region=Bounds{{-4,-3,-3},{9,3,3}};
    Session session(world,grant);
    Json records=Json::array();
    const auto record=[&](const char* label,const Result& result,const char* expected="") {
      require(result.code==expected && result.page.has_value()==(result.code.empty()));
      records.push_back({{"case",label},{"code",result.code},{"page",result.page ? Json::parse(result.page->json) : Json(nullptr)}});
    };
    Query red;red.all_tags={"red"};red.page_size=2;
    const auto first=session.start(red); record("red-first",first);require(first.page && first.page->next);
    const auto cursor=*first.page->next;
    apply(world,{TransformSet{id(6),{20,0,0}},EntityTagsSet{id(6),{"blue"}}});
    const auto stable=world.canonical_bytes();
    record("retained-tail",session.next(cursor));record("replayed-tail",session.next(cursor));
    record("fresh-red",session.start(red));record("replaced-cursor",session.next(cursor),"REQUIRES_RESYNC");
    Query chairs;chairs.all_tags={"chair"};record("chairs",session.start(chairs));
    Query touch;touch.intersects=Bounds{{-2,0,0},{-2,0,0}};record("inclusive-contact",session.start(touch));
    Query combined;combined.all_tags={"red","chair"};combined.intersects=Bounds{{4.5,-2,-2},{8,2,2}};
    record("combined",session.start(combined));
    Session denied(world,ReadGrant{});record("deny-all",denied.start(Query{}));
    auto region_grant=grant;region_grant.region=Bounds{{-2,-3,-3},{9,3,3}};
    Session region(world,region_grant);record("full-grant-containment",region.start(Query{}));
    Query tiny=red;tiny.response_bytes=20;record("response-budget",session.start(tiny),"BUDGET_EXCEEDED");
    record("default-cursor",session.next(Cursor{}),"REQUIRES_RESYNC");
    Session foreign(world,grant);Query one=red;one.page_size=1;
    const auto other=foreign.start(one);require(other.page && other.page->next);
    record("foreign-cursor",session.next(*other.page->next),"REQUIRES_RESYNC");
    auto short_grant=grant;short_grant.snapshot_lifetime=std::chrono::milliseconds(2000);
    Session expiring(world,short_grant);const auto initial=expiring.start(one);
    record("expiry-first",initial);require(initial.page && initial.page->next);
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    record("expired-cursor",expiring.next(*initial.page->next),"REQUIRES_RESYNC");
    const auto recovered=expiring.start(one);record("expiry-recovery",recovered);require(recovered.page && recovered.page->next);
    expiring.close();record("closed-cursor",expiring.next(*recovered.page->next),"REQUIRES_RESYNC");
    record("closed-start",expiring.start(one),"NOT_AUTHORIZED");
    require(world.canonical_bytes()==stable);
    const Json report={{"schema_version",1},{"example","observe.semantic_query"},{"seed",7},
      {"verified",verify},{"state_unchanged_by_queries",true},{"records",std::move(records)}};
    if (!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
    require(std::filesystem::create_directory(directory));
    std::ofstream stream(directory/"result.json.tmp",std::ios::binary);
    stream.exceptions(std::ios::badbit|std::ios::failbit);stream<<report.dump(2)<<'\n';stream.close();
    std::filesystem::rename(directory/"result.json.tmp",directory/"result.json");
    std::cout<<"observe.semantic_query: bounded native query fixture passed\n";
    return 0;
  } catch (...) {std::cerr<<"observe.semantic_query failed: check fixture options and fresh writable output\n";return 2;}
}
