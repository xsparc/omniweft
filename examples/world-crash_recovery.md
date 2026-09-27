# world.crash_recovery

Status: **implementation under review; not merged**. Work item: **PR-014 — Crash-safe persistence and recovery**.

Dependencies: PR-013. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Build using the pinned CPU instructions in [platform.bootstrap](platform-bootstrap.md). Choose a fresh private store directory outside the public output directory; store files contain private epoch data and must not be uploaded as evidence.

```sh
omniweft_examples --example world.crash_recovery --headless --seed 7 --verify --store private-world-crash-store --output artifacts/world.crash_recovery
```

## Pass criteria

Inject termination at journal/snapshot boundaries; recover the last durable commit exactly once with stable entity IDs.

## Negative and recovery cases

Truncated package, exhausted disk quota and hash mismatch preserve the last-known-good world. Crash immediately before/after durable acknowledgement and during compaction; restore durable epoch/watermarks and discover unacknowledged durable edits.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.

## Implemented boundary

The opt-in native Store owns an initially empty World with at most eight slots and accepts at most eight committed batches of four operations. It persists a private owner epoch, full accepted command prefix, resolved creation identities, exact nonphysics checkpoints and four retained durable receipts. Only committed mutations consume sequence keys in this distinct native profile. Existing network/policy/SDK profiles remain volatile.

Canonical identical retained retries return the original durable receipt; changed payloads reject. Evicted keys require resynchronization, with durable high/low watermarks restored before new admission. Any uncertain write outcome seals the Store until reopening and reconciliation. Quota preflight rejects without changing durable data. The full-prefix lifetime bound remains after compaction; compaction reclaims duplicate journal storage rather than granting unlimited history.

Journal frames flush their inline immutable asset and command data before writing/flushing their completion footer. Compaction writes and verifies a replacement checkpoint before atomic publication and journal reclamation. The old checkpoint and journal remain available until publication; no separate historical backup survives reclamation. Corruption of the selected checkpoint fails closed, never silently rolls back acknowledged sequence watermarks.

The guarantee covers abrupt process termination on supported local filesystems with successful native flush calls. It does not establish arbitrary power-loss, storage-controller failure, remote filesystem behavior or automatic repair after post-acknowledgement storage corruption. The format is a first-version bounded native store, not a general world-package migration or authenticated import. No remote durability endpoint, new authority, physics or GPU capability is claimed.

The independent oracle launches distinct native processes and terminates them at 11 storage/acknowledgement boundaries. Before completion-footer writing it requires the earlier prefix; a complete pre-flush frame may recover either complete prefix. After final flush it requires the new commit exactly once. It privately compares epochs across crashes, checks full states/bytes/receipts/watermarks, retries the interrupted key, and tests interrupted compaction after receipt eviction. Logical configured byte-quota testing never fills the host filesystem.

```sh
python tests/persistence_oracle.py --executable build/linux-headless/omniweft_examples --native-test build/linux-headless/persistence_native_test --evidence artifacts/persistence
```

Windows uses the corresponding `build/windows-headless` executables with `.exe` suffix. Publish only the allowlisted JSON evidence reports, never private stores, epoch witnesses or raw subprocess output. See [plan](../docs/execution/PR-014-PLAN.md) and [handoff](../docs/execution/PR-014-HANDOFF.md) for measured validation and delivery status.
