// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ow::assets {
using Bytes = std::vector<std::uint8_t>;
using AssetId = std::string;
using BlobId = std::string;
inline constexpr std::size_t manifest_limit = 8, blob_limit = 8;
inline constexpr std::size_t history_limit = 4, roots_limit = 8;
inline constexpr std::size_t blob_byte_limit = 65536, decoded_byte_limit = 262144;
inline constexpr std::size_t bundle_byte_limit = 524288;

class Failure final : public std::runtime_error {
 public:
  explicit Failure(const char* code) : std::runtime_error(code) {}
  const char* code() const noexcept { return what(); }
};

struct Manifest {
  std::uint32_t format_version = 1;
  BlobId content_hash;
  std::uint32_t decoded_length = 0;
  std::string media_type, source, license;
  bool operator==(const Manifest&) const = default;
};
struct Asset {
  AssetId id;
  Manifest manifest;
  bool operator==(const Asset&) const = default;
};
struct Blob {
  BlobId hash;
  Bytes bytes;
  bool operator==(const Blob&) const = default;
};
struct HistoryRoots {
  std::string name;
  std::vector<AssetId> asset_ids;
  bool operator==(const HistoryRoots&) const = default;
};
struct Snapshot {
  std::uint32_t format_version = 1;
  std::uint64_t revision = 0;
  std::vector<Asset> assets;
  std::vector<Blob> blobs;
  std::vector<AssetId> current_roots;
  std::vector<HistoryRoots> history_roots;
  bool operator==(const Snapshot&) const = default;
};

struct AssetRetention {
  AssetId id;
  BlobId content_hash;
  bool current_root = false;
  std::vector<std::string> history_roots;
  bool operator==(const AssetRetention&) const = default;
};
struct BlobRetention {
  BlobId hash;
  std::vector<AssetId> referencing_assets, retaining_assets;
  bool operator==(const BlobRetention&) const = default;
};
struct RetentionReport {
  std::uint32_t format_version = 1;
  std::vector<AssetId> current_roots;
  std::vector<HistoryRoots> history_roots;
  std::vector<AssetRetention> assets;
  std::vector<BlobRetention> blobs;
  std::vector<AssetId> removable_assets;
  std::vector<BlobId> removable_blobs;
  bool operator==(const RetentionReport&) const = default;
};
struct RetentionResult {
  // Only static literals: reporting allocation failure must not allocate.
  std::string_view status = "rejected", code;
  std::uint64_t revision = 0;
  std::optional<RetentionReport> report;
  bool operator==(const RetentionResult&) const = default;
};

struct Import { Bytes bundle; };
// Each root set contains at most 8 distinct known AssetIds. Names are 1..32
// ASCII characters matching [A-Za-z][A-Za-z0-9_.-]*; at most 4 named sets exist.
// Duplicate IDs reject, rather than silently normalizing an ambiguous command.
struct SetCurrentRoots { std::vector<AssetId> asset_ids; };
struct SetHistoryRoots { std::string name; std::vector<AssetId> asset_ids; };
struct RemoveHistoryRoots { std::string name; };
struct Collect {};
using Command = std::variant<Import, SetCurrentRoots, SetHistoryRoots, RemoveHistoryRoots, Collect>;
struct Receipt {
  std::string status = "rejected";
  std::uint64_t revision = 0;
  std::string code;
  std::vector<AssetId> imported;
  std::uint32_t manifests_removed = 0, blobs_removed = 0;
  bool operator==(const Receipt&) const = default;
};

// All integers in the two binary formats are unsigned little-endian.
// Manifest: "OWAMNF01", u32 version=1, 32 digest bytes, u32 decoded length,
// then three (u16 length, printable ASCII bytes) fields: media<=64, source<=128,
// license<=64. Empty fields are invalid. AssetId hashes these exact bytes.
Bytes canonical_manifest(const Manifest&);
AssetId asset_id(const Manifest&);
BlobId content_hash(std::span<const std::uint8_t>);

// Single-owner, volatile native catalog. No filesystem access or World/history
// authority is imported. History roots are explicit owner-held retention sets.
// Query results are detached and sorted. Every successful command, even a no-op,
// advances revision once; a failed command preserves the complete prior state.
class Catalog final {
 public:
  Catalog() = default;
  Catalog(const Catalog&) = delete;
  Catalog& operator=(const Catalog&) = delete;
  Catalog(Catalog&&) = delete;
  Catalog& operator=(Catalog&&) = delete;
  Snapshot snapshot() const;
  std::uint64_t revision() const noexcept;
  // Read-only, revision-bound explanation of the existing Collect policy.
  // All IDs/names/rows are detached and sorted, including empty named sets.
  // No content bytes or provenance fields are copied into the report.
  // Success is status="ok", empty code, and a complete report. Rejection is
  // status="rejected" with no report: REVISION_CONFLICT (checked first) or
  // RESOURCE_EXHAUSTED on catchable report-payload std::bad_alloc.
  // No recovery is promised for a library's noexcept allocation termination.
  // Revision never advances.
  RetentionResult inspect_retention(std::uint64_t expected_revision) const;
  Receipt apply(const Command&, std::uint64_t expected_revision);
  // Bundle: "OWASB001", u32 version=1, u32 asset count, u32 blob count;
  // each asset: 32 ID bytes, u32 manifest byte length, canonical manifest;
  // each blob: u16 path length, "blobs/" + 64 lowercase hex, u8 codec,
  // u32 decoded length, u32 encoded length, encoded bytes. Codec 0 is raw;
  // codec 1 is RLE8 (nonzero u8 count, u8 value pairs). Exports use raw only.
  // Exactly the referenced blobs must appear, with no duplicates/trailing data.
  // Input order is immaterial; output order is ascending AssetId / BlobId.
  Bytes export_bundle() const;
 private:
  Snapshot state_;
};
} // namespace ow::assets
