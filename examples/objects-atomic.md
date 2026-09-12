# objects.atomic

Status: **implementation in progress; verification and merge pending**. Work item: **PR-003 — Atomic object lifecycle and transforms**.

Dependencies: PR-002. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Build with the pinned instructions in [platform.bootstrap](platform-bootstrap.md). The same native executable exposes:

```sh
omniweft_examples --example objects.atomic --headless --seed 7 --verify --output artifacts/objects.atomic
```

The example emits actual receipts, complete detached snapshots and canonical authoring bytes for successful and rejected typed batches. [The slice plan](../docs/execution/PR-003-PLAN.md) fixes the slot/generation allocator, execution invariants, error policy and binary encoding before implementation. Existing output must be preserved; choose a fresh directory.

## Contract boundaries

All current objects are bare roots with the procedural `builtin.unit_cube` prefab. A batch stages complete bounded state, including deleted slots/generations, then publishes once. Root transforms require finite values, a unit quaternion within the declared tolerance and nonzero scale; negative scale is allowed. No hierarchy, collision, renderer or physics state is implied.

The host invokes a synchronous, single-owner-thread boundary. `committed` means visible in memory with `durability: volatile`; this slice supplies no transport authentication, epoch admission, receipt deduplication, queued scheduling or persistence. Diagnostic canonical bytes are versioned independently of future world saves.

## Pass criteria

Create two entities and apply transforms in one transaction. Then modify a pre-existing object's transform before an invalid final operation and assert that the entire canonical authoring state, entity identities/count and revision match the pretransaction snapshot.

## Negative and recovery cases

Deleted generations and invalid temporary references fail without partial creation.

The decisive failure batch first changes an existing transform, creates another object and deletes an existing object, then fails on a schema-valid final reference. Compare the entire before/after state and independently encoded canonical bytes. A control world with the same successful history checks that the next allocation is also unchanged. Deletion/reuse, transaction-local temporary references, stale revisions, capacity failures and subsequent successful recovery exercise the same public typed path. No mutable-state seeding or fake rollback counter establishes these claims.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

The [handoff](../docs/execution/PR-003-HANDOFF.md) records actual candidate checks and review. Required Windows/Linux native and planning checks are pending until implementation integration. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
