// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ow::transactions { class Coordinator; }

namespace ow::world {
struct Transform {
  std::array<double, 3> position_m{};
  std::array<double, 4> rotation_xyzw{0.0, 0.0, 0.0, 1.0};
  std::array<double, 3> scale{1.0, 1.0, 1.0};
  bool operator==(const Transform&) const = default;
};
struct ParentIdentity {
  std::string entity_uuid;
  std::uint64_t generation = 0;
  bool operator==(const ParentIdentity&) const = default;
};
struct Entity {
  std::string prefab;
  std::uint64_t authoring_revision = 0;
  Transform transform;  // Cached world-space TRS, including for parented entities.
  std::optional<ParentIdentity> parent;
  Transform local_transform;  // Authored local TRS only while parent is present.
  std::vector<std::string> tags;  // Sorted bounded semantic labels; never authority.
  bool operator==(const Entity&) const = default;
};
struct Slot {
  std::string entity_uuid;
  std::uint64_t generation = 1;
  bool retired = false;
  std::optional<Entity> entity;
  bool operator==(const Slot&) const = default;
};
struct Snapshot {
  std::uint32_t format_version = 1;
  std::string world_id;
  std::uint32_t seed = 7;
  std::uint32_t max_slots = 1024;
  std::uint64_t world_revision = 0;
  std::vector<Slot> slots;
  bool operator==(const Snapshot&) const = default;
};

// Diagnostic encoding of a trusted detached snapshot; not validation or restoration.
std::vector<std::uint8_t> canonical_bytes(const Snapshot&);

// Single-owner-thread authoring storage. Snapshots are detached deep value copies.
class World {
 public:
  explicit World(std::string world_id, std::uint32_t seed = 7, std::uint32_t max_slots = 1024);
  World(const World&) = delete;
  World& operator=(const World&) = delete;
  World(World&&) = delete;
  World& operator=(World&&) = delete;
  Snapshot snapshot() const;
  std::vector<std::uint8_t> canonical_bytes() const;

 private:
  Snapshot state_;
  friend class ow::transactions::Coordinator;
};
}  // namespace ow::world
