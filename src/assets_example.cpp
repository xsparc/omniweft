// SPDX-License-Identifier: Apache-2.0
#include "assets_example.hpp"
#include "omniweft/assets.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
using namespace ow::assets;
using Json = nlohmann::json;
void require(bool value) { if (!value) throw std::runtime_error("ASSET_FIXTURE_FAILED"); }
std::string hex(const Bytes& value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out; out.reserve(value.size() * 2);
  for (auto byte : value) { out += digits[byte >> 4]; out += digits[byte & 15]; }
  return out;
}
void number(Bytes& out, std::uint32_t value, unsigned count) {
  for (unsigned i = 0; i < count; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
void append(Bytes& out, const Bytes& value) { out.insert(out.end(), value.begin(), value.end()); }
void text(Bytes& out, std::string_view value) { out.insert(out.end(), value.begin(), value.end()); }
Bytes digest_bytes(std::string_view value) {
  const auto nibble = [](char c) { return static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10); };
  require(value.size() == 64); Bytes result;
  for (std::size_t i = 0; i < value.size(); i += 2)
    result.push_back(static_cast<std::uint8_t>(nibble(value[i]) * 16 + nibble(value[i + 1])));
  return result;
}
struct Packed { std::string path; std::uint8_t codec; std::uint32_t decoded; Bytes encoded; };
Packed raw(const Bytes& b) { return {"blobs/" + content_hash(b), 0, static_cast<std::uint32_t>(b.size()), b}; }
Bytes bundle(const std::vector<Manifest>& manifests, const std::vector<Packed>& blobs) {
  Bytes out; text(out, "OWASB001"); number(out, 1, 4);
  number(out, static_cast<std::uint32_t>(manifests.size()), 4);
  number(out, static_cast<std::uint32_t>(blobs.size()), 4);
  for (const auto& manifest : manifests) {
    const auto value = canonical_manifest(manifest); append(out, digest_bytes(asset_id(manifest)));
    number(out, static_cast<std::uint32_t>(value.size()), 4); append(out, value);
  }
  for (const auto& blob : blobs) {
    number(out, static_cast<std::uint32_t>(blob.path.size()), 2); text(out, blob.path);
    out.push_back(blob.codec); number(out, blob.decoded, 4);
    number(out, static_cast<std::uint32_t>(blob.encoded.size()), 4); append(out, blob.encoded);
  }
  return out;
}
Json manifest_json(const Manifest& m) {
  return {{"format_version", m.format_version}, {"content_hash", m.content_hash},
    {"decoded_length", m.decoded_length}, {"media_type", m.media_type}, {"source", m.source}, {"license", m.license}};
}
Json snapshot_json(const Snapshot& s) {
  Json assets = Json::array(), blobs = Json::array(), history = Json::array();
  for (const auto& a : s.assets) assets.push_back({{"id", a.id}, {"manifest", manifest_json(a.manifest)}});
  for (const auto& b : s.blobs) blobs.push_back({{"hash", b.hash}, {"bytes_hex", hex(b.bytes)}});
  for (const auto& h : s.history_roots) history.push_back({{"name", h.name}, {"asset_ids", h.asset_ids}});
  return {{"format_version", s.format_version}, {"revision", s.revision}, {"assets", assets}, {"blobs", blobs},
    {"current_roots", s.current_roots}, {"history_roots", history}};
}
Json receipt_json(const Receipt& r) {
  return {{"status", r.status}, {"revision", r.revision}, {"code", r.code}, {"imported", r.imported},
    {"manifests_removed", r.manifests_removed}, {"blobs_removed", r.blobs_removed}};
}
Snapshot state(std::uint64_t revision, std::vector<Manifest> manifests, std::vector<Bytes> blobs,
               std::vector<AssetId> roots = {}, std::vector<HistoryRoots> history = {}) {
  Snapshot s; s.revision = revision;
  for (const auto& m : manifests) s.assets.push_back({asset_id(m), m});
  for (const auto& b : blobs) s.blobs.push_back({content_hash(b), b});
  std::sort(s.assets.begin(), s.assets.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
  std::sort(s.blobs.begin(), s.blobs.end(), [](const auto& a, const auto& b) { return a.hash < b.hash; });
  std::sort(roots.begin(), roots.end()); s.current_roots = std::move(roots); s.history_roots = std::move(history);
  return s;
}
void write_bytes(const std::filesystem::path& file, const Bytes& bytes) {
  std::ofstream stream(file, std::ios::binary); stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); stream.close();
}
Bytes read_bytes(const std::filesystem::path& file) {
  const auto size = std::filesystem::file_size(file); require(size <= bundle_byte_limit);
  Bytes bytes(static_cast<std::size_t>(size)); std::ifstream stream(file, std::ios::binary);
  stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  require(stream.peek() == std::char_traits<char>::eof()); return bytes;
}
} // namespace

int run_assets_example(int argc, char* argv[]) { try {
  bool example = false, headless = false, seed = false, verify = false, output = false;
  std::filesystem::path directory;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    const auto value = [&]() -> std::string_view { require(i + 1 < argc); return argv[++i]; };
    if (arg == "--example" && !example) { example = true; require(value() == "assets.roundtrip"); }
    else if (arg == "--headless" && !headless) headless = true;
    else if (arg == "--seed" && !seed) { seed = true; require(value() == "7"); }
    else if (arg == "--verify" && !verify) verify = true;
    else if (arg == "--output" && !output) { output = true; directory = value(); require(!directory.empty()); }
    else require(false);
  }
  require(example && headless && seed && output);
  if (!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  require(std::filesystem::create_directory(directory));
  const Bytes a_bytes{7, 7, 7, 14, 21, 21, 28, 35}, c_bytes{0, 7, 14, 21};
  const Manifest a{1, content_hash(a_bytes), 8, "application/octet-stream", "generated:seed7/a", "Apache-2.0"};
  auto b = a; b.source = "generated:seed7/a-alt";
  const Manifest c{1, content_hash(c_bytes), 4, "application/octet-stream", "generated:seed7/c", "Apache-2.0"};
  const auto aid = asset_id(a), bid = asset_id(b), cid = asset_id(c);
  const auto a_bundle = bundle({a}, {raw(a_bytes)});
  Catalog catalog; Json steps = Json::array();
  const auto apply = [&](const char* label, const Command& command, const Snapshot& expected,
                         std::vector<AssetId> imported = {}, std::uint32_t removed = 0, std::uint32_t blobs_removed = 0) {
    auto r = catalog.apply(command, catalog.revision()); require(r.status == "committed");
    std::sort(imported.begin(), imported.end());
    if (verify) {
      require(r == Receipt{"committed", expected.revision, "", imported, removed, blobs_removed});
      require(catalog.snapshot() == expected);
    }
    steps.push_back({{"case", label}, {"receipt", receipt_json(r)}, {"snapshot", snapshot_json(catalog.snapshot())}});
  };
  apply("import-a", Import{a_bundle}, state(1, {a}, {a_bytes}), {aid});
  apply("deduplicate-a", Import{a_bundle}, state(2, {a}, {a_bytes}), {aid});
  apply("provenance-and-content", Import{bundle({c, b}, {raw(c_bytes), raw(a_bytes)})}, state(3, {a, b, c}, {a_bytes, c_bytes}), {bid, cid});
  apply("current-roots", SetCurrentRoots{{cid, bid}}, state(4, {a, b, c}, {a_bytes, c_bytes}, {bid, cid}));
  apply("history-root", SetHistoryRoots{"undo-1", {aid}}, state(5, {a, b, c}, {a_bytes, c_bytes}, {bid, cid}, {{"undo-1", {aid}}}));

  const auto exported = catalog.export_bundle(); write_bytes(directory / "seed7.owas", exported);
  const auto readback = read_bytes(directory / "seed7.owas"); Catalog restored;
  const auto imported = restored.apply(Import{readback}, 0); require(imported.status == "committed");
  require(restored.export_bundle() == exported && readback == exported);
  if (verify) require(restored.snapshot() == state(1, {a, b, c}, {a_bytes, c_bytes}));
  const auto roundtrip = Json{{"bundle_hex", hex(exported)}, {"file_sha256", content_hash(readback)},
    {"receipt", receipt_json(imported)}, {"snapshot", snapshot_json(restored.snapshot())}, {"export_hex", hex(restored.export_bundle())}};
  const auto packed_a = Packed{raw(a_bytes).path, 1, 8, {3, 7, 1, 14, 2, 21, 1, 28, 1, 35}};
  const auto rle_bundle = bundle({a}, {packed_a}); Catalog rle;
  const auto rle_receipt = rle.apply(Import{rle_bundle}, 0); require(rle_receipt.status == "committed");
  if (verify) require(rle.snapshot() == state(1, {a}, {a_bytes}) && rle.export_bundle() == a_bundle);
  const auto rle_proof = Json{{"input_hex", hex(rle_bundle)}, {"receipt", receipt_json(rle_receipt)},
    {"snapshot", snapshot_json(rle.snapshot())}, {"export_hex", hex(rle.export_bundle())}};

  auto detached = catalog.snapshot(); detached.assets.clear(); detached.blobs.clear(); detached.current_roots.clear(); detached.history_roots.clear(); detached.revision = 0;
  const bool detached_unchanged = catalog.snapshot() == state(5, {a, b, c}, {a_bytes, c_bytes}, {bid, cid}, {{"undo-1", {aid}}});
  require(detached_unchanged);
  apply("drop-current-c", SetCurrentRoots{{bid}}, state(6, {a, b, c}, {a_bytes, c_bytes}, {bid}, {{"undo-1", {aid}}}));
  apply("history-retains-a", Collect{}, state(7, {a, b}, {a_bytes}, {bid}, {{"undo-1", {aid}}}), {}, 1, 1);
  apply("remove-history", RemoveHistoryRoots{"undo-1"}, state(8, {a, b}, {a_bytes}, {bid}));
  apply("shared-blob-survives", Collect{}, state(9, {b}, {a_bytes}, {bid}), {}, 1, 0);
  apply("drop-last-root", SetCurrentRoots{{}}, state(10, {b}, {a_bytes}));
  apply("reclaim-last-blob", Collect{}, state(11, {}, {}), {}, 1, 1);
  apply("reimport-recovery", Import{a_bundle}, state(12, {a}, {a_bytes}), {aid});
  apply("restore-current", SetCurrentRoots{{aid}}, state(13, {a}, {a_bytes}, {aid}));

  Json negative = Json::array();
  const auto reject = [&](const char* label, const Command& command, const char* code, std::uint64_t revision) {
    const auto before = catalog.snapshot(); const auto* request = std::get_if<Import>(&command);
    const Bytes input = request ? request->bundle : Bytes{};
    const auto r = catalog.apply(command, revision); const auto after = catalog.snapshot();
    require(r.status == "rejected" && after == before && (!request || request->bundle == input));
    if (verify) require(r == Receipt{"rejected", before.revision, code, {}, 0, 0});
    negative.push_back({{"case", label}, {"input_hex", request ? Json(hex(input)) : Json(nullptr)},
      {"input_unchanged", !request || request->bundle == input}, {"receipt", receipt_json(r)},
      {"before", snapshot_json(before)}, {"after", snapshot_json(after)}});
  };
  reject("missing-blob", Import{bundle({a}, {})}, "MISSING_BLOB", 13);
  auto packed = raw(a_bytes); packed.path = "../" + packed.path;
  reject("traversal-path", Import{bundle({a}, {packed})}, "INVALID_PATH", 13);
  packed = raw(a_bytes); packed.encoded[0] ^= 1;
  reject("wrong-content-hash", Import{bundle({a}, {packed})}, "HASH_MISMATCH", 13);
  auto bad = a_bundle; bad[20] ^= 1;
  reject("wrong-manifest-hash", Import{bad}, "HASH_MISMATCH", 13);
  packed = packed_a; packed.encoded = {9, 7};
  reject("decompression-overflow", Import{bundle({a}, {packed})}, "DECOMPRESSION_ERROR", 13);
  packed.encoded = {0, 7}; reject("decompression-zero", Import{bundle({a}, {packed})}, "DECOMPRESSION_ERROR", 13);
  packed.encoded = {8}; reject("decompression-truncated", Import{bundle({a}, {packed})}, "DECOMPRESSION_ERROR", 13);
  bad = a_bundle; bad[8] = 2; reject("unknown-version", Import{bad}, "UNSUPPORTED_VERSION", 13);
  packed = raw(a_bytes); packed.codec = 9; reject("unknown-codec", Import{bundle({a}, {packed})}, "INVALID_BUNDLE", 13);
  bad = a_bundle; bad.push_back(0); reject("trailing-data", Import{bad}, "INVALID_BUNDLE", 13);
  reject("duplicate-manifest", Import{bundle({a, a}, {raw(a_bytes)})}, "DUPLICATE_ENTRY", 13);
  reject("duplicate-blob", Import{bundle({a}, {raw(a_bytes), raw(a_bytes)})}, "DUPLICATE_ENTRY", 13);
  reject("extra-blob", Import{bundle({a}, {raw(a_bytes), raw(c_bytes)})}, "EXTRA_BLOB", 13);
  packed = raw(a_bytes); packed.decoded = 65537;
  reject("decoded-budget", Import{bundle({a}, {packed})}, "BUDGET_EXCEEDED", 13);
  reject("stale-revision", Collect{}, "REVISION_CONFLICT", 12);
  reject("unknown-root", SetCurrentRoots{{std::string(64, '0')}}, "UNKNOWN_ASSET", 13);
  apply("post-rejection-recovery", SetHistoryRoots{"undo-2", {aid}}, state(14, {a}, {a_bytes}, {aid}, {{"undo-2", {aid}}}));
  const Json report{{"schema_version", 1}, {"example", "assets.roundtrip"}, {"seed", 7}, {"verified", verify},
    {"detached_query_unchanged", detached_unchanged}, {"steps", steps}, {"roundtrip", roundtrip},
    {"rle", rle_proof}, {"negative", negative}};
  std::ofstream stream(directory / "result.json.tmp", std::ios::binary); stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream << report.dump(2) << '\n'; stream.close(); std::filesystem::rename(directory / "result.json.tmp", directory / "result.json");
  std::cout << "assets.roundtrip: bounded catalog fixture completed\n"; return 0;
} catch (...) { std::cerr << "assets.roundtrip failed: check fixture options and fresh writable output\n"; return 2; } }
