// SPDX-License-Identifier: Apache-2.0
#include "omniweft/presentation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace ow::presentation {
namespace {
using Vec3 = std::array<double, 3>;
constexpr std::array<std::array<Vec3, 4>, 6> faces{{
  {{{.5,-.5,-.5},{.5,.5,-.5},{.5,.5,.5},{.5,-.5,.5}}},
  {{{-.5,-.5,.5},{-.5,.5,.5},{-.5,.5,-.5},{-.5,-.5,-.5}}},
  {{{-.5,.5,-.5},{-.5,.5,.5},{.5,.5,.5},{.5,.5,-.5}}},
  {{{-.5,-.5,.5},{-.5,-.5,-.5},{.5,-.5,-.5},{.5,-.5,.5}}},
  {{{-.5,-.5,.5},{.5,-.5,.5},{.5,.5,.5},{-.5,.5,.5}}},
  {{{.5,-.5,-.5},{-.5,-.5,-.5},{-.5,.5,-.5},{.5,.5,-.5}}}
}};
constexpr std::array<std::array<unsigned, 4>, 6> colors{{
  {255,64,64,255},{128,32,32,255},{64,255,64,255},
  {32,128,32,255},{64,64,255,255},{32,32,128,255}
}};
void require(bool condition) {
  if (!condition) throw std::invalid_argument("Presentation requires bounded, finite committed object data.");
}
bool identifier(std::string_view value) {
  const auto alnum = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); };
  if (value.empty() || value.size() > 128 || !alnum(value.front())) return false;
  for (char c : value) if (!alnum(c) && c != '.' && c != '_' && c != ':' && c != '-') return false;
  return true;
}
bool uuid(std::string_view value) {
  if (value.size() != 36) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (value[i] != '-') return false; }
    else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
  }
  return true;
}
template<std::size_t Size> void finite(const std::array<double, Size>& values) {
  for (double value : values) require(std::isfinite(value));
}
float checked_float(double value) {
  require(std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max());
  const auto result = static_cast<float>(value);
  require(std::isfinite(result));
  return result == 0 ? 0 : result;
}
Vec3 transformed(Vec3 local, const world::Transform& transform) {
  for (std::size_t i = 0; i < 3; ++i) local[i] *= transform.scale[i];
  const auto& q = transform.rotation_xyzw;
  const double x=q[0], y=q[1], z=q[2], w=q[3];
  return {
    (1-2*(y*y+z*z))*local[0] + 2*(x*y-z*w)*local[1] + 2*(x*z+y*w)*local[2] + transform.position_m[0],
    2*(x*y+z*w)*local[0] + (1-2*(x*x+z*z))*local[1] + 2*(y*z-x*w)*local[2] + transform.position_m[1],
    2*(x*z-y*w)*local[0] + 2*(y*z+x*w)*local[1] + (1-2*(x*x+y*y))*local[2] + transform.position_m[2]};
}
}
Packet make_packet(const world::Snapshot& snapshot) {
  require(snapshot.format_version == 1 && identifier(snapshot.world_id));
  require(snapshot.max_slots > 0 && snapshot.max_slots <= 1024 &&
    snapshot.slots.size() <= snapshot.max_slots);
  Packet packet;
  packet.world_id = snapshot.world_id;
  packet.world_revision = snapshot.world_revision;
  std::vector<const world::Slot*> slots;
  for (const auto& slot : snapshot.slots) {
    require(uuid(slot.entity_uuid) && slot.generation > 0 && !(slot.retired && slot.entity));
    slots.push_back(&slot);
  }
  std::sort(slots.begin(), slots.end(), [](const auto* a, const auto* b) { return a->entity_uuid < b->entity_uuid; });
  std::string previous;
  for (const auto* slot : slots) {
    require(slot->entity_uuid != previous);
    previous = slot->entity_uuid;
    if (!slot->entity) continue;
    const auto& entity = *slot->entity;
    require(entity.prefab == "builtin.unit_cube" && entity.authoring_revision > 0 &&
      entity.authoring_revision <= snapshot.world_revision);
    const auto& source = entity.transform;
    finite(source.position_m); finite(source.rotation_xyzw); finite(source.scale);
    double norm = 0;
    for (double value : source.rotation_xyzw) norm += value*value;
    require(std::isfinite(norm) && std::abs(norm-1) <= 1e-12);
    for (double value : source.scale) require(value != 0);
    const auto id = static_cast<std::uint32_t>(packet.objects.size()) + 1;
    packet.objects.push_back({id, slot->entity_uuid, slot->generation, entity.authoring_revision, source});
    // Mutation proof replaces this exact source-transform binding in a disposable copy.
    const world::Transform render_transform = source;
    for (std::size_t face = 0; face < faces.size(); ++face) {
      const auto base = static_cast<std::uint32_t>(packet.vertices.size());
      for (const auto& local : faces[face]) {
        const auto position = transformed(local, render_transform);
        finite(position);
        const auto& c = packet.camera;
        Vertex vertex;
        vertex.clip_position = {
          checked_float((2*(position[0]-c.position_m[0])-c.right-c.left)/(c.right-c.left)),
          checked_float(-(2*(position[1]-c.position_m[1])-c.top-c.bottom)/(c.top-c.bottom)),
          checked_float((c.position_m[2]-position[2]-c.near_m)/(c.far_m-c.near_m)), 1};
        for (std::size_t channel = 0; channel < 4; ++channel)
          vertex.color[channel] = static_cast<float>(colors[face][channel])/255.0F;
        vertex.object_id = id;
        packet.vertices.push_back(vertex);
      }
      for (const auto offset : {0U,1U,2U,2U,3U,0U}) packet.indices.push_back(base+offset);
    }
  }
  return packet;
}
std::uint64_t checked_pixel_count(Extent extent) {
  require(extent.width > 0 && extent.height > 0 && extent.width <= 1024 && extent.height <= 1024);
  const auto pixels = static_cast<std::uint64_t>(extent.width) * extent.height;
  require(pixels <= 786432);
  return pixels;
}
bool allocation_fits(std::uint64_t current_bytes, std::uint64_t requested_bytes) {
  constexpr std::uint64_t limit = 64ULL * 1024 * 1024;
  return current_bytes <= limit && requested_bytes <= limit - current_bytes;
}
std::vector<std::string> missing_capabilities(const Capabilities& c) {
  std::vector<std::string> missing;
  if (!c.runtime_1_3) missing.emplace_back("Vulkan 1.3 runtime");
  if (!c.graphics_present_queue) missing.emplace_back("one graphics and present queue");
  if (!c.unorm_surface) missing.emplace_back("RGBA8 or BGRA8 UNORM surface");
  if (!c.color_attachment) missing.emplace_back("color attachment format");
  if (!c.id_attachment) missing.emplace_back("R32_UINT attachment format");
  if (!c.depth_attachment) missing.emplace_back("D32_SFLOAT attachment format");
  if (!c.transfer_source) missing.emplace_back("attachment transfer-source formats");
  if (!c.swapchain_transfer_destination) missing.emplace_back("swapchain transfer-destination usage");
  if (!c.maintenance_extensions) missing.emplace_back("swapchain maintenance extension dependencies");
  if (!c.maintenance_feature) missing.emplace_back("swapchainMaintenance1 feature");
  return missing;
}
}  // namespace ow::presentation

