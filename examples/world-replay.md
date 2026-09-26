# world.replay

Status: **implementation under review; not merged**. Work item: **PR-013 — Recorded command replay**.

Dependencies: PR-006, PR-011, PR-012. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Use the pinned CPU build instructions in [platform.bootstrap](platform-bootstrap.md), then run the native example into a fresh directory:

```sh
omniweft_examples --example world.replay --headless --seed 7 --verify --output artifacts/world.replay
```

## Pass criteria

Replay accepted command order from recorded assets/seeds and match exact nonphysics checkpoints.

## Negative and recovery cases

Corrupt log, unknown required schema and missing asset fail before unsafe application. Assert identical resolved entity UUID/generation mappings, not only equivalent transforms.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.

## Implemented boundary

`ow::replay::Recorder` is opt-in at a trusted native owner boundary. It requires an empty revision-zero World with at most eight slots, stores at most eight accepted batches of four operations, bounds each canonical command to 16 KiB and checkpoint to 8 KiB, and rejects full capacity before publication. It records owner-supplied nondecreasing boundary ticks and assigns accepted order; these labels do not reproduce simulation timing, deadlines or admission decisions. Rejected batches consume no record. Unrecorded outside edits invalidate both recording and export.

The Coordinator prepares final snapshot/receipt data and lets the recorder allocate the record before publishing the world or committing the existing staging guard. Publication is noexcept. The typed in-memory log contains operations, budgets, revisions, resolved temporary-ID bindings and exact diagnostic bytes, never original authentication epochs or full request headers. It records the builtin cube's versioned 592-byte content descriptor and SHA-256 identity; the native suite checks that descriptor against the actual compiled CPU mesh positions, colors and indices. There is no general external asset loader.

`reconstruct(log, available_assets)` validates required schemas, inventory and bounds, then replays through the Coordinator into a private fresh World. Recorded creation mappings are consumed in operation order and checked against allocator UUID/generation expectations, including entities created and deleted in the same batch. Every checkpoint must agree before a World is returned. A bad later record returns no partial world. An internally coherent edited log is new offline data, not authenticated provenance: this is neither a signature nor permission to alter a live world. No live session, policy lease, grant or retry receipt is restored. Existing History/policy calls are not automatically captured.

## Independent proof

The seed-7 fixture creates two cubes, tags/moves one, rejects a late invalid transform, reuses both identities with increased generations, and reconstructs every accepted checkpoint. The Python oracle specifies complete operations, receipts, allocator slots, identities, revisions, canonical bytes, asset content and negative/recovery outcomes independently. The native suite additionally verifies observer allocation failure, guard/publication ordering, full-log rejection, bounds, transient create/delete/recreate mappings, hierarchy formats and outside-edit detection. Numeric comparison is exact for this fixture; no tolerance is inferred after execution.

```sh
python tests/replay_oracle.py --executable build/linux-headless/omniweft_examples --native-test build/linux-headless/replay_native_test --evidence artifacts/replay
```

Use the Windows build directory and `.exe` suffixes on Windows. CTest runs `world.replay_native`, `world.replay` and `world.replay_oracle_selftest`. See [the handoff](../docs/execution/PR-013-HANDOFF.md) for actual validation and delivery state. This typed volatile record is not a file format, durable journal, saved world, remote replay endpoint, physics replay or GPU proof; PR-014 remains responsible for durable recovery.
