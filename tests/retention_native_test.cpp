// SPDX-License-Identifier: Apache-2.0
#include "omniweft/assets.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Test-only allocation failure, armed exclusively around the const query.
// No production fault hook or Catalog internals are needed. MSVC iterator-debug
// bookkeeping allocates a proxy even in noexcept container constructors/moves.
// Exclude exactly that allocation size in Debug: the bounded fixture's report
// payload allocations are all larger, and failing the proxy would terminate
// inside the STL rather than exercise catchable report-allocation failure.
namespace allocation_probe {
bool armed = false, triggered = false;
std::size_t remaining = 0, bookkeeping_bypasses = 0;
#if defined(_MSVC_STL_VERSION) && _ITERATOR_DEBUG_LEVEL != 0
constexpr std::size_t bookkeeping_size = sizeof(std::_Container_proxy);
static_assert(bookkeeping_size == 2 * sizeof(void*));
static_assert(sizeof(ow::assets::AssetId) > bookkeeping_size);
static_assert(sizeof(ow::assets::HistoryRoots) > bookkeeping_size);
static_assert(sizeof(ow::assets::AssetRetention) > bookkeeping_size);
static_assert(sizeof(ow::assets::BlobRetention) > bookkeeping_size);
static_assert(64 + 1 > bookkeeping_size); // Every asset/blob digest string.
#else
constexpr std::size_t bookkeeping_size = 0;
#endif
void before_allocation(std::size_t size) {
  if (!armed) return;
  if constexpr (bookkeeping_size != 0) {
    if (size == bookkeeping_size) {
      ++bookkeeping_bypasses;
      return;
    }
  }
  if (remaining != 0) { --remaining; return; }
  armed = false;
  triggered = true;
  throw std::bad_alloc{};
}
struct Scope {
  explicit Scope(std::size_t after) {
    remaining = after;
    bookkeeping_bypasses = 0;
    triggered = false;
    armed = true;
  }
  ~Scope() { armed = false; }
  void stop() const { armed = false; }
};
}
void* operator new(std::size_t size) {
  allocation_probe::before_allocation(size);
  if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
using namespace ow::assets;
std::uint32_t assertions = 0;
class TestFailure final : public std::runtime_error {
 public:
  explicit TestFailure(const char* label) : std::runtime_error(label) {}
};
void check(bool condition, const char* label) {
  ++assertions;
  if (!condition) throw TestFailure(label);
}
template<class T> std::vector<T> sorted(std::vector<T> values) {
  std::sort(values.begin(), values.end());
  return values;
}
void put16(Bytes& out, std::size_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void put32(Bytes& out, std::size_t value) {
  for (unsigned i = 0; i < 4; ++i)
    out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void digest(Bytes& out, std::string_view value) {
  const auto nibble = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
  for (std::size_t i = 0; i < value.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>((nibble(value[i]) << 4) | nibble(value[i + 1])));
}
// Only fixture encoding uses manifest/hash helpers. Retention expectations
// below are authored from the fixture's explicit root relationships.
Bytes bundle(const std::vector<Asset>& assets, const std::vector<Blob>& blobs) {
  Bytes out{'O','W','A','S','B','0','0','1'};
  put32(out, 1); put32(out, assets.size()); put32(out, blobs.size());
  for (const auto& asset : assets) {
    const auto manifest = canonical_manifest(asset.manifest);
    digest(out, asset.id); put32(out, manifest.size());
    out.insert(out.end(), manifest.begin(), manifest.end());
  }
  for (const auto& blob : blobs) {
    const auto path = "blobs/" + blob.hash;
    put16(out, path.size()); out.insert(out.end(), path.begin(), path.end());
    out.push_back(0); put32(out, blob.bytes.size()); put32(out, blob.bytes.size());
    out.insert(out.end(), blob.bytes.begin(), blob.bytes.end());
  }
  return out;
}
Asset asset(const Blob& blob, std::string source) {
  Manifest manifest{1, blob.hash, static_cast<std::uint32_t>(blob.bytes.size()),
                    "application/octet-stream", std::move(source), "Apache-2.0"};
  return {asset_id(manifest), std::move(manifest)};
}
Receipt apply(Catalog& catalog, const Command& command) {
  const auto prior = catalog.revision();
  auto receipt = catalog.apply(command, prior);
  check(receipt.status == "committed" && receipt.code.empty(), "fixture command accepted");
  check(receipt.revision == prior + 1 && catalog.revision() == prior + 1,
        "fixture command advances revision once");
  return receipt;
}
RetentionReport ordered(RetentionReport report) {
  report.current_roots = sorted(std::move(report.current_roots));
  std::sort(report.history_roots.begin(), report.history_roots.end(),
            [](const auto& a, const auto& b) { return a.name < b.name; });
  for (auto& roots : report.history_roots) roots.asset_ids = sorted(std::move(roots.asset_ids));
  std::sort(report.assets.begin(), report.assets.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });
  for (auto& row : report.assets) row.history_roots = sorted(std::move(row.history_roots));
  std::sort(report.blobs.begin(), report.blobs.end(),
            [](const auto& a, const auto& b) { return a.hash < b.hash; });
  for (auto& row : report.blobs) {
    row.referencing_assets = sorted(std::move(row.referencing_assets));
    row.retaining_assets = sorted(std::move(row.retaining_assets));
  }
  report.removable_assets = sorted(std::move(report.removable_assets));
  report.removable_blobs = sorted(std::move(report.removable_blobs));
  return report;
}
RetentionResult inspect(const Catalog& catalog, const RetentionReport& expected) {
  const auto before = catalog.snapshot();
  const auto bytes = catalog.export_bundle();
  const auto result = catalog.inspect_retention(before.revision);
  check(result.status == "ok" && result.code.empty(), "query success status");
  check(result.revision == before.revision, "query revision binding");
  check(result.report.has_value(), "query complete report exists");
  check(*result.report == expected, "complete literal retention explanation");
  check(catalog.snapshot() == before, "query preserves full snapshot");
  check(catalog.export_bundle() == bytes, "query preserves export bytes");
  check(catalog.revision() == before.revision, "query does not advance revision");
  check(result == catalog.inspect_retention(before.revision), "repeat query deterministic");
  return result;
}
void stale(const Catalog& catalog, std::uint64_t expected_revision) {
  const auto before = catalog.snapshot();
  const auto bytes = catalog.export_bundle();
  const auto result = catalog.inspect_retention(expected_revision);
  check(result.status == "rejected" && result.code == "REVISION_CONFLICT",
        "stale query rejects");
  check(result.revision == before.revision && !result.report, "stale has actual revision and no rows");
  check(catalog.snapshot() == before && catalog.export_bundle() == bytes,
        "stale preserves complete catalog");
  // Staleness must win before any report allocation, even with allocation zero armed.
  allocation_probe::Scope failure(0);
  const auto no_allocation = catalog.inspect_retention(expected_revision);
  failure.stop();
  check(!allocation_probe::triggered, "stale query allocates no rows");
  check(no_allocation == result, "stale result independent of allocation availability");
}
template<class Row, class Key>
std::vector<std::string> removed(const std::vector<Row>& before,
                                 const std::vector<Row>& after, Key key) {
  std::vector<std::string> result;
  for (const auto& row : before) {
    const auto found = std::find_if(after.begin(), after.end(),
        [&](const auto& other) { return key(row) == key(other); });
    if (found == after.end()) result.push_back(key(row));
  }
  return sorted(std::move(result));
}
void prediction(const Catalog& original) {
  const auto original_state = original.snapshot();
  const auto original_bytes = original.export_bundle();
  Catalog rebuilt;
  apply(rebuilt, Import{original_bytes});
  apply(rebuilt, SetCurrentRoots{original_state.current_roots});
  for (const auto& roots : original_state.history_roots)
    apply(rebuilt, SetHistoryRoots{roots.name, roots.asset_ids});
  const auto before = rebuilt.snapshot();
  check(before.assets == original_state.assets && before.blobs == original_state.blobs &&
        before.current_roots == original_state.current_roots &&
        before.history_roots == original_state.history_roots, "ordinary commands reconstruct content and roots");
  const auto predicted = rebuilt.inspect_retention(rebuilt.revision());
  check(predicted.status == "ok" && predicted.report.has_value(), "reconstructed query succeeds");
  const auto receipt = apply(rebuilt, Collect{});
  const auto after = rebuilt.snapshot();
  const auto assets_removed = removed(before.assets, after.assets, [](const auto& row) { return row.id; });
  const auto blobs_removed = removed(before.blobs, after.blobs, [](const auto& row) { return row.hash; });
  check(assets_removed == predicted.report->removable_assets, "prediction matches actual manifest removal");
  check(blobs_removed == predicted.report->removable_blobs, "prediction matches actual blob removal");
  check(receipt.manifests_removed == assets_removed.size() &&
        receipt.blobs_removed == blobs_removed.size(), "collection receipt matches independent set difference");
  check(after.current_roots == before.current_roots && after.history_roots == before.history_roots,
        "collection preserves root sets");
  check(original.snapshot() == original_state && original.export_bundle() == original_bytes,
        "private collection leaves original unchanged");
}
void allocation_failures(const Catalog& catalog, const RetentionReport& expected) {
  const auto before = catalog.snapshot();
  const auto bytes = catalog.export_bundle();
  // Verified pinned MSVC small-string storage holds these fixture names;
  // therefore they never add a proxy-sized character allocation to the sweep.
  check(std::all_of(before.history_roots.begin(), before.history_roots.end(),
          [](const auto& roots) { return roots.name.size() <= 7; }),
        "allocation fixture history names fit small-string storage");
  std::size_t failures = 0;
  bool reached_success = false;
  // Every catchable report-payload allocation is failed in turn, excluding
  // only the proven MSVC noexcept iterator-debug bookkeeping above. This
  // covers failures after partial roots/asset/blob rows were staged.
  for (std::size_t index = 0; index < 1024; ++index) {
    allocation_probe::Scope failure(index);
    const auto result = catalog.inspect_retention(before.revision);
    failure.stop();
    if (!allocation_probe::triggered) {
      check(result.status == "ok" && result.report && *result.report == expected,
            "allocation sweep reaches complete successful report");
      check(allocation_probe::bookkeeping_size == 0
                ? allocation_probe::bookkeeping_bypasses == 0
                : allocation_probe::bookkeeping_bypasses > 0,
            "allocation sweep observes only configured debug bookkeeping bypass");
      reached_success = true;
      break;
    }
    ++failures;
    check(result.status == "rejected" && result.code == "RESOURCE_EXHAUSTED",
          "allocation failure has fixed rejection");
    check(!result.report && result.revision == before.revision,
          "allocation failure publishes no partial report");
    check(catalog.snapshot() == before && catalog.export_bundle() == bytes,
          "allocation failure leaves catalog byte-for-byte unchanged");
  }
  check(reached_success && failures > 16, "allocation sweep covers staged report construction");
  inspect(catalog, expected);
}
void empty_catalog() {
  Catalog catalog;
  inspect(catalog, RetentionReport{});
  stale(catalog, 1);
  apply(catalog, SetHistoryRoots{"empty", {}});
  RetentionReport expected;
  expected.history_roots = {{"empty", {}}};
  inspect(catalog, expected);
  prediction(catalog);
  const auto before = catalog.revision();
  apply(catalog, RemoveHistoryRoots{"absent"});
  check(catalog.revision() == before + 1, "existing no-op mutation semantics unchanged");
  inspect(catalog, expected);
}
void shared_content() {
  const Blob x{content_hash(Bytes{7,7,14}), {7,7,14}};
  const Blob y{content_hash(Bytes{0,21}), {0,21}};
  const auto a = asset(x, "generated:retention/a");
  const auto b = asset(x, "generated:retention/a-alternate");
  const auto c = asset(y, "generated:retention/c");
  check(a.id != b.id && a.manifest.content_hash == b.manifest.content_hash,
        "distinct provenance fixture shares content");
  Catalog catalog;
  apply(catalog, Import{bundle({c,b,a}, {y,x})});
  RetentionReport expected;
  expected.assets = {{a.id,x.hash,false,{}}, {b.id,x.hash,false,{}}, {c.id,y.hash,false,{}}};
  expected.blobs = {{x.hash,{a.id,b.id},{}}, {y.hash,{c.id},{}}};
  expected.removable_assets = {a.id,b.id,c.id};
  expected.removable_blobs = {x.hash,y.hash};
  inspect(catalog, ordered(expected));
  prediction(catalog);

  apply(catalog, SetCurrentRoots{{a.id}});
  apply(catalog, SetHistoryRoots{"zeta", {b.id,a.id}});
  apply(catalog, SetHistoryRoots{"empty", {}});
  apply(catalog, SetHistoryRoots{"alpha", {a.id}});
  // A history set named current remains distinct from the current-root flag.
  apply(catalog, SetHistoryRoots{"current", {b.id}});
  expected.current_roots = {a.id};
  expected.history_roots = {{"zeta",{b.id,a.id}}, {"empty",{}}, {"alpha",{a.id}}, {"current",{b.id}}};
  expected.assets = {{a.id,x.hash,true,{"alpha","zeta"}},
                     {b.id,x.hash,false,{"current","zeta"}}, {c.id,y.hash,false,{}}};
  expected.blobs = {{x.hash,{a.id,b.id},{a.id,b.id}}, {y.hash,{c.id},{}}};
  expected.removable_assets = {c.id}; expected.removable_blobs = {y.hash};
  auto baseline = inspect(catalog, ordered(expected));
  prediction(catalog);
  allocation_failures(catalog, ordered(expected));
  auto detached = baseline;
  detached.report->current_roots.clear();
  detached.report->history_roots.front().name = "changed";
  detached.report->history_roots.front().asset_ids.clear();
  detached.report->assets.front().id = "changed";
  detached.report->assets.front().content_hash = "changed";
  detached.report->assets.front().current_root = !detached.report->assets.front().current_root;
  detached.report->assets.front().history_roots = {"changed"};
  detached.report->blobs.front().hash = "changed";
  detached.report->blobs.front().referencing_assets.clear();
  detached.report->blobs.front().retaining_assets.clear();
  detached.report->removable_assets.clear(); detached.report->removable_blobs.clear();
  check(detached != baseline, "detached report independently mutable");
  check(catalog.inspect_retention(catalog.revision()) == baseline, "editing report cannot edit catalog");
  stale(catalog, catalog.revision() - 1);
  stale(catalog, catalog.revision() + 1);
  stale(catalog, std::numeric_limits<std::uint64_t>::max());

  const auto old_revision = catalog.revision();
  apply(catalog, SetCurrentRoots{{}});
  apply(catalog, RemoveHistoryRoots{"alpha"});
  apply(catalog, SetHistoryRoots{"zeta", {b.id}});
  expected.current_roots.clear();
  expected.history_roots = {{"zeta",{b.id}}, {"empty",{}}, {"current",{b.id}}};
  expected.assets = {{a.id,x.hash,false,{}}, {b.id,x.hash,false,{"current","zeta"}},
                     {c.id,y.hash,false,{}}};
  expected.blobs = {{x.hash,{a.id,b.id},{b.id}}, {y.hash,{c.id},{}}};
  expected.removable_assets = {a.id,c.id}; expected.removable_blobs = {y.hash};
  stale(catalog, old_revision);
  inspect(catalog, ordered(expected));
  check(baseline.report->current_roots == std::vector<AssetId>{a.id},
        "retained old report unaffected by later root changes");
  prediction(catalog);

  apply(catalog, SetHistoryRoots{"zeta", {}});
  apply(catalog, RemoveHistoryRoots{"current"});
  expected.history_roots = {{"zeta",{}}, {"empty",{}}};
  expected.assets = {{a.id,x.hash,false,{}}, {b.id,x.hash,false,{}}, {c.id,y.hash,false,{}}};
  expected.blobs = {{x.hash,{a.id,b.id},{}}, {y.hash,{c.id},{}}};
  expected.removable_assets = {a.id,b.id,c.id}; expected.removable_blobs = {x.hash,y.hash};
  inspect(catalog, ordered(expected)); prediction(catalog);
  apply(catalog, SetCurrentRoots{{c.id,b.id,a.id}});
  expected.current_roots = {c.id,b.id,a.id};
  expected.assets = {{a.id,x.hash,true,{}}, {b.id,x.hash,true,{}}, {c.id,y.hash,true,{}}};
  expected.blobs = {{x.hash,{a.id,b.id},{a.id,b.id}}, {y.hash,{c.id},{c.id}}};
  expected.removable_assets.clear(); expected.removable_blobs.clear();
  inspect(catalog, ordered(expected)); prediction(catalog);
}
void full_capacity() {
  std::vector<Asset> assets;
  std::vector<Blob> blobs;
  std::vector<AssetId> ids;
  RetentionReport expected;
  for (std::uint8_t i = 0; i < 8; ++i) {
    const Bytes data{i,7};
    blobs.push_back({content_hash(data), data});
    assets.push_back(asset(blobs.back(), "generated:capacity/" + std::to_string(i)));
    ids.push_back(assets.back().id);
    expected.assets.push_back({ids.back(),blobs.back().hash,true,{"alpha","bravo","current","zeta"}});
    expected.blobs.push_back({blobs.back().hash,{ids.back()},{ids.back()}});
  }
  Catalog catalog;
  apply(catalog, Import{bundle(assets,blobs)});
  std::reverse(ids.begin(), ids.end());
  apply(catalog, SetCurrentRoots{ids});
  for (const auto* name : {"zeta","current","bravo","alpha"})
    apply(catalog, SetHistoryRoots{name, ids});
  expected.current_roots = ids;
  expected.history_roots = {{"zeta",ids},{"current",ids},{"bravo",ids},{"alpha",ids}};
  inspect(catalog, ordered(expected));
  prediction(catalog);
  allocation_failures(catalog, ordered(expected));
  apply(catalog, SetCurrentRoots{{}});
  expected.current_roots.clear();
  for (auto& row : expected.assets) row.current_root = false;
  inspect(catalog, ordered(expected));
  prediction(catalog);
}
void eight_shared_variants() {
  const Blob blob{content_hash(Bytes{7,14,21}), {7,14,21}};
  std::vector<Asset> assets;
  std::vector<AssetId> ids;
  RetentionReport expected;
  for (std::uint8_t i = 0; i < 8; ++i) {
    assets.push_back(asset(blob, "generated:shared/" + std::to_string(i)));
    ids.push_back(assets.back().id);
    const bool in_history = i == 3 || i == 7;
    expected.assets.push_back({ids.back(), blob.hash, i == 0,
        in_history ? std::vector<std::string>{"keep"} : std::vector<std::string>{}});
    if (i != 0 && !in_history) expected.removable_assets.push_back(ids.back());
  }
  Catalog catalog;
  apply(catalog, Import{bundle(assets, {blob})});
  apply(catalog, SetCurrentRoots{{ids[0]}});
  apply(catalog, SetHistoryRoots{"keep", {ids[7],ids[3]}});
  expected.current_roots = {ids[0]};
  expected.history_roots = {{"keep", {ids[7],ids[3]}}};
  expected.blobs = {{blob.hash, ids, {ids[0],ids[3],ids[7]}}};
  inspect(catalog, ordered(expected));
  prediction(catalog);

  apply(catalog, SetCurrentRoots{{}});
  apply(catalog, RemoveHistoryRoots{"keep"});
  expected.current_roots.clear(); expected.history_roots.clear();
  for (auto& row : expected.assets) { row.current_root = false; row.history_roots.clear(); }
  expected.blobs = {{blob.hash, ids, {}}};
  expected.removable_assets = ids; expected.removable_blobs = {blob.hash};
  inspect(catalog, ordered(expected));
  prediction(catalog);
}
void rejected_command(Catalog& catalog, const Command& command, std::string_view code) {
  const auto before = catalog.snapshot();
  const auto bytes = catalog.export_bundle();
  const auto report = catalog.inspect_retention(before.revision);
  const auto receipt = catalog.apply(command, before.revision);
  check(receipt.status == "rejected" && receipt.code == code, "mutation rejects with expected code");
  check(receipt.revision == before.revision && catalog.revision() == before.revision,
        "rejected mutation preserves revision");
  check(receipt.imported.empty() && receipt.manifests_removed == 0 && receipt.blobs_removed == 0,
        "rejected mutation publishes no partial receipt");
  check(catalog.snapshot() == before, "rejected mutation preserves complete snapshot");
  check(catalog.export_bundle() == bytes, "rejected mutation preserves export bytes");
  check(catalog.inspect_retention(before.revision) == report,
        "rejected mutation preserves complete retention report");
}
void rejected_mutation_recovery() {
  const Blob x{content_hash(Bytes{7,28}), {7,28}};
  const Blob y{content_hash(Bytes{14,35}), {14,35}};
  const auto a = asset(x, "generated:recovery/a");
  const auto b = asset(y, "generated:recovery/b");
  Catalog catalog;
  apply(catalog, Import{bundle({a}, {x})});
  apply(catalog, SetCurrentRoots{{a.id}});
  apply(catalog, SetHistoryRoots{"keep", {a.id}});
  RetentionReport expected;
  expected.current_roots = {a.id}; expected.history_roots = {{"keep",{a.id}}};
  expected.assets = {{a.id,x.hash,true,{"keep"}}};
  expected.blobs = {{x.hash,{a.id},{a.id}}};
  inspect(catalog, ordered(expected));

  auto damaged = bundle({b}, {y});
  damaged.back() ^= 1;
  rejected_command(catalog, Import{std::move(damaged)}, "HASH_MISMATCH");
  apply(catalog, Import{bundle({b}, {y})});
  expected.assets.push_back({b.id,y.hash,false,{}});
  expected.blobs.push_back({y.hash,{b.id},{}});
  expected.removable_assets = {b.id}; expected.removable_blobs = {y.hash};
  inspect(catalog, ordered(expected));

  const AssetId unknown(64, 'f');
  rejected_command(catalog, SetCurrentRoots{{a.id,b.id,unknown}}, "UNKNOWN_ASSET");
  apply(catalog, SetCurrentRoots{{b.id}});
  expected.current_roots = {b.id};
  expected.assets = {{a.id,x.hash,false,{"keep"}}, {b.id,y.hash,true,{}}};
  expected.blobs = {{x.hash,{a.id},{a.id}}, {y.hash,{b.id},{b.id}}};
  expected.removable_assets.clear(); expected.removable_blobs.clear();
  inspect(catalog, ordered(expected));

  rejected_command(catalog, SetHistoryRoots{"keep", {b.id,unknown}}, "UNKNOWN_ASSET");
  apply(catalog, SetHistoryRoots{"keep", {b.id}});
  expected.history_roots = {{"keep",{b.id}}};
  expected.assets = {{a.id,x.hash,false,{}}, {b.id,y.hash,true,{"keep"}}};
  expected.blobs = {{x.hash,{a.id},{}}, {y.hash,{b.id},{b.id}}};
  expected.removable_assets = {a.id}; expected.removable_blobs = {x.hash};
  inspect(catalog, ordered(expected));
  prediction(catalog);
}
}
int main() {
  // A future noexcept failure must be a bounded test failure, never an abort
  // dialog that makes a headless test wait until its external timeout.
  std::set_terminate([] {
    std::fputs("retention oracle failed: unexpected termination\n", stderr);
    std::_Exit(1);
  });
  try {
    empty_catalog(); shared_content(); full_capacity();
    eight_shared_variants(); rejected_mutation_recovery();
    std::cout << "{\"status\":\"passed\",\"assertions\":" << assertions << "}\n";
    return 0;
  } catch (const TestFailure& error) {
    std::cerr << "retention oracle failed: " << error.what() << '\n';
  } catch (...) {
    std::cerr << "retention oracle failed: unexpected exception\n";
  }
  return 1;
}
