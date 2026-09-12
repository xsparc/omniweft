// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "omniweft/commands.hpp"
#include "omniweft/world.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ow::transactions {
struct Error {
  std::string code;
  std::string path;
  std::string message;
  std::optional<std::size_t> operation_index;
  bool operator==(const Error&) const = default;
};
struct CreatedBinding {
  std::string temporary_id;
  std::string world_id;
  std::string entity_uuid;
  std::uint64_t generation = 0;
  bool operator==(const CreatedBinding&) const = default;
};
struct Receipt {
  std::string status = "rejected";
  std::string durability = "volatile";
  std::string transaction_id;
  std::uint64_t world_revision = 0;
  std::vector<CreatedBinding> created;
  std::vector<Error> errors;
  bool operator==(const Receipt&) const = default;
};

// Trusted host boundary only: no authentication, scheduling or exactly-once admission.
class Coordinator {
 public:
  static Receipt apply_at_boundary(world::World& world, const commands::Envelope& envelope);
};
}  // namespace ow::transactions
