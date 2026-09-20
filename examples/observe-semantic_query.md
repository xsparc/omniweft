# observe.semantic_query

Status: **native implementation under review; not merged**. Work item: **PR-010 — Bounded semantic and spatial queries**.

Dependencies: PR-007, PR-009. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Use the pinned CPU build instructions in [platform.bootstrap](platform-bootstrap.md), then run:

```sh
omniweft_examples --example observe.semantic_query --headless --seed 7 --verify --output artifacts/observe.semantic_query
```

The fixture uses typed create/transform/reparent/tag transactions. Native `ow::observe::Session` takes an immutable host-issued `ReadGrant`; queries cannot supply or widen permissions. The default grant denies all; an explicit whole-world grant or generation-bearing identity allowlist may be narrowed by a read region. That region requires full world-space cube containment. Query AABB filtering instead uses inclusive intersection. Tag filters require every requested tag.

Pages contain UUID-sorted authorized identities, generations, authoring revisions, world transforms, tags and bounds. Hidden parents, local transforms, raw slot positions and unfiltered counts are not projected. World ID and revision are intentionally part of the grant. One bounded snapshot survives world edits during pagination; replay is idempotent. A successful fresh query replaces its cursor, while a rejected query preserves it. Snapshot/grant deadlines are absolute, and close releases authority. `World` must outlive its single-owner session; hosts bound the number of sessions.

Limits: 8 unique authored ASCII identifier tags of 1–32 bytes; 4 query tags; 64 rows/page; 4,096 complete JSON bytes/page; 1 MiB retained projected payload/session, further restricted by the host. Cursor handles are opaque native values, not serialized tokens. Per-session bounds are not a global RSS guarantee. Expired rows are reclaimed on the next call or destruction.

The existing HTTP/SDK/policy profiles reject native tag operations and tagged observations. No query endpoint, new grant, quota, provider, dependency or save migration is introduced. Tagged diagnostic states use `OWOBJ003`; clearing tags restores unchanged `OWOBJ001`/`OWOBJ002` layouts. See [the execution plan](../docs/execution/PR-010-PLAN.md).

## Pass criteria

Return the exact fixture set for tag/AABB filters with stable pagination and permission filtering.

## Negative and recovery cases

Response-size limits and inaccessible objects do not leak data; expired snapshot cursor requires resync.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

The independent [query oracle](../tests/observe_oracle.py) checks literal geometry and exact filtered sets, retained/replayed/fresh pages, byte-budget rejection, expiry recovery, complete tag transaction rollback and independently encoded diagnostic bytes. Native tests compare worlds with changed hidden entities and test generation reuse, exact/one-byte-short response budgets, grant bounds and absolute deadlines. Oracle selftests reject corrupted proof. Geometry tolerance is preregistered at `1e-10` metres/unitless; identities, revisions, tags and bytes remain exact.

For retained evidence, run (append `.exe` on Windows):

```sh
python tests/observe_oracle.py --executable build/linux-headless/omniweft_examples --control build/linux-headless/omniweft_control --agents build/linux-headless/omniweft_agents --policy build/linux-headless/omniweft_policy --tag-legacy build/linux-headless/observe_legacy_fixture --tag-policy build/linux-headless/observe_policy_fixture --native-test build/linux-headless/observe_native_test --evidence artifacts/observe
```

Current verification and limitations are in [the handoff](../docs/execution/PR-010-HANDOFF.md). CPU checks do not establish GPU or physics support. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
