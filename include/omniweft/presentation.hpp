// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/world.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ow::presentation {
struct Extent { std::uint32_t width = 0, height = 0; bool operator==(const Extent&) const = default; };
struct Camera {
  std::array<double, 3> position_m{0, 0, 5};
  double left = -4, right = 4, bottom = -3, top = 3, near_m = .1, far_m = 20;
  bool operator==(const Camera&) const = default;
};
struct Vertex {
  std::array<float, 4> clip_position{};
  std::array<float, 4> color{};
  std::uint32_t object_id = 0;
  bool operator==(const Vertex&) const = default;
};
struct Object {
  std::uint32_t object_id = 0;
  std::string entity_uuid;
  std::uint64_t generation = 0, authoring_revision = 0;
  world::Transform transform;
  bool operator==(const Object&) const = default;
};
struct Packet {
  std::string world_id;
  std::uint64_t world_revision = 0;
  Camera camera;
  std::vector<Object> objects;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  bool operator==(const Packet&) const = default;
};
Packet make_packet(const world::Snapshot& snapshot);
// Returns width*height after checking the frozen presentation allocation limits.
std::uint64_t checked_pixel_count(Extent extent);
bool allocation_fits(std::uint64_t current_bytes, std::uint64_t requested_bytes);

// Capability policy is pure data so unsupported startup cases need no Vulkan driver.
struct Capabilities {
  bool runtime_1_3 = false, graphics_present_queue = false, unorm_surface = false;
  bool color_attachment = false, id_attachment = false, depth_attachment = false;
  bool transfer_source = false, swapchain_transfer_destination = false;
  bool maintenance_extensions = false, maintenance_feature = false;
};
std::vector<std::string> missing_capabilities(const Capabilities& capabilities);
}  // namespace ow::presentation

