// SPDX-License-Identifier: Apache-2.0
#include "omniweft/assets.hpp"
#include "persistence_io.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ow::assets {
namespace {
constexpr std::string_view manifest_magic = "OWAMNF01", bundle_magic = "OWASB001";
constexpr std::size_t canonical_limit = 310;
[[noreturn]] void fail(const char* code) { throw Failure(code); }
bool valid_hash(std::string_view value) {
  return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}
std::string hex(std::span<const std::uint8_t> bytes) {
  constexpr char alphabet[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (auto byte : bytes) {
    out.push_back(alphabet[byte >> 4]);
    out.push_back(alphabet[byte & 15]);
  }
  return out;
}
void digest_bytes(Bytes& out, std::string_view value) {
  if (!valid_hash(value)) fail("INVALID_MANIFEST");
  const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (std::size_t i = 0; i < value.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>((nibble(value[i]) << 4) | nibble(value[i + 1])));
}
bool ascii(std::string_view value, std::size_t limit) {
  return !value.empty() && value.size() <= limit
      && std::all_of(value.begin(), value.end(), [](char c) { return c >= 0x20 && c <= 0x7e; });
}
void manifest_valid(const Manifest& value) {
  if (value.format_version != 1) fail("UNSUPPORTED_VERSION");
  if (value.decoded_length > blob_byte_limit) fail("BUDGET_EXCEEDED");
  if (!valid_hash(value.content_hash) || !ascii(value.media_type, 64)
      || !ascii(value.source, 128) || !ascii(value.license, 64)) fail("INVALID_MANIFEST");
}
void u16(Bytes& out, std::size_t value) {
  out.push_back(static_cast<std::uint8_t>(value));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
}
void u32(Bytes& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void text(Bytes& out, std::string_view value) {
  u16(out, value.size());
  out.insert(out.end(), value.begin(), value.end());
}
class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> input, const char* code = "INVALID_BUNDLE")
      : remaining_(input), code_(code) {}
  std::span<const std::uint8_t> take(std::size_t size) {
    if (size > remaining_.size()) fail(code_);
    const auto result = remaining_.first(size);
    remaining_ = remaining_.subspan(size);
    return result;
  }
  std::uint8_t byte() { return take(1)[0]; }
  std::uint16_t read16() {
    const auto data = take(2);
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[0])
        | (static_cast<std::uint16_t>(data[1]) << 8));
  }
  std::uint32_t read32() {
    const auto data = take(4);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(data[i]) << (i * 8);
    return value;
  }
  std::string read_text(std::size_t limit, const char* limit_code = nullptr) {
    const auto length = read16();
    if (length > limit) fail(limit_code == nullptr ? code_ : limit_code);
    const auto data = take(length);
    return {data.begin(), data.end()};
  }
  void magic(std::string_view expected) {
    const auto value = take(expected.size());
    if (!std::equal(value.begin(), value.end(), expected.begin(), expected.end())) fail(code_);
  }
  bool empty() const noexcept { return remaining_.empty(); }
 private:
  std::span<const std::uint8_t> remaining_;
  const char* code_;
};
Manifest read_manifest(std::span<const std::uint8_t> bytes) {
  if (bytes.size() > canonical_limit) fail("INVALID_MANIFEST");
  Reader input(bytes, "INVALID_MANIFEST");
  input.magic(manifest_magic);
  Manifest value;
  value.format_version = input.read32();
  if (value.format_version != 1) fail("UNSUPPORTED_VERSION");
  value.content_hash = hex(input.take(32));
  value.decoded_length = input.read32();
  value.media_type = input.read_text(64);
  value.source = input.read_text(128);
  value.license = input.read_text(64);
  if (!input.empty()) fail("INVALID_MANIFEST");
  manifest_valid(value);
  return value;
}
template<class Value, class Key>
auto find_asset(Value& state, const Key& id) {
  return std::find_if(state.assets.begin(), state.assets.end(), [&](const Asset& value) { return value.id == id; });
}
template<class Value, class Key>
auto find_blob(Value& state, const Key& hash) {
  return std::find_if(state.blobs.begin(), state.blobs.end(), [&](const Blob& value) { return value.hash == hash; });
}
void sort_entries(Snapshot& state) {
  std::sort(state.assets.begin(), state.assets.end(), [](const Asset& a, const Asset& b) { return a.id < b.id; });
  std::sort(state.blobs.begin(), state.blobs.end(), [](const Blob& a, const Blob& b) { return a.hash < b.hash; });
}
void quota(const Snapshot& state) {
  if (state.assets.size() > manifest_limit || state.blobs.size() > blob_limit) fail("BUDGET_EXCEEDED");
  std::size_t total = 0;
  for (const auto& blob : state.blobs) {
    if (blob.bytes.size() > blob_byte_limit || blob.bytes.size() > decoded_byte_limit - total)
      fail("BUDGET_EXCEEDED");
    total += blob.bytes.size();
  }
}
Snapshot decode_bundle(std::span<const std::uint8_t> bytes) {
  if (bytes.size() > bundle_byte_limit) fail("BUDGET_EXCEEDED");
  Reader input(bytes);
  input.magic(bundle_magic);
  if (input.read32() != 1) fail("UNSUPPORTED_VERSION");
  const auto asset_count = input.read32(), blob_count = input.read32();
  if (asset_count > manifest_limit || blob_count > blob_limit) fail("BUDGET_EXCEEDED");
  Snapshot decoded;
  decoded.assets.reserve(asset_count);
  decoded.blobs.reserve(blob_count);
  for (std::uint32_t i = 0; i < asset_count; ++i) {
    Asset value;
    value.id = hex(input.take(32));
    const auto length = input.read32();
    if (length > canonical_limit) fail("INVALID_MANIFEST");
    value.manifest = read_manifest(input.take(length));
    if (asset_id(value.manifest) != value.id) fail("HASH_MISMATCH");
    if (find_asset(decoded, value.id) != decoded.assets.end()) fail("DUPLICATE_ENTRY");
    decoded.assets.push_back(std::move(value));
  }
  std::size_t total = 0;
  for (std::uint32_t i = 0; i < blob_count; ++i) {
    const auto path = input.read_text(70, "INVALID_PATH");
    if (path.size() != 70 || path.substr(0, 6) != "blobs/" || !valid_hash(std::string_view(path).substr(6)))
      fail("INVALID_PATH");
    Blob value{path.substr(6), {}};
    if (find_blob(decoded, value.hash) != decoded.blobs.end()) fail("DUPLICATE_ENTRY");
    const auto codec = input.byte();
    const auto decoded_length = input.read32(), encoded_length = input.read32();
    if (decoded_length > blob_byte_limit || decoded_length > decoded_byte_limit - total
        || encoded_length > blob_byte_limit * 2) fail("BUDGET_EXCEEDED");
    if (codec > 1) fail("INVALID_BUNDLE");
    const auto encoded = input.take(encoded_length);
    if (codec == 0) {
      if (encoded_length != decoded_length) fail("SIZE_MISMATCH");
      value.bytes.assign(encoded.begin(), encoded.end());
    } else {
      if (encoded_length % 2 != 0) fail("DECOMPRESSION_ERROR");
      // Validate the complete expansion before allocating or appending any output.
      std::size_t expanded = 0;
      for (std::size_t at = 0; at < encoded.size(); at += 2) {
        const auto count = encoded[at];
        if (count == 0 || count > decoded_length - expanded) fail("DECOMPRESSION_ERROR");
        expanded += count;
      }
      if (expanded != decoded_length) fail("SIZE_MISMATCH");
      value.bytes.reserve(decoded_length);
      for (std::size_t at = 0; at < encoded.size(); at += 2)
        value.bytes.insert(value.bytes.end(), encoded[at], encoded[at + 1]);
    }
    if (content_hash(value.bytes) != value.hash) fail("HASH_MISMATCH");
    total += decoded_length;
    decoded.blobs.push_back(std::move(value));
  }
  if (!input.empty()) fail("INVALID_BUNDLE");
  std::set<BlobId> referenced;
  for (const auto& asset : decoded.assets) {
    const auto blob = find_blob(decoded, asset.manifest.content_hash);
    if (blob == decoded.blobs.end()) fail("MISSING_BLOB");
    if (blob->bytes.size() != asset.manifest.decoded_length) fail("SIZE_MISMATCH");
    referenced.insert(blob->hash);
  }
  if (referenced.size() != decoded.blobs.size()) fail("EXTRA_BLOB");
  sort_entries(decoded);
  return decoded;
}
void import_bundle(Snapshot& state, const Import& command, Receipt& receipt) {
  auto incoming = decode_bundle(command.bundle);
  receipt.imported.reserve(incoming.assets.size());
  for (auto& asset : incoming.assets) {
    receipt.imported.push_back(asset.id);
    const auto found = find_asset(state, asset.id);
    if (found == state.assets.end()) {
      if (state.assets.size() == manifest_limit) fail("BUDGET_EXCEEDED");
      state.assets.push_back(std::move(asset));
    } else if (found->manifest != asset.manifest) fail("HASH_MISMATCH");
  }
  for (auto& blob : incoming.blobs) {
    const auto found = find_blob(state, blob.hash);
    if (found == state.blobs.end()) {
      if (state.blobs.size() == blob_limit) fail("BUDGET_EXCEEDED");
      state.blobs.push_back(std::move(blob));
    } else if (found->bytes != blob.bytes) fail("HASH_MISMATCH");
  }
  quota(state);
  sort_entries(state);
}
std::vector<AssetId> roots(const Snapshot& state, const std::vector<AssetId>& values) {
  if (values.size() > roots_limit) fail("BUDGET_EXCEEDED");
  for (const auto& id : values) {
    if (!valid_hash(id)) fail("INVALID_ROOT");
    if (find_asset(state, id) == state.assets.end()) fail("UNKNOWN_ASSET");
  }
  auto sorted = values;
  std::sort(sorted.begin(), sorted.end());
  if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) fail("DUPLICATE_ENTRY");
  return sorted;
}
void root_name(std::string_view name) {
  const auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
  if (name.empty() || name.size() > 32 || !letter(name[0])) fail("INVALID_ROOT");
  for (const auto c : name)
    if (!letter(c) && !(c >= '0' && c <= '9') && c != '_' && c != '.' && c != '-') fail("INVALID_ROOT");
}
void collect(Snapshot& state, Receipt& receipt) {
  std::set<AssetId> retained(state.current_roots.begin(), state.current_roots.end());
  for (const auto& history : state.history_roots)
    retained.insert(history.asset_ids.begin(), history.asset_ids.end());
  const auto old_assets = state.assets.size(), old_blobs = state.blobs.size();
  std::erase_if(state.assets, [&](const Asset& value) { return !retained.contains(value.id); });
  std::set<BlobId> reachable;
  for (const auto& value : state.assets) reachable.insert(value.manifest.content_hash);
  std::erase_if(state.blobs, [&](const Blob& value) { return !reachable.contains(value.hash); });
  receipt.manifests_removed = static_cast<std::uint32_t>(old_assets - state.assets.size());
  receipt.blobs_removed = static_cast<std::uint32_t>(old_blobs - state.blobs.size());
}
} // namespace

BlobId content_hash(std::span<const std::uint8_t> input) {
  if (input.size() > blob_byte_limit) fail("BUDGET_EXCEEDED");
  const auto digest = persistence::io::sha256(input);
  return hex(digest);
}
Bytes canonical_manifest(const Manifest& value) {
  manifest_valid(value);
  Bytes bytes;
  bytes.reserve(canonical_limit);
  bytes.insert(bytes.end(), manifest_magic.begin(), manifest_magic.end());
  u32(bytes, value.format_version);
  digest_bytes(bytes, value.content_hash);
  u32(bytes, value.decoded_length);
  text(bytes, value.media_type);
  text(bytes, value.source);
  text(bytes, value.license);
  return bytes;
}
AssetId asset_id(const Manifest& value) { return content_hash(canonical_manifest(value)); }
Snapshot Catalog::snapshot() const { return state_; }
std::uint64_t Catalog::revision() const noexcept { return state_.revision; }
Receipt Catalog::apply(const Command& command, std::uint64_t expected_revision) {
  Receipt receipt;
  receipt.revision = state_.revision;
  if (expected_revision != state_.revision) { receipt.code = "REVISION_CONFLICT"; return receipt; }
  if (state_.revision == std::numeric_limits<std::uint64_t>::max()) {
    receipt.code = "REVISION_EXHAUSTED";
    return receipt;
  }
  try {
    auto staged = state_;
    std::visit([&](const auto& value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, Import>) import_bundle(staged, value, receipt);
      else if constexpr (std::is_same_v<T, SetCurrentRoots>) staged.current_roots = roots(staged, value.asset_ids);
      else if constexpr (std::is_same_v<T, SetHistoryRoots>) {
        root_name(value.name);
        auto valid_roots = roots(staged, value.asset_ids);
        auto found = std::find_if(staged.history_roots.begin(), staged.history_roots.end(),
            [&](const HistoryRoots& entry) { return entry.name == value.name; });
        if (found != staged.history_roots.end()) found->asset_ids = std::move(valid_roots);
        else {
          if (staged.history_roots.size() == history_limit) fail("BUDGET_EXCEEDED");
          staged.history_roots.push_back({value.name, std::move(valid_roots)});
        }
        std::sort(staged.history_roots.begin(), staged.history_roots.end(),
            [](const HistoryRoots& a, const HistoryRoots& b) { return a.name < b.name; });
      } else if constexpr (std::is_same_v<T, RemoveHistoryRoots>) {
        root_name(value.name);
        std::erase_if(staged.history_roots, [&](const HistoryRoots& entry) { return entry.name == value.name; });
      } else collect(staged, receipt);
    }, command);
    staged.revision += 1;
    receipt.status = "committed";
    receipt.revision = staged.revision;
    // No allocation, validation or receipt construction follows publication.
    static_assert(std::is_nothrow_swappable_v<Snapshot>);
    static_assert(std::is_nothrow_move_constructible_v<Receipt>);
    using std::swap;
    swap(state_, staged);
    return receipt;
  } catch (const Failure& error) {
    receipt = Receipt{};
    receipt.revision = state_.revision;
    receipt.code = error.code();
    return receipt;
  } catch (const std::bad_alloc&) {
    receipt = Receipt{};
    receipt.revision = state_.revision;
    receipt.code = "BUDGET_EXCEEDED";
    return receipt;
  }
}
Bytes Catalog::export_bundle() const {
  Bytes bytes;
  bytes.reserve(20);
  bytes.insert(bytes.end(), bundle_magic.begin(), bundle_magic.end());
  u32(bytes, 1);
  u32(bytes, static_cast<std::uint32_t>(state_.assets.size()));
  u32(bytes, static_cast<std::uint32_t>(state_.blobs.size()));
  for (const auto& asset : state_.assets) {
    digest_bytes(bytes, asset.id);
    const auto manifest = canonical_manifest(asset.manifest);
    u32(bytes, static_cast<std::uint32_t>(manifest.size()));
    bytes.insert(bytes.end(), manifest.begin(), manifest.end());
  }
  for (const auto& blob : state_.blobs) {
    text(bytes, "blobs/" + blob.hash);
    bytes.push_back(0);
    u32(bytes, static_cast<std::uint32_t>(blob.bytes.size()));
    u32(bytes, static_cast<std::uint32_t>(blob.bytes.size()));
    bytes.insert(bytes.end(), blob.bytes.begin(), blob.bytes.end());
  }
  if (bytes.size() > bundle_byte_limit) fail("BUDGET_EXCEEDED");
  return bytes;
}
} // namespace ow::assets
