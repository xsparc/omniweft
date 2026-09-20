// SPDX-License-Identifier: Apache-2.0
// Public-API expectations; no mutable World seeding or private query hooks.
#include <omniweft/observe.hpp>
#include <omniweft/transactions.hpp>
#include <omniweft/policy.hpp>
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {
using namespace ow::commands;
using namespace ow::observe;
using ow::world::World;
using Json=nlohmann::json;
unsigned checks=0;
void check(bool value,const char* label) {++checks;if (!value) throw std::runtime_error(label);}
EntityTarget id(unsigned n,std::uint64_t generation=1) {
  return {"workshop","00000007-0000-4000-8000-00000000000"+std::to_string(n),generation};
}
Envelope batch(const World& world,std::vector<Operation> operations) {
  Envelope e;e.world_id="workshop";e.transaction_id="018f7242-4387-7c98-a114-67787915a399";
  e.idempotency={"fixture-epoch",1};e.expected_world_revision=world.snapshot().world_revision;
  e.apply_at={"next_tick",120};e.budget={operations.size(),0};e.operations=std::move(operations);return e;
}
void commit(World& world,std::vector<Operation> operations) {
  const auto r=ow::transactions::Coordinator::apply_at_boundary(world,batch(world,std::move(operations)));
  check(r.status=="committed" && r.errors.empty(),"fixture typed commit");
}
Json page(const Result& result) {
  check(result.page.has_value() && result.code.empty(),"expected query success");
  const auto value=Json::parse(result.page->json);
  check(value.size()==5 && value.contains("schema_version") && value["schema_version"]==1 &&
    value["world_id"]=="workshop" && value.contains("world_revision") && value.contains("items") &&
    value["has_more"]==result.page->next.has_value(),"bounded page fields and cursor agreement");
  for (const auto& item:value["items"])
    check(item.size()==6 && item.contains("entity_uuid") && item.contains("generation") &&
      item.contains("authoring_revision") && item.contains("tags") && item.contains("transform") &&
      item.contains("bounds"),"only allowed projection fields");
  return value;
}
void denied(const Result& result,const char* code) {
  check(!result.page && result.code==code,"generic rejection without partial payload");
}
std::vector<std::string> ids(const Json& p) {
  std::vector<std::string> result;for (const auto& item:p["items"]) result.push_back(item["entity_uuid"]);return result;
}
ReadGrant visible() {ReadGrant g;for (unsigned n:{2U,4U,6U})g.entities.push_back({id(n).entity_uuid,1});return g;}
std::vector<Operation> fixture(bool hidden_changes) {
  std::vector<Operation> ops;
  for (unsigned i=1;i<= (hidden_changes ? 8U : 7U);++i) {
    const auto name="cube"+std::to_string(i);
    ops.emplace_back(EntityCreate{name,"builtin.unit_cube"});
    ops.emplace_back(TransformSet{TemporaryTarget{name},{static_cast<double>(i),0,0}});
    ops.emplace_back(EntityTagsSet{TemporaryTarget{name},{"red","chair"}});
    if (hidden_changes && i%2==1) {
      ops.emplace_back(EntityTagsSet{TemporaryTarget{name},{"hidden-secret","other"}});
      ops.emplace_back(TransformSet{TemporaryTarget{name},{1e308,0,0},{0,0,0,1},{1e308,1e308,1e308}});
    }
  }
  return ops;
}
}
int main() {
  try {
    World world("workshop",7,8), hidden("workshop",7,8);
    commit(world,fixture(false));commit(hidden,fixture(true));
    const auto before=world.canonical_bytes();
    const auto invalid_grant=[&](const ReadGrant& invalid) {
      bool threw=false;
      try { Session forbidden(world,invalid); }
      catch(const std::invalid_argument& error) { threw=std::string(error.what())=="INVALID_READ_GRANT"; }
      check(threw,"invalid host grant rejected at construction");
    };
    auto malformed=visible();malformed.whole_world=true;invalid_grant(malformed);
    malformed=visible();malformed.entities[0].generation=0;invalid_grant(malformed);
    malformed=visible();malformed.entities[0].entity_uuid=std::string(100000,'x');invalid_grant(malformed);
    malformed=visible();malformed.entities.resize(1025,malformed.entities[0]);invalid_grant(malformed);
    malformed=visible();malformed.region=Bounds{{1,0,0},{0,0,0}};invalid_grant(malformed);
    for (const std::size_t cap:{0U,4097U}) {malformed=visible();malformed.response_bytes=cap;invalid_grant(malformed);}
    for (const std::size_t cap:{0U,1048577U}) {malformed=visible();malformed.retained_bytes=cap;invalid_grant(malformed);}
    for (const int ms:{0,300001}) {malformed=visible();malformed.lifetime=std::chrono::milliseconds(ms);invalid_grant(malformed);}
    for (const int ms:{0,30001}) {malformed=visible();malformed.snapshot_lifetime=std::chrono::milliseconds(ms);invalid_grant(malformed);}
    auto grant=visible();Session session(world,grant),other(hidden,grant);
    Query query;query.all_tags={"red","chair"};query.page_size=1;
    auto first=session.start(query),same=other.start(query);
    check(first.page && same.page && first.page->json==same.page->json,"hidden matching/nonmatching/huge rows do not alter first payload");
    check(ids(page(first))==std::vector<std::string>{id(2).entity_uuid},"literal first authorized ID");
    const auto cursor=*first.page->next;
    auto second=session.next(cursor),same_second=other.next(*same.page->next);
    check(second.page && same_second.page && second.page->json==same_second.page->json,"authorized cursor position ignores hidden rows");
    check(ids(page(second))==std::vector<std::string>{id(4).entity_uuid},"literal second authorized ID");
    check(session.next(cursor).page->json==second.page->json,"page replay is exact");
    auto third=session.next(*second.page->next);
    check(ids(page(third))==std::vector<std::string>{id(6).entity_uuid} && !third.page->next,"literal final ID and termination");
    denied(session.next(*same.page->next),"REQUIRES_RESYNC");denied(session.next(Cursor{}),"REQUIRES_RESYNC");
    Query bad=query;bad.all_tags={std::string(100000,'x')};denied(session.start(bad),"INVALID_SCHEMA");
    check(session.next(cursor).page->json==second.page->json,"failed start preserves retained set");
    bad=query;bad.all_tags={"red","red"};denied(session.start(bad),"INVALID_SCHEMA");
    bad=query;bad.all_tags={"a","b","c","d","e"};denied(session.start(bad),"INVALID_SCHEMA");
    bad=query;bad.page_size=65;denied(session.start(bad),"INVALID_SCHEMA");
    bad=query;bad.intersects=Bounds{{1,0,0},{0,0,0}};denied(session.start(bad),"INVALID_SCHEMA");
    bad=query;bad.intersects=Bounds{{0,0,0},{std::numeric_limits<double>::infinity(),0,0}};denied(session.start(bad),"INVALID_SCHEMA");
    bad=query;bad.all_tags={"bad\"tag"};denied(session.start(bad),"INVALID_SCHEMA");
    const auto exact_cap=first.page->json.size();
    Query capped=query;capped.response_bytes=exact_cap;
    check(session.start(capped).page->json==first.page->json,"exact complete JSON cap accepts");
    check(other.start(capped).page->json==first.page->json,"hidden rows do not alter exact byte budget");
    capped.response_bytes=exact_cap-1;
    denied(other.start(capped),"BUDGET_EXCEEDED");
    for (unsigned i=0;i<20;++i) denied(session.start(capped),"BUDGET_EXCEEDED");
    auto host_cap=grant;host_cap.response_bytes=exact_cap-1;Session limited(world,host_cap);
    denied(limited.start(query),"BUDGET_EXCEEDED");
    check(page(session.start(query))["items"].size()==1,"budget rejection recovers");
    denied(session.next(cursor),"REQUIRES_RESYNC");
    auto retain_limit=grant;retain_limit.retained_bytes=1;Session no_retention(world,retain_limit);
    denied(no_retention.start(query),"BUDGET_EXCEEDED");
    for (std::size_t size:{1U,2U,3U,64U}) {
      Query paged=query;paged.page_size=size;auto r=session.start(paged);std::vector<std::string> found;
      for (;;) {
        const auto values=ids(page(r));found.insert(found.end(),values.begin(),values.end());
        if (!r.page->next) break;
        r=session.next(*r.page->next);
      }
      check(found==std::vector<std::string>{id(2).entity_uuid,id(4).entity_uuid,id(6).entity_uuid},"page-size-independent ordered complete set");
    }
    Session none(world,ReadGrant{});check(page(none.start(Query{}))["items"].empty(),"default grant denies all");
    auto region_grant=grant;region_grant.region=Bounds{{2,-1,-1},{6.5,1,1}};Session region(world,region_grant);
    check(ids(page(region.start(Query{})))==std::vector<std::string>{id(4).entity_uuid,id(6).entity_uuid},"identity AND full-bound region containment");
    ReadGrant whole;whole.whole_world=true;Session all(world,whole);Query touch;touch.intersects=Bounds{{2.5,0,0},{2.5,0,0}};
    check(ids(page(all.start(touch)))==std::vector<std::string>{id(2).entity_uuid,id(3).entity_uuid},"inclusive cube query intersection");
    check(world.canonical_bytes()==before,"query calls cannot mutate canonical state");
    auto frozen=session.start(query);const auto retained_cursor=*frozen.page->next;
    commit(world,{EntityDelete{id(4)},EntityCreate{"replacement","builtin.unit_cube"},
      TransformSet{TemporaryTarget{"replacement"},{4,0,0}},EntityTagsSet{TemporaryTarget{"replacement"},{"red","chair"}}});
    const auto old=page(session.next(retained_cursor));
    check(old["world_revision"]==1 && old["items"][0]["generation"]==1,"retained snapshot survives deletion/reuse");
    Query fresh=query;fresh.page_size=64;
    check(ids(page(session.start(fresh)))==std::vector<std::string>{id(2).entity_uuid,id(6).entity_uuid},"stale-generation grant cannot authorize replacement");
    auto mutable_grant=grant;Session sealed(world,mutable_grant);mutable_grant.whole_world=true;mutable_grant.entities.clear();
    check(ids(page(sealed.start(fresh)))==std::vector<std::string>{id(2).entity_uuid,id(6).entity_uuid},"grant copied immutably at host boundary");
    auto short_grant=grant;short_grant.snapshot_lifetime=std::chrono::milliseconds(2000);
    Session expiry(world,short_grant);auto start=expiry.start(query);check(start.page && start.page->next,"expiry fixture has next");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));page(expiry.next(*start.page->next));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));denied(expiry.next(*start.page->next),"REQUIRES_RESYNC");
    page(expiry.start(query));expiry.close();denied(expiry.next(*start.page->next),"REQUIRES_RESYNC");denied(expiry.start(query),"NOT_AUTHORIZED");
    auto lifetime=grant;lifetime.lifetime=std::chrono::milliseconds(100);
    Session expired_grant(world,lifetime);auto token=expired_grant.start(query);check(token.page && token.page->next,"authority expiry fixture");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    denied(expired_grant.next(*token.page->next),"REQUIRES_RESYNC");denied(expired_grant.start(query),"NOT_AUTHORIZED");

    World tags("workshop",7,8),control("workshop",7,8);
    const std::vector<Operation> setup{EntityCreate{"P","builtin.unit_cube"},EntityCreate{"C","builtin.unit_cube"},
      EntityReparent{TemporaryTarget{"C"},TemporaryTarget{"P"},"preserve_world"}};
    commit(tags,setup);commit(control,setup);
    commit(tags,{EntityTagsSet{id(1),{"z","a"}}});commit(control,{EntityTagsSet{id(1),{}}});
    check(tags.snapshot().format_version==3 && tags.snapshot().slots[0].entity->tags==std::vector<std::string>{"a","z"},"canonical sorted authored tags");
    const auto tagged=tags.canonical_bytes();const auto snap=tags.snapshot();
    auto rejected=ow::transactions::Coordinator::apply_at_boundary(tags,batch(tags,{EntityTagsSet{id(1),{"changed"}},
      EntityCreate{"rollback","builtin.unit_cube"},EntityTagsSet{TemporaryTarget{"missing"},{"x"}}}));
    check(rejected.status=="rejected" && rejected.created.empty() && tags.snapshot()==snap && tags.canonical_bytes()==tagged,"late tag failure rolls back entire state and allocator");
    for (const auto& labels:std::vector<std::vector<std::string>>{{"a","a"},{"bad tag"},{std::string(33,'x')},std::vector<std::string>(9,"a")}) {
      rejected=ow::transactions::Coordinator::apply_at_boundary(tags,batch(tags,{EntityTagsSet{id(1),labels}}));
      check(rejected.status=="rejected" && tags.canonical_bytes()==tagged,"invalid typed labels cannot change state");
    }
    commit(tags,{EntityTagsSet{id(1),{}}});commit(control,{EntityTagsSet{id(1),{}}});
    check(tags.snapshot().format_version==2 && tags.canonical_bytes()==control.canonical_bytes(),"clearing tags restores exact untagged hierarchy bytes");
    commit(tags,{EntityReparent{id(2),std::nullopt,"preserve_world"},EntityTagsSet{id(2),{"red"}}});
    commit(tags,{EntityTagsSet{id(2),{}}});check(tags.snapshot().format_version==1,"cleared root restores format1");
    const auto serialized=serialize(batch(tags,{EntityTagsSet{id(1),{"red","chair"}}}));
    check(serialized.json && parse(*serialized.json).envelope==batch(tags,{EntityTagsSet{id(1),{"red","chair"}}}),"native tag round trip");
    ow::policy::Ledger ledger;World policy("workshop",7,8);
    auto not_admitted=batch(policy,{EntityCreate{"a","builtin.unit_cube"},EntityTagsSet{TemporaryTarget{"a"},{std::string(100000,'x')}}});
    const auto empty=policy.canonical_bytes();const auto denial=ow::policy::apply(policy,not_admitted,ledger,1);
    check(denial.status=="rejected" && denial.errors[0].code=="UNSUPPORTED_OPERATION" && policy.canonical_bytes()==empty &&
      ledger.usage()==ow::policy::Usage{},"policy rejects unsupported metadata before serialization or charges");
    commit(policy,{EntityCreate{"a","builtin.unit_cube"},EntityTagsSet{TemporaryTarget{"a"},{"red"}}});
    const auto tagged_policy=policy.canonical_bytes();
    const auto blocked=ow::policy::apply(policy,batch(policy,{TransformSet{id(1),{3,0,0}}}),ledger,1);
    check(blocked.errors[0].code=="UNSUPPORTED_OPERATION" && policy.canonical_bytes()==tagged_policy &&
      ledger.usage()==ow::policy::Usage{},"existing tagged world cannot enter old policy");
    std::cout<<"{\"status\":\"passed\",\"assertions\":"<<checks<<"}\n";return 0;
  } catch(const std::exception& error) {std::cerr<<"observe native failed: "<<error.what()<<'\n';return 1;}
}
