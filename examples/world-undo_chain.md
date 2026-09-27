# world.undo_chain

Status: merged in PR-012; post-merge Windows/Linux checks passed. Dependencies: PR-009, PR-011. Validation lanes: cpu.

## Run and verify

Build with `python tools/bootstrap.py`, then run the platform's `omniweft_examples` executable:

```sh
omniweft_examples --example world.undo_chain --headless --seed 7 --verify --output artifacts/world.undo_chain
python tests/undo_oracle.py --executable <build>/omniweft_examples --native-test <build>/history_native_test --evidence artifacts/undo
```

On Windows append `.exe` to executable names. The output directory must be fresh. CTest includes `world.history_native`, `world.undo_chain` and `world.undo_oracle_selftest`.

## Behavior

A native World-bound `ow::history::History` records up to eight batches of four operations, each forward/inverse envelope bounded to 16 KiB. Supported edits are generation-qualified tags and transforms on isolated roots. Creation, deletion, reparenting and hierarchy transforms reject before mutation. There is no remote history endpoint, persistence, editor UI or physical rewind.

Every forward, undo and redo uses the normal transaction Coordinator and the caller's optional live staging guard. Undo/redo takes a header-only envelope with current revision, admission metadata and operation budget; history supplies the private inverse/forward operations. It does not grant authority or implement retry deduplication. A successful branch edit discards redo; a rejected edit leaves world and history intact. Capacity exhaustion rejects until undo/branch or explicit clear makes space.

Both the caller revision and history's recorded head must match the live world. An outside mutation makes history stale. Explicit clear discards history without changing world data, after which fresh edits can be recorded.

## Oracle and recovery

The generated seed-7 two-cube fixture applies three supported batches, undoes all three, redoes all three, rejects physical rewind, rejects an inverse after an outside edit, then clears history and verifies a new edit/undo recovery. The independent Python oracle checks full literal receipts, snapshots, exact authored content, identities, generation/allocator state, history cursors and independently encoded OWOBJ003 bytes at all 15 checkpoints. World and affected entity revisions advance; historical revision counters are never restored. The native suite also checks capacity, branch truncation, failed branch preservation, bounded inputs, generation mismatch, unsupported hierarchy/lifecycle operations and synthetic staging-hook rejection.

Local development validation passed 98 native assertions, 1666 oracle assertions and 14 deliberate report corruptions. The first additional content comparison paired the wrong two checkpoints; correcting that test-only pair made it pass without runtime changes. Clean-candidate and hosted evidence will be recorded in [the handoff](../docs/execution/PR-012-HANDOFF.md). CPU evidence makes no GPU or physics claim.

Rollback is an ordinary revert. This slice changes neither persistent save formats nor remote protocol profiles.

## Adopted acceptance and delivery contract

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Undo/redo a supported edit chain to exact canonical authoring states using inverse transactions.

A stale inverse and a request to undo elapsed physical time fail explicitly.

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.
