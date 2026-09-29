// SPDX-License-Identifier: Apache-2.0
#include "omniweft/assets.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace ow::assets;
std::uint32_t assertions = 0;
class TestFailure final : public std::runtime_error {
 public:
  explicit TestFailure(const char* text) : std::runtime_error(text) {}
};
void check(bool condition, const char* label) {
  ++assertions;
  if (!condition) throw TestFailure(label);
}
void put16(Bytes& out, std::size_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void put32(Bytes& out, std::uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void put_text(Bytes& out, std::string_view text) {
  put16(out, text.size()); out.insert(out.end(), text.begin(), text.end());
}
Bytes unhex(std::string_view text) {
  const auto nibble = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
  Bytes result;
  for (std::size_t i = 0; i < text.size(); i += 2)
    result.push_back(static_cast<std::uint8_t>((nibble(text[i]) << 4) | nibble(text[i + 1])));
  return result;
}
// Separately written encoder: tests submit bytes without using catalog export.
Bytes manifest_wire(const Manifest& value) {
  Bytes bytes{'O','W','A','M','N','F','0','1'};
  put32(bytes, value.format_version);
  const auto digest = unhex(value.content_hash);
  bytes.insert(bytes.end(), digest.begin(), digest.end());
  put32(bytes, value.decoded_length);
  put_text(bytes, value.media_type); put_text(bytes, value.source); put_text(bytes, value.license);
  return bytes;
}
struct WireAsset { Bytes manifest; std::string id; };
struct WireBlob { std::string path; std::uint8_t codec; std::uint32_t decoded; Bytes encoded; };
WireAsset wire_asset(const Manifest& value) {
  auto bytes = manifest_wire(value);
  return {bytes, content_hash(bytes)};
}
WireBlob raw(const Bytes& bytes) {
  return {"blobs/" + content_hash(bytes), 0, static_cast<std::uint32_t>(bytes.size()), bytes};
}
Bytes bundle(const std::vector<WireAsset>& assets, const std::vector<WireBlob>& blobs) {
  Bytes out{'O','W','A','S','B','0','0','1'};
  put32(out, 1);
  put32(out, static_cast<std::uint32_t>(assets.size()));
  put32(out, static_cast<std::uint32_t>(blobs.size()));
  for (const auto& asset : assets) {
    const auto id = unhex(asset.id);
    out.insert(out.end(), id.begin(), id.end());
    put32(out, static_cast<std::uint32_t>(asset.manifest.size()));
    out.insert(out.end(), asset.manifest.begin(), asset.manifest.end());
  }
  for (const auto& blob : blobs) {
    put_text(out, blob.path); out.push_back(blob.codec);
    put32(out, blob.decoded); put32(out, static_cast<std::uint32_t>(blob.encoded.size()));
    out.insert(out.end(), blob.encoded.begin(), blob.encoded.end());
  }
  return out;
}
Manifest manifest(const Bytes& bytes, std::string source = "generated:seed7/a") {
  return {1, content_hash(bytes), static_cast<std::uint32_t>(bytes.size()),
          "application/octet-stream", std::move(source), "Apache-2.0"};
}
Receipt apply(Catalog& catalog, const Command& command) {
  const auto revision = catalog.revision();
  auto receipt = catalog.apply(command, revision);
  check(receipt.status == "committed" && receipt.code.empty(), "valid command commits");
  check(receipt.revision == revision + 1 && catalog.revision() == revision + 1, "success increments once");
  return receipt;
}
void reject(Catalog& catalog, const Command& command, std::string_view code,
            std::uint64_t expected = std::numeric_limits<std::uint64_t>::max()) {
  const auto before = catalog.snapshot();
  const auto exported = catalog.export_bundle();
  const auto receipt = catalog.apply(command,
      expected == std::numeric_limits<std::uint64_t>::max() ? catalog.revision() : expected);
  check(receipt.status == "rejected" && receipt.code == code, "fixed rejection code");
  check(receipt.revision == before.revision && receipt.imported.empty()
      && receipt.manifests_removed == 0 && receipt.blobs_removed == 0, "rejection receipt has no partial effects");
  check(catalog.snapshot() == before, "complete rejection atomicity");
  check(catalog.export_bundle() == exported, "rejection preserves canonical export");
}
template<class F>
void fails(F&& call, std::string_view code) {
  try { call(); }
  catch (const Failure& error) { check(error.code() == code, "fixed helper failure"); return; }
  throw TestFailure("helper accepted invalid value");
}
const Bytes a{7,7,7,14,21,21,28,35}, c{0,7,14,21};

void identity_and_collection() {
  static_assert(!std::is_copy_constructible_v<Catalog> && !std::is_move_constructible_v<Catalog>);
  check(content_hash({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "empty SHA256 golden");
  check(content_hash(Bytes{'a','b','c'}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc SHA256 golden");
  const auto ma = manifest(a), mb = manifest(a, "generated:seed7/a-alt"), mc = manifest(c, "generated:seed7/c");
  check(ma.content_hash == "edea910f6048c9757f972ea0e46581aefd833083e86fc52acbfe24ba7508b575", "fixture blob SHA256 golden");
  check(asset_id(ma) == "189d1ca58461831aaf8b25ea7cbf2354457319cb3ca5c9d74723959deab18947", "fixture AssetId golden");
  const auto golden = unhex("4f57414d4e46303101000000edea910f6048c9757f972ea0e46581aefd833083e86fc52acbfe24ba7508b5750800000018006170706c69636174696f6e2f6f637465742d73747265616d110067656e6572617465643a73656564372f610a004170616368652d322e30");
  check(canonical_manifest(ma) == golden, "canonical manifest literal bytes");
  check(asset_id(ma) != asset_id(mb) && ma.content_hash == mb.content_hash, "provenance binds identity");
  const auto original = bundle({wire_asset(mc), wire_asset(mb), wire_asset(ma)}, {raw(c), raw(a)});
  Catalog catalog;
  check(catalog.snapshot() == Snapshot{}, "initial complete catalog");
  const auto imported = apply(catalog, Import{original});
  check(imported.imported.size() == 3 && std::is_sorted(imported.imported.begin(), imported.imported.end()), "sorted imported IDs");
  check(catalog.snapshot().assets.size() == 3 && catalog.snapshot().blobs.size() == 2, "shared blob storage deduplication");
  auto detached = catalog.snapshot();
  detached.assets[0].manifest.source = "modified"; detached.blobs[0].bytes.clear(); detached.revision = 99;
  check(catalog.snapshot() != detached && catalog.revision() == 1, "detached full snapshot");
  const auto first_export = catalog.export_bundle();
  const auto dedup = apply(catalog, Import{original});
  check(dedup.imported == imported.imported && catalog.export_bundle() == first_export, "no-op import increments without duplicate storage");
  apply(catalog, SetCurrentRoots{{asset_id(ma)}});
  apply(catalog, SetHistoryRoots{"undo.7", {asset_id(mb)}});
  auto receipt = apply(catalog, Collect{});
  check(receipt.manifests_removed == 1 && receipt.blobs_removed == 1, "collect unrooted manifest and blob");
  apply(catalog, SetCurrentRoots{{}});
  receipt = apply(catalog, Collect{});
  check(receipt.manifests_removed == 1 && receipt.blobs_removed == 0, "history transitively retains shared blob");
  check(catalog.snapshot().assets[0].id == asset_id(mb) && catalog.snapshot().blobs[0].bytes == a, "retained exact manifest and content");
  apply(catalog, RemoveHistoryRoots{"undo.7"});
  receipt = apply(catalog, Collect{});
  check(receipt.manifests_removed == 1 && receipt.blobs_removed == 1, "released history permits final collection");
  apply(catalog, RemoveHistoryRoots{"absent"});
  receipt = apply(catalog, Collect{});
  check(receipt.manifests_removed == 0 && receipt.blobs_removed == 0, "no-op collection receipt");
  Catalog restored;
  apply(restored, Import{first_export});
  check(restored.export_bundle() == first_export, "deterministic raw export round trip");
  check(restored.snapshot().current_roots.empty() && restored.snapshot().history_roots.empty()
      && restored.revision() == 1, "bundles carry no roots or revision authority");
  Catalog empty;
  check(empty.export_bundle() == bundle({}, {}), "empty bundle literal layout");
  apply(empty, Import{empty.export_bundle()});
}

void invalid_packages() {
  Catalog catalog;
  const auto ma = manifest(a), mc = manifest(c, "generated:seed7/c");
  const auto wa = wire_asset(ma), wc = wire_asset(mc);
  const auto good = bundle({wa}, {raw(a)});
  apply(catalog, Import{good});
  reject(catalog, Collect{}, "REVISION_CONFLICT", 0);
  reject(catalog, Import{bundle({wa}, {})}, "MISSING_BLOB");
  reject(catalog, Import{bundle({}, {raw(a)})}, "EXTRA_BLOB");
  reject(catalog, Import{bundle({wa}, {raw(a), raw(c)})}, "EXTRA_BLOB");
  reject(catalog, Import{bundle({wa, wa}, {raw(a)})}, "DUPLICATE_ENTRY");
  reject(catalog, Import{bundle({wa}, {raw(a), raw(a)})}, "DUPLICATE_ENTRY");
  auto corrupted = raw(a); corrupted.encoded[0] ^= 1;
  reject(catalog, Import{bundle({wa}, {corrupted})}, "HASH_MISMATCH");
  auto wrong_id = wa; wrong_id.id[0] = wrong_id.id[0] == '0' ? '1' : '0';
  reject(catalog, Import{bundle({wrong_id}, {raw(a)})}, "HASH_MISMATCH");
  auto raw_length = raw(a); --raw_length.decoded;
  reject(catalog, Import{bundle({wa}, {raw_length})}, "SIZE_MISMATCH");
  auto mismatched = ma; ++mismatched.decoded_length;
  reject(catalog, Import{bundle({wire_asset(mismatched)}, {raw(a)})}, "SIZE_MISMATCH");
  for (const auto& path : std::vector<std::string>{"../escape", "blobs/../escape", "C:/escape", "/absolute", "blobs\\" + ma.content_hash,
        "blobs/" + std::string(64, 'A'), "blobs/" + ma.content_hash + "/extra", std::string("blobs/\0", 7) + ma.content_hash}) {
    auto blob = raw(a); blob.path = path;
    reject(catalog, Import{bundle({wa}, {blob})}, "INVALID_PATH");
  }
  auto bad_codec = raw(a); bad_codec.codec = 2;
  reject(catalog, Import{bundle({wa}, {bad_codec})}, "INVALID_BUNDLE");
  auto oversized = raw(a); oversized.decoded = 65537;
  reject(catalog, Import{bundle({wa}, {oversized})}, "BUDGET_EXCEEDED");
  auto bad = good; bad[8] = 2;
  reject(catalog, Import{bad}, "UNSUPPORTED_VERSION");
  bad = good; bad[0] = 'X';
  reject(catalog, Import{bad}, "INVALID_BUNDLE");
  bad = good; bad.push_back(0);
  reject(catalog, Import{bad}, "INVALID_BUNDLE");
  bad = good; bad[12] = 9;
  reject(catalog, Import{bad}, "BUDGET_EXCEEDED");
  bad = good; bad[16] = 9;
  reject(catalog, Import{bad}, "BUDGET_EXCEEDED");
  bad = good; bad[52] = 255; bad[53] = 255; bad[54] = 255; bad[55] = 255;
  reject(catalog, Import{bad}, "INVALID_MANIFEST");
  reject(catalog, Import{Bytes(bundle_byte_limit + 1, 0)}, "BUDGET_EXCEEDED");
  auto invalid_manifest = ma; invalid_manifest.format_version = 2;
  reject(catalog, Import{bundle({wire_asset(invalid_manifest)}, {raw(a)})}, "UNSUPPORTED_VERSION");
  invalid_manifest = ma; invalid_manifest.source = "bad\nsource";
  reject(catalog, Import{bundle({wire_asset(invalid_manifest)}, {raw(a)})}, "INVALID_MANIFEST");
  invalid_manifest = ma; invalid_manifest.license.clear();
  reject(catalog, Import{bundle({wire_asset(invalid_manifest)}, {raw(a)})}, "INVALID_MANIFEST");
  invalid_manifest = ma; invalid_manifest.decoded_length = 65537;
  reject(catalog, Import{bundle({wire_asset(invalid_manifest)}, {raw(a)})}, "BUDGET_EXCEEDED");
  // A valid prefix followed by an invalid second manifest may not publish anything.
  auto broken_second = wc; broken_second.id[0] = broken_second.id[0] == '0' ? '1' : '0';
  reject(catalog, Import{bundle({wa, broken_second}, {raw(a), raw(c)})}, "HASH_MISMATCH");
  // Every truncated byte position is rejected with identical state/export.
  for (std::size_t cut = 0; cut < good.size(); ++cut) {
    const auto before = catalog.snapshot();
    const auto exported = catalog.export_bundle();
    const auto receipt = catalog.apply(Import{Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(cut))}, catalog.revision());
    check(receipt.status == "rejected" && !receipt.code.empty(), "all truncations reject");
    check(catalog.snapshot() == before && catalog.export_bundle() == exported, "truncation preserves all state");
  }
  apply(catalog, Import{bundle({wc}, {raw(c)})});
  check(catalog.snapshot().assets.size() == 2, "valid recovery after all failed imports");
}

void decompression() {
  Catalog catalog;
  const auto wa = wire_asset(manifest(a));
  auto compressed = raw(a); compressed.codec = 1; compressed.encoded = {3,7,1,14,2,21,1,28,1,35};
  apply(catalog, Import{bundle({wa}, {compressed})});
  check(catalog.snapshot().blobs[0].bytes == a, "RLE8 exact decode");
  Catalog raw_copy; apply(raw_copy, Import{bundle({wa}, {raw(a)})});
  check(raw_copy.export_bundle() == catalog.export_bundle(), "RLE imports export canonical raw");
  for (const auto& encoded : std::vector<Bytes>{{0,7}, {9,7}, {1}, {1,7,0,8}, {255,7}}) {
    auto wrong = compressed; wrong.encoded = encoded;
    reject(catalog, Import{bundle({wa}, {wrong})}, "DECOMPRESSION_ERROR");
  }
  auto short_output = compressed; short_output.encoded = {7,7};
  reject(catalog, Import{bundle({wa}, {short_output})}, "SIZE_MISMATCH");
  auto wrong_hash = compressed; wrong_hash.encoded = {8,7};
  reject(catalog, Import{bundle({wa}, {wrong_hash})}, "HASH_MISMATCH");
  auto too_long = compressed; too_long.encoded = Bytes(131073, 1);
  reject(catalog, Import{bundle({wa}, {too_long})}, "BUDGET_EXCEEDED");
  const Bytes large(65536, 7);
  auto large_rle = raw(large); large_rle.codec = 1; large_rle.encoded.clear();
  for (std::size_t count = 0; count < large.size(); ++count) {
    large_rle.encoded.push_back(1); large_rle.encoded.push_back(7);
  }
  apply(catalog, Import{bundle({wire_asset(manifest(large))}, {large_rle})});
  check(catalog.snapshot().blobs.size() == 2, "max decoded and max encoded RLE accepted");
  Catalog empty;
  auto empty_rle = raw({}); empty_rle.codec = 1;
  apply(empty, Import{bundle({wire_asset(manifest({}))}, {empty_rle})});
  check(empty.snapshot().blobs[0].bytes.empty(), "empty raw content and RLE stream accepted");
}

void caps_and_roots() {
  Catalog catalog;
  std::vector<WireAsset> manifests;
  std::vector<AssetId> ids;
  for (unsigned i = 0; i < 8; ++i) {
    const auto value = manifest(a, "generated:seed7/" + std::to_string(i));
    manifests.push_back(wire_asset(value)); ids.push_back(asset_id(value));
  }
  apply(catalog, Import{bundle(manifests, {raw(a)})});
  apply(catalog, Import{bundle(manifests, {raw(a)})});
  const auto ninth = wire_asset(manifest(a, "generated:seed7/ninth"));
  reject(catalog, Import{bundle({ninth}, {raw(a)})}, "BUDGET_EXCEEDED");
  std::reverse(ids.begin(), ids.end());
  apply(catalog, SetCurrentRoots{ids});
  const auto rooted = catalog.snapshot();
  check(std::is_sorted(rooted.current_roots.begin(), rooted.current_roots.end()), "root normalization order");
  auto too_many = ids; too_many.push_back(ids[0]);
  reject(catalog, SetCurrentRoots{too_many}, "BUDGET_EXCEEDED");
  reject(catalog, SetCurrentRoots{{ids[0], ids[0]}}, "DUPLICATE_ENTRY");
  reject(catalog, SetCurrentRoots{{std::string(64, '0')}}, "UNKNOWN_ASSET");
  reject(catalog, SetCurrentRoots{{"../bad"}}, "INVALID_ROOT");
  for (const auto& name : std::vector<std::string>{"", "1bad", "../bad", "x/y", std::string(33, 'x'), "bad\n"}) {
    reject(catalog, SetHistoryRoots{name, {}}, "INVALID_ROOT");
    reject(catalog, RemoveHistoryRoots{name}, "INVALID_ROOT");
  }
  for (const auto& name : {"z", "a", "c", "b"}) apply(catalog, SetHistoryRoots{name, ids});
  auto snap = catalog.snapshot();
  check(snap.history_roots[0].name == "a" && snap.history_roots[3].name == "z", "named roots sorted");
  reject(catalog, SetHistoryRoots{"fifth", {}}, "BUDGET_EXCEEDED");
  reject(catalog, SetHistoryRoots{"a", {ids[0], ids[0]}}, "DUPLICATE_ENTRY");
  reject(catalog, SetHistoryRoots{"a", {std::string(64, '0')}}, "UNKNOWN_ASSET");
  apply(catalog, SetHistoryRoots{"a", {}});
  apply(catalog, RemoveHistoryRoots{"z"});
  apply(catalog, SetHistoryRoots{std::string(32, 'x'), {}});
  apply(catalog, Collect{});
  check(catalog.snapshot().assets.size() == 8, "all rooted manifests remain at capacity");
  Catalog storage;
  for (unsigned i = 0; i < 4; ++i) {
    const Bytes blob(65536, static_cast<std::uint8_t>(i));
    apply(storage, Import{bundle({wire_asset(manifest(blob))}, {raw(blob)})});
  }
  check(storage.snapshot().blobs.size() == 4, "exact aggregate decoded cap accepted");
  const Bytes extra{9};
  reject(storage, Import{bundle({wire_asset(manifest(extra))}, {raw(extra)})}, "BUDGET_EXCEEDED");
  apply(storage, Collect{});
  apply(storage, Import{bundle({wire_asset(manifest(extra))}, {raw(extra)})});
  check(storage.snapshot().blobs[0].bytes == extra, "quota recovers after collection");
  Catalog eight;
  std::vector<WireAsset> eight_assets;
  std::vector<WireBlob> eight_blobs;
  for (unsigned i = 0; i < 8; ++i) {
    const Bytes blob{static_cast<std::uint8_t>(i)};
    eight_assets.push_back(wire_asset(manifest(blob))); eight_blobs.push_back(raw(blob));
  }
  apply(eight, Import{bundle(eight_assets, eight_blobs)});
  check(eight.snapshot().blobs.size() == 8, "exact unique blob cap accepted");
  auto metadata = manifest(a);
  metadata.media_type = std::string(64, 'm'); metadata.source = std::string(128, 's'); metadata.license = std::string(64, 'l');
  check(canonical_manifest(metadata).size() == 310, "exact ASCII field caps accepted");
  metadata.source.push_back('s'); fails([&] { (void)canonical_manifest(metadata); }, "INVALID_MANIFEST");
  metadata = manifest(a); metadata.media_type[0] = static_cast<char>(0x80);
  fails([&] { (void)canonical_manifest(metadata); }, "INVALID_MANIFEST");
  metadata = manifest(a); metadata.content_hash[0] = 'A';
  fails([&] { (void)asset_id(metadata); }, "INVALID_MANIFEST");
  fails([&] { (void)content_hash(Bytes(65537)); }, "BUDGET_EXCEEDED");
}
} // namespace

int main() {
  try {
    identity_and_collection(); invalid_packages(); decompression(); caps_and_roots();
    std::cout << "{\"status\":\"passed\",\"assertions\":" << assertions << "}\n";
    return 0;
  } catch (const TestFailure& error) {
    std::cerr << "assets native invariant failed: " << error.what() << '\n';
  } catch (...) {
    std::cerr << "assets native invariant failed: unexpected exception\n";
  }
  return 1;
}
