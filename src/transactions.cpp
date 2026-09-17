// SPDX-License-Identifier: Apache-2.0
#include "omniweft/transactions.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <iterator>
#include <map>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ow::transactions {
namespace {
[[noreturn]] void reject(const char* code, std::string path, const char* message,
                        std::optional<std::size_t> index = std::nullopt) {
  throw Error{code, std::move(path), message, index};
}
Error schema_error(const commands::ValidationError& source) {
  Error error{source.code, source.path, source.message, std::nullopt};
  constexpr std::string_view prefix = "/operations/";
  if (std::string_view(source.path).starts_with(prefix)) {
    const auto suffix = std::string_view(source.path).substr(prefix.size());
    std::size_t index = 0;
    const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
    if (result.ec == std::errc{} && result.ptr != suffix.data() &&
        (result.ptr == suffix.data() + suffix.size() || *result.ptr == '/'))
      error.operation_index = index;
  }
  return error;
}
std::string hex(std::uint64_t value, std::size_t width) {
  std::string result(width, '0');
  constexpr char digits[] = "0123456789abcdef";
  while (width != 0) {
    result[--width] = digits[value & 15U];
    value >>= 4U;
  }
  return result;
}
std::string uuid(std::uint32_t seed, std::size_t index) {
  return hex(seed, 8) + "-0000-4000-8000-" + hex(static_cast<std::uint64_t>(index) + 1, 12);
}
template<std::size_t Size>
void normalize_zero(std::array<double, Size>& values) {
  for (double& value : values) if (value == 0.0) value = 0.0;
}
world::Transform transform(const commands::TransformSet& operation, const std::string& path,
                           std::size_t index) {
  world::Transform result{operation.position_m, operation.rotation_xyzw, operation.scale};
  double squared_norm = 0.0;
  for (const double value : result.rotation_xyzw) squared_norm += value * value;
  if (!std::isfinite(squared_norm) || std::abs(squared_norm - 1.0) > 1e-12)
    reject("INVALID_SCHEMA", path + "/rotation_xyzw", "Quaternion squared norm must be within 1e-12 of one.", index);
  for (const double value : result.scale) {
    if (value == 0.0)
      reject("INVALID_SCHEMA", path + "/scale", "Scale components must be nonzero.", index);
  }
  normalize_zero(result.position_m);
  normalize_zero(result.rotation_xyzw);
  normalize_zero(result.scale);
  return result;
}
using Names = std::map<std::string, commands::EntityTarget>;
world::Slot& resolve(world::Snapshot& staged, const commands::Target& target, const Names& names,
                     const std::string& path, std::size_t index) {
  const commands::EntityTarget* identity = nullptr;
  const bool temporary = std::holds_alternative<commands::TemporaryTarget>(target);
  if (temporary) {
    const auto found = names.find(std::get<commands::TemporaryTarget>(target).temporary_id);
    if (found == names.end())
      reject("NOT_FOUND", path + "/temporary_id", "Temporary target must reference an earlier create in this batch.", index);
    identity = &found->second;
  } else {
    identity = &std::get<commands::EntityTarget>(target);
  }
  if (identity->world_id != staged.world_id)
    reject("NOT_FOUND", path + "/world_id", "Target world is not this world.", index);
  const auto found = std::find_if(staged.slots.begin(), staged.slots.end(),
    [&](const world::Slot& slot) { return slot.entity_uuid == identity->entity_uuid; });
  if (found == staged.slots.end())
    reject("NOT_FOUND", path + "/entity_uuid", "Entity UUID is not known in this world.", index);
  if (found->retired || !found->entity || found->generation != identity->generation)
    reject("STALE_HANDLE", path + (temporary ? "/temporary_id" : "/generation"),
           "Entity handle is deleted, retired or has a stale generation.", index);
  return *found;
}
}  // namespace

Receipt Coordinator::apply_at_boundary(world::World& target_world, const commands::Envelope& envelope, StagingGuard* guard) {
  Receipt receipt;
  // Preserve valid receipt correlation without copying unbounded typed input.
  if (!guard || envelope.transaction_id.size() <= 128) receipt.transaction_id = envelope.transaction_id;
  receipt.world_revision = target_world.state_.world_revision;
  // Policy admission precedes serialization, complete-world clone and staging
  // allocation. A guard exception leaves both authoritative stores untouched.
  try { if (guard) guard->begin(target_world, target_world.state_, envelope); }
  catch (const Error& error) { receipt.errors.push_back(error); return receipt; }
  const auto validated = commands::serialize(envelope);
  if (!validated.json) {
    for (const auto& error : validated.errors) receipt.errors.push_back(schema_error(error));
    return receipt;
  }
  if (envelope.world_id != target_world.state_.world_id) {
    receipt.errors.push_back({"NOT_FOUND", "/world_id", "Envelope world is not this world.", std::nullopt});
    return receipt;
  }
  if (envelope.expected_world_revision != target_world.state_.world_revision) {
    receipt.errors.push_back({"REVISION_CONFLICT", "/expected_world_revision", "Expected authoring revision is stale.", std::nullopt});
    return receipt;
  }
  if (target_world.state_.world_revision == std::numeric_limits<std::uint64_t>::max()) {
    receipt.errors.push_back({"BUDGET_EXCEEDED", "/expected_world_revision", "World revision is exhausted.", std::nullopt});
    return receipt;
  }

  // The complete allocator, tombstones and authored state are copied before any operation.
  auto staged = target_world.state_;
  const auto next_revision = staged.world_revision + 1;
  Names names;
  std::vector<CreatedBinding> created;
  created.reserve(envelope.operations.size());
  try {
    for (std::size_t index = 0; index < envelope.operations.size(); ++index) {
      const auto path = "/operations/" + std::to_string(index);
      std::visit([&](const auto& operation) {
        using Type = std::decay_t<decltype(operation)>;
        if constexpr (std::is_same_v<Type, commands::EntityCreate>) {
          if (names.contains(operation.temporary_id))
            reject("INVALID_SCHEMA", path + "/temporary_id", "Temporary names must be unique throughout the batch.", index);
          if (operation.prefab != "builtin.unit_cube")
            reject("NOT_FOUND", path + "/prefab", "Only the builtin.unit_cube prefab is executable.", index);
          auto slot = std::find_if(staged.slots.begin(), staged.slots.end(),
            [](const world::Slot& entry) { return !entry.entity && !entry.retired; });
          if (slot == staged.slots.end()) {
            if (staged.slots.size() >= staged.max_slots)
              reject("BUDGET_EXCEEDED", path, "World slot capacity is exhausted.", index);
            if (guard) guard->create(staged.slots.size(), index);
            const auto identifier = uuid(staged.seed, staged.slots.size());
            staged.slots.push_back(world::Slot{identifier, 1, false, std::nullopt});
            slot = std::prev(staged.slots.end());
          }
          else if (guard) guard->create(static_cast<std::size_t>(slot - staged.slots.begin()), index);
          slot->entity = world::Entity{operation.prefab, next_revision, {}};
          names.emplace(operation.temporary_id, commands::EntityTarget{
            staged.world_id, slot->entity_uuid, slot->generation});
          created.push_back({operation.temporary_id, staged.world_id, slot->entity_uuid, slot->generation});
        } else if constexpr (std::is_same_v<Type, commands::TransformSet>) {
          const auto replacement = transform(operation, path, index);
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          if (guard) guard->transform(static_cast<std::size_t>(&slot - staged.slots.data()),
                                      slot.entity->transform, replacement, index);
          slot.entity->transform = replacement;
          slot.entity->authoring_revision = next_revision;
        } else {
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          if (guard) guard->erase(static_cast<std::size_t>(&slot - staged.slots.data()),
                                  slot.entity->transform, index);
          slot.entity.reset();
          if (slot.generation == std::numeric_limits<std::uint64_t>::max())
            slot.retired = true;
          else
            ++slot.generation;
        }
      }, envelope.operations[index]);
    }
    if (guard) guard->finish(staged);
  } catch (const Error& error) {
    // Rejection discards every staged write and provisional identity binding.
    receipt.errors.push_back(error);
    return receipt;
  }

  staged.world_revision = next_revision;
  receipt.status = "committed";
  receipt.world_revision = next_revision;
  receipt.created = std::move(created);
  // All receipt allocation precedes publication. Swap and return cannot allocate.
  static_assert(std::is_nothrow_swappable_v<world::Snapshot>);
  static_assert(std::is_nothrow_move_constructible_v<Receipt>);
  using std::swap;
  swap(target_world.state_, staged);
  if (guard) guard->commit();
  return receipt;
}
}  // namespace ow::transactions
