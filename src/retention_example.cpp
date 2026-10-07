// SPDX-License-Identifier: Apache-2.0
#include "retention_example.hpp"
#include "omniweft/assets.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <type_traits>

namespace {
using namespace ow::assets;
using Json = nlohmann::json;
void require(bool value) { if (!value) throw std::runtime_error("RETENTION_FIXTURE_FAILED"); }
std::string hex(const Bytes& value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out; out.reserve(value.size() * 2);
  for (auto byte : value) { out += digits[byte >> 4]; out += digits[byte & 15]; }
  return out;
}
void number(Bytes& out, std::uint32_t value, unsigned count) {
  for (unsigned i = 0; i < count; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
void text(Bytes& out, std::string_view value) { out.insert(out.end(), value.begin(), value.end()); }
void digest_bytes(Bytes& out, std::string_view value) {
  const auto nibble = [](char c) { return static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10); };
  require(value.size() == 64);
  for (std::size_t i = 0; i < value.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(nibble(value[i]) * 16 + nibble(value[i + 1])));
}
Bytes bundle(const std::vector<Manifest>& manifests, const std::vector<Bytes>& blobs) {
  Bytes out; text(out, "OWASB001"); number(out, 1, 4);
  number(out, static_cast<std::uint32_t>(manifests.size()), 4);
  number(out, static_cast<std::uint32_t>(blobs.size()), 4);
  for (const auto& manifest : manifests) {
    const auto encoded = canonical_manifest(manifest); digest_bytes(out, asset_id(manifest));
    number(out, static_cast<std::uint32_t>(encoded.size()), 4);
    out.insert(out.end(), encoded.begin(), encoded.end());
  }
  for (const auto& bytes : blobs) {
    const auto path = "blobs/" + content_hash(bytes);
    number(out, static_cast<std::uint32_t>(path.size()), 2); text(out, path); out.push_back(0);
    number(out, static_cast<std::uint32_t>(bytes.size()), 4);
    number(out, static_cast<std::uint32_t>(bytes.size()), 4);
    out.insert(out.end(), bytes.begin(), bytes.end());
  }
  return out;
}
Json histories(const std::vector<HistoryRoots>& values) {
  Json out = Json::array();
  for (const auto& value : values) out.push_back({{"name", value.name}, {"asset_ids", value.asset_ids}});
  return out;
}
Json snapshot_json(const Snapshot& s) {
  Json assets = Json::array(), blobs = Json::array();
  for (const auto& a : s.assets) {
    const auto& m = a.manifest;
    assets.push_back({{"id", a.id}, {"manifest", {{"format_version", m.format_version},
      {"content_hash", m.content_hash}, {"decoded_length", m.decoded_length}, {"media_type", m.media_type},
      {"source", m.source}, {"license", m.license}}}});
  }
  for (const auto& b : s.blobs) blobs.push_back({{"hash", b.hash}, {"bytes_hex", hex(b.bytes)}});
  return {{"format_version", s.format_version}, {"revision", s.revision}, {"assets", assets}, {"blobs", blobs},
    {"current_roots", s.current_roots}, {"history_roots", histories(s.history_roots)}};
}
Json receipt_json(const Receipt& r) {
  return {{"status", r.status}, {"revision", r.revision}, {"code", r.code}, {"imported", r.imported},
    {"manifests_removed", r.manifests_removed}, {"blobs_removed", r.blobs_removed}};
}
Json result_json(const RetentionResult& result) {
  Json report = nullptr;
  if (result.report) {
    const auto& r = *result.report; Json assets = Json::array(), blobs = Json::array();
    for (const auto& a : r.assets) assets.push_back({{"id", a.id}, {"content_hash", a.content_hash},
      {"current_root", a.current_root}, {"history_roots", a.history_roots}});
    for (const auto& b : r.blobs) blobs.push_back({{"hash", b.hash}, {"referencing_assets", b.referencing_assets},
      {"retaining_assets", b.retaining_assets}});
    report = {{"format_version", r.format_version}, {"current_roots", r.current_roots},
      {"history_roots", histories(r.history_roots)}, {"assets", assets}, {"blobs", blobs},
      {"removable_assets", r.removable_assets}, {"removable_blobs", r.removable_blobs}};
  }
  return {{"status", std::string(result.status)}, {"code", std::string(result.code)},
    {"revision", result.revision}, {"report", report}};
}
Json command_json(const Command& command) {
  return std::visit([](const auto& value) -> Json {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, Import>) return {{"type", "import"}, {"bundle_hex", hex(value.bundle)}};
    else if constexpr (std::is_same_v<T, SetCurrentRoots>) return {{"type", "set_current_roots"}, {"asset_ids", value.asset_ids}};
    else if constexpr (std::is_same_v<T, SetHistoryRoots>) return {{"type", "set_history_roots"}, {"name", value.name}, {"asset_ids", value.asset_ids}};
    else if constexpr (std::is_same_v<T, RemoveHistoryRoots>) return {{"type", "remove_history_roots"}, {"name", value.name}};
    else return {{"type", "collect"}};
  }, command);
}
Json apply(Catalog& catalog, const char* label, const Command& command,
           const Snapshot& expected, std::vector<AssetId> imported = {}) {
  require(expected.revision > 0);
  const auto revision = expected.revision - 1;
  require(catalog.revision() == revision);
  std::sort(imported.begin(), imported.end());
  const Receipt expected_receipt{"committed", expected.revision, "", imported, 0, 0};
  const auto receipt = catalog.apply(command, revision);
  require(receipt == expected_receipt && catalog.snapshot() == expected);
  return {{"case", label}, {"command", command_json(command)}, {"expected_revision", revision},
    {"receipt", receipt_json(receipt)}, {"snapshot", snapshot_json(catalog.snapshot())}};
}
std::vector<std::string> ids(const Snapshot& state, bool blobs) {
  std::vector<std::string> out;
  if (blobs) for (const auto& b : state.blobs) out.push_back(b.hash);
  else for (const auto& a : state.assets) out.push_back(a.id);
  std::sort(out.begin(), out.end()); return out;
}
std::vector<std::string> removed(const Snapshot& before, const Snapshot& after, bool blobs) {
  const auto a = ids(before, blobs), b = ids(after, blobs); std::vector<std::string> out;
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out)); return out;
}
Json collect_prediction(const Snapshot& source, const Bytes& exported, const RetentionReport& prediction, bool verify) {
  // Reconstruct through ordinary commands. Import deliberately does NOT restore
  // source revision or roots; the new catalog has its own authoritative history.
  Catalog fresh; Json steps = Json::array();
  auto expected = source;
  expected.revision = 1; expected.current_roots.clear(); expected.history_roots.clear();
  steps.push_back(apply(fresh, "import", Import{exported}, expected, ids(source, false)));
  expected.revision = 2; expected.current_roots = source.current_roots;
  steps.push_back(apply(fresh, "current", SetCurrentRoots{source.current_roots}, expected));
  for (const auto& root : source.history_roots) {
    ++expected.revision; expected.history_roots.push_back(root);
    steps.push_back(apply(fresh, "history", SetHistoryRoots{root.name, root.asset_ids}, expected));
  }
  const auto before = fresh.snapshot(); const auto bytes_before = fresh.export_bundle();
  require(bytes_before == exported);
  const auto receipt = fresh.apply(Collect{}, expected.revision);
  const auto after = fresh.snapshot(); const auto removed_assets = removed(before, after, false);
  const auto removed_blobs = removed(before, after, true);
  if (verify) {
    ++expected.revision;
    std::erase_if(expected.assets, [&](const Asset& asset) {
      return std::find(prediction.removable_assets.begin(), prediction.removable_assets.end(), asset.id)
        != prediction.removable_assets.end();
    });
    std::erase_if(expected.blobs, [&](const Blob& blob) {
      return std::find(prediction.removable_blobs.begin(), prediction.removable_blobs.end(), blob.hash)
        != prediction.removable_blobs.end();
    });
    const Receipt expected_receipt{"committed", expected.revision, "", {},
      static_cast<std::uint32_t>(prediction.removable_assets.size()),
      static_cast<std::uint32_t>(prediction.removable_blobs.size())};
    require(receipt == expected_receipt && after == expected
      && removed_assets == prediction.removable_assets && removed_blobs == prediction.removable_blobs);
  } else require(receipt.status == "committed");
  return {{"steps", steps}, {"before", snapshot_json(before)}, {"export_before_hex", hex(bytes_before)},
    {"receipt", receipt_json(receipt)}, {"after", snapshot_json(after)}, {"export_after_hex", hex(fresh.export_bundle())},
    {"removed_assets", removed_assets}, {"removed_blobs", removed_blobs}};
}
Json query(Catalog& catalog, const char* label, std::uint64_t expected_revision,
           const RetentionResult& expected, bool verify) {
  const auto before = catalog.snapshot(); const auto exported = catalog.export_bundle();
  const auto result = catalog.inspect_retention(expected_revision);
  if (verify) require(result == expected);
  Json collection = nullptr;
  if (result.report) collection = collect_prediction(before, exported, *result.report, verify);
  const auto after = catalog.snapshot(); const auto after_export = catalog.export_bundle();
  require(before == after && exported == after_export);
  return {{"case", label}, {"expected_revision", expected_revision}, {"before", snapshot_json(before)},
    {"after", snapshot_json(after)}, {"export_before_hex", hex(exported)}, {"export_after_hex", hex(after_export)},
    {"result", result_json(result)}, {"collection", collection}};
}
Json detached(Catalog& catalog, const RetentionResult& expected, bool verify) {
  const auto before = catalog.snapshot(); const auto exported = catalog.export_bundle();
  auto result = catalog.inspect_retention(catalog.revision()); require(result.report.has_value());
  if (verify) require(result == expected);
  const auto original = result_json(result); const std::string changed(64, '0');
  result.status = "rejected"; result.code = "DETACHED_EDIT"; result.revision = 0;
  auto& r = *result.report; r.format_version = 0; r.current_roots = {changed};
  require(!r.history_roots.empty() && !r.assets.empty() && !r.blobs.empty());
  r.history_roots.front() = {"edited", {changed}};
  auto& a = r.assets.front(); a.id = changed; a.content_hash = changed;
  a.current_root = !a.current_root; a.history_roots = {"edited"};
  auto& b = r.blobs.front(); b.hash = changed; b.referencing_assets.clear(); b.retaining_assets = {changed};
  r.removable_assets = {changed}; r.removable_blobs = {changed};
  const auto fresh = catalog.inspect_retention(catalog.revision());
  const auto after = catalog.snapshot(); const auto after_export = catalog.export_bundle();
  require(before == after && exported == after_export && result_json(fresh) == original);
  return {{"original", original}, {"mutated", result_json(result)}, {"fresh", result_json(fresh)},
    {"before", snapshot_json(before)}, {"after", snapshot_json(after)},
    {"export_before_hex", hex(exported)}, {"export_after_hex", hex(after_export)}};
}
RetentionResult literal_result(std::uint64_t revision, RetentionReport report) {
  // Ordering only: callers supply every relationship and removal set literally.
  // This fixture does not recompute the inspector's reachability decision.
  std::sort(report.current_roots.begin(), report.current_roots.end());
  std::sort(report.history_roots.begin(), report.history_roots.end(),
    [](const auto& a, const auto& b) { return a.name < b.name; });
  for (auto& root : report.history_roots) std::sort(root.asset_ids.begin(), root.asset_ids.end());
  std::sort(report.assets.begin(), report.assets.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
  for (auto& asset : report.assets) std::sort(asset.history_roots.begin(), asset.history_roots.end());
  std::sort(report.blobs.begin(), report.blobs.end(), [](const auto& a, const auto& b) { return a.hash < b.hash; });
  for (auto& blob : report.blobs) {
    std::sort(blob.referencing_assets.begin(), blob.referencing_assets.end());
    std::sort(blob.retaining_assets.begin(), blob.retaining_assets.end());
  }
  std::sort(report.removable_assets.begin(), report.removable_assets.end());
  std::sort(report.removable_blobs.begin(), report.removable_blobs.end());
  return {"ok", "", revision, report};
}
} // namespace

int run_retention_example(int argc, char* argv[]) { try {
  bool example = false, headless = false, seed = false, verify = false, output = false;
  std::filesystem::path directory;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    const auto value = [&]() -> std::string_view { require(i + 1 < argc); return argv[++i]; };
    if (arg == "--example" && !example) { example = true; require(value() == "assets.explain_retention"); }
    else if (arg == "--headless" && !headless) headless = true;
    else if (arg == "--seed" && !seed) { seed = true; require(value() == "7"); }
    else if (arg == "--verify" && !verify) verify = true;
    else if (arg == "--output" && !output) { output = true; directory = value(); require(!directory.empty()); }
    else require(false);
  }
  require(example && headless && seed && output);
  if (!directory.parent_path().empty()) std::filesystem::create_directories(directory.parent_path());
  require(std::filesystem::create_directory(directory));
  const Bytes x{7, 7, 7, 14, 21, 21, 28, 35}, y{0, 7, 14, 21};
  const Manifest a{1, content_hash(x), 8, "application/octet-stream", "generated:seed7/a", "Apache-2.0"};
  auto b = a; b.source = "generated:seed7/a-alt";
  const Manifest c{1, content_hash(y), 4, "application/octet-stream", "generated:seed7/c", "Apache-2.0"};
  const auto aid = asset_id(a), bid = asset_id(b), cid = asset_id(c);
  const auto xid = content_hash(x), yid = content_hash(y);
  const auto imported = bundle({c, b, a}, {y, x});
  const auto state = [&](std::uint64_t revision, std::vector<AssetId> current,
                         std::vector<HistoryRoots> history) {
    Snapshot expected{1, revision, {{aid, a}, {bid, b}, {cid, c}}, {{xid, x}, {yid, y}}, current, history};
    std::sort(expected.assets.begin(), expected.assets.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(expected.blobs.begin(), expected.blobs.end(), [](const auto& left, const auto& right) { return left.hash < right.hash; });
    return expected;
  };
  // Literal expected graphs, independent of the Catalog's inspection traversal.
  // A and B have different manifest identities but share X; C alone references Y.
  const auto all_roots = literal_result(5, {1, {aid}, {{"alpha", {aid}}, {"beta", {bid}}, {"empty", {}}},
    {{aid, xid, true, {"alpha"}}, {bid, xid, false, {"beta"}}, {cid, yid, false, {}}},
    {{xid, {aid, bid}, {aid, bid}}, {yid, {cid}, {}}}, {cid}, {yid}});
  const auto history_only = literal_result(6, {1, {}, {{"alpha", {aid}}, {"beta", {bid}}, {"empty", {}}},
    {{aid, xid, false, {"alpha"}}, {bid, xid, false, {"beta"}}, {cid, yid, false, {}}},
    {{xid, {aid, bid}, {aid, bid}}, {yid, {cid}, {}}}, {cid}, {yid}});
  const auto shared_blob = literal_result(7, {1, {}, {{"beta", {bid}}, {"empty", {}}},
    {{aid, xid, false, {}}, {bid, xid, false, {"beta"}}, {cid, yid, false, {}}},
    {{xid, {aid, bid}, {bid}}, {yid, {cid}, {}}}, {aid, cid}, {yid}});
  const auto unrooted = literal_result(8, {1, {}, {{"empty", {}}},
    {{aid, xid, false, {}}, {bid, xid, false, {}}, {cid, yid, false, {}}},
    {{xid, {aid, bid}, {}}, {yid, {cid}, {}}}, {aid, bid, cid}, {xid, yid}});
  const auto restored = literal_result(9, {1, {aid}, {{"empty", {}}},
    {{aid, xid, true, {}}, {bid, xid, false, {}}, {cid, yid, false, {}}},
    {{xid, {aid, bid}, {aid}}, {yid, {cid}, {}}}, {bid, cid}, {yid}});
  Catalog catalog; Json commands = Json::array(), queries = Json::array();
  require(catalog.snapshot() == Snapshot{});
  queries.push_back(query(catalog, "empty", 0, literal_result(0, {}), verify));
  commands.push_back(apply(catalog, "import", Import{imported}, state(1, {}, {}), {aid, bid, cid}));
  commands.push_back(apply(catalog, "current-a", SetCurrentRoots{{aid}}, state(2, {aid}, {})));
  commands.push_back(apply(catalog, "alpha-a", SetHistoryRoots{"alpha", {aid}}, state(3, {aid}, {{"alpha", {aid}}})));
  commands.push_back(apply(catalog, "beta-b", SetHistoryRoots{"beta", {bid}}, state(4, {aid}, {{"alpha", {aid}}, {"beta", {bid}}})));
  commands.push_back(apply(catalog, "empty-history", SetHistoryRoots{"empty", {}},
    state(5, {aid}, {{"alpha", {aid}}, {"beta", {bid}}, {"empty", {}}})));
  queries.push_back(query(catalog, "all-roots", 5, all_roots, verify));
  const auto detached_proof = detached(catalog, all_roots, verify);
  commands.push_back(apply(catalog, "drop-current", SetCurrentRoots{{}}, state(6, {}, {{"alpha", {aid}}, {"beta", {bid}}, {"empty", {}}})));
  queries.push_back(query(catalog, "history-only", 6, history_only, verify));
  commands.push_back(apply(catalog, "drop-alpha", RemoveHistoryRoots{"alpha"}, state(7, {}, {{"beta", {bid}}, {"empty", {}}})));
  queries.push_back(query(catalog, "stale", 6, {"rejected", "REVISION_CONFLICT", 7, std::nullopt}, verify));
  queries.push_back(query(catalog, "shared-blob", 7, shared_blob, verify));
  commands.push_back(apply(catalog, "drop-beta", RemoveHistoryRoots{"beta"}, state(8, {}, {{"empty", {}}})));
  queries.push_back(query(catalog, "unrooted", 8, unrooted, verify));
  commands.push_back(apply(catalog, "restore-current", SetCurrentRoots{{aid}}, state(9, {aid}, {{"empty", {}}})));
  queries.push_back(query(catalog, "restored", 9, restored, verify));
  const Json report{{"schema_version", 1}, {"example", "assets.explain_retention"}, {"seed", 7}, {"verified", verify},
    {"fixture", {{"input_bundle_hex", hex(imported)}, {"A", aid}, {"B", bid}, {"C", cid},
                 {"X", content_hash(x)}, {"Y", content_hash(y)}}},
    {"commands", commands}, {"queries", queries}, {"detached", detached_proof}};
  std::ofstream stream(directory / "result.json.tmp", std::ios::binary); stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream << report.dump(2) << '\n'; stream.close();
  std::filesystem::rename(directory / "result.json.tmp", directory / "result.json");
  std::cout << "assets.explain_retention: bounded read-only inspection completed\n"; return 0;
} catch (...) {
  std::cerr << "assets.explain_retention failed: check fixture options and fresh writable output\n"; return 2;
} }
