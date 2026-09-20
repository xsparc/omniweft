// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ow::commands {
inline constexpr std::size_t max_envelope_bytes = 1048576;
inline constexpr std::size_t max_nesting_depth = 32;
inline constexpr std::size_t max_operations = 256;

struct ValidationError {
  std::string code;
  std::string path;
  std::string message;
  bool operator==(const ValidationError&) const = default;
};
struct Idempotency {
  std::string epoch;
  std::uint64_t sequence = 0;
  bool operator==(const Idempotency&) const = default;
};
struct ApplyAt {
  std::string mode = "next_tick";
  std::uint64_t expires_after_ticks = 0;
  bool operator==(const ApplyAt&) const = default;
};
struct Budget {
  std::uint64_t max_operations = 0;
  std::uint64_t max_blob_bytes = 0;
  bool operator==(const Budget&) const = default;
};
struct TemporaryTarget {
  std::string temporary_id;
  bool operator==(const TemporaryTarget&) const = default;
};
struct EntityTarget {
  std::string world_id;
  std::string entity_uuid;
  std::uint64_t generation = 0;
  bool operator==(const EntityTarget&) const = default;
};
using Target = std::variant<TemporaryTarget, EntityTarget>;
struct EntityCreate {
  std::string temporary_id;
  std::string prefab;
  bool operator==(const EntityCreate&) const = default;
};
struct TransformSet {
  Target target;
  std::array<double, 3> position_m{};
  std::array<double, 4> rotation_xyzw{0.0, 0.0, 0.0, 1.0};
  std::array<double, 3> scale{1.0, 1.0, 1.0};
  bool operator==(const TransformSet&) const = default;
};
struct EntityDelete {
  Target target;
  std::string child_policy = "reject_if_children";
  bool operator==(const EntityDelete&) const = default;
};
struct EntityReparent {
  Target target;
  std::optional<Target> parent;
  std::string mode = "preserve_world";
  bool operator==(const EntityReparent&) const = default;
};
struct EntityTagsSet {
  Target target;
  std::vector<std::string> tags;
  bool operator==(const EntityTagsSet&) const = default;
};
using Operation = std::variant<EntityCreate, TransformSet, EntityDelete, EntityReparent, EntityTagsSet>;
struct Envelope {
  std::string protocol_version = "0.1";
  std::string world_id;
  std::string transaction_id;
  Idempotency idempotency;
  std::uint64_t expected_world_revision = 0;
  ApplyAt apply_at;
  Budget budget;
  std::vector<Operation> operations;
  bool operator==(const Envelope&) const = default;
};
struct ParseResult {
  std::optional<Envelope> envelope;
  std::vector<ValidationError> errors;
};
struct SerializeResult {
  std::optional<std::string> json;
  std::vector<ValidationError> errors;
};

// Validation only. Neither function authenticates, schedules, or mutates a world.
ParseResult parse(std::string_view bytes);
SerializeResult serialize(const Envelope& envelope);
}  // namespace ow::commands
