# assets.explain_retention

Status: implemented in [draft PR #30](https://github.com/xsparc/omniweft/pull/30); validation and integration are tracked in the [handoff](../docs/execution/FP-008-HANDOFF.md). Work item: **FP-008 — Asset retention and impact inspector**. See the [plan](../docs/execution/FP-008-PLAN.md) and [asset contract](../docs/ASSETS.md).

The seed-7 native fixture inspects two provenance-distinct assets sharing a blob and an independent third asset. Current, multiple history and empty history sets expose every retention reason. Removing reasons changes candidate sets; old revisions reject and a fresh query recovers. Reconstructing a private Catalog through ordinary import and root commands compares the predicted IDs with actual collection. Queries and detached-report edits preserve the original full snapshot and exported bytes.

```sh
omniweft_examples --example assets.explain_retention --headless --seed 7 --verify --output artifacts/assets.explain_retention
python tests/retention_oracle.py --executable <build>/omniweft_examples --native-test <build>/retention_native_test --evidence artifacts/retention
```

Use `.exe` on Windows and new output/evidence directories. Build with the pinned [bootstrap instructions](platform-bootstrap.md). The example writes synthetic JSON; the Python oracle independently derives fixture hashes and expected states, checks exact types and fields, and tests corrupt reports. This read-only native API grants no new authority and does not infer World or undo references. CPU evidence is not GPU, persistence or physics evidence.

## Adopted acceptance contract

The initial source passed all 51 local Windows CTests. After the documented test-only Debug injector repair, all four affected Release checks passed and the corrected native test passed in Debug and Release. Corrected clean Windows evidence retained 3,895 oracle and 973 native assertions with an [independently audited archive](../docs/evidence/FP-008/README.md). The corruption selftest rejected 41 reports. Hosted Windows/Linux evidence is tracked through the draft's latest checks and handoff; no merge is claimed.

Dependencies: PR-016. Validation lanes: cpu.

Add only native read-only retention inspection using existing Catalog limits and owner-held roots; preserve collection, commands, authority and bundle formats.

Return complete sorted root reasons, shared-blob retainers and exact collection candidates at the expected Catalog revision; verify prediction against ordinary collection in a fresh Catalog.

Reject stale expected revisions without a report or mutation; re-query after root changes and recover the exact new removal sets.

Retain source-bound complete fixture states, literal independent oracle results, negative and recovery reports, native boundaries and Windows/Linux verification.

Remove the additive inspector through a PR while preserving existing asset bundles and collection behavior.
