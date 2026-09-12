// SPDX-License-Identifier: Apache-2.0
#include "omniweft/world.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ow::world {
namespace {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "Canonical object state requires IEEE-754 binary64.");
bool alphanumeric(char value) {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9');
}
bool valid_id(std::string_view text) {
  if (text.empty() || text.size() > 128 || !alphanumeric(text.front())) return false;
  for (const char value : text) {
    if (!alphanumeric(value) && value != '.' && value != '_' && value != ':' && value != '-')
      return false;
  }
  return true;
}
void integer(std::vector<std::uint8_t>& bytes, std::uint64_t value, unsigned count) {
  for (unsigned index = 0; index < count; ++index)
    bytes.push_back(static_cast<std::uint8_t>((value >> (8U * index)) & 255U));
}
void text(std::vector<std::uint8_t>& bytes, std::string_view value) {
  integer(bytes, static_cast<std::uint32_t>(value.size()), 4);
  bytes.insert(bytes.end(), value.begin(), value.end());
}
template<std::size_t Size>
void numbers(std::vector<std::uint8_t>& bytes, const std::array<double, Size>& values) {
  for (const double value : values)
    integer(bytes, std::bit_cast<std::uint64_t>(value == 0.0 ? 0.0 : value), 8);
}
}  // namespace

World::World(std::string world_id, std::uint32_t seed, std::uint32_t max_slots) {
  if (!valid_id(world_id))
    throw std::invalid_argument("World ID must match the bounded ASCII identifier contract.");
  if (max_slots == 0 || max_slots > 1024)
    throw std::invalid_argument("World max_slots must be from 1 through 1024.");
  state_.world_id = std::move(world_id);
  state_.seed = seed;
  state_.max_slots = max_slots;
}
Snapshot World::snapshot() const { return state_; }

std::vector<std::uint8_t> World::canonical_bytes() const {
  std::vector<std::uint8_t> bytes{'O', 'W', 'O', 'B', 'J', '0', '0', '1'};
  text(bytes, state_.world_id);
  integer(bytes, state_.seed, 4);
  integer(bytes, state_.max_slots, 4);
  integer(bytes, state_.world_revision, 8);
  integer(bytes, static_cast<std::uint32_t>(state_.slots.size()), 4);
  // Slots are appended in fixed-width UUID order and are never reordered.
  for (const auto& slot : state_.slots) {
    bytes.insert(bytes.end(), slot.entity_uuid.begin(), slot.entity_uuid.end());
    integer(bytes, slot.generation, 8);
    bytes.push_back(static_cast<std::uint8_t>(slot.retired ? 1 : 0));
    bytes.push_back(static_cast<std::uint8_t>(slot.entity ? 1 : 0));
    if (slot.entity) {
      text(bytes, slot.entity->prefab);
      integer(bytes, slot.entity->authoring_revision, 8);
      numbers(bytes, slot.entity->transform.position_m);
      numbers(bytes, slot.entity->transform.rotation_xyzw);
      numbers(bytes, slot.entity->transform.scale);
    }
  }
  return bytes;
}
}  // namespace ow::world
