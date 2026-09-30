// SPDX-License-Identifier: Apache-2.0
#include "gltf_example.hpp"
#include "omniweft/gltf.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
using namespace ow::gltf;
using ow::assets::Bytes;
using Json = nlohmann::json;
void require(bool value) { if (!value) throw std::runtime_error("GLTF_FIXTURE_FAILED"); }
void number(Bytes& out, std::uint32_t value, unsigned count = 4) {
  for (unsigned i = 0; i < count; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
std::string hex(const Bytes& bytes) {
  constexpr char digits[] = "0123456789abcdef"; std::string out; out.reserve(bytes.size() * 2);
  for (const auto byte : bytes) { out += digits[byte >> 4]; out += digits[byte & 15]; }
  return out;
}
Json document() {
  return {{"asset", {{"version", "2.0"}, {"generator", "Omniweft seed7 fixture"}}},
    {"buffers", Json::array({{{"byteLength", 82}}})},
    {"bufferViews", Json::array({{{"buffer", 0}, {"byteOffset", 4}, {"byteLength", 60}, {"byteStride", 16}, {"target", 34962}},
      {{"buffer", 0}, {"byteOffset", 68}, {"byteLength", 6}, {"target", 34963}},
      {{"buffer", 0}, {"byteOffset", 76}, {"byteLength", 6}, {"target", 34963}}})},
    {"accessors", Json::array({{{"bufferView", 0}, {"componentType", 5126}, {"count", 4}, {"type", "VEC3"}, {"min", {0,0,0}}, {"max", {2,1,0}}},
      {{"bufferView", 1}, {"componentType", 5123}, {"count", 3}, {"type", "SCALAR"}},
      {{"bufferView", 2}, {"componentType", 5123}, {"count", 3}, {"type", "SCALAR"}}})},
    {"meshes", Json::array({{{"primitives", Json::array({{{"attributes", {{"POSITION", 0}}}, {"indices", 1}, {"material", 0}, {"mode", 4}},
      {{"attributes", {{"POSITION", 0}}}, {"indices", 2}}})}}})},
    {"materials", Json::array({{{"name", "seed7-blue"}, {"doubleSided", true}, {"alphaMode", "OPAQUE"},
      {"pbrMetallicRoughness", {{"baseColorFactor", {0.25,0.5,1,1}}, {"metallicFactor", 0}, {"roughnessFactor", 0.75}}}}})},
    {"nodes", Json::array({{{"translation", {7,2,1}}, {"scale", {2,2,2}}, {"children", {1}}},
      {{"mesh", 0}, {"translation", {1,0,0}}, {"rotation", {0,0,1,0}}, {"scale", {0.5,1,1}}},
      {{"mesh", 0}, {"translation", {-3,0,-1}}, {"scale", {-1,1,1}}}})},
    {"scenes", Json::array({{{"nodes", {0,2}}}})}, {"scene", 0}};
}
Bytes binary() {
  Bytes out; number(out, 7);
  for (const auto& p : std::vector<std::array<float,4>>{{0,0,0,777},{2,0,0,777},{0,1,0,777},{2,1,0,777}})
    for (float v : p) number(out, std::bit_cast<std::uint32_t>(v));
  for (auto i : {0U,1U,2U,0U,1U,3U,2U,0U}) number(out, i, 2);
  return out;
}
Bytes glb(const Json& doc, const Bytes& bin) {
  auto text = doc.dump(); while (text.size() % 4) text += ' ';
  Bytes out; number(out, 0x46546c67); number(out, 2);
  number(out, static_cast<std::uint32_t>(28 + text.size() + bin.size()));
  number(out, static_cast<std::uint32_t>(text.size())); number(out, 0x4e4f534a);
  out.insert(out.end(), text.begin(), text.end()); number(out, static_cast<std::uint32_t>(bin.size())); number(out, 0x004e4942);
  out.insert(out.end(), bin.begin(), bin.end()); return out;
}
Json bounds(const Bounds& b) { return {{"minimum", b.minimum}, {"maximum", b.maximum}}; }
Json asset(const ow::assets::Asset& a) {
  const auto& m = a.manifest;
  return {{"id", a.id}, {"manifest", {{"format_version", m.format_version}, {"content_hash", m.content_hash},
    {"decoded_length", m.decoded_length}, {"media_type", m.media_type}, {"source", m.source}, {"license", m.license}}}};
}
Json snapshot(const ow::assets::Snapshot& s) {
  Json assets = Json::array(), blobs = Json::array(), history = Json::array();
  for (const auto& a : s.assets) assets.push_back(asset(a));
  for (const auto& b : s.blobs) blobs.push_back({{"hash", b.hash}, {"bytes_hex", hex(b.bytes)}});
  for (const auto& h : s.history_roots) history.push_back({{"name", h.name}, {"asset_ids", h.asset_ids}});
  return {{"format_version", s.format_version}, {"revision", s.revision}, {"assets", assets}, {"blobs", blobs},
    {"current_roots", s.current_roots}, {"history_roots", history}};
}
Json receipt(const ow::assets::Receipt& r) {
  return {{"status", r.status}, {"revision", r.revision}, {"code", r.code}, {"imported", r.imported},
    {"manifests_removed", r.manifests_removed}, {"blobs_removed", r.blobs_removed}};
}
Json scene(const Scene& s) {
  Json meshes = Json::array(), nodes = Json::array(), materials = Json::array();
  for (const auto& mesh : s.meshes) {
    Json primitives = Json::array();
    for (const auto& p : mesh.primitives) primitives.push_back({{"positions", p.positions}, {"indices", p.indices},
      {"material", p.material ? Json(*p.material) : Json(nullptr)}, {"bounds", bounds(p.bounds)}});
    meshes.push_back({{"primitives", primitives}});
  }
  for (const auto& n : s.nodes) nodes.push_back({{"mesh", n.mesh ? Json(*n.mesh) : Json(nullptr)}, {"children", n.children},
    {"translation", n.translation}, {"rotation", n.rotation}, {"scale", n.scale}, {"local_matrix", n.local_matrix},
    {"world_matrix", n.world_matrix}, {"world_bounds", n.world_bounds ? bounds(*n.world_bounds) : Json(nullptr)}});
  for (const auto& m : s.materials) materials.push_back({{"name", m.name}, {"base_color_factor", m.base_color_factor},
    {"metallic_factor", m.metallic_factor}, {"roughness_factor", m.roughness_factor}, {"double_sided", m.double_sided}});
  return {{"profile", s.profile}, {"source_asset", asset(s.source_asset)}, {"roots", s.roots}, {"meshes", meshes},
    {"nodes", nodes}, {"materials", materials}, {"world_bounds", bounds(s.world_bounds)}};
}
Mat4 matrix(double x, double y, double z, double tx, double ty, double tz) {
  return {x,0,0,0, 0,y,0,0, 0,0,z,0, tx,ty,tz,1};
}
Scene expected_scene(const Bytes& bytes, const std::string& source) {
  Scene s;
  s.source_asset.manifest = {1, ow::assets::content_hash(bytes), static_cast<std::uint32_t>(bytes.size()), "model/gltf-binary", source, "Apache-2.0"};
  s.source_asset.id = ow::assets::asset_id(s.source_asset.manifest); s.roots = {0,2};
  const std::vector<Vec3> positions{{0,0,0},{2,0,0},{0,1,0},{2,1,0}};
  s.meshes = {{{{positions,{0,1,2},0,{{0,0,0},{2,1,0}}}, {positions,{1,3,2},std::nullopt,{{0,0,0},{2,1,0}}}}}};
  Node a; a.children={1}; a.translation={7,2,1}; a.scale={2,2,2}; a.local_matrix=a.world_matrix=matrix(2,2,2,7,2,1);
  Node b; b.mesh=0; b.translation={1,0,0}; b.rotation={0,0,1,0}; b.scale={0.5,1,1};
  b.local_matrix=matrix(-0.5,-1,1,1,0,0); b.world_matrix=matrix(-1,-2,2,9,2,1); b.world_bounds=Bounds{{7,0,1},{9,2,1}};
  Node c; c.mesh=0; c.translation={-3,0,-1}; c.scale={-1,1,1}; c.local_matrix=c.world_matrix=matrix(-1,1,1,-3,0,-1);
  c.world_bounds=Bounds{{-5,0,-1},{-3,1,-1}}; s.nodes={a,b,c};
  s.materials={{"seed7-blue",{0.25,0.5,1,1},0,0.75,true}}; s.world_bounds={{-5,0,-1},{9,2,1}}; return s;
}
void write(const std::filesystem::path& path, const Bytes& bytes) {
  std::ofstream stream(path, std::ios::binary); stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); stream.close();
}
Bytes read(const std::filesystem::path& path) {
  const auto size=std::filesystem::file_size(path); require(size<=65536); Bytes out(static_cast<std::size_t>(size));
  std::ifstream stream(path,std::ios::binary); stream.exceptions(std::ios::badbit|std::ios::failbit);
  stream.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(size)); require(stream.peek()==std::char_traits<char>::eof()); return out;
}
} // namespace

int run_gltf_example(int argc, char* argv[]) { try {
  bool example=false,headless=false,seed=false,verify=false,output=false; std::filesystem::path directory;
  for (int i=1;i<argc;++i) {
    const std::string_view arg(argv[i]); const auto value=[&]() -> std::string_view { require(i+1<argc); return argv[++i]; };
    if (arg=="--example"&&!example) { example=true; require(value()=="meshes.import_gltf"); }
    else if (arg=="--headless"&&!headless) headless=true;
    else if (arg=="--seed"&&!seed) { seed=true; require(value()=="7"); }
    else if (arg=="--verify"&&!verify) verify=true;
    else if (arg=="--output"&&!output) { output=true; directory=value(); require(!directory.empty()); }
    else require(false);
  }
  require(example&&headless&&seed&&output);
  if (!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  require(std::filesystem::create_directory(directory)); const auto doc=document(); const auto bin=binary(); const auto bytes=glb(doc,bin);
  write(directory/"seed7.glb",bytes); const auto readback=read(directory/"seed7.glb"); require(readback==bytes);
  ow::assets::Catalog catalog; Json steps=Json::array(), negatives=Json::array();
  ow::assets::Snapshot expected_catalog;
  const auto accept=[&](const char* label,const std::string& source) {
    const auto expected=expected_scene(bytes,source);
    const auto before=catalog.revision(); const ImportRequest request{readback,source,"Apache-2.0",before};
    auto result=import_glb(catalog,request); require(result.scene.has_value());
    expected_catalog.revision=before+1;
    if (std::none_of(expected_catalog.assets.begin(),expected_catalog.assets.end(),[&](const auto& a){return a.id==expected.source_asset.id;})) expected_catalog.assets.push_back(expected.source_asset);
    if (expected_catalog.blobs.empty()) expected_catalog.blobs.push_back({expected.source_asset.manifest.content_hash,bytes});
    std::sort(expected_catalog.assets.begin(),expected_catalog.assets.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    if (verify) {
      require(*result.scene==expected); require(catalog.snapshot()==expected_catalog);
      require(result.receipt==ow::assets::Receipt{"committed",before+1,"",{expected.source_asset.id},0,0}); require(request.glb==bytes);
    }
    steps.push_back({{"case",label},{"receipt",receipt(result.receipt)},{"scene",scene(*result.scene)},{"snapshot",snapshot(catalog.snapshot())}});
    result.scene->meshes[0].primitives[0].positions[0][0]=999;
    require(catalog.snapshot()==expected_catalog);
  };
  accept("import","generated:seed7/quad"); accept("deduplicate","generated:seed7/quad"); accept("distinct-provenance","generated:seed7/quad-alt");
  const auto reject=[&](const char* label,const Bytes& input,const char* code,std::uint64_t revision) {
    const auto before=catalog.snapshot(); const auto saved=catalog.export_bundle(); const ImportRequest request{input,"generated:seed7/quad","Apache-2.0",revision};
    const auto result=import_glb(catalog,request); const auto after=catalog.snapshot();
    require(!result.scene); require(result.receipt==ow::assets::Receipt{"rejected",before.revision,code,{},0,0});
    require(before==after&&catalog.export_bundle()==saved&&request.glb==input);
    negatives.push_back({{"case",label},{"input_hex",hex(input)},{"input_unchanged",true},{"receipt",receipt(result.receipt)},
      {"scene",nullptr},{"before",snapshot(before)},{"after",snapshot(after)}});
  };
  auto bad_bin=bin; bad_bin[68]=4; reject("invalid-index",glb(doc,bad_bin),"INVALID_INDEX",3);
  bad_bin=bin; bad_bin[4]=0; bad_bin[5]=0; bad_bin[6]=0xc0; bad_bin[7]=0x7f;
  reject("nonfinite-position",glb(doc,bad_bin),"NONFINITE_VALUE",3);
  auto bad_doc=doc; bad_doc["extensionsUsed"]={"KHR_draco_mesh_compression"}; bad_doc["extensionsRequired"]={"KHR_draco_mesh_compression"};
  reject("required-extension",glb(bad_doc,bin),"UNSUPPORTED_REQUIRED_EXTENSION",3);
  reject("file-budget",Bytes(65537,0),"BUDGET_EXCEEDED",3);
  bad_doc=doc; bad_doc["accessors"][0]["count"]=257; reject("vertex-budget",glb(bad_doc,bin),"BUDGET_EXCEEDED",3);
  reject("stale-revision",bytes,"REVISION_CONFLICT",2);
  bad_doc=doc; bad_doc["meshes"][0]["primitives"][0]["material"]=4; reject("material-reference",glb(bad_doc,bin),"INVALID_SCHEMA",3);
  bad_doc=doc; bad_doc["buffers"][0]["uri"]="fixture.bin"; reject("external-uri",glb(bad_doc,bin),"UNSUPPORTED_PROFILE",3);
  bad_doc=doc; bad_doc["nodes"][1]["children"]={0}; reject("cycle",glb(bad_doc,bin),"INVALID_SCHEMA",3);
  bad_doc=doc; bad_doc["accessors"][0]["max"]={3,1,0}; reject("declared-bounds",glb(bad_doc,bin),"INVALID_SCHEMA",3);
  bad_bin=bin; bad_bin[82]=1; reject("bin-padding",glb(doc,bad_bin),"INVALID_GLB",3);
  auto truncated=bytes; truncated.pop_back(); reject("truncated",truncated,"INVALID_GLB",3);
  accept("recovery","generated:seed7/quad");
  Json result={{"schema_version",1},{"example","meshes.import_gltf"},{"seed",7},{"verified",verify},
    {"file_hex",hex(readback)},{"file_sha256",ow::assets::content_hash(readback)},{"detached_output_unchanged",true},{"steps",steps},{"negative",negatives}};
  const auto text=result.dump(2)+'\n'; write(directory/"result.json",Bytes(text.begin(),text.end()));
  std::cout<<"meshes.import_gltf passed\n"; return 0;
} catch (...) { std::cerr<<"meshes.import_gltf failed\n"; return 1; } }
