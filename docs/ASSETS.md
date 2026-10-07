# Bounded native asset catalog

PR-016 introduces `ow::assets::Catalog` in [assets.hpp](../include/omniweft/assets.hpp), with the runnable [assets.roundtrip](../examples/assets-roundtrip.md) fixture. Validation and integration state are tracked in [the handoff](execution/PR-016-HANDOFF.md).

The Catalog owns opaque bytes and immutable provenance manifests. `content_hash` identifies decoded blob bytes with SHA-256. `asset_id` identifies the complete canonical versioned manifest, including the content hash, decoded length, media type, source and license. Equal manifests deduplicate; differing source/license metadata produces separate assets sharing the same blob. These fields preserve attribution supplied by the importer; the Catalog does not authenticate licensing assertions or execute content.

One native owner serializes access. Every mutation is an `Import`, `SetCurrentRoots`, `SetHistoryRoots`, `RemoveHistoryRoots` or `Collect` command passed to `apply(command, expected_revision)`. Successful commands, including no-ops, increment the revision once. Rejection reports the unchanged revision with an empty imported-ID list and zero removal counts. Validation and staged state complete before publication. Queries return detached, sorted values. Catalog instances cannot be copied or moved. Concurrent access requires caller synchronization.

Current roots and up to four named history root sets contain full asset identities. `Collect` removes unrooted manifests, then blobs unreferenced by any remaining manifest. Removing one provenance variant cannot collect bytes still used by another rooted variant. Roots are explicit owner-held retention sets; they do not automatically connect to World undo, replay or persistence. Bundles contain no roots, revisions, policy credentials or authority. A fresh import starts from its own Catalog revision.

The limits are eight manifests, eight blobs, eight references per root set, 64 KiB per decoded blob, 256 KiB of unique decoded bytes and 512 KiB per encoded bundle. Empty content and empty catalogs are valid. Manifest fields are nonempty printable ASCII: media type at most 64 bytes, source at most 128, license at most 64. Root names contain 1–32 ASCII characters matching `[A-Za-z][A-Za-z0-9_.-]*`. Duplicate references are rejected. Replacing roots is atomic; removing an absent valid root name is a successful no-op.

## Read-only retention inspection

FP-008 adds `Catalog::inspect_retention(expected_revision)` and [assets.explain_retention](../examples/assets-explain_retention.md). The result contains the observed Catalog revision, `status`/`code` and an optional detached version-1 report. `ok` has an empty code and a complete report. A stale or future revision produces `rejected/REVISION_CONFLICT` with no report; catchable report-payload `std::bad_alloc` produces `rejected/RESOURCE_EXHAUSTED` without partial output. The query neither increments revision nor changes collection policy. Revision binding is to the queried live Catalog instance; equal revisions in different instances are not interchangeable identities. Caller synchronization remains required.

The report preserves sorted current roots and every named history set, including empty sets. Each asset row contains its immutable identity, content hash, current-root membership and sorted history-root names. Each blob row separates all referencing manifest IDs from the rooted subset that retains it. `removable_assets` contains exactly the manifests with no root reason; `removable_blobs` contains exactly the blobs with no retaining manifest. A shared blob may survive removal of one provenance variant. Existing Catalog bounds limit the graph to eight manifests, eight blobs and forty root-reference edges. There are no blob bytes or provenance strings in the report, no World-reference inference and no collection authority in a diagnostic result.

The report's format version describes a native detached value, not a new bundle, save or network format. Existing Import/Collect/root commands, HTTP/SDK profiles, grants and OWASB001 bytes remain unchanged. See [the FP-008 handoff](execution/FP-008-HANDOFF.md) for actual verification and integration state.

## Version 1 byte format

All integers are unsigned little-endian. Digests occupy 32 raw bytes in manifest entries; displayed identities use 64 lowercase hexadecimal characters. Text lengths count bytes.

| Record | Fields in order |
| --- | --- |
| Canonical manifest | `OWAMNF01` (8 bytes), version `u32=1`, content digest (32 bytes), decoded length `u32`, media/source/license each as `u16 length` followed by ASCII bytes |
| Bundle header | `OWASB001` (8 bytes), version `u32=1`, manifest count `u32`, blob count `u32` |
| Each manifest entry | asset digest (32 bytes), canonical manifest length `u32`, canonical manifest bytes |
| Each blob entry | path length `u16`, exact `blobs/<content hash>` bytes, codec `u8`, decoded length `u32`, encoded length `u32`, encoded bytes |

Export sorts manifests by asset identity and blobs by content hash and emits raw codec `0`. Import accepts either entry order and raw codec `0` or RLE8 codec `1`. RLE8 consists of nonzero one-byte run counts followed by one-byte values; the decoder validates the entire expansion against declared output before allocating output. All declared sizes and running totals are bounded. No other codecs or versions are supported.

Only exact `blobs/<64 lowercase hex>` identifiers are accepted. They are bundle identifiers, never filesystem paths to normalize or extract. Duplicate identities, missing/extra blobs, hash/size mismatch, trailing bytes and malformed data reject the whole import, including when some content already exists. Successful imports return sorted asset identities, including deduplicated ones. Export/import/export of a canonical bundle preserves exact bytes.

The module has no filesystem, network, HTTP or SDK endpoint. The example alone writes and rereads a fixture bundle in a fresh output directory. OWASB001 is an additive opaque exchange format, not a replacement or migration for OWPKG001 world saves. Existing builtin World/replay/persistence assets and policy grants are unchanged. There is no glTF loader, external World attachment, durable asset store, GPU or physics capability in this slice. Future format changes require explicit compatibility decisions; preserve original bundles when reverting this module.

Allocation-recovery evidence covers catchable report payload failures. MSVC Debug allocates iterator bookkeeping inside `noexcept` container constructors/moves; a real allocation failure there terminates under the STL contract and cannot be translated by this query. The test injector excludes only that platform-specific bookkeeping size, proves the tested payload allocations are larger, and verifies the exclusion is exercised. It continues sweeping every catchable payload site, checking unchanged state and complete recovery; no production fault hook, timeout relaxation or host configuration is added.
