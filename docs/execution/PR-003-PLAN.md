# PR-003 slice plan

Work item: **PR-003 — Atomic object lifecycle and transforms**, under [adopted scope](AUTHORIZATION.md). Base: PR-002's actual squash merge `32e32ae19b5138231159fe961246dbcf5c8a88c2`, verified on GitHub at 2026-09-12T23:11:17Z. Coordinator branch: `codex/pr-003-atomic-objects`.

## Outcome and boundaries

Implement one synchronous, single-owner-thread authoring boundary: a typed batch creates, transforms and deletes bare root objects atomically. Full-state rollback includes live entities, tombstones, generations, allocation selection and authoring revision. No renderer, physics body, hierarchy, asynchronous preparation, transport, authentication, receipt deduplication, session/epoch admission, scheduling queue, tick advancement or crash durability is delivered.

The trusted host invokes the immediate boundary. Structural idempotency and scheduling fields remain metadata for later admission layers; this internal call must not be exposed as an authenticated gateway endpoint or advertised as an exactly-once/scheduled API. Existing schema byte/depth/operation limits are enforced again for typed callers. Engine storage has an independent hard capacity of at most 1,024 slots, including deleted and retired slots.

## Module and public API contract

- `ow_world` owns independent world configuration, transforms, entities, slots, detached snapshots and canonical bytes. It depends only on the standard library, not commands.
- `ow_transactions` owns the sole mutation coordinator, depending on `ow_world` and the existing pure structural `ow_commands` API. The world grants the coordinator private access; clients get no mutable world/entity pointer or replacement-state entry point.
- Public world construction takes world ID, uint32 seed and a max-slots limit from 1 through 1,024 (default 1,024). Invalid host configuration fails before a world is usable. Only the procedural `builtin.unit_cube` prefab is executable.
- Coordinator invocation is synchronous `apply_at_boundary(World&, const commands::Envelope&)` or an equivalent bound coordinator method; implementation/tests agree exact header spellings before code publication. A detached `Snapshot` is a deep value copy and cannot mutate the world.
- Receipt: `status` is `committed` or `rejected`; `durability` is `volatile`; transaction ID and resulting/current world revision are explicit. Resolved temporary-to-durable bindings are returned only on commit. They describe creation history, including an object deleted later in the same committed batch, and do not promise current liveness. Structured errors retain code/path and an operation index when attributable.
- Validate typed input before staging, verify world and expected authoring revision, copy complete bounded state, apply operations in order to that copy, build the complete successful receipt, then publish with a nonthrowing swap. Increment world revision exactly once per successful batch, including an authored no-op; rejected work leaves it unchanged. No allocating operation follows publication.

## Identity, deletion and transforms

Each allocated slot has an immutable canonical lowercase UUID, generation initially 1, retirement flag and optional live entity. Allocate the lowest free nonretired slot, otherwise append while below capacity. There is no allocator state outside the copied slot vector/configuration. UUID = 8 lowercase hex digits of the uint32 seed followed by `-0000-4000-8000-` and 12 lowercase hex digits of one-based slot number. Seed 7's first slot is `00000007-0000-4000-8000-000000000001`. These are deterministic world-local fixture identities, not random/security tokens; world ID is part of the durable identity.

Deletion clears the entity and increments generation. If generation is already uint64 maximum, retire the slot instead of wrapping. Reuse keeps its UUID and uses the incremented generation. Unknown UUIDs fail `NOT_FOUND`; known deleted slots, mismatched generations and retired slots fail `STALE_HANDLE`. Check target world before resolving UUID/generation; a wrong envelope or target world fails `NOT_FOUND` without exposing another world's contents.

Temporary names are unique for the entire batch and resolve only earlier successful creates in that batch. Duplicate names fail `INVALID_SCHEMA` at the create temporary_id path. Forward, unknown and prior-transaction names fail `NOT_FOUND`; after-delete names resolve their saved durable binding and fail `STALE_HANDLE`, even if the slot has been reused. Rejecting any later operation discards all provisional bindings.

Every live entity stores prefab, all ten transform components and `authoring_revision`, set to the next world revision on create or transform. Default transform is position [0,0,0], quaternion [0,0,0,1], scale [1,1,1]. Executable transforms require finite components, nonzero scale components (negative scale is permitted for these bare visual roots), and quaternion squared norm within 1e-12 of one. Reject invalid transforms with `INVALID_SCHEMA` at the corresponding vector path, and non-finite typed values with the existing `NONFINITE_VALUE` path. Preserve quaternion values; do not silently normalize. Normalize negative zero when storing/encoding. Required inverse, hierarchy, renderer and collider-specific constraints remain later slices.

Missing prefab fails `NOT_FOUND`; capacity/operation budgets and revision exhaustion fail `BUDGET_EXCEEDED`; stale expected revision fails `REVISION_CONFLICT`. Maximum revision/generation guards receive independent source review; unreachable-at-fixture-scale overflow is not represented as runtime-tested by injecting privileged mutable state.

## Additive schema extension

Add `entity.delete` under protocol 0.1, with exactly `type`, `target` and required `child_policy: "reject_if_children"`. Target uses the existing temporary/durable shapes. All current objects are roots; no recursive/reparent mode is implemented. Existing envelope meanings and parser numeric rules remain unchanged. PR-002-era readers reject the new operation as `UNSUPPORTED_OPERATION`; this is an additive authoring operation before an external server/SDK release. Extend the machine schema, native variant and independent parse/serialize fixtures in this PR. Schema-valid zero scale/nonunit quaternions remain structurally valid; execution now rejects them under the rules above.

## Canonical diagnostic state encoding

This is version 1 of a diagnostic authoring-state encoding, not a world save/journal format or compatibility promise. Snapshot JSON exposes format version, world ID, seed, max_slots, world_revision and every slot in ascending UUID order, including free/retired slots. Live entity data includes prefab, authoring_revision and full transform.

Canonical bytes are exactly: ASCII `OWOBJ001` (8 bytes); world ID as uint32 little-endian byte length plus ASCII bytes; seed uint32 LE; max_slots uint32 LE; world_revision uint64 LE; slot count uint32 LE. For each slot in ascending UUID order: UUID as exactly 36 ASCII bytes; generation uint64 LE; retired flag as one byte 0/1; live flag as one byte 0/1. If live: prefab as uint32 LE length plus ASCII bytes; entity authoring_revision uint64 LE; position[3], rotation_xyzw[4], scale[3] as ten IEEE-754 binary64 little-endian values, with negative zero normalized to positive zero. Unsupported floating representation is rejected at build time. Native output emits these bytes as lowercase hex; Python independently encodes expected bytes and computes SHA-256 without introducing a native hash dependency.

## Example, oracle and recovery

Keep exact public command: `omniweft_examples --example objects.atomic --headless --seed 7 --verify --output artifacts/objects.atomic`. Preserve existing platform/protocol commands and fixture bytes. The output directory must be new. Native/example and independent tests agree a report containing actual receipts, before/after detached snapshots and canonical bytes; no fake rollback counters.

The independent oracle verifies literal expected successful states after two creates/transforms in one transaction. A structurally valid failing batch modifies existing A, creates C, deletes B, then fails on an invalid final temporary reference; compare the complete before/after state and canonical bytes, identities/count, resource revisions and world revision. Pair an experimental world with an identical control world: only one receives the failed batch, then both receive the same successful create and must allocate identical identities and end in identical state. Test deletion/stale handles before and after slot reuse, duplicate/forward/prior-transaction/after-delete temporary references, wrong world, stale revision, capacity, invalid execution transforms and successful recovery. A native fixture checks detached snapshot isolation.

A real mutation proof modifies a disposable source copy to leak staged state on rejection and must be caught by the original independent full-state oracle; preserve original source hashes. No production mutation hook, direct-state test seeding or weakened normal check is allowed. Independent schema fixtures cover the additive delete operation.

## Delivery and ownership

Coordinator owns schema, ledger, documentation and workflow. Native helper owns world/transaction/command/example C++ files, headers and CMake on `codex/pr-003-code`. Independent test helper owns `tests/objects*`, object fixtures and the needed additive protocol tests on `codex/pr-003-tests`. Both branch from this contract commit; integration is serialized.

GitHub APIs and hosted CI are the reliable execution path. One local git status read succeeded, but subsequent worktree/metadata calls did not return; do not claim local test execution or alter preserved worktrees. Required evidence: Windows/Linux native CTest and independent oracles/mutation proof, planning validation/regressions, exact tested candidate and artifacts, independent source/tests/evidence review, then actual maintainer merge. No automatic merge authority is inferred from the previous maintainer merge.
