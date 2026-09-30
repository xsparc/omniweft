// SPDX-License-Identifier: Apache-2.0
#include "omniweft/gltf.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <limits>
#include <new>
#include <set>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ow::gltf {
namespace {
using Json = nlohmann::json;
using Bytes = assets::Bytes;
constexpr std::size_t file_cap = 65536, json_cap = 16384;
[[noreturn]] void fail(const char* code) { throw assets::Failure(code); }
class Sax final : public nlohmann::json_sax<Json> {
 public:
  const char* error = "INVALID_SCHEMA";
  bool null() override { return event(); }
  bool boolean(bool) override { return event(); }
  bool number_integer(number_integer_t) override { return event(); }
  bool number_unsigned(number_unsigned_t) override { return event(); }
  bool number_float(number_float_t value, const string_t&) override {
    return std::isfinite(value) ? event() : stop("NONFINITE_VALUE");
  }
  bool string(string_t& value) override { return value.size() <= 256 ? event() : stop("BUDGET_EXCEEDED"); }
  bool binary(binary_t&) override { return stop("INVALID_SCHEMA"); }
  bool start_object(std::size_t) override { return open(true); }
  bool start_array(std::size_t) override { return open(false); }
  bool key(string_t& value) override {
    if (!event()) return false;
    if (value.size() > 256) return stop("BUDGET_EXCEEDED");
    if (stack_.empty() || !stack_.back().object || !stack_.back().keys.insert(value).second)
      return stop("INVALID_SCHEMA");
    return true;
  }
  bool end_object() override { stack_.pop_back(); return event(); }
  bool end_array() override { stack_.pop_back(); return event(); }
  bool parse_error(std::size_t, const std::string&, const Json::exception& value) override {
    return stop(value.id == 406 ? "NONFINITE_VALUE" : "INVALID_SCHEMA");
  }
 private:
  struct Frame { bool object; std::set<std::string> keys; };
  std::vector<Frame> stack_;
  std::size_t events_ = 0;
  bool stop(const char* code) { error = code; return false; }
  bool event() { return ++events_ <= 4096 || stop("BUDGET_EXCEEDED"); }
  bool open(bool object) {
    if (!event()) return false;
    if (stack_.size() == 16) return stop("BUDGET_EXCEEDED");
    stack_.push_back({object, {}});
    return true;
  }
};
std::uint32_t le32(std::span<const std::uint8_t> bytes, std::size_t at) {
  if (at > bytes.size() || bytes.size() - at < 4) fail("INVALID_GLB");
  std::uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i);
  return value;
}
struct Container { Json json; std::span<const std::uint8_t> bin; };
Container container(const Bytes& bytes) {
  if (bytes.size() > file_cap) fail("BUDGET_EXCEEDED");
  if (bytes.size() < 12 || le32(bytes, 0) != 0x46546c67 || le32(bytes, 4) != 2
      || le32(bytes, 8) != bytes.size()) fail("INVALID_GLB");
  std::span<const std::uint8_t> json, bin;
  bool seen_json = false, seen_bin = false;
  std::size_t offset = 12, ordinal = 0;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 8) fail("INVALID_GLB");
    const auto length = le32(bytes, offset), type = le32(bytes, offset + 4);
    offset += 8;
    if (length % 4 != 0 || length > bytes.size() - offset) fail("INVALID_GLB");
    const auto data = std::span<const std::uint8_t>(bytes).subspan(offset, length);
    if (ordinal == 0 && type != 0x4e4f534a) fail("INVALID_GLB");
    if (type == 0x4e4f534a) {
      if (seen_json || ordinal != 0) fail("INVALID_GLB");
      seen_json = true; json = data;
    } else if (type == 0x004e4942) {
      if (seen_bin || ordinal != 1) fail("INVALID_GLB");
      seen_bin = true; bin = data;
    }
    offset += length;
    ++ordinal;
  }
  if (!seen_json || !seen_bin) fail("INVALID_GLB");
  if (json.size() > json_cap) fail("BUDGET_EXCEEDED");
  if (json.empty() || std::find(json.begin(), json.end(), 0) != json.end()
      || (json.size() >= 3 && json[0] == 0xef && json[1] == 0xbb && json[2] == 0xbf)) fail("INVALID_SCHEMA");
  Sax sax;
  if (!Json::sax_parse(json.begin(), json.end(), &sax)) fail(sax.error);
  return {Json::parse(json.begin(), json.end()), bin};
}
bool listed(std::string_view value, std::initializer_list<std::string_view> values) {
  return std::find(values.begin(), values.end(), value) != values.end();
}
void fields(const Json& value, std::initializer_list<std::string_view> allowed,
            std::initializer_list<std::string_view> required = {}) {
  if (!value.is_object()) fail("INVALID_SCHEMA");
  for (auto it = value.begin(); it != value.end(); ++it) {
    if (it.key() == "extras") continue; // Bounded metadata; never interpreted as authority.
    if (it.key() == "extensions") {
      if (!it.value().is_object()) fail("INVALID_SCHEMA");
      if (!it.value().empty()) fail("UNSUPPORTED_PROFILE");
      continue;
    }
    if (!listed(it.key(), allowed)) fail("UNSUPPORTED_PROFILE");
  }
  for (const auto name : required) if (!value.contains(name)) fail("INVALID_SCHEMA");
}
std::string text(const Json& value) {
  if (!value.is_string()) fail("INVALID_SCHEMA");
  const auto& result = value.get_ref<const std::string&>();
  if (result.size() > 256) fail("BUDGET_EXCEEDED");
  return result;
}
void name(const Json& value) { if (value.contains("name")) (void)text(value["name"]); }
std::uint32_t integer(const Json& value) {
  if (!value.is_number()) fail("INVALID_SCHEMA");
  if (value.is_number_unsigned()) {
    const auto number = value.get<std::uint64_t>();
    if (number > std::numeric_limits<std::uint32_t>::max()) fail("INVALID_SCHEMA");
    return static_cast<std::uint32_t>(number);
  }
  const auto number = value.get<double>();
  if (!std::isfinite(number)) fail("NONFINITE_VALUE");
  if (number < 0 || number > std::numeric_limits<std::uint32_t>::max() || std::floor(number) != number)
    fail("INVALID_SCHEMA");
  return static_cast<std::uint32_t>(number);
}
std::uint32_t optional_integer(const Json& value, const char* key, std::uint32_t fallback = 0) {
  return value.contains(key) ? integer(value[key]) : fallback;
}
std::uint32_t index(const Json& value, std::size_t count) {
  const auto result = integer(value);
  if (result >= count) fail("INVALID_SCHEMA");
  return result;
}
const Json& array(const Json& value, std::size_t maximum, bool nonempty = true) {
  if (!value.is_array() || (nonempty && value.empty())) fail("INVALID_SCHEMA");
  if (value.size() > maximum) fail("BUDGET_EXCEEDED");
  return value;
}
double number(const Json& value) {
  if (!value.is_number()) fail("INVALID_SCHEMA");
  const auto result = value.get<double>();
  if (!std::isfinite(result)) fail("NONFINITE_VALUE");
  return result == 0 ? 0 : result;
}
template<std::size_t N>
std::array<double, N> vector(const Json& value) {
  if (!value.is_array() || value.size() != N) fail("INVALID_SCHEMA");
  std::array<double, N> out{};
  for (std::size_t i = 0; i < N; ++i) out[i] = number(value[i]);
  return out;
}
Vec3 position_extrema(const Json& value) {
  auto out = vector<3>(value);
  for (auto& component : out) {
    if (std::abs(component) > static_cast<double>(std::numeric_limits<float>::max())) fail("NONFINITE_VALUE");
    component = static_cast<double>(static_cast<float>(component));
    if (!std::isfinite(component)) fail("NONFINITE_VALUE");
  }
  return out;
}
void add(Bounds& bounds, const Vec3& position) {
  for (std::size_t i = 0; i < 3; ++i) {
    bounds.minimum[i] = std::min(bounds.minimum[i], position[i]);
    bounds.maximum[i] = std::max(bounds.maximum[i], position[i]);
  }
}
void merge(std::optional<Bounds>& bounds, const Bounds& value) {
  if (!bounds) bounds = value;
  else { add(*bounds, value.minimum); add(*bounds, value.maximum); }
}
struct View { std::uint32_t offset, length, stride, target; std::size_t positions = 0, indices = 0; };
struct Accessor {
  std::vector<Vec3> positions;
  std::vector<std::uint32_t> indices;
  Bounds bounds;
};
std::vector<View> views(const Json& input, std::size_t length) {
  std::vector<View> out;
  for (const auto& value : array(input, 8)) {
    fields(value, {"buffer", "byteOffset", "byteLength", "byteStride", "target", "name"}, {"buffer", "byteLength"});
    name(value);
    if (integer(value["buffer"]) != 0) fail("INVALID_SCHEMA");
    View view{optional_integer(value, "byteOffset"), integer(value["byteLength"]),
              optional_integer(value, "byteStride"), optional_integer(value, "target")};
    if (view.length == 0 || view.offset > length || view.length > length - view.offset) fail("INVALID_SCHEMA");
    if (value.contains("byteStride") && (view.stride < 4 || view.stride > 252 || view.stride % 4 != 0))
      fail("INVALID_SCHEMA");
    if (value.contains("target") && view.target != 34962 && view.target != 34963) fail("INVALID_SCHEMA");
    if (view.target == 34963 && value.contains("byteStride")) fail("INVALID_SCHEMA");
    out.push_back(view);
  }
  return out;
}
std::vector<Accessor> accessors(const Json& input, std::vector<View>& buffer_views,
                              std::span<const std::uint8_t> bin) {
  std::vector<Accessor> out;
  std::size_t vertex_total = 0, index_total = 0;
  for (const auto& value : array(input, 8)) {
    fields(value, {"bufferView", "byteOffset", "componentType", "normalized", "count", "type", "min", "max", "name"},
           {"bufferView", "componentType", "count", "type"});
    name(value);
    if (value.contains("normalized")) {
      if (!value["normalized"].is_boolean()) fail("INVALID_SCHEMA");
      if (value["normalized"].get<bool>()) fail("UNSUPPORTED_PROFILE");
    }
    auto& view = buffer_views[index(value["bufferView"], buffer_views.size())];
    const auto component = integer(value["componentType"]), count = integer(value["count"]);
    const auto type = text(value["type"]);
    const bool position = type == "VEC3" && component == 5126;
    if (!position && !(type == "SCALAR" && (component == 5121 || component == 5123 || component == 5125)))
      fail("UNSUPPORTED_PROFILE");
    if (count == 0) fail("INVALID_SCHEMA");
    if (count > (position ? 256U : 768U)) fail("BUDGET_EXCEEDED");
    auto& total = position ? vertex_total : index_total;
    if (count > (position ? 1024U : 3072U) - total) fail("BUDGET_EXCEEDED");
    total += count;
    const std::uint32_t component_size = component == 5121 ? 1U : component == 5123 ? 2U : 4U;
    const auto element_size = position ? 12U : component_size;
    const auto offset = optional_integer(value, "byteOffset"), stride = view.stride == 0 ? element_size : view.stride;
    if (offset % component_size != 0 || (static_cast<std::uint64_t>(view.offset) + offset) % component_size != 0
        || stride < element_size || stride % component_size != 0 || offset > view.length
        || element_size > view.length - offset || count - 1 > (view.length - offset - element_size) / stride)
      fail("INVALID_SCHEMA");
    if (position) {
      if (view.target == 34963 || offset % 4 != 0 || stride % 4 != 0) fail("INVALID_SCHEMA");
      ++view.positions;
    } else {
      if (view.target == 34962 || view.stride != 0) fail("INVALID_SCHEMA");
      ++view.indices;
    }
    if (view.positions != 0 && view.indices != 0) fail("INVALID_SCHEMA");
    if (view.positions > 1 && view.stride == 0) fail("INVALID_SCHEMA");
    Accessor decoded;
    if (position) decoded.positions.reserve(count); else decoded.indices.reserve(count);
    for (std::uint32_t n = 0; n < count; ++n) {
      const auto at = static_cast<std::size_t>(view.offset) + offset + static_cast<std::size_t>(n) * stride;
      if (position) {
        Vec3 p{};
        for (std::size_t j = 0; j < 3; ++j) {
          p[j] = static_cast<double>(std::bit_cast<float>(le32(bin, at + j * 4)));
          if (!std::isfinite(p[j])) fail("NONFINITE_VALUE");
          if (std::abs(p[j]) > 1e6) fail("INVALID_SCHEMA");
          if (p[j] == 0) p[j] = 0;
        }
        if (decoded.positions.empty()) decoded.bounds = {p, p}; else add(decoded.bounds, p);
        decoded.positions.push_back(p);
      } else {
        std::uint32_t vertex = 0;
        for (std::uint32_t j = 0; j < component_size; ++j)
          vertex |= static_cast<std::uint32_t>(bin[at + j]) << (8 * j);
        const auto sentinel = component == 5121 ? 255U : component == 5123 ? 65535U : 4294967295U;
        if (vertex == sentinel) fail("INVALID_INDEX");
        decoded.indices.push_back(vertex);
      }
    }
    if (position) {
      if (!value.contains("min") || !value.contains("max") || position_extrema(value["min"]) != decoded.bounds.minimum
          || position_extrema(value["max"]) != decoded.bounds.maximum) fail("INVALID_SCHEMA");
    } else {
      if (value.contains("min") && vector<1>(value["min"])[0] != *std::min_element(decoded.indices.begin(), decoded.indices.end()))
        fail("INVALID_SCHEMA");
      if (value.contains("max") && vector<1>(value["max"])[0] != *std::max_element(decoded.indices.begin(), decoded.indices.end()))
        fail("INVALID_SCHEMA");
    }
    out.push_back(std::move(decoded));
  }
  return out;
}
std::vector<Material> materials(const Json& input) {
  std::vector<Material> out;
  for (const auto& value : array(input, 4)) {
    fields(value, {"name", "pbrMetallicRoughness", "doubleSided", "alphaMode"});
    Material material;
    if (value.contains("name")) material.name = text(value["name"]);
    if (value.contains("alphaMode") && text(value["alphaMode"]) != "OPAQUE") fail("UNSUPPORTED_PROFILE");
    if (value.contains("doubleSided")) {
      if (!value["doubleSided"].is_boolean()) fail("INVALID_SCHEMA");
      material.double_sided = value["doubleSided"].get<bool>();
    }
    if (value.contains("pbrMetallicRoughness")) {
      const auto& pbr = value["pbrMetallicRoughness"];
      fields(pbr, {"baseColorFactor", "metallicFactor", "roughnessFactor"});
      if (pbr.contains("baseColorFactor")) material.base_color_factor = vector<4>(pbr["baseColorFactor"]);
      if (pbr.contains("metallicFactor")) material.metallic_factor = number(pbr["metallicFactor"]);
      if (pbr.contains("roughnessFactor")) material.roughness_factor = number(pbr["roughnessFactor"]);
    }
    for (const auto channel : material.base_color_factor) if (channel < 0 || channel > 1) fail("INVALID_SCHEMA");
    if (material.metallic_factor < 0 || material.metallic_factor > 1
        || material.roughness_factor < 0 || material.roughness_factor > 1) fail("INVALID_SCHEMA");
    out.push_back(std::move(material));
  }
  return out;
}
std::vector<Mesh> meshes(const Json& input, const std::vector<Accessor>& decoded, std::size_t material_count) {
  std::vector<Mesh> out;
  std::size_t primitives = 0, vertices = 0, indices = 0;
  for (const auto& value : array(input, 4)) {
    fields(value, {"primitives", "name"}, {"primitives"}); name(value);
    Mesh mesh;
    for (const auto& primitive : array(value["primitives"], 4)) {
      if (++primitives > 4) fail("BUDGET_EXCEEDED");
      fields(primitive, {"attributes", "indices", "material", "mode"}, {"attributes", "indices"});
      fields(primitive["attributes"], {"POSITION"}, {"POSITION"});
      if (primitive["attributes"].size() != 1) fail("UNSUPPORTED_PROFILE");
      if (optional_integer(primitive, "mode", 4) != 4) fail("UNSUPPORTED_PROFILE");
      const auto& p = decoded[index(primitive["attributes"]["POSITION"], decoded.size())];
      const auto& i = decoded[index(primitive["indices"], decoded.size())];
      if (p.positions.empty() || i.indices.empty() || i.indices.size() % 3 != 0) fail("INVALID_SCHEMA");
      for (const auto vertex : i.indices) if (vertex >= p.positions.size()) fail("INVALID_INDEX");
      if (p.positions.size() > 1024 - vertices || i.indices.size() > 3072 - indices) fail("BUDGET_EXCEEDED");
      vertices += p.positions.size(); indices += i.indices.size();
      Primitive result{p.positions, i.indices, {}, p.bounds};
      if (primitive.contains("material")) result.material = index(primitive["material"], material_count);
      mesh.primitives.push_back(std::move(result));
    }
    out.push_back(std::move(mesh));
  }
  return out;
}
Mat4 local(Node& node) {
  for (const auto value : node.translation) if (std::abs(value) > 1e6) fail("INVALID_TRANSFORM");
  for (const auto value : node.scale) if (std::abs(value) > 1e3) fail("INVALID_TRANSFORM");
  double norm = 0;
  for (const auto value : node.rotation) norm += value * value;
  if (!std::isfinite(norm) || std::abs(norm - 1) > 1e-6) fail("INVALID_TRANSFORM");
  const auto divisor = std::sqrt(norm);
  for (auto& value : node.rotation) value /= divisor;
  const auto x = node.rotation[0], y = node.rotation[1], z = node.rotation[2], w = node.rotation[3];
  return {(1-2*(y*y+z*z))*node.scale[0], (2*(x*y+z*w))*node.scale[0], (2*(x*z-y*w))*node.scale[0], 0,
          (2*(x*y-z*w))*node.scale[1], (1-2*(x*x+z*z))*node.scale[1], (2*(y*z+x*w))*node.scale[1], 0,
          (2*(x*z+y*w))*node.scale[2], (2*(y*z-x*w))*node.scale[2], (1-2*(x*x+y*y))*node.scale[2], 0,
          node.translation[0], node.translation[1], node.translation[2], 1};
}
Mat4 multiply(const Mat4& a, const Mat4& b) {
  Mat4 out{};
  for (std::size_t col = 0; col < 4; ++col) for (std::size_t row = 0; row < 4; ++row) {
    for (std::size_t k = 0; k < 4; ++k) out[col * 4 + row] += a[k * 4 + row] * b[col * 4 + k];
    if (!std::isfinite(out[col * 4 + row])) fail("NONFINITE_VALUE");
  }
  return out;
}
Vec3 transform(const Mat4& matrix, const Vec3& value) {
  Vec3 out{};
  for (std::size_t row = 0; row < 3; ++row) {
    out[row] = matrix[12 + row];
    for (std::size_t k = 0; k < 3; ++k) out[row] += matrix[k * 4 + row] * value[k];
    if (!std::isfinite(out[row])) fail("NONFINITE_VALUE");
    if (std::abs(out[row]) > 1e12) fail("INVALID_TRANSFORM");
  }
  return out;
}
void nodes(Scene& scene, const Json& input, const Json& roots) {
  const auto& values = array(input, 8);
  std::vector<int> parents(values.size(), -1);
  for (std::size_t n = 0; n < values.size(); ++n) {
    const auto& value = values[n];
    fields(value, {"mesh", "children", "translation", "rotation", "scale", "name"}); name(value);
    Node node;
    if (value.contains("mesh")) node.mesh = index(value["mesh"], scene.meshes.size());
    if (value.contains("translation")) node.translation = vector<3>(value["translation"]);
    if (value.contains("rotation")) node.rotation = vector<4>(value["rotation"]);
    if (value.contains("scale")) node.scale = vector<3>(value["scale"]);
    node.local_matrix = local(node);
    if (value.contains("children")) for (const auto& child : array(value["children"], 8)) {
      const auto id = index(child, values.size());
      if (parents[id] != -1) fail("INVALID_SCHEMA");
      parents[id] = static_cast<int>(n);
      node.children.push_back(id);
    }
    scene.nodes.push_back(std::move(node));
  }
  std::vector<unsigned> state(values.size(), 0);
  const std::function<void(std::size_t)> visit = [&](std::size_t id) {
    if (state[id] == 1) fail("INVALID_SCHEMA");
    if (state[id] == 2) return;
    state[id] = 1;
    auto& node = scene.nodes[id];
    if (parents[id] >= 0) {
      const auto parent = static_cast<std::size_t>(parents[id]);
      visit(parent);
      node.world_matrix = multiply(scene.nodes[parent].world_matrix, node.local_matrix);
    } else node.world_matrix = node.local_matrix;
    (void)transform(node.world_matrix, {0,0,0});
    if (node.mesh) for (const auto& primitive : scene.meshes[*node.mesh].primitives) {
      for (const auto& vertex : primitive.positions) {
        const auto p = transform(node.world_matrix, vertex);
        if (!node.world_bounds) node.world_bounds = Bounds{p, p}; else add(*node.world_bounds, p);
      }
    }
    state[id] = 2;
  };
  for (std::size_t i = 0; i < scene.nodes.size(); ++i) visit(i);
  std::set<std::uint32_t> selected;
  for (const auto& root : array(roots, 8)) {
    const auto id = index(root, scene.nodes.size());
    if (parents[id] != -1 || !selected.insert(id).second) fail("INVALID_SCHEMA");
    scene.roots.push_back(id);
  }
  std::optional<Bounds> aggregate;
  const std::function<void(std::uint32_t)> aggregate_node = [&](std::uint32_t id) {
    const auto& node = scene.nodes[id];
    if (node.world_bounds) merge(aggregate, *node.world_bounds);
    for (const auto child : node.children) aggregate_node(child);
  };
  for (const auto root : scene.roots) aggregate_node(root);
  if (!aggregate) fail("UNSUPPORTED_PROFILE");
  scene.world_bounds = *aggregate;
}
Scene decode(const Bytes& bytes) {
  auto data = container(bytes);
  const auto& value = data.json;
  if (value.is_object() && value.contains("extensionsRequired")) {
    const auto& required = array(value["extensionsRequired"], 4096, false);
    for (const auto& entry : required) (void)text(entry);
    if (!required.empty()) fail("UNSUPPORTED_REQUIRED_EXTENSION");
  }
  fields(value, {"asset", "buffers", "bufferViews", "accessors", "meshes", "nodes", "materials", "scenes", "scene",
                 "extensionsUsed", "extensionsRequired"}, {"asset", "buffers", "bufferViews", "accessors", "meshes", "nodes", "scenes"});
  for (const auto key : {"extensionsUsed", "extensionsRequired"}) if (value.contains(key)) {
    const auto& list = array(value[key], 4096, false);
    std::set<std::string> unique;
    for (const auto& entry : list) if (!unique.insert(text(entry)).second) fail("INVALID_SCHEMA");
    if (std::string_view(key) == "extensionsRequired" && !list.empty()) fail("UNSUPPORTED_REQUIRED_EXTENSION");
  }
  fields(value["asset"], {"version", "minVersion", "generator", "copyright"}, {"version"});
  if (text(value["asset"]["version"]) != "2.0") fail("UNSUPPORTED_PROFILE");
  for (const auto key : {"generator", "copyright"}) if (value["asset"].contains(key)) (void)text(value["asset"][key]);
  if (value["asset"].contains("minVersion") && text(value["asset"]["minVersion"]) != "2.0") fail("UNSUPPORTED_PROFILE");
  const auto& buffers = array(value["buffers"], 1);
  fields(buffers[0], {"byteLength", "name"}, {"byteLength"}); name(buffers[0]);
  const auto length = integer(buffers[0]["byteLength"]);
  if (length == 0 || length > data.bin.size() || data.bin.size() - length > 3) fail("INVALID_GLB");
  for (const auto pad : data.bin.subspan(length)) if (pad != 0) fail("INVALID_GLB");
  auto buffer_views = views(value["bufferViews"], length);
  const auto decoded = accessors(value["accessors"], buffer_views, data.bin.first(length));
  Scene scene;
  if (value.contains("materials")) scene.materials = materials(value["materials"]);
  scene.meshes = meshes(value["meshes"], decoded, scene.materials.size());
  const auto& scenes = array(value["scenes"], 1);
  fields(scenes[0], {"nodes", "name"}, {"nodes"}); name(scenes[0]);
  if (value.contains("scene")) (void)index(value["scene"], 1);
  nodes(scene, value["nodes"], scenes[0]["nodes"]);
  return scene;
}
void put32(Bytes& bytes, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void digest(Bytes& bytes, const std::string& value) {
  const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (std::size_t i = 0; i < value.size(); i += 2)
    bytes.push_back(static_cast<std::uint8_t>((nibble(value[i]) << 4) | nibble(value[i + 1])));
}
Bytes bundle(const assets::Asset& asset, const Bytes& glb) {
  const auto manifest = assets::canonical_manifest(asset.manifest);
  Bytes out{'O','W','A','S','B','0','0','1'};
  put32(out, 1); put32(out, 1); put32(out, 1);
  digest(out, asset.id); put32(out, static_cast<std::uint32_t>(manifest.size()));
  out.insert(out.end(), manifest.begin(), manifest.end());
  const auto path = "blobs/" + asset.manifest.content_hash;
  out.push_back(70); out.push_back(0); out.insert(out.end(), path.begin(), path.end()); out.push_back(0);
  put32(out, static_cast<std::uint32_t>(glb.size())); put32(out, static_cast<std::uint32_t>(glb.size()));
  out.insert(out.end(), glb.begin(), glb.end());
  return out;
}
} // namespace

ImportResult import_glb(assets::Catalog& catalog, const ImportRequest& request) {
  ImportResult result;
  result.receipt.revision = catalog.revision();
  if (request.expected_revision != catalog.revision()) {
    result.receipt.code = "REVISION_CONFLICT";
    return result;
  }
  try {
    const auto provenance = [](std::string_view value, std::size_t limit) {
      if (value.empty() || value.size() > limit || !std::all_of(value.begin(), value.end(),
          [](char c) { return c >= 0x20 && c <= 0x7e; })) fail("INVALID_MANIFEST");
    };
    provenance(request.source, 128);
    provenance(request.license, 64);
    auto scene = decode(request.glb);
    scene.source_asset.manifest = {1, assets::content_hash(request.glb), static_cast<std::uint32_t>(request.glb.size()),
                                    "model/gltf-binary", request.source, request.license};
    scene.source_asset.id = assets::asset_id(scene.source_asset.manifest);
    assets::Command command = assets::Import{bundle(scene.source_asset, request.glb)};
    result.scene.emplace(std::move(scene));
    // All scene/manifest/bundle allocations precede the sole ordinary Catalog commit.
    static_assert(std::is_nothrow_move_assignable_v<assets::Receipt>);
    static_assert(std::is_nothrow_move_constructible_v<ImportResult>);
    result.receipt = catalog.apply(command, request.expected_revision);
    if (result.receipt.status != "committed") result.scene.reset();
    return result;
  } catch (const assets::Failure& error) {
    result.scene.reset(); result.receipt.code = error.code();
  } catch (const Json::exception&) {
    result.scene.reset(); result.receipt.code = "INVALID_SCHEMA";
  } catch (const std::bad_alloc&) {
    result.scene.reset(); result.receipt.code = "BUDGET_EXCEEDED";
  }
  return result;
}
} // namespace ow::gltf
