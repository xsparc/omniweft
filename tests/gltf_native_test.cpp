// SPDX-License-Identifier: Apache-2.0
#include "omniweft/gltf.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace ow::gltf;
using ow::assets::Bytes;
using Json = nlohmann::json;
std::uint32_t checks = 0;
class TestFailure : public std::runtime_error {
 public:
  explicit TestFailure(const char* label) : std::runtime_error(label) {}
};
void check(bool condition, const char* label) { ++checks; if (!condition) throw TestFailure(label); }
void u32(Bytes& bytes, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void patch32(Bytes& bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
Json doc() {
  return {{"asset", {{"version", "2.0"}}}, {"buffers", Json::array({{{"byteLength", 42}}})},
    {"bufferViews", Json::array({{{"buffer", 0}, {"byteLength", 36}, {"target", 34962}},
      {{"buffer", 0}, {"byteOffset", 36}, {"byteLength", 6}, {"target", 34963}}})},
    {"accessors", Json::array({{{"bufferView", 0}, {"componentType", 5126}, {"count", 3}, {"type", "VEC3"},
      {"min", {-1,0,0}}, {"max", {1,2,0}}}, {{"bufferView", 1}, {"componentType", 5123}, {"count", 3}, {"type", "SCALAR"}}})},
    {"meshes", Json::array({{{"primitives", Json::array({{{"attributes", {{"POSITION", 0}}}, {"indices", 1}}})}}})},
    {"nodes", Json::array({{{"translation", {4,1,0}}, {"scale", {2,1,1}}, {"children", {1}}},
      {{"mesh", 0}, {"translation", {1,0,0}}, {"rotation", {0,0,1,0}}}, {{"mesh", 0}, {"translation", {100,0,0}}}})},
    {"scenes", Json::array({{{"nodes", {0}}}})}, {"scene", 0}};
}
Bytes binary() {
  Bytes out;
  for (float value : {-1.F,0.F,0.F,1.F,0.F,0.F,0.F,2.F,0.F}) u32(out, std::bit_cast<std::uint32_t>(value));
  for (const auto value : {0,0,1,0,2,0}) out.push_back(static_cast<std::uint8_t>(value));
  return out;
}
Bytes glb_text(std::string text, Bytes bin = binary()) {
  while (text.size() % 4) text.push_back(' ');
  while (bin.size() % 4) bin.push_back(0);
  Bytes out;
  u32(out, 0x46546c67); u32(out, 2); u32(out, static_cast<std::uint32_t>(28 + text.size() + bin.size()));
  u32(out, static_cast<std::uint32_t>(text.size())); u32(out, 0x4e4f534a);
  out.insert(out.end(), text.begin(), text.end());
  u32(out, static_cast<std::uint32_t>(bin.size())); u32(out, 0x004e4942); out.insert(out.end(), bin.begin(), bin.end());
  return out;
}
Bytes glb(const Json& value, const Bytes& bin = binary()) { return glb_text(value.dump(), bin); }
ImportResult accept(ow::assets::Catalog& catalog, const Bytes& bytes, std::string source = "generated:unit/triangle") {
  const auto revision = catalog.revision();
  const ImportRequest request{bytes, std::move(source), "Apache-2.0", revision};
  const auto copy = request.glb;
  auto result = import_glb(catalog, request);
  check(result.scene.has_value() && result.receipt.status == "committed" && result.receipt.code.empty(), "valid import succeeds");
  check(result.receipt.revision == revision + 1 && catalog.revision() == revision + 1, "one ordinary Catalog revision");
  check(request.glb == copy, "accepted caller input unchanged");
  check(result.receipt.imported == std::vector<std::string>{result.scene->source_asset.id}, "receipt binds source asset");
  return result;
}
void reject(ow::assets::Catalog& catalog, const Bytes& bytes, std::string_view code,
            std::optional<std::uint64_t> revision = {}) {
  const auto before = catalog.snapshot();
  const auto exported = catalog.export_bundle();
  const ImportRequest request{bytes, "generated:unit/triangle", "Apache-2.0", revision.value_or(catalog.revision())};
  const auto original = request.glb;
  const auto result = import_glb(catalog, request);
  check(!result.scene && result.receipt.status == "rejected", "no partial scene on rejection");
  check(result.receipt.code == code, "fixed importer rejection code");
  check(result.receipt.revision == before.revision && result.receipt.imported.empty()
      && result.receipt.manifests_removed == 0 && result.receipt.blobs_removed == 0, "no partial receipt on rejection");
  check(catalog.snapshot() == before && catalog.export_bundle() == exported, "full Catalog rollback");
  check(request.glb == original, "rejected caller bytes unchanged");
}
Mat4 diagonal(double x, double y, double z, double tx, double ty, double tz) {
  return {x,0,0,0, 0,y,0,0, 0,0,z,0, tx,ty,tz,1};
}
void geometry() {
  ow::assets::Catalog catalog;
  const auto bytes = glb(doc());
  auto result = accept(catalog, bytes);
  const auto scene = *result.scene;
  check(scene.profile == 1 && scene.roots == std::vector<std::uint32_t>{0}, "profile and selected roots");
  check(scene.meshes.size() == 1 && scene.meshes[0].primitives.size() == 1, "source mesh shape");
  const auto& primitive = scene.meshes[0].primitives[0];
  check(primitive.positions == std::vector<Vec3>{{-1,0,0},{1,0,0},{0,2,0}}, "literal decoded vertex order");
  check(primitive.indices == std::vector<std::uint32_t>{0,1,2} && !primitive.material, "literal indices and absent material");
  check(primitive.bounds == Bounds{{-1,0,0},{1,2,0}}, "local vertex bounds");
  check(scene.nodes[0].local_matrix == diagonal(2,1,1,4,1,0)
      && scene.nodes[0].world_matrix == diagonal(2,1,1,4,1,0), "literal parent matrix");
  check(scene.nodes[1].local_matrix == diagonal(-1,-1,1,1,0,0)
      && scene.nodes[1].world_matrix == diagonal(-2,-1,1,6,1,0), "parent times local composition");
  check(!scene.nodes[0].world_bounds && scene.nodes[1].world_bounds == Bounds{{4,-1,0},{8,1,0}}, "bounds only own mesh");
  check(scene.nodes[2].world_matrix == diagonal(1,1,1,100,0,0)
      && scene.nodes[2].world_bounds == Bounds{{99,0,0},{101,2,0}}, "unselected forest still fully evaluated");
  check(scene.world_bounds == Bounds{{4,-1,0},{8,1,0}}, "selected-scene aggregate excludes unused root");
  check(scene.source_asset.manifest.media_type == "model/gltf-binary"
      && scene.source_asset.manifest.source == "generated:unit/triangle"
      && scene.source_asset.manifest.license == "Apache-2.0", "source provenance retained");
  check(catalog.snapshot().blobs[0].bytes == bytes, "original GLB bytes stored unchanged");
  result.scene->nodes[0].translation[0] = 999;
  check(catalog.snapshot().blobs[0].bytes == bytes, "detached returned scene");
  auto duplicate = accept(catalog, bytes);
  check(*duplicate.scene == scene && catalog.snapshot().assets.size() == 1, "exact same-source deduplication");
  auto attributed = accept(catalog, bytes, "generated:unit/alternative");
  check(attributed.scene->source_asset.id != scene.source_asset.id && catalog.snapshot().blobs.size() == 1, "distinct provenance shared blob");
  auto material = doc();
  material["materials"] = Json::array({{{"name", "unit"}, {"doubleSided", true}, {"alphaMode", "OPAQUE"},
    {"pbrMetallicRoughness", {{"baseColorFactor", {0.25,0.5,1,1}}, {"metallicFactor", 0.5}, {"roughnessFactor", 0.25}}}}, Json::object()});
  material["meshes"][0]["primitives"][0]["material"] = 0;
  ow::assets::Catalog materials;
  auto m = accept(materials, glb(material));
  check(m.scene->materials == std::vector<Material>{{"unit",{0.25,0.5,1,1},0.5,0.25,true}, Material{}}, "all explicit/default materials decoded");
  check(m.scene->meshes[0].primitives[0].material == 0U, "material binding retained");
  auto scaled = doc(); scaled["nodes"][1]["scale"] = {-1,0,2};
  ow::assets::Catalog scales;
  auto s = accept(scales, glb(scaled));
  check(s.scene->nodes[1].world_matrix == diagonal(2,0,2,6,1,0)
      && s.scene->world_bounds == Bounds{{4,1,0},{8,1,0}}, "negative and zero scales allowed");
  auto normalized = doc(); normalized["nodes"][1]["rotation"] = {0,0,0,1.0000001};
  ow::assets::Catalog rotations;
  auto q = accept(rotations, glb(normalized));
  check(q.scene->nodes[1].rotation == Vec4{0,0,0,1}, "near-unit quaternion normalized once");
  // Nonexact fixture declares a fixed absolute tolerance, independent of observed output.
  normalized["nodes"][1]["rotation"] = {0,0,0.6,0.8};
  ow::assets::Catalog angled;
  auto angle = accept(angled, glb(normalized));
  check(std::abs(angle.scene->nodes[1].local_matrix[0] - 0.28) <= 1e-12
      && std::abs(angle.scene->nodes[1].local_matrix[1] - 0.96) <= 1e-12, "fixed nonexact rotation tolerance");
  auto decimal = doc(); auto decimal_bin = binary();
  patch32(decimal_bin, 0, std::bit_cast<std::uint32_t>(0.1F));
  patch32(decimal_bin, 12, std::bit_cast<std::uint32_t>(0.2F));
  patch32(decimal_bin, 24, std::bit_cast<std::uint32_t>(0.3F));
  decimal["accessors"][0]["min"] = {0.1,0,0}; decimal["accessors"][0]["max"] = {0.3,2,0};
  ow::assets::Catalog decimals;
  auto d = accept(decimals, glb(decimal, decimal_bin));
  check(d.scene->meshes[0].primitives[0].bounds.minimum[0] == static_cast<double>(0.1F)
      && d.scene->meshes[0].primitives[0].bounds.maximum[0] == static_cast<double>(0.3F), "JSON extrema checked in float32 domain");
  decimal["accessors"][0]["max"] = {1e100,2,0};
  reject(decimals, glb(decimal, decimal_bin), "NONFINITE_VALUE");
}
void malformed_container_and_json() {
  ow::assets::Catalog catalog;
  const auto good = glb(doc()); accept(catalog, good);
  for (std::size_t cut = 0; cut < good.size(); ++cut)
    reject(catalog, Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(cut)), "INVALID_GLB");
  auto bad = good; patch32(bad, 0, 0); reject(catalog, bad, "INVALID_GLB");
  bad = good; patch32(bad, 4, 1); reject(catalog, bad, "INVALID_GLB");
  bad = good; patch32(bad, 8, 12); reject(catalog, bad, "INVALID_GLB");
  bad = good; patch32(bad, 12, 0xffffffff); reject(catalog, bad, "INVALID_GLB");
  bad = good; patch32(bad, 16, 0x004e4942); reject(catalog, bad, "INVALID_GLB");
  bad = good; bad.back() = 1; reject(catalog, bad, "INVALID_GLB");
  bad = good; u32(bad, 0); u32(bad, 0x4e4f534a); patch32(bad, 8, static_cast<std::uint32_t>(bad.size()));
  reject(catalog, bad, "INVALID_GLB");
  bad = good; u32(bad, 0); u32(bad, 0x004e4942); patch32(bad, 8, static_cast<std::uint32_t>(bad.size()));
  reject(catalog, bad, "INVALID_GLB");
  bad = good; u32(bad, 4); u32(bad, 0x12345678); u32(bad, 7); patch32(bad, 8, static_cast<std::uint32_t>(bad.size()));
  ow::assets::Catalog extra; accept(extra, bad);
  check(extra.snapshot().blobs[0].bytes == bad, "bounded unknown extra chunk retained as opaque source");
  bad = good; u32(bad, static_cast<std::uint32_t>(65536 - good.size() - 8)); u32(bad, 0x12345678);
  bad.resize(65536, 0); patch32(bad, 8, 65536);
  ow::assets::Catalog exact; accept(exact, bad);
  bad.push_back(0); reject(catalog, bad, "BUDGET_EXCEEDED");
  reject(catalog, glb_text("[]"), "INVALID_SCHEMA");
  reject(catalog, glb_text("{"), "INVALID_SCHEMA");
  auto text = doc().dump(); text.insert(1, "\"sc\\u0065ne\":0,");
  reject(catalog, glb_text(text), "INVALID_SCHEMA");
  text = doc().dump(); text += '\0'; text += "garbage";
  reject(catalog, glb_text(text), "INVALID_SCHEMA");
  text = doc().dump(); text.insert(1, "\"extras\":NaN,");
  reject(catalog, glb_text(text), "INVALID_SCHEMA");
  text = doc().dump(); text.insert(1, "\"extras\":1e309,");
  reject(catalog, glb_text(text), "NONFINITE_VALUE");
  text = doc().dump(); text.insert(1, "\"extras\":\"" + std::string(257, 'x') + "\",");
  reject(catalog, glb_text(text), "BUDGET_EXCEEDED");
  text = doc().dump(); text.insert(1, "\"" + std::string(257, 'k') + "\":0,");
  reject(catalog, glb_text(text), "BUDGET_EXCEEDED");
  text = doc().dump(); text.insert(1, "\"extras\":" + std::string(16, '[') + "0" + std::string(16, ']') + ",");
  reject(catalog, glb_text(text), "BUDGET_EXCEEDED");
  text = doc().dump(); text.insert(1, "\"extras\":" + std::string(15, '[') + "0" + std::string(15, ']') + ",");
  ow::assets::Catalog depth; accept(depth, glb_text(text));
  auto many = doc(); many["extras"] = Json::array();
  for (unsigned i = 0; i < 4096; ++i) many["extras"].push_back(0);
  reject(catalog, glb(many), "BUDGET_EXCEEDED");
  text = doc().dump(); text.resize(16384, ' ');
  ow::assets::Catalog json_exact; accept(json_exact, glb_text(text));
  text.push_back(' '); reject(catalog, glb_text(text), "BUDGET_EXCEEDED");
  text = doc().dump(); text.insert(1, "\"extras\":\""); text.insert(11, 1, static_cast<char>(0xff));
  reject(catalog, glb_text(text), "INVALID_SCHEMA");
  auto numeric = doc(); numeric["scene"] = 0.0; numeric["accessors"][0]["count"] = 3.0;
  ow::assets::Catalog integers; accept(integers, glb(numeric));
  for (const auto& number : std::vector<Json>{true, -1, 0.5, 4294967296ULL, "0"}) {
    numeric = doc(); numeric["scene"] = number; reject(catalog, glb(numeric), "INVALID_SCHEMA");
  }
}
void invalid_schema_and_geometry() {
  ow::assets::Catalog catalog; accept(catalog, glb(doc()));
  auto bad = doc();
  bad["extensionsUsed"] = {"KHR_draco_mesh_compression"}; bad["extensionsRequired"] = {"KHR_draco_mesh_compression"};
  bad["extensions"] = {{"KHR_draco_mesh_compression", Json::object()}};
  reject(catalog, glb(bad), "UNSUPPORTED_REQUIRED_EXTENSION");
  for (const auto key : {"animations", "skins", "textures", "images", "cameras"}) {
    bad = doc(); bad[key] = Json::array(); reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  }
  bad = doc(); bad["buffers"][0]["uri"] = "data:application/octet-stream;base64,AAAA";
  reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["nodes"][0]["matrix"] = Json::array(); reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["meshes"][0]["primitives"][0]["mode"] = 5; reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["meshes"][0]["primitives"][0]["attributes"]["NORMAL"] = 0;
  reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["accessors"][0]["sparse"] = Json::object(); reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["accessors"][0]["normalized"] = true; reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["accessors"][0]["componentType"] = 5122; reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["accessors"][0]["count"] = 257; reject(catalog, glb(bad), "BUDGET_EXCEEDED");
  bad = doc(); bad["accessors"][1]["count"] = 769; reject(catalog, glb(bad), "BUDGET_EXCEEDED");
  bad = doc(); bad["accessors"][0].erase("min"); reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][0]["max"] = {9,2,0}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][1]["min"] = {1}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][1]["max"] = {1}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  for (const auto field : {"bufferView", "byteOffset"}) {
    bad = doc(); bad["accessors"][0][field] = 4294967295U; reject(catalog, glb(bad), "INVALID_SCHEMA");
  }
  bad = doc(); bad["bufferViews"][0]["buffer"] = 1; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"][0]["byteOffset"] = 4294967295U; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"][0]["byteLength"] = 4294967295U; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"][1]["byteLength"] = 8; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][0]["byteOffset"] = 1; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][1]["byteOffset"] = 1; reject(catalog, glb(bad), "INVALID_SCHEMA");
  for (const auto stride : {0,2,8,13,256}) {
    bad = doc(); bad["bufferViews"][0]["byteStride"] = stride; reject(catalog, glb(bad), "INVALID_SCHEMA");
  }
  bad = doc(); bad["bufferViews"][1]["byteStride"] = 4; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"][0]["target"] = 34963; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"][1]["target"] = 34962; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"][1]["count"] = 2; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["meshes"][0]["primitives"][0]["attributes"]["POSITION"] = 1; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["meshes"][0]["primitives"][0]["indices"] = 9; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["meshes"][0]["primitives"][0]["material"] = 0; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][2]["mesh"] = 9; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][2]["children"] = {1}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][0]["children"] = {1,1}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][1]["children"] = {0}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][2]["children"] = {2}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][0]["children"] = {9}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["scenes"][0]["nodes"] = {0,0}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["scenes"][0]["nodes"] = {1}; reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["nodes"][0].erase("children"); reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  for (const auto& rotation : std::vector<Json>{{0,0,0,0},{0,0,0,2},{0,0,0,1.000001}}) {
    bad = doc(); bad["nodes"][2]["rotation"] = rotation; reject(catalog, glb(bad), "INVALID_TRANSFORM");
  }
  bad = doc(); bad["nodes"][2]["translation"] = {1000001,0,0}; reject(catalog, glb(bad), "INVALID_TRANSFORM");
  bad = doc(); bad["nodes"][2]["scale"] = {1001,1,1}; reject(catalog, glb(bad), "INVALID_TRANSFORM");
  bad = doc(); bad["nodes"] = Json::array();
  for (unsigned i = 0; i < 8; ++i) {
    Json node = {{"scale", {1000,1000,1000}}};
    if (i < 7) node["children"] = {i + 1}; else node["mesh"] = 0;
    bad["nodes"].push_back(node);
  }
  reject(catalog, glb(bad), "INVALID_TRANSFORM");
  for (const auto pattern : {0x7fc00000U,0x7f800000U,0xff800000U}) {
    auto bin = binary(); patch32(bin, 0, pattern); reject(catalog, glb(doc(), bin), "NONFINITE_VALUE");
  }
  auto bin = binary(); patch32(bin, 0, std::bit_cast<std::uint32_t>(1000001.F));
  reject(catalog, glb(doc(), bin), "INVALID_SCHEMA");
  bin = binary(); bin[36] = 3; reject(catalog, glb(doc(), bin), "INVALID_INDEX");
  bin = binary(); bin[36] = 255; bin[37] = 255; reject(catalog, glb(doc(), bin), "INVALID_INDEX");
  for (const auto field : {"skins", "weights", "camera"}) {
    bad = doc(); bad["nodes"][2][field] = 0; reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  }
  bad = doc(); bad["materials"] = Json::array({{{"normalTexture", {{"index", 0}}}}});
  reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["materials"] = Json::array({{{"pbrMetallicRoughness", {{"metallicFactor", 2}}}}});
  reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["materials"] = Json::array({{{"alphaMode", "BLEND"}}}); reject(catalog, glb(bad), "UNSUPPORTED_PROFILE");
  bad = doc(); bad["materials"] = Json::array({{{"doubleSided", 1}}}); reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["meshes"].push_back(bad["meshes"][0]); bad["meshes"][1]["primitives"][0]["indices"] = 9;
  reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", 99}, {"byteLength", 1}});
  reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["bufferViews"].push_back({{"buffer", 0}, {"byteLength", 4}, {"byteStride", 4}, {"target", 34963}});
  reject(catalog, glb(bad), "INVALID_SCHEMA");
  bad = doc(); bad["accessors"].push_back(bad["accessors"][1]); bad["accessors"][2]["count"] = 4;
  reject(catalog, glb(bad), "INVALID_SCHEMA");
  accept(catalog, glb(doc()));
}
void indices_stride_and_budgets() {
  for (const auto component : {5121U,5123U,5125U}) {
    auto value = doc(); auto bin = binary(); bin.resize(36);
    const unsigned size = component == 5121 ? 1 : component == 5123 ? 2 : 4;
    for (std::uint32_t i = 0; i < 3; ++i)
      for (unsigned j = 0; j < size; ++j) bin.push_back(static_cast<std::uint8_t>(i >> (8 * j)));
    value["buffers"][0]["byteLength"] = bin.size(); value["bufferViews"][1]["byteLength"] = 3 * size;
    value["accessors"][1]["componentType"] = component;
    value["accessors"][1]["min"] = {0}; value["accessors"][1]["max"] = {2};
    ow::assets::Catalog catalog; auto result = accept(catalog, glb(value, bin));
    check(result.scene->meshes[0].primitives[0].indices == std::vector<std::uint32_t>{0,1,2}, "all unsigned index widths decoded");
    for (unsigned j = 0; j < size; ++j) bin[36 + j] = 255;
    reject(catalog, glb(value, bin), "INVALID_INDEX");
  }
  auto value = doc(); Bytes interleaved{9,9,9,9};
  const auto packed = binary();
  for (std::size_t at = 0; at < 36; at += 12) {
    interleaved.insert(interleaved.end(), packed.begin() + static_cast<std::ptrdiff_t>(at), packed.begin() + static_cast<std::ptrdiff_t>(at + 12));
    u32(interleaved, 0x7fc00000); // Unused interleaved bytes must never be interpreted as POSITION.
  }
  interleaved.insert(interleaved.end(), packed.begin() + 36, packed.end());
  value["bufferViews"][0]["byteOffset"] = 4; value["bufferViews"][0]["byteLength"] = 44; value["bufferViews"][0]["byteStride"] = 16;
  value["bufferViews"][1]["byteOffset"] = 52; value["buffers"][0]["byteLength"] = interleaved.size();
  ow::assets::Catalog stride; auto result = accept(stride, glb(value, interleaved));
  check(result.scene->meshes[0].primitives[0].positions == std::vector<Vec3>{{-1,0,0},{1,0,0},{0,2,0}}, "strided offset decode skips padding");
  auto large = doc(); Bytes bin(256 * 12, 0);
  for (unsigned i = 0; i < 768; ++i) { bin.push_back(static_cast<std::uint8_t>(i % 3)); bin.push_back(0); }
  large["accessors"][0]["count"] = 256; large["accessors"][0]["min"] = {0,0,0}; large["accessors"][0]["max"] = {0,0,0};
  large["accessors"][1]["count"] = 768;
  large["bufferViews"][0]["byteLength"] = 256 * 12; large["bufferViews"][0]["byteStride"] = 12;
  large["bufferViews"][1]["byteOffset"] = 256 * 12; large["bufferViews"][1]["byteLength"] = 768 * 2;
  large["buffers"][0]["byteLength"] = bin.size();
  for (unsigned i = 0; i < 3; ++i) large["meshes"][0]["primitives"].push_back(large["meshes"][0]["primitives"][0]);
  ow::assets::Catalog caps; auto maximum = accept(caps, glb(large, bin));
  check(maximum.scene->meshes[0].primitives.size() == 4
      && maximum.scene->meshes[0].primitives[3].positions.size() == 256
      && maximum.scene->meshes[0].primitives[3].indices.size() == 768, "exact materialized vertex/index caps");
  auto bad = large; bad["meshes"][0]["primitives"].push_back(bad["meshes"][0]["primitives"][0]);
  reject(caps, glb(bad, bin), "BUDGET_EXCEEDED");
  auto byte_sentinel = large; auto byte_bin = bin; byte_bin.resize(256 * 12);
  byte_bin.insert(byte_bin.end(), {0,1,255});
  byte_sentinel["buffers"][0]["byteLength"] = byte_bin.size();
  byte_sentinel["bufferViews"][1]["byteLength"] = 3;
  byte_sentinel["accessors"][1]["componentType"] = 5121; byte_sentinel["accessors"][1]["count"] = 3;
  reject(caps, glb(byte_sentinel, byte_bin), "INVALID_INDEX"); // 255 is in vertex range but is forbidden restart.
  bad = large; for (unsigned i = 0; i < 4; ++i) bad["accessors"].push_back(bad["accessors"][0]);
  reject(caps, glb(bad, bin), "BUDGET_EXCEEDED");
  bad = large; for (unsigned i = 0; i < 4; ++i) bad["accessors"].push_back(bad["accessors"][1]);
  reject(caps, glb(bad, bin), "BUDGET_EXCEEDED");
  for (const auto& cap : std::vector<std::pair<const char*, unsigned>>{{"bufferViews",8},{"accessors",8},{"meshes",4},{"nodes",8}}) {
    bad = doc(); while (bad[cap.first].size() <= cap.second) bad[cap.first].push_back(bad[cap.first][0]);
    reject(caps, glb(bad), "BUDGET_EXCEEDED");
  }
  bad = doc(); bad["materials"] = Json::array({Json::object(),Json::object(),Json::object(),Json::object(),Json::object()});
  reject(caps, glb(bad), "BUDGET_EXCEEDED");
  reject(caps, glb(doc()), "REVISION_CONFLICT", 0);
  ow::assets::Catalog full;
  const auto bytes = glb(doc());
  for (unsigned i = 0; i < 8; ++i) accept(full, bytes, "generated:unit/" + std::to_string(i));
  reject(full, bytes, "BUDGET_EXCEEDED");
  auto receipt = full.apply(ow::assets::Collect{}, full.revision());
  check(receipt.status == "committed" && receipt.manifests_removed == 8 && receipt.blobs_removed == 1, "catalog quota release");
  accept(full, bytes);
  const auto before = full.snapshot();
  const auto invalid = import_glb(full, {bytes, "bad\nsource", "Apache-2.0", full.revision()});
  check(!invalid.scene && invalid.receipt.code == "INVALID_MANIFEST" && full.snapshot() == before, "invalid provenance is atomic");
  const auto oversized_provenance = import_glb(full, {bytes, std::string(10000, 'x'), "Apache-2.0", full.revision()});
  check(!oversized_provenance.scene && oversized_provenance.receipt.code == "INVALID_MANIFEST"
      && full.snapshot() == before, "oversized caller provenance rejected before copy");
}
} // namespace
int main() {
  try {
    geometry(); malformed_container_and_json(); invalid_schema_and_geometry(); indices_stride_and_budgets();
    std::cout << "{\"status\":\"passed\",\"assertions\":" << checks << "}\n";
    return 0;
  } catch (const TestFailure& error) {
    std::cerr << "gltf native invariant failed: " << error.what() << " (assertion " << checks << ")\n";
  } catch (...) {
    std::cerr << "gltf native invariant failed: unexpected exception\n";
  }
  return 1;
}
