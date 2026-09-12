// SPDX-License-Identifier: Apache-2.0
#include "omniweft/commands.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace ow::commands {
namespace {
using Json = nlohmann::json;
[[noreturn]] void reject(std::string code, std::string path, std::string message) {
  throw ValidationError{std::move(code), std::move(path), std::move(message)};
}
std::string child(const std::string& path, std::string_view key) {
  std::string result = path + "/";
  for (const char character : key) {
    if (character == '~') result += "~0";
    else if (character == '/') result += "~1";
    else result += character;
  }
  return result;
}
class BoundedSax final : public nlohmann::json_sax<Json> {
 public:
  std::optional<ValidationError> error;
  bool null() override { return true; }
  bool boolean(bool) override { return true; }
  bool number_integer(number_integer_t) override { return true; }
  bool number_unsigned(number_unsigned_t) override { return true; }
  bool number_float(number_float_t value, const string_t&) override {
    return std::isfinite(value) || fail("NONFINITE_VALUE", "Numbers must be finite.");
  }
  bool string(string_t&) override { return true; }
  bool binary(binary_t&) override { return fail("INVALID_SCHEMA", "Binary values are not JSON."); }
  bool start_object(std::size_t) override { return open(true); }
  bool start_array(std::size_t) override { return open(false); }
  bool key(string_t& value) override {
    if (frames_.empty() || !frames_.back().object)
      return fail("INVALID_SCHEMA", "Object key outside an object.");
    return frames_.back().keys.insert(value).second ||
           fail("INVALID_SCHEMA", "Decoded object keys must be unique.");
  }
  bool end_object() override { frames_.pop_back(); return true; }
  bool end_array() override { frames_.pop_back(); return true; }
  bool parse_error(std::size_t, const std::string&, const Json::exception& exception) override {
    return fail(exception.id == 406 ? "NONFINITE_VALUE" : "INVALID_SCHEMA",
                exception.id == 406 ? "Numbers must fit a finite double." : "Input must be one strict UTF-8 JSON value.");
  }
 private:
  struct Frame { bool object; std::unordered_set<std::string> keys; };
  std::vector<Frame> frames_;
  bool fail(const char* code, const char* message) {
    error = ValidationError{code, "", message};
    return false;
  }
  bool open(bool object) {
    if (frames_.size() >= max_nesting_depth)
      return fail("BUDGET_EXCEEDED", "JSON container nesting exceeds depth 32.");
    frames_.push_back(Frame{object, {}});
    return true;
  }
};
void fields(const Json& object, const std::string& path,
            std::initializer_list<std::string_view> names) {
  if (!object.is_object()) reject("INVALID_SCHEMA", path, "Expected an object.");
  for (auto item = object.begin(); item != object.end(); ++item) {
    if (std::find(names.begin(), names.end(), std::string_view(item.key())) == names.end())
      reject("INVALID_SCHEMA", child(path, item.key()), "Unknown field.");
  }
  for (const auto name : names) {
    if (!object.contains(std::string(name)))
      reject("INVALID_SCHEMA", child(path, name), "Required field is missing.");
  }
}
std::uint64_t unsigned_integer(const Json& value, const std::string& path) {
  if (!value.is_number_unsigned())
    reject("INVALID_SCHEMA", path, "Expected an unsigned 64-bit integer token, without fraction or exponent.");
  return value.get<std::uint64_t>();
}
template<std::size_t Size>
std::array<double, Size> vector_value(const Json& value, const std::string& path) {
  if (!value.is_array() || value.size() != Size)
    reject("INVALID_SCHEMA", path, "Numeric vector has the wrong shape.");
  std::array<double, Size> result{};
  for (std::size_t index = 0; index < Size; ++index) {
    const auto item_path = child(path, std::to_string(index));
    if (!value[index].is_number()) reject("INVALID_SCHEMA", item_path, "Expected a number.");
    result[index] = value[index].get<double>();
    if (!std::isfinite(result[index]))
      reject("NONFINITE_VALUE", item_path, "Numbers must be finite.");
  }
  return result;
}
template<std::size_t Size>
void finite_vector(const std::array<double, Size>& value, const std::string& path) {
  for (std::size_t index = 0; index < Size; ++index) {
    if (!std::isfinite(value[index]))
      reject("NONFINITE_VALUE", child(path, std::to_string(index)), "Numbers must be finite.");
  }
}

bool ascii_alphanumeric(char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
         (value >= '0' && value <= '9');
}
std::string identifier(const Json& value, const std::string& path, std::size_t maximum = 128) {
  if (!value.is_string()) reject("INVALID_SCHEMA", path, "Expected an ASCII identifier string.");
  const auto& text = value.get_ref<const std::string&>();
  if (text.empty() || text.size() > maximum || !ascii_alphanumeric(text[0]))
    reject("INVALID_SCHEMA", path, "Identifier length or first character is invalid.");
  for (const char character : text) {
    if (!ascii_alphanumeric(character) && character != '.' && character != '_' &&
        character != ':' && character != '-')
      reject("INVALID_SCHEMA", path, "Identifier contains an unsupported character.");
  }
  return text;
}
std::string uuid(const Json& value, const std::string& path) {
  if (!value.is_string()) reject("INVALID_SCHEMA", path, "Expected a lowercase hyphenated UUID string.");
  const auto& text = value.get_ref<const std::string&>();
  if (text.size() != 36) reject("INVALID_SCHEMA", path, "UUID must have 36 characters.");
  for (std::size_t index = 0; index < text.size(); ++index) {
    const bool separator = index == 8 || index == 13 || index == 18 || index == 23;
    const char character = text[index];
    if (separator ? character != '-' :
        !((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
      reject("INVALID_SCHEMA", path, "UUID must use lowercase hexadecimal and canonical hyphens.");
  }
  return text;
}
Target target_value(const Json& target, const std::string& path) {
  if (target.is_object() && target.contains("temporary_id")) {
    fields(target, path, {"temporary_id"});
    return TemporaryTarget{identifier(target["temporary_id"], child(path, "temporary_id"), 64)};
  }
  fields(target, path, {"world_id", "entity_uuid", "generation"});
  return EntityTarget{
    identifier(target["world_id"], child(path, "world_id")),
    uuid(target["entity_uuid"], child(path, "entity_uuid")),
    unsigned_integer(target["generation"], child(path, "generation"))};
}
Json target_json(const Target& target) {
  return std::visit([](const auto& reference) -> Json {
    using Reference = std::decay_t<decltype(reference)>;
    if constexpr (std::is_same_v<Reference, TemporaryTarget>)
      return {{"temporary_id", reference.temporary_id}};
    else
      return {{"world_id", reference.world_id}, {"entity_uuid", reference.entity_uuid}, {"generation", reference.generation}};
  }, target);
}
Envelope convert(const Json& value) {
  fields(value, "", {"protocol_version", "world_id", "transaction_id", "idempotency",
                     "expected_world_revision", "apply_at", "budget", "operations"});
  if (!value["protocol_version"].is_string())
    reject("INVALID_SCHEMA", "/protocol_version", "Protocol version must be a string.");
  if (value["protocol_version"] != "0.1")
    reject("UNSUPPORTED_VERSION", "/protocol_version", "Only protocol version 0.1 is supported.");
  Envelope envelope;
  envelope.protocol_version = value["protocol_version"].get<std::string>();
  envelope.world_id = identifier(value["world_id"], "/world_id");
  envelope.transaction_id = uuid(value["transaction_id"], "/transaction_id");
  fields(value["idempotency"], "/idempotency", {"epoch", "sequence"});
  envelope.idempotency.epoch = identifier(value["idempotency"]["epoch"], "/idempotency/epoch");
  envelope.idempotency.sequence = unsigned_integer(value["idempotency"]["sequence"], "/idempotency/sequence");
  envelope.expected_world_revision = unsigned_integer(value["expected_world_revision"], "/expected_world_revision");
  fields(value["apply_at"], "/apply_at", {"mode", "expires_after_ticks"});
  if (!value["apply_at"]["mode"].is_string() || value["apply_at"]["mode"] != "next_tick")
    reject("INVALID_SCHEMA", "/apply_at/mode", "Only next_tick scheduling syntax is supported.");
  envelope.apply_at.expires_after_ticks = unsigned_integer(value["apply_at"]["expires_after_ticks"], "/apply_at/expires_after_ticks");
  if (envelope.apply_at.expires_after_ticks == 0 || envelope.apply_at.expires_after_ticks > 4294967295ULL)
    reject("INVALID_SCHEMA", "/apply_at/expires_after_ticks", "Expiry must be an integer from 1 through 4294967295.");
  fields(value["budget"], "/budget", {"max_operations", "max_blob_bytes"});
  envelope.budget.max_operations = unsigned_integer(value["budget"]["max_operations"], "/budget/max_operations");
  if (envelope.budget.max_operations == 0 || envelope.budget.max_operations > max_operations)
    reject("BUDGET_EXCEEDED", "/budget/max_operations", "Operation budget must be from 1 through 256.");
  envelope.budget.max_blob_bytes = unsigned_integer(value["budget"]["max_blob_bytes"], "/budget/max_blob_bytes");
  const auto& operations = value["operations"];
  if (!operations.is_array()) reject("INVALID_SCHEMA", "/operations", "Expected an operation array.");
  if (operations.empty() || operations.size() > max_operations || operations.size() > envelope.budget.max_operations)
    reject("BUDGET_EXCEEDED", "/operations", "Require 1 through 256 operations within the declared operation budget.");
  envelope.operations.reserve(operations.size());
  for (std::size_t index = 0; index < operations.size(); ++index) {
    const auto path = "/operations/" + std::to_string(index);
    const auto& operation = operations[index];
    if (!operation.is_object()) reject("INVALID_SCHEMA", path, "Expected an operation object.");
    if (!operation.contains("type") || !operation["type"].is_string())
      reject("INVALID_SCHEMA", child(path, "type"), "Operation type must be a string.");
    const auto& type = operation["type"].get_ref<const std::string&>();
    if (type == "entity.create") {
      fields(operation, path, {"type", "temporary_id", "prefab"});
      envelope.operations.emplace_back(EntityCreate{
        identifier(operation["temporary_id"], child(path, "temporary_id"), 64),
        identifier(operation["prefab"], child(path, "prefab"))});
    } else if (type == "transform.set") {
      fields(operation, path, {"type", "target", "position_m", "rotation_xyzw", "scale"});
      TransformSet transform;
      transform.target = target_value(operation["target"], child(path, "target"));
      transform.position_m = vector_value<3>(operation["position_m"], child(path, "position_m"));
      transform.rotation_xyzw = vector_value<4>(operation["rotation_xyzw"], child(path, "rotation_xyzw"));
      transform.scale = vector_value<3>(operation["scale"], child(path, "scale"));
      envelope.operations.emplace_back(std::move(transform));
    } else if (type == "entity.delete") {
      fields(operation, path, {"type", "target", "child_policy"});
      if (!operation["child_policy"].is_string() || operation["child_policy"] != "reject_if_children")
        reject("INVALID_SCHEMA", child(path, "child_policy"), "Only reject_if_children deletion is supported.");
      envelope.operations.emplace_back(EntityDelete{
        target_value(operation["target"], child(path, "target")), "reject_if_children"});
    } else {
      reject("UNSUPPORTED_OPERATION", child(path, "type"), "Operation type is not supported by schema 0.1.");
    }
  }
  return envelope;
}

}  // namespace

ParseResult parse(std::string_view bytes) {
  if (bytes.size() > max_envelope_bytes)
    return {std::nullopt, {{"BUDGET_EXCEEDED", "", "JSON envelope exceeds 1048576 bytes."}}};
  try {
    BoundedSax sax;
    if (!Json::sax_parse(bytes.begin(), bytes.end(), &sax))
      return {std::nullopt, {sax.error.value_or(ValidationError{"INVALID_SCHEMA", "", "Input must be one strict JSON value."})}};
    // Upstream treats raw NUL as end-of-input; strict wire JSON must consume every byte.
    if (bytes.find('\0') != std::string_view::npos)
      return {std::nullopt, {{"INVALID_SCHEMA", "", "Raw NUL bytes are not valid JSON."}}};
    return {convert(Json::parse(bytes.begin(), bytes.end())), {}};
  } catch (const ValidationError& error) {
    return {std::nullopt, {error}};
  } catch (const Json::exception&) {
    return {std::nullopt, {{"INVALID_SCHEMA", "", "Input could not be decoded as the required JSON envelope."}}};
  }
}
SerializeResult serialize(const Envelope& envelope) {
  try {
    if (envelope.operations.size() > max_operations)
      reject("BUDGET_EXCEEDED", "/operations", "At most 256 operations are allowed.");
    Json value = {
      {"protocol_version", envelope.protocol_version},
      {"world_id", envelope.world_id},
      {"transaction_id", envelope.transaction_id},
      {"idempotency", {{"epoch", envelope.idempotency.epoch}, {"sequence", envelope.idempotency.sequence}}},
      {"expected_world_revision", envelope.expected_world_revision},
      {"apply_at", {{"mode", envelope.apply_at.mode}, {"expires_after_ticks", envelope.apply_at.expires_after_ticks}}},
      {"budget", {{"max_operations", envelope.budget.max_operations}, {"max_blob_bytes", envelope.budget.max_blob_bytes}}},
      {"operations", Json::array()}
    };
    for (std::size_t index = 0; index < envelope.operations.size(); ++index) {
      const auto path = "/operations/" + std::to_string(index);
      std::visit([&](const auto& operation) {
        using Type = std::decay_t<decltype(operation)>;
        if constexpr (std::is_same_v<Type, EntityCreate>) {
          value["operations"].push_back({{"type", "entity.create"}, {"temporary_id", operation.temporary_id}, {"prefab", operation.prefab}});
        } else if constexpr (std::is_same_v<Type, TransformSet>) {
          finite_vector(operation.position_m, child(path, "position_m"));
          finite_vector(operation.rotation_xyzw, child(path, "rotation_xyzw"));
          finite_vector(operation.scale, child(path, "scale"));
          const auto target = target_json(operation.target);
          value["operations"].push_back({{"type", "transform.set"}, {"target", target},
            {"position_m", operation.position_m}, {"rotation_xyzw", operation.rotation_xyzw}, {"scale", operation.scale}});
        } else {
          value["operations"].push_back({{"type", "entity.delete"}, {"target", target_json(operation.target)},
            {"child_policy", operation.child_policy}});
        }
      }, envelope.operations[index]);
    }
    // Validate typed callers before publishing bytes. Never let dump() turn NaN into null.
    static_cast<void>(convert(value));
    auto bytes = value.dump();
    const auto validated = parse(bytes);
    if (!validated.envelope) return {std::nullopt, validated.errors};
    return {std::move(bytes), {}};
  } catch (const ValidationError& error) {
    return {std::nullopt, {error}};
  } catch (const Json::exception&) {
    return {std::nullopt, {{"INVALID_SCHEMA", "", "Typed envelope could not be encoded as strict UTF-8 JSON."}}};
  }
}
}  // namespace ow::commands
