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
// The graph is private committed state; every traversal is iterative and bounded by slots.
bool child_of(const world::Slot& child, const world::Slot& parent) {
  return child.entity && child.entity->parent &&
    child.entity->parent->entity_uuid == parent.entity_uuid &&
    child.entity->parent->generation == parent.generation;
}
bool has_children(const world::Snapshot& state, const world::Slot& parent) {
  return std::any_of(state.slots.begin(), state.slots.end(),
    [&](const auto& child) { return child_of(child, parent); });
}
world::Slot* parent_of(world::Snapshot& state, const world::Slot& child) {
  if (!child.entity->parent) return nullptr;
  const auto found = std::find_if(state.slots.begin(), state.slots.end(),
    [&](const auto& parent) { return parent.entity && !parent.retired && child_of(child, parent); });
  if (found == state.slots.end())
    reject("STALE_HANDLE", "/parent", "Parent generation is no longer live.");
  return &*found;
}
void parent_scale(const world::Transform& value, const std::string& path, std::size_t index) {
  if (!(value.scale[0] > 0) || value.scale[0] != value.scale[1] || value.scale[0] != value.scale[2])
    reject("INVALID_SCHEMA", path, "Parents require positive uniform world scale.", index);
}
using Vec3 = std::array<double, 3>;
using Quat = std::array<double, 4>;
Quat unit(Quat q) {
  double norm = 0;
  for (const double v : q) norm += v*v;
  const double length = std::sqrt(norm);
  for (double& v : q) v /= length;
  return q;
}
Quat product(const Quat& a, const Quat& b) {
  return unit({a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],
    a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],
    a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],
    a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]});
}
Vec3 rotate(const Quat& q, const Vec3& v) {
  const double x=q[0], y=q[1], z=q[2], w=q[3];
  return {(1-2*(y*y+z*z))*v[0]+2*(x*y-z*w)*v[1]+2*(x*z+y*w)*v[2],
    2*(x*y+z*w)*v[0]+(1-2*(x*x+z*z))*v[1]+2*(y*z-x*w)*v[2],
    2*(x*z-y*w)*v[0]+2*(y*z+x*w)*v[1]+(1-2*(x*x+y*y))*v[2]};
}
world::Transform checked(world::Transform value, const std::string& path, std::size_t index) {
  const auto finite = [](const auto& items) {
    return std::all_of(items.begin(), items.end(), [](double v) { return std::isfinite(v); });
  };
  if (!finite(value.position_m) || !finite(value.rotation_xyzw) || !finite(value.scale) ||
      std::any_of(value.scale.begin(), value.scale.end(), [](double v) { return v == 0; }))
    reject("INVALID_SCHEMA", path, "Derived hierarchy transform must be finite with nonzero scale.", index);
  normalize_zero(value.position_m); normalize_zero(value.rotation_xyzw); normalize_zero(value.scale);
  return value;
}
world::Transform compose(const world::Transform& parent, const world::Transform& local,
                          const std::string& path, std::size_t index) {
  parent_scale(parent, path, index);
  auto displacement = local.position_m;
  for (double& v : displacement) v *= parent.scale[0];
  auto position = rotate(unit(parent.rotation_xyzw), displacement);
  auto scale = local.scale;
  for (std::size_t i=0; i<3; ++i) {
    position[i] += parent.position_m[i]; scale[i] *= parent.scale[0];
  }
  return checked({position, product(unit(parent.rotation_xyzw), unit(local.rotation_xyzw)), scale}, path, index);
}
world::Transform relative(const world::Transform& parent, const world::Transform& value,
                           const std::string& path, std::size_t index) {
  parent_scale(parent, path, index);
  auto inverse = unit(parent.rotation_xyzw);
  for (std::size_t i=0; i<3; ++i) inverse[i] = -inverse[i];
  auto displacement = value.position_m;
  for (std::size_t i=0; i<3; ++i) displacement[i] -= parent.position_m[i];
  auto position = rotate(inverse, displacement);
  auto scale = value.scale;
  for (std::size_t i=0; i<3; ++i) { position[i] /= parent.scale[0]; scale[i] /= parent.scale[0]; }
  return checked({position, product(inverse, unit(value.rotation_xyzw)), scale}, path, index);
}
void subtree(world::Snapshot& state, world::Slot& root, std::uint64_t revision,
             const std::string& path, std::size_t index) {
  std::vector<world::Slot*> queue{&root};
  for (std::size_t at=0; at<queue.size(); ++at) {
    auto& parent = *queue[at];
    parent.entity->authoring_revision = revision;
    for (auto& child : state.slots) {
      if (!child_of(child, parent)) continue;
      if (queue.size() >= state.slots.size())
        reject("INVALID_SCHEMA", path, "Hierarchy must be acyclic.", index);
      child.entity->transform = compose(parent.entity->transform, child.entity->local_transform, path, index);
      queue.push_back(&child);
    }
  }
}
void reparent(world::Snapshot& state, world::Slot& slot, world::Slot* parent,
              const std::string& mode, const std::string& path, std::size_t index) {
  auto* ancestor = parent;
  for (std::size_t count=0; ancestor; ++count) {
    if (ancestor == &slot || count >= state.slots.size())
      reject("INVALID_SCHEMA", path + "/parent", "Reparenting must not create a cycle.", index);
    ancestor = parent_of(state, *ancestor);
  }
  auto& entity = *slot.entity;
  const auto local = entity.parent ? entity.local_transform : entity.transform;
  if (parent) {
    parent_scale(parent->entity->transform, path + "/parent", index);
    if (mode == "preserve_world")
      entity.local_transform = relative(parent->entity->transform, entity.transform, path, index);
    else {
      entity.local_transform = local;
      entity.transform = compose(parent->entity->transform, local, path, index);
    }
    entity.parent = world::ParentIdentity{parent->entity_uuid, parent->generation};
  } else {
    if (mode == "preserve_local") entity.transform = local;
    entity.parent.reset();
    entity.local_transform = {};
  }
}
}  // namespace

Receipt Coordinator::apply_at_boundary(world::World& world, const commands::Envelope& envelope, StagingGuard* guard, CommitObserver* observer) {
  return apply(world,envelope,guard,observer,std::nullopt);
}
Receipt Coordinator::replay_at_boundary(world::World& world, const commands::Envelope& envelope,
                                       std::span<const CreatedBinding> bindings) {
  return apply(world,envelope,nullptr,nullptr,bindings);
}
Receipt Coordinator::apply(world::World& target_world, const commands::Envelope& envelope, StagingGuard* guard,
                           CommitObserver* observer, std::optional<std::span<const CreatedBinding>> bindings) {
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
          if(bindings) {
            if(created.size()>=bindings->size()) reject("INVALID_REPLAY","/created","Missing recorded creation mapping.",index);
            const auto& binding=(*bindings)[created.size()];
            if(binding.temporary_id!=operation.temporary_id || binding.world_id!=staged.world_id ||
               binding.entity_uuid!=slot->entity_uuid || binding.generation!=slot->generation)
              reject("INVALID_REPLAY","/created","Recorded creation mapping differs from allocator contract.",index);
            slot->entity_uuid=binding.entity_uuid;slot->generation=binding.generation;
          }
          slot->entity = world::Entity{operation.prefab, next_revision, {}, std::nullopt, {}, {}};
          names.emplace(operation.temporary_id, commands::EntityTarget{
            staged.world_id, slot->entity_uuid, slot->generation});
          created.push_back({operation.temporary_id, staged.world_id, slot->entity_uuid, slot->generation});
        } else if constexpr (std::is_same_v<Type, commands::TransformSet>) {
          const auto replacement = transform(operation, path, index);
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          if (guard) guard->transform(static_cast<std::size_t>(&slot - staged.slots.data()),
                                      slot.entity->transform, replacement, index);
          if (has_children(staged, slot)) parent_scale(replacement, path + "/scale", index);
          if (auto* parent = parent_of(staged, slot))
            slot.entity->local_transform = relative(parent->entity->transform, replacement, path, index);
          slot.entity->transform = replacement;
          subtree(staged, slot, next_revision, path, index);
        } else if constexpr (std::is_same_v<Type, commands::EntityTagsSet>) {
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          slot.entity->tags = operation.tags;
          std::sort(slot.entity->tags.begin(), slot.entity->tags.end());
          slot.entity->authoring_revision = next_revision;
        } else if constexpr (std::is_same_v<Type, commands::EntityReparent>) {
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          auto* parent = operation.parent ? &resolve(staged, *operation.parent, names, path + "/parent", index) : nullptr;
          reparent(staged, slot, parent, operation.mode, path, index);
          subtree(staged, slot, next_revision, path, index);
        } else {
          auto& slot = resolve(staged, operation.target, names, path + "/target", index);
          if (has_children(staged, slot))
            reject("INVALID_SCHEMA", path + "/child_policy", "Detach or delete children before deleting their parent.", index);
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
    staged.format_version = std::any_of(staged.slots.begin(), staged.slots.end(),
      [](const auto& slot) { return slot.entity && slot.entity->parent; }) ? 2U : 1U;
    if (std::any_of(staged.slots.begin(), staged.slots.end(),
        [](const auto& slot) { return slot.entity && !slot.entity->tags.empty(); })) staged.format_version = 3;
    if(bindings && created.size()!=bindings->size()) reject("INVALID_REPLAY","/created","Extra recorded creation mapping.");
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
  try { if(observer) observer->prepare(staged,receipt); }
  catch(const Error& error) {
    receipt.status="rejected";receipt.world_revision=target_world.state_.world_revision;receipt.created.clear();
    receipt.errors.push_back(error);return receipt;
  }
  // All receipt allocation precedes publication. Swap and return cannot allocate.
  static_assert(std::is_nothrow_swappable_v<world::Snapshot>);
  static_assert(std::is_nothrow_move_constructible_v<Receipt>);
  using std::swap;
  swap(target_world.state_, staged);
  if (guard) guard->commit();
  if(observer) observer->publish();
  return receipt;
}
}  // namespace ow::transactions
