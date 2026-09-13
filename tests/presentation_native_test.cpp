// SPDX-License-Identifier: Apache-2.0
// Independent public-API checks; expected vertices use literal rational yaw.
#include <omniweft/commands.hpp>
#include <omniweft/presentation.hpp>
#include <omniweft/transactions.hpp>
#include <omniweft/world.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
unsigned checks = 0;
void require(bool condition, const char* label) {
  ++checks;
  if (!condition) throw std::runtime_error(label);
}
using ow::commands::Operation;
ow::commands::Envelope envelope(std::uint64_t revision, std::vector<Operation> operations) {
  ow::commands::Envelope value;
  value.world_id = "workshop";
  value.transaction_id = "018f7242-4387-7c98-a114-67787915a499";
  value.idempotency = {"fixture-epoch", 1};
  value.expected_world_revision = revision;
  value.apply_at = {"next_tick", 120};
  value.budget = {static_cast<std::uint64_t>(operations.size()), 0};
  value.operations = std::move(operations);
  return value;
}
ow::commands::TransformSet transform(ow::commands::Target target, bool second) {
  ow::commands::TransformSet result;
  result.target = std::move(target);
  result.position_m = second ? std::array<double,3>{1,.25,0} : std::array<double,3>{-1,0,0};
  result.rotation_xyzw = {0,.6,0,.8};
  result.scale = second ? std::array<double,3>{.5,1.5,1} : std::array<double,3>{1,1,1};
  return result;
}
constexpr std::array<std::array<int,4>,6> colors{{
  {255,64,64,255},{128,32,32,255},{64,255,64,255},
  {32,128,32,255},{64,64,255,255},{32,32,128,255}}};
std::array<double,4> expected_clip(std::array<double,3> local, bool second) {
  if (second) { local[0] *= .5; local[1] *= 1.5; }
  const double x = .28 * local[0] + .96 * local[2] + (second ? 1 : -1);
  const double y = local[1] + (second ? .25 : 0);
  const double z = -.96 * local[0] + .28 * local[2];
  return {x / 4, -y / 3, (5 - z - .1) / 19.9, 1};
}
bool close(const std::array<float,4>& actual, const std::array<double,4>& expected) {
  for (std::size_t c = 0; c < 4; ++c)
    if (!std::isfinite(actual[c]) || std::abs(static_cast<double>(actual[c]) - expected[c]) > 2e-6) return false;
  return true;
}
void check_packet(const ow::presentation::Packet& packet, bool second) {
  require(packet.world_id == "workshop" && packet.world_revision == (second ? 2U : 1U), "literal packet revision");
  require(packet.camera.position_m == std::array<double,3>{0,0,5}, "literal camera position");
  require(packet.camera.left == -4 && packet.camera.right == 4 && packet.camera.bottom == -3 &&
          packet.camera.top == 3 && packet.camera.near_m == .1 && packet.camera.far_m == 20, "literal orthographic camera");
  require(packet.objects.size() == 1 && packet.vertices.size() == 24 && packet.indices.size() == 36, "indexed per-face cube size");
  const auto& object = packet.objects[0];
  require(object.object_id == 1 && object.entity_uuid == "00000007-0000-4000-8000-000000000001" &&
          object.generation == 1 && object.authoring_revision == (second ? 2U : 1U), "literal packet object identity");
  const auto literal = transform(ow::commands::TemporaryTarget{"cube"}, second);
  require(object.transform.position_m == literal.position_m && object.transform.rotation_xyzw == literal.rotation_xyzw &&
          object.transform.scale == literal.scale, "literal packet authored TRS");
  std::array<std::vector<std::size_t>,6> by_face;
  for (std::size_t v = 0; v < packet.vertices.size(); ++v) {
    const auto& vertex = packet.vertices[v];
    require(vertex.object_id == 1, "vertex carries mapped object ID");
    std::size_t selected = 6;
    for (std::size_t f = 0; f < colors.size(); ++f) {
      std::array<double,4> expected{};
      for (std::size_t c = 0; c < 4; ++c) expected[c] = static_cast<double>(colors[f][c]) / 255;
      if (close(vertex.color, expected)) selected = f;
    }
    require(selected < 6, "vertex has one literal face color");
    by_face[selected].push_back(v);
  }
  for (std::size_t face = 0; face < 6; ++face) {
    require(by_face[face].size() == 4, "each face has four vertices");
    const std::size_t axis = face / 2;
    std::vector<std::array<double,4>> expected;
    for (const double a : {-.5,.5}) for (const double b : {-.5,.5}) {
      std::array<double,3> local{};
      local[axis] = face % 2 == 0 ? .5 : -.5;
      local[(axis+1)%3] = a; local[(axis+2)%3] = b;
      expected.push_back(expected_clip(local, second));
    }
    for (const auto index : by_face[face]) {
      const auto match = std::find_if(expected.begin(), expected.end(), [&](const auto& value) {
        return close(packet.vertices[index].clip_position, value);
      });
      require(match != expected.end(), "independent rational-yaw corner projection");
      expected.erase(match);
    }
    require(expected.empty(), "all four distinct face corners present");
    std::vector<std::array<std::uint32_t,3>> triangles;
    for (std::size_t i = 0; i < packet.indices.size(); i += 3) {
      std::array<std::uint32_t,3> tri{packet.indices[i],packet.indices[i+1],packet.indices[i+2]};
      for (auto index : tri) require(index < packet.vertices.size(), "triangle index in range");
      if (std::find(by_face[face].begin(),by_face[face].end(),tri[0]) == by_face[face].end()) continue;
      require(tri[0] != tri[1] && tri[1] != tri[2] && tri[0] != tri[2], "nondegenerate triangle indices");
      for (auto index : tri)
        require(std::find(by_face[face].begin(),by_face[face].end(),index) != by_face[face].end(), "triangle remains on one face");
      triangles.push_back(tri);
    }
    require(triangles.size() == 2, "two indexed triangles per face");
    std::vector<std::uint32_t> shared;
    for (auto index : triangles[0])
      if (std::find(triangles[1].begin(),triangles[1].end(),index) != triangles[1].end()) shared.push_back(index);
    require(shared.size() == 2, "face triangles share one diagonal");
    for (std::size_t c = 0; c < 3; ++c) {
      double sum = 0;
      for (auto index : by_face[face]) sum += packet.vertices[index].clip_position[c];
      const double diagonal = packet.vertices[shared[0]].clip_position[c] + packet.vertices[shared[1]].clip_position[c];
      require(std::abs(diagonal * 2 - sum) < 8e-6, "shared edge is face diagonal");
    }
  }
}
void rejects(const ow::world::Snapshot& snapshot) {
  bool rejected = false;
  try { (void)ow::presentation::make_packet(snapshot); }
  catch (const std::invalid_argument&) { rejected = true; }
  require(rejected, "invalid detached input rejects without a usable packet");
}
}

int main() {
  using namespace ow::commands;
  using ow::transactions::Coordinator;
  try {
    using ow::presentation::Extent;
    for (const auto size : {Extent{1,1},Extent{1024,768},Extent{768,1024},Extent{320,240},Extent{400,300}})
      require(ow::presentation::checked_pixel_count(size) == static_cast<std::uint64_t>(size.width)*size.height, "legal extent exact pixel count");
    for (const auto size : {Extent{0,240},Extent{320,0},Extent{1025,1},Extent{1,1025},
                           Extent{1024,769},Extent{769,1024},Extent{1024,1024},
                           Extent{std::numeric_limits<std::uint32_t>::max(),std::numeric_limits<std::uint32_t>::max()}}) {
      bool rejected = false;
      try { (void)ow::presentation::checked_pixel_count(size); } catch (const std::invalid_argument&) { rejected = true; }
      require(rejected, "oversized or empty extent rejects before allocation");
    }
    require(ow::presentation::checked_pixel_count({400,300}) == 120000, "valid extent recovers after invalid requests");
    constexpr std::uint64_t budget = 64ULL*1024*1024;
    require(ow::presentation::allocation_fits(0,budget), "exact allocation budget is allowed");
    require(ow::presentation::allocation_fits(budget-1,1), "last allocation byte is allowed");
    require(!ow::presentation::allocation_fits(budget,1), "allocation above budget rejects");
    require(!ow::presentation::allocation_fits(1,budget), "aggregate allocations are bounded");
    require(!ow::presentation::allocation_fits(std::numeric_limits<std::uint64_t>::max(),1), "allocation addition cannot wrap");
    require(!ow::presentation::allocation_fits(1,std::numeric_limits<std::uint64_t>::max()), "allocation request cannot wrap");
    using Cap = ow::presentation::Capabilities;
    Cap supported{true,true,true,true,true,true,true,true,true,true};
    require(ow::presentation::missing_capabilities(supported).empty(), "complete capabilities are supported");
    const std::array<bool Cap::*,10> capability_fields{
      &Cap::runtime_1_3,&Cap::graphics_present_queue,&Cap::unorm_surface,&Cap::color_attachment,
      &Cap::id_attachment,&Cap::depth_attachment,&Cap::transfer_source,&Cap::swapchain_transfer_destination,
      &Cap::maintenance_extensions,&Cap::maintenance_feature};
    for (const auto member : capability_fields) {
      auto missing = supported; missing.*member = false;
      require(ow::presentation::missing_capabilities(missing).size() == 1, "each required capability independently prevents startup");
    }
    require(ow::presentation::missing_capabilities(Cap{}).size() == 10, "unsupported capabilities report all missing requirements");
    ow::world::World world("workshop",7,4);
    const auto empty = ow::presentation::make_packet(world.snapshot());
    require(empty.world_revision == 0 && empty.objects.empty() && empty.vertices.empty() && empty.indices.empty(), "empty world is an empty packet");
    auto first = Coordinator::apply_at_boundary(world, envelope(0,{
      EntityCreate{"cube","builtin.unit_cube"},transform(TemporaryTarget{"cube"},false)}));
    require(first.status == "committed" && first.world_revision == 1 && first.created.size() == 1, "actual first transaction commits");
    const auto snapshot1 = world.snapshot();
    const auto packet1 = ow::presentation::make_packet(snapshot1);
    check_packet(packet1,false);
    const EntityTarget target{"workshop","00000007-0000-4000-8000-000000000001",1};
    auto second = Coordinator::apply_at_boundary(world,envelope(1,{transform(target,true)}));
    require(second.status == "committed" && second.world_revision == 2, "actual second transaction commits");
    const auto snapshot2 = world.snapshot();
    const auto packet2 = ow::presentation::make_packet(snapshot2);
    check_packet(packet2,true);
    require(ow::presentation::make_packet(snapshot1) == packet1 && world.snapshot() == snapshot2, "old snapshot and packet remain detached");
    auto edited = packet2;
    edited.objects[0].transform.position_m[0] = 999;
    edited.vertices[0].clip_position[0] = 999;
    require(ow::presentation::make_packet(snapshot2) == packet2 && world.snapshot() == snapshot2, "packet owns its memory without world aliases");
    for (double huge : {1e300,-1e300,std::numeric_limits<double>::max()}) {
      auto move = transform(target,true); move.position_m[0] = huge;
      require(Coordinator::apply_at_boundary(world,envelope(world.snapshot().world_revision,{move})).status == "committed", "finite world double commits before presentation rejection");
      const auto before = world.snapshot();
      rejects(before);
      require(world.snapshot() == before, "float conversion rejection cannot mutate world");
      require(Coordinator::apply_at_boundary(world,envelope(before.world_revision,{transform(target,true)})).status == "committed", "valid typed transform recovers");
      require(ow::presentation::make_packet(world.snapshot()).vertices.size() == 24, "packet creation recovers after float overflow");
    }
    for (double invalid : {std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
      auto malformed = snapshot2; malformed.slots[0].entity->transform.position_m[0] = invalid; rejects(malformed);
    }
    auto oversized = snapshot2; oversized.slots.resize(1025); rejects(oversized);
    auto zero_scale = snapshot2; zero_scale.slots[0].entity->transform.scale[0] = 0; rejects(zero_scale);
    auto bad_quaternion = snapshot2; bad_quaternion.slots[0].entity->transform.rotation_xyzw = {0,0,0,0}; rejects(bad_quaternion);
    auto unknown = snapshot2; unknown.slots[0].entity->prefab = "missing"; rejects(unknown);
    const auto revision = world.snapshot().world_revision;
    require(Coordinator::apply_at_boundary(world,envelope(revision,{EntityDelete{target,"reject_if_children"}})).status == "committed", "typed deletion commits");
    const auto deleted = ow::presentation::make_packet(world.snapshot());
    require(deleted.objects.empty() && deleted.vertices.empty() && deleted.indices.empty(), "tombstone produces no stale draw");
    require(Coordinator::apply_at_boundary(world,envelope(revision+1,{EntityCreate{"new","builtin.unit_cube"}})).status == "committed", "typed recreation commits");
    const auto reused = ow::presentation::make_packet(world.snapshot());
    require(reused.objects.size() == 1 && reused.objects[0].generation == 2 && reused.objects[0].entity_uuid == target.entity_uuid, "new generation reaches presentation mapping");
    ow::world::World maximum_world("workshop",7,1024);
    for (std::uint64_t batch = 0; batch < 4; ++batch) {
      std::vector<Operation> operations;
      for (unsigned i = 0; i < 256; ++i)
        operations.push_back(EntityCreate{"cube"+std::to_string(i),"builtin.unit_cube"});
      require(Coordinator::apply_at_boundary(maximum_world,envelope(batch,std::move(operations))).status == "committed", "maximum packet fixture uses real bounded commits");
    }
    const auto maximum_snapshot = maximum_world.snapshot();
    const auto maximum_packet = ow::presentation::make_packet(maximum_snapshot);
    require(maximum_packet.objects.size() == 1024 && maximum_packet.vertices.size() == 24576 &&
            maximum_packet.indices.size() == 36864, "maximum legal packet stays within exact bounded counts");
    require(maximum_packet.objects.front().object_id == 1 && maximum_packet.objects.back().object_id == 1024 &&
            maximum_packet.objects.back().entity_uuid == "00000007-0000-4000-8000-000000000400", "maximum packet has literal stable sorted identity mapping");
    auto permuted_snapshot = maximum_snapshot;
    std::reverse(permuted_snapshot.slots.begin(),permuted_snapshot.slots.end());
    require(ow::presentation::make_packet(permuted_snapshot) == maximum_packet, "detached slot order cannot change presentation identity order");
    std::cout << "{\"status\":\"passed\",\"assertions\":" << checks << "}\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "presentation_native_test failed: " << error.what() << '\n';
    return 1;
  }
}

