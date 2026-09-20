// SPDX-License-Identifier: Apache-2.0
#include "omniweft/observe.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ow::observe {
namespace {
using Json=nlohmann::json;
bool valid_bounds(const Bounds& b) {
  for (std::size_t i=0;i<3;++i)
    if (!std::isfinite(b.lower[i]) || !std::isfinite(b.upper[i]) || b.lower[i]>b.upper[i]) return false;
  return true;
}
bool identifier(std::string_view value) {
  const auto alnum=[](char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9'); };
  if (value.empty() || value.size()>32 || !alnum(value.front())) return false;
  for (char c:value) if (!alnum(c) && c!='.' && c!='_' && c!=':' && c!='-') return false;
  return true;
}
bool uuid(std::string_view value) {
  if (value.size()!=36) return false;
  for (std::size_t i=0;i<value.size();++i) {
    if (i==8 || i==13 || i==18 || i==23) { if (value[i]!='-') return false; }
    else if (!((value[i]>='0' && value[i]<='9') || (value[i]>='a' && value[i]<='f'))) return false;
  }
  return true;
}
ReadGrant checked_grant(const ReadGrant& grant) {
  if (grant.entities.size()>1024 || (grant.whole_world && !grant.entities.empty()) ||
      grant.response_bytes==0 || grant.response_bytes>4096 || grant.retained_bytes==0 || grant.retained_bytes>1048576 ||
      grant.lifetime.count()<1 || grant.lifetime.count()>300000 ||
      grant.snapshot_lifetime.count()<1 || grant.snapshot_lifetime.count()>30000 ||
      (grant.region && !valid_bounds(*grant.region))) throw std::invalid_argument("INVALID_READ_GRANT");
  for (const auto& id:grant.entities)
    if (!uuid(id.entity_uuid) || id.generation==0) throw std::invalid_argument("INVALID_READ_GRANT");
  return grant;
}
bool valid_query(const Query& q) {
  if (q.all_tags.size()>4 || q.page_size==0 || q.page_size>64 || q.response_bytes==0 || q.response_bytes>4096 ||
      (q.intersects && !valid_bounds(*q.intersects))) return false;
  for (std::size_t i=0;i<q.all_tags.size();++i) {
    if (!identifier(q.all_tags[i])) return false;
    for (std::size_t j=0;j<i;++j) if (q.all_tags[i]==q.all_tags[j]) return false;
  }
  return true;
}
std::optional<Bounds> bounds(const world::Transform& t) {
  const auto& q=t.rotation_xyzw;
  const double x=q[0],y=q[1],z=q[2],w=q[3];
  const std::array<std::array<double,3>,3> r{{
    {1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
    {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
    {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}}};
  Bounds value;
  for (std::size_t i=0;i<3;++i) {
    double extent=0;
    for (std::size_t j=0;j<3;++j) extent+=std::abs(r[i][j])*(.5*std::abs(t.scale[j]));
    value.lower[i]=t.position_m[i]-extent; value.upper[i]=t.position_m[i]+extent;
  }
  if (!valid_bounds(value)) return std::nullopt;
  return value;
}
bool contains(const Bounds& outer,const Bounds& inner) {
  for (std::size_t i=0;i<3;++i) if (inner.lower[i]<outer.lower[i] || inner.upper[i]>outer.upper[i]) return false;
  return true;
}
bool overlaps(const Bounds& a,const Bounds& b) {
  for (std::size_t i=0;i<3;++i) if (a.upper[i]<b.lower[i] || b.upper[i]<a.lower[i]) return false;
  return true;
}
bool permitted(const ReadGrant& grant,const world::Slot& slot) {
  if (grant.whole_world) return true;
  return std::any_of(grant.entities.begin(),grant.entities.end(),[&](const auto& id) {
    return id.entity_uuid==slot.entity_uuid && id.generation==slot.generation;
  });
}
Result error(const char* code) { return {std::nullopt,code}; }
}  // namespace

Session::Session(const world::World& world,const ReadGrant& grant)
  : world_(world),grant_(checked_grant(grant)),expires_(Clock::now()+grant_.lifetime) {}
void Session::close() noexcept { snapshot_.reset(); closed_=true; identity_.reset(); }
bool Session::expire() noexcept {
  const auto now=Clock::now();
  if (now>=expires_) close();
  else if (snapshot_ && now>=snapshot_->deadline) snapshot_.reset();
  return closed_;
}
Result Session::page(const Snapshot& data,std::size_t offset,std::uint64_t serial) const {
  std::size_t end=offset, payload=0;
  const auto suffix=[](bool more) -> std::string_view { return more ? "],\"has_more\":true}" : "],\"has_more\":false}"; };
  while (end<data.rows.size() && end-offset<data.page_size) {
    const auto candidate=payload+(end>offset ? 1U : 0U)+data.rows[end].size();
    if (data.prefix.size()+candidate+suffix(end+1<data.rows.size()).size()>data.response_bytes) break;
    payload=candidate; ++end;
  }
  const bool more=end<data.rows.size();
  const auto count=data.prefix.size()+payload+suffix(more).size();
  if ((end==offset && more) || count>data.response_bytes) return error("BUDGET_EXCEEDED");
  Page output;
  output.json.reserve(count); output.json=data.prefix;
  for (std::size_t i=offset;i<end;++i) { if (i>offset) output.json+=','; output.json+=data.rows[i]; }
  output.json+=suffix(more);
  if (more) {
    Cursor next; next.owner_=identity_; next.snapshot_=serial; next.offset_=end; output.next=std::move(next);
  }
  return {std::move(output),{}};
}
Result Session::start(const Query& query) {
  if (expire()) return error("NOT_AUTHORIZED");
  if (!valid_query(query)) return error("INVALID_SCHEMA");
  if (serial_==std::numeric_limits<std::uint64_t>::max()) return error("BUDGET_EXCEEDED");
  // Query/host grant bounds are checked before copying authoritative state.
  const auto captured=world_.snapshot();
  Snapshot staged;
  staged.deadline=std::min(expires_,Clock::now()+grant_.snapshot_lifetime);
  staged.page_size=query.page_size; staged.response_bytes=std::min(query.response_bytes,grant_.response_bytes);
  staged.prefix="{\"schema_version\":1,\"world_id\":"+Json(captured.world_id).dump()+
    ",\"world_revision\":"+std::to_string(captured.world_revision)+",\"items\":[";
  std::vector<const world::Slot*> ordered;
  for (const auto& slot:captured.slots) if (slot.entity && !slot.retired && permitted(grant_,slot)) ordered.push_back(&slot);
  std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b) { return a->entity_uuid<b->entity_uuid; });
  std::size_t retained=staged.prefix.size();
  if (retained>grant_.retained_bytes) return error("BUDGET_EXCEEDED");
  for (const auto* slot:ordered) {
    const auto& e=*slot->entity;
    const auto box=bounds(e.transform);
    // A nonfinite cube cannot be contained in a finite granted read region.
    // Identity/region authorization precedes predicates and response budgeting.
    if (grant_.region && (!box || !contains(*grant_.region,*box))) continue;
    if (!std::all_of(query.all_tags.begin(),query.all_tags.end(),[&](const auto& tag) {
      return std::binary_search(e.tags.begin(),e.tags.end(),tag); })) continue;
    if (!box) return error("INVALID_STATE");
    if (query.intersects && !overlaps(*query.intersects,*box)) continue;
    const Json item={{"entity_uuid",slot->entity_uuid},{"generation",slot->generation},
      {"authoring_revision",e.authoring_revision},{"tags",e.tags},
      {"transform",{{"position_m",e.transform.position_m},{"rotation_xyzw",e.transform.rotation_xyzw},{"scale",e.transform.scale}}},
      {"bounds",{{"lower",box->lower},{"upper",box->upper}}}};
    auto row=item.dump();
    if (row.size()>grant_.retained_bytes-retained) return error("BUDGET_EXCEEDED");
    retained+=row.size(); staged.rows.push_back(std::move(row));
  }
  auto result=page(staged,0,serial_+1);
  if (!result.page) return result;
  if (Clock::now()>=staged.deadline) { expire(); return error("REQUIRES_RESYNC"); }
  snapshot_=std::move(staged); ++serial_;
  return result;
}
Result Session::next(const Cursor& cursor) {
  if (expire() || !snapshot_ || cursor.owner_.lock()!=identity_ || cursor.snapshot_!=serial_ ||
      cursor.offset_>=snapshot_->rows.size()) return error("REQUIRES_RESYNC");
  auto result=page(*snapshot_,cursor.offset_,serial_);
  if (Clock::now()>=snapshot_->deadline) { expire(); return error("REQUIRES_RESYNC"); }
  return result;
}
}  // namespace ow::observe
