// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/assets.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ow::gltf {
using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;
using Mat4 = std::array<double, 16>; // Column-major; parent-world times local.
struct Bounds {
  Vec3 minimum{}, maximum{};
  bool operator==(const Bounds&) const = default;
};
struct Primitive {
  std::vector<Vec3> positions;
  std::vector<std::uint32_t> indices;
  std::optional<std::uint32_t> material;
  Bounds bounds;
  bool operator==(const Primitive&) const = default;
};
struct Mesh {
  std::vector<Primitive> primitives;
  bool operator==(const Mesh&) const = default;
};
struct Node {
  std::optional<std::uint32_t> mesh;
  std::vector<std::uint32_t> children;
  Vec3 translation{0,0,0};
  Vec4 rotation{0,0,0,1};
  Vec3 scale{1,1,1};
  Mat4 local_matrix{}, world_matrix{};
  std::optional<Bounds> world_bounds; // This node's mesh, not its descendants.
  bool operator==(const Node&) const = default;
};
struct Material {
  std::string name;
  Vec4 base_color_factor{1,1,1,1};
  double metallic_factor = 1, roughness_factor = 1;
  bool double_sided = false;
  bool operator==(const Material&) const = default;
};
struct Scene {
  std::uint32_t profile = 1;
  assets::Asset source_asset;
  std::vector<std::uint32_t> roots;
  std::vector<Mesh> meshes;
  std::vector<Node> nodes;
  std::vector<Material> materials;
  Bounds world_bounds; // Only meshes reachable from the selected scene roots.
  bool operator==(const Scene&) const = default;
};
struct ImportRequest {
  assets::Bytes glb;
  std::string source, license;
  std::uint64_t expected_revision = 0;
};
struct ImportResult {
  assets::Receipt receipt;
  std::optional<Scene> scene;
  bool operator==(const ImportResult&) const = default;
};
// Profile 1: bounded GLB 2.0, internal BIN, POSITION/indexed triangles and TRS.
// Stage every returned value and immutable source bundle before Catalog Import.
// Failure returns no scene and preserves the full Catalog and caller bytes.
// Success (including deduplication) advances its revision exactly once.
ImportResult import_glb(assets::Catalog&, const ImportRequest&);
} // namespace ow::gltf
