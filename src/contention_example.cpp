// SPDX-License-Identifier: Apache-2.0
#include "contention_example.hpp"
#include "omniweft/retry.hpp"
#include "omniweft/policy.hpp"
#include <nlohmann/json.hpp>
#include <barrier>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>

namespace {
using namespace ow::commands;
using Json=nlohmann::json;
void require(bool value) {if(!value) throw std::runtime_error("CONTENTION_FIXTURE_FAILED");}
Json receipt_json(const ow::transactions::Receipt& r) {
  Json bindings=Json::array(),errors=Json::array();
  for(const auto& b:r.created) bindings.push_back({{"temporary_id",b.temporary_id},{"world_id",b.world_id},
    {"entity_uuid",b.entity_uuid},{"generation",b.generation}});
  for(const auto& e:r.errors) {
    Json item={{"code",e.code},{"path",e.path},{"message",e.message}};
    if(e.operation_index) item["operation_index"]=*e.operation_index;
    errors.push_back(std::move(item));
  }
  return {{"status",r.status},{"durability",r.durability},{"transaction_id",r.transaction_id},
    {"world_revision",r.world_revision},{"created",std::move(bindings)},{"errors",std::move(errors)}};
}
Json snapshot_json(const ow::world::World& world) {
  const auto s=world.snapshot();Json slots=Json::array();
  for(const auto& slot:s.slots) {
    const auto& e=*slot.entity;
    slots.push_back({{"entity_uuid",slot.entity_uuid},{"generation",slot.generation},{"retired",slot.retired},
      {"entity",{{"prefab",e.prefab},{"authoring_revision",e.authoring_revision},
       {"transform",{{"position_m",e.transform.position_m},{"rotation_xyzw",e.transform.rotation_xyzw},{"scale",e.transform.scale}}}}}});
  }
  return {{"format_version",s.format_version},{"world_id",s.world_id},{"seed",s.seed},{"max_slots",s.max_slots},
    {"world_revision",s.world_revision},{"slots",std::move(slots)}};
}
Envelope proposal(std::size_t principal,std::uint64_t sequence,std::uint64_t revision,bool create) {
  Envelope e;e.world_id="workshop";
  e.transaction_id=principal==0?"018f7242-4387-7c98-a114-67787915a401":"018f7242-4387-7c98-a114-67787915a402";
  e.idempotency={"native-fixture-epoch",sequence};e.expected_world_revision=revision;e.apply_at={"next_tick",120};
  if(create) e.operations.emplace_back(EntityCreate{"A","builtin.unit_cube"});
  Target target=create?Target(TemporaryTarget{"A"}):Target(EntityTarget{"workshop","00000007-0000-4000-8000-000000000001",1});
  e.operations.emplace_back(TransformSet{target,{principal==0?-3.0:3.0,0,0}});
  e.budget={e.operations.size(),0};return e;
}
}
int run_contention_example(int argc,char* argv[]) {
  try {
    bool example=false,seed=false,headless=false,verify=false,output=false;
    std::filesystem::path directory;
    for(int i=1;i<argc;++i) {
      const std::string_view arg(argv[i]);
      const auto value=[&]() -> std::string_view {require(i+1<argc);return argv[++i];};
      if(arg=="--example" && !example) {example=true;require(value()=="agents.contention");}
      else if(arg=="--seed" && !seed) {seed=true;require(value()=="7");}
      else if(arg=="--headless" && !headless) headless=true;
      else if(arg=="--verify" && !verify) verify=true;
      else if(arg=="--output" && !output) {output=true;directory=value();require(!directory.empty());}
      else require(false);
    }
    require(example && seed && headless && output);
    ow::world::World world("workshop",7,8);ow::policy::Ledger ledger;
    std::array<ow::retry::Window,2> windows;
    std::array<ow::transactions::Receipt,2> receipts;
    std::array<std::exception_ptr,2> failures;
    std::mutex owner;std::barrier ready(2);
    struct OwnedResult {std::optional<ow::transactions::Receipt> receipt;const char* code;bool replayed;};
    const auto submit=[&](std::size_t p,const Envelope& e) {
      const auto serialized=serialize(e);require(serialized.json.has_value());
      auto lease=ledger.admit(p,serialized.json->size());
      const auto result=windows[p].execute(e.idempotency.sequence,*serialized.json,[&] {
        ow::policy::Guard guard(lease);
        return ow::transactions::Coordinator::apply_at_boundary(world,e,&guard);
      });
      // Copy the public result while the request's live reservation still exists.
      return OwnedResult{result.receipt?std::optional(*result.receipt):std::nullopt,result.code,result.replayed};
    };
    std::array<std::thread,2> threads;
    for(std::size_t p=0;p<2;++p) threads[p]=std::thread([&,p] {
      ready.arrive_and_wait();
      try {std::lock_guard lock(owner);const auto r=submit(p,proposal(p,1,0,true));require(r.receipt.has_value());receipts[p]=*r.receipt;}
      catch(...) {failures[p]=std::current_exception();}
    });
    for(auto& thread:threads) thread.join();
    for(const auto& failure:failures) if(failure) std::rethrow_exception(failure);
    const std::size_t winner=receipts[0].status=="committed"?0:1;
    require(receipts[winner].status=="committed" && receipts[1-winner].status=="rejected");
    const auto first=snapshot_json(world);const auto stable=world.canonical_bytes();
    Json replay=Json::array();
    for(std::size_t p=0;p<2;++p) {
      const auto r=submit(p,proposal(p,1,0,true));require(r.receipt && r.replayed && *r.receipt==receipts[p]);
      replay.push_back(receipt_json(*r.receipt));
    }
    auto changed=proposal(winner,1,0,true);changed.transaction_id="018f7242-4387-7c98-a114-67787915a403";
    const auto mismatch=submit(winner,changed);require(std::string_view(mismatch.code)=="IDEMPOTENCY_MISMATCH");
    require(world.canonical_bytes()==stable);
    for(std::uint64_t n=2;n<=5;++n) {const auto r=submit(winner,proposal(winner,n,n-1,false));require(r.receipt && r.receipt->status=="committed");}
    const auto compacted=submit(winner,proposal(winner,1,0,true));require(std::string_view(compacted.code)=="REQUIRES_RESYNC");
    const auto usage=ledger.usage();
    const Json report={{"schema_version",1},{"example","agents.contention"},{"seed",7},{"verified",verify},
      {"winner",winner==0?"west":"east"},{"receipts",{receipt_json(receipts[0]),receipt_json(receipts[1])}},
      {"replayed_receipts",std::move(replay)},{"after_race",first},{"after_compaction",snapshot_json(world)},
      {"mismatch_code",mismatch.code},{"compacted_code",compacted.code},
      {"next_sequences",{windows[0].next_sequence(),windows[1].next_sequence()}},
      {"usage",{{"retained",usage.retained},{"working",usage.working},{"requests",usage.requests},
                {"global_requests",usage.global_requests},{"world_revision",usage.world_revision}}}};
    if(!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
    require(std::filesystem::create_directory(directory));
    std::ofstream stream(directory/"result.json.tmp",std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);
    stream<<report.dump(2)<<'\n';stream.close();std::filesystem::rename(directory/"result.json.tmp",directory/"result.json");
    std::cout<<"agents.contention: concurrent proposals and bounded retries passed\n";return 0;
  } catch(...) {std::cerr<<"agents.contention failed: check fixture options and fresh writable output\n";return 2;}
}
