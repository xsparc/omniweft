# Ten feature proposals for Omniweft

Status: **proposed, unimplemented, and not adopted for autonomous execution**. These ten candidates answer the maintainer's 2026-09-30 request for new feature ideas in a draft PR. They supplement the [adopted PR-001–038 roadmap](ROADMAP.md); they do not renumber it, change its dependencies or enlarge the v0.1 release gate. Merging this proposal document records ideas, not approval to implement them.

Baseline: PR-017 squash merge `c9a50e6e6826d6448e09d1e7f6c1bebdf3af9fcc`. The current GLB importer produces detached scenes and Catalog assets. World attachment, remote hierarchy/query support, physics and much of the renderer remain separate work. See [README](../README.md), [architecture](ARCHITECTURE.md) and the [integration handoff](execution/PR-017-HANDOFF.md) for actual support.

## Candidate overview

Each row is one candidate feature with a small first implementation. IDs `FP-001` through `FP-010` are proposal identifiers, not adopted backlog items or GitHub PR numbers. All ten have status `proposed`.

| ID | Feature | User benefit | First-slice effort / principal risk |
| --- | --- | --- | --- |
| FP-001 | Reusable prefab recipes | Place a tested arrangement repeatedly with fresh object identities | Medium / identity remapping and atomic creation |
| FP-002 | Explicit snap sockets | Assemble objects at known attachment points precisely | Medium / transform conventions and stale selections |
| FP-003 | Typed semantic relations | Ask how objects relate, beyond matching tags | Medium / deletion and generation safety |
| FP-004 | Revision-filtered change feed | Let agents inspect only changes since their last observation | High / access filtering and lost history |
| FP-005 | Bounded navigation routes | Plan a route across an explicit occupancy grid | Medium / deterministic search and stale input |
| FP-006 | Fixed-tick transform timelines | Author a repeatable sequence of object motion | Medium / competing authoring and tick semantics |
| FP-007 | Spatial audio cues | Position and schedule sounds in a scene | High / numerical mixing and device support boundaries |
| FP-008 | Asset retention and impact inspector | Explain why content is retained and what collection would remove | Small / complete revision-bound explanations |
| FP-009 | Seeded procedural scatter | Produce reproducible variations of a layout | Medium / bounded attempts and atomic placement |
| FP-010 | Reproducible observation datasets | Export auditable offline agent-evaluation fixtures | Medium / observation privacy and provenance |

Effort is a relative design estimate, not a delivery promise. No performance target or engine capability is established by this document.

## Shared acceptance contract

Before adoption, each candidate needs its own bounded slice plan, example specification, independent oracle, explicit limits and dependencies in the existing backlog. Initial limits below are proposed testable ceilings, not current public contracts. Examples named below are future examples, not runnable commands.

Every World mutation must use typed operations and the authoritative Coordinator with generation, revision, admission and quota checks appropriate to its entry point. Other authoritative modules retain their own typed revision-checked owner paths, such as `Catalog::apply`. A native owner-only first slice does not introduce remote authority; later SDK exposure requires explicit policy design. Read-only tools consume authorized detached snapshots. Semantic claims, content hashes and model output never grant capabilities. No generated code is executed.

Each adopted slice must pass actual Windows/Linux CPU checks and independent review. Visual, device-audio or physics claims require their own applicable evidence. Rejection must leave committed state unchanged; recovery must be demonstrated. Persistence, public protocol or dependency changes require explicit compatibility/provenance review, and are not silently included here. Existing [merge and certification rules](AUTONOMOUS_DEVELOPMENT.md#merge-authority) continue to apply.

## FP-001 — Reusable prefab recipes

**Value and distinction.** Save a small desk-and-chair arrangement and instantiate it twice with stable recipe provenance and different entity identities. The existing `builtin.unit_cube` selector is not an authored reusable arrangement.

**First slice.** A typed immutable in-memory recipe holds at most four builtin root objects, fixed tags and relative translations. Instantiation takes a placement offset and expected world revision, validates the whole expansion, then creates it in one transaction with an explicit recipe-member-to-entity mapping. No nested recipes, live instance updates, imported-mesh attachment or file format.

**Prerequisites and contract work.** PR-003/009 identity/transform rules and PR-010's native tag operations. Define recipe schema/hash version and parameter ranges; a Catalog-backed recipe format is a later PR-016 integration, not presumed present.

**Example and oracle.** Proposed `authoring.prefab_twice`: independently derive all eight placements and prove disjoint identities. An oversized recipe or exhausted World rejects without consuming slots or incrementing revision. Reduce the recipe and retry at a fresh revision; the valid instance must succeed. Test deleting/reusing a slot cannot retarget a recipe mapping.

## FP-002 — Explicit snap sockets

**Value and distinction.** Connect a cabinet handle to a named mounting point without hand-calculating coordinates. This adds an attachment-frame contract beyond PR-034's generic picking and movement.

**First slice.** At most four typed named local frames on each of two isolated authoring roots. Produce a detached preview that aligns a selected source socket to a target socket; commit one root transform through the ordinary command path. Bind the preview to both entity generations, socket revisions and the world revision. Restrict the first profile to unit scale.

**Prerequisites and contract work.** PR-003/009; define frame orientation and uniqueness of socket names. No parenting, physical joint, automatic collision-clearance claim or dynamic-body support.

**Example and oracle.** Proposed `authoring.snap_socket`: literal translated/rotated frames independently predict the resulting transform. A missing socket, non-unit scale or stale target rejects atomically. Refresh the target and preview again to recover; deleting/recreating the target never validates the old preview.

## FP-003 — Typed semantic relations

**Value and distinction.** Express authored links such as a switch being associated with a lamp. PR-010 tags classify individual objects; this feature records relationships between them.

**First slice.** A native, revisioned graph of at most 16 directed edges with a fixed relation vocabulary, for example `associated_with` and `part_of_design`. Add/remove operations and exact bounded queries use generation-safe endpoints. A relation is a label for authoring intent, never proof of support, ownership, collision or access rights.

**Prerequisites and contract work.** PR-003/010. Define atomic edge storage and deletion policy before implementation: initially reject object deletion while incident edges exist, then allow explicit edge removal and deletion in one supported batch. No inference engine, scripts, RDF dependency or remote graph endpoint.

**Example and oracle.** Proposed `observe.relation_graph`: compare a complete expected edge set and query result. Invalid endpoints and the seventeenth edge leave the graph and World unchanged. Remove an edge, retry, and prove that slot reuse does not reconnect an old relation.

## FP-004 — Revision-filtered change feed

**Value and distinction.** Let an agent poll changes instead of repeatedly reading a whole snapshot. Bounded subscriptions and resync are already architectural intentions; this is a concrete first implementation absent from the adopted roadmap, not a new transport decision.

**First slice.** Native pull queries over eight retained authoring batches for supported create, root transform, tag and delete operations. A cursor is bound to one World/session, observation scope and revision. Complete batch boundaries, deletion tombstones and an explicit `resync_required` result prevent a partial history from appearing complete.

**Prerequisites and contract work.** PR-007/010/011; explicit observation filtering and revocation semantics must be designed first. Until remote observation authority is implemented, expose only the native owner query. Do not change retry receipt retention or introduce sockets/streaming.

**Example and oracle.** Proposed `observe.change_feed`: apply returned deltas to a detached baseline and compare it with an independently expected snapshot. Test eviction, wrong-World cursors, scope changes and deleted/reused identities. A stale cursor must require a fresh authorized snapshot; resumed polling must reconstruct the correct next revision without leaking excluded object IDs or tags.

## FP-005 — Bounded navigation routes

**Value and distinction.** Find a repeatable path across a small authored walkability map. AABB queries locate objects; they do not find routes.

**First slice.** One immutable, versioned occupancy grid, at most 16 by 16 cells. Four-neighbor unit-cost search has fixed neighbor ordering, at most 256 unique cell expansions, and a path bound of 256 cells. Queries return the input identity/revision and either a complete route, unreachable result or explicit rejection.

**Prerequisites and contract work.** PR-016 for content/provenance conventions. Start with an explicit grid input, not automatic World/voxel extraction. Geometry-derived navigation, navmeshes, moving obstacles and character controllers need later slices; a returned route makes no physical traversability claim.

**Example and oracle.** Proposed `navigation.grid_route`: a separate breadth-first oracle checks shortest length, tie-breaking and every cell. Reject malformed dimensions, invalid endpoints and stale identity without state changes. Restore a blocked cell in a new grid revision and verify a route is found; old results stay bound to the old grid.

## FP-006 — Fixed-tick transform timelines

**Value and distinction.** Author a short moving-platform or demonstration sequence. Animation is already a future product goal; this defines a small authoring-only beginning, separate from replay of previously accepted commands.

**First slice.** One isolated nonphysical root, at most four translation keys across eight ticks, fixed rotation/scale and a defined linear interpolation rule. The opt-in owner emits ordinary revision-checked authoring transactions. Each accepted sample advances authoring revision normally; the scheduling tick remains separate. A competing edit stops playback and requires explicit re-planning.

**Prerequisites and contract work.** PR-003/006/013; agree on tick scheduling, cancellation and replay receipts before coding. No implicit revision retry, physical-time rewind, skeletal animation, glTF animation import or dynamic-body motion.

**Example and oracle.** Proposed `animation.root_timeline`: compare all sampled transforms and receipts with literal expected values. Duplicate/out-of-range keys reject before playback; an intervening manual edit stops before the next sample. Explicitly prepare a new timeline from the new revision to recover without overwriting the manual edit.

## FP-007 — Spatial audio cues

**Value and distinction.** Hear where a scripted event occurs. Audio is already identified as future engine work; this proposal provides an independently testable mixing core before device integration.

**First slice.** Mix at most eight generated mono PCM cues into a bounded stereo buffer for one listener, with specified distance attenuation, panning, saturation and tick-to-sample scheduling. Use a fixed 48 kHz format and a one-second output cap. Report unsupported formats before allocating output.

**Prerequisites and contract work.** PR-006 timing and PR-016 provenance conventions. Define channel layout, numerical rounding and voice lifetime. Headless mixing needs no SDL or new decoder; a later optional playback adapter needs Windows/Linux device tests. No microphone/system-audio capture, HRTF, streaming codecs or device names in artifacts.

**Example and oracle.** Proposed `audio.spatial_cues`: independent sample-level reference checks left/right energy, scheduled silence and clipping. The ninth voice or malformed PCM rejects with no partial buffer publication. Stop/restart the mixer and verify no stale voice survives. Offline PCM evidence alone cannot certify audible device output.

## FP-008 — Asset retention and impact inspector

**Value and distinction.** Explain why a seemingly unused asset remains and what a collection would remove. PR-016 performs collection; this feature exposes its reasons as a read-only diagnostic.

**First slice.** At one Catalog revision, enumerate the existing current/history roots, their retained manifests and shared content blobs. Return a bounded, sorted explanation and candidate removal set using the Catalog's existing limits. Call shared blobs out separately from distinct provenance manifests. The diagnostic cannot collect or alter retention roots.

**Prerequisites and contract work.** PR-016. Use only relationships that exist today; history roots are owner-held sets, not automatic World undo references. No speculative mesh/material dependency graph, World-reference inference, package manager or garbage-collection policy change.

**Example and oracle.** Proposed `assets.explain_retention`: two provenance-distinct assets share a blob, one root is removed, and the exact explanation still retains the blob. Reconstruct a fresh private Catalog using ordinary Import and explicit root commands; compare the predicted asset/blob removal sets with actual collection there. Catalog itself is noncopyable, and bundle import does not restore authoritative roots or revision metadata. A stale revision rejects; re-query after the root change recovers with the new exact removal set.

## FP-009 — Seeded procedural scatter

**Value and distinction.** Produce repeatable variations of a garden or block layout. PR-006's fixed scripted scene does not define a reusable seeded placement algorithm and receipt contract.

**First slice.** One versioned integer PRNG and integer-grid placement rule generate a detached proposal for at most four builtin roots, with at most 64 attempts. Inputs include seed, bounded rectangle, excluded cells and minimum grid spacing. No hidden randomness. Publish the whole valid proposal through one existing atomic creation transaction.

**Prerequisites and contract work.** PR-003/007; the first native owner tool gains no new grant. World-derived exclusions require a later PR-010 integration. No generated code, imported mesh instancing, terrain projection or collision-safety claim from the spacing rule.

**Example and oracle.** Proposed `authoring.seeded_scatter`: independently reproduce the integer sequence, selected cells, transforms and creation mapping for two seeds. Impossible spacing, attempt exhaustion or quota failure publishes nothing. Reduce the count and retry from the same seed at a fresh revision; equal inputs must reproduce the preview on Windows and Linux.

## FP-010 — Reproducible observation datasets

**Value and distinction.** Share a compact, licensed fixture for evaluating agent decisions. Replay rebuilds accepted state; a dataset pairs selected observations with accepted actions and expected outcomes for offline evaluation.

**First slice.** Export at most eight steps from an explicitly selected generated scene into a versioned JSON dataset. Allowlist authorized object observations, typed accepted actions, expected next observations, fixture seed and asset provenance. Use deterministic ordering and relative logical artifact names. Export to a new destination and publish a completion manifest only after all content verifies.

**Prerequisites and contract work.** PR-010/013/016. Define source snapshot binding, field exposure and a schema distinct from saves or journals. Recorded private replay state is not automatically authorized for export. No model API, upload, user-scene scan or training consent inference. Image/depth variants require PR-029 and separate GPU/privacy evidence.

**Example and oracle.** Proposed `agents.dataset_fixture`: independently regenerate all eight observations and outcomes, compare content hashes and verify provenance. A disallowed field, mismatched snapshot or interrupted write must produce no completed dataset and preserve earlier exports. Correct the fixture or retry into a fresh destination, then verify deterministic output and absence of credentials, private paths and session/epoch material.

## Research and ordering advice

Primary-source scan: 2026-09-30, focused on scene reuse, navigation, animation and audio. These are design references, not dependency selections or claims that upstream guarantees Omniweft behavior. No release recency or benchmark claim is made.

- [Godot PackedScene](https://docs.godotengine.org/en/stable/classes/class_packedscene.html) documents packing and instantiating scenes. [OpenUSD composition](https://openusd.org/release/intro.html) documents references, overrides and variants. These support the reuse motivation; FP-001 deliberately starts with a much smaller typed recipe. Adopting either engine/format is outside this proposal.
- [Recast Navigation](https://recastnav.com/) separates navigation-mesh generation and pathfinding components. FP-005's explicit grid avoids promising geometry-to-navigation integration before that contract exists.
- [Khronos glTF animation](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations) describes animation channels, samplers and interpolation. FP-006 uses a bounded native timeline first; PR-017's unsupported-animation rejection remains valid.
- [SDL3 audio](https://wiki.libsdl.org/SDL3/CategoryAudio) describes streams, PCM and playback devices. FP-007 separates testable mixing from a future device adapter. Compatibility with Omniweft's pinned SDL revision must be checked before any adapter implementation.

Recommendation, subject to adoption: start with FP-008 because it uses an existing complete Catalog contract, then FP-001 and FP-002 for visible authoring value. FP-003, FP-005 and FP-009 can follow as independent bounded tools. Design FP-004's observation boundary before FP-010's export authority. FP-006 and FP-007 need dedicated scheduling and presentation decisions. This ordering is advisory and does not interrupt or supersede PR-018–038.

Open questions: whether users prefer native or SDK workflows first; which reusable arrangements matter most; whether image datasets are wanted; and whether device audio should become a future release gate. No user study, performance experiment or runtime spike was conducted for these proposals. An adopted candidate may need multiple subsequent PRs to reach its full product vision.

Delivery and validation status: [proposal handoff](execution/FEATURE-PROPOSALS-HANDOFF.md).
