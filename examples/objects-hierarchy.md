# objects.hierarchy

Status: **implemented candidate; maintainer merge pending**. Work item: **PR-009 — Hierarchy and reparenting**. Dependencies: PR-003. Validation lanes: cpu.

## Run

Build with the pinned Windows or Linux headless preset from the [bootstrap instructions](platform-bootstrap.md), then use a fresh output directory:

```sh
omniweft_examples --example objects.hierarchy --headless --seed 7 --verify --max-slots 8 --output artifacts/objects.hierarchy
```

The native example creates a parent P and child C, attaches C while preserving its local transform, reparents it under Q while preserving its world transform, rejects a cycle, moves Q and detaches C while preserving its local transform. `result.json` contains actual receipts, complete before/after snapshots and diagnostic canonical bytes. Repeatable `--input <json-file>` runs bounded custom native fixtures (at most 64 files). The CLI verification flag checks receipts and stable encoding; the separate oracle establishes correctness.

## Public contract

Native `entity.reparent` has exactly `type`, `target`, `parent` and `mode`. Target and non-null parent use an earlier transaction-local temporary name or a same-world UUID plus generation. A null parent detaches. Modes are `preserve_world` and `preserve_local`. Both `Entity.transform` and `transform.set` remain world-space; parented entities also store authored local TRS and a generation-bearing parent link.

An entity used as a parent requires positive exactly uniform world scale. Root/leaf signed or nonuniform scales remain valid. This restriction keeps composition within TRS without silently introducing shear. Derived quaternion products are normalized. Nonfinite composition/inversion and scale underflow to zero reject the whole transaction. Cycle checks use each staged prefix, and traversals are iterative within the existing 1..1024 slot bound. Deleting a parent with live children rejects; explicitly detach/delete children first. Transform/reparent advances all traversed subtree resource revisions to the next authoring revision. Rejected prefixes preserve state, allocation, revisions and diagnostic bytes.

The existing remote control and policy profiles reject reparent operations before owner scheduling or sequence consumption, including otherwise valid mixed batches. Native hierarchy snapshots supplied through host callbacks are rejected by remote observe/runtime serialization. Policy typed admission rejects reparent before serialization/staging and rejects worlds that already contain hierarchy. SDK models, advertised remote operations, grants, quotas and admission leases are unchanged. Remote hierarchy is unsupported until a compatible observation and authorization contract is implemented.

## Independent verification

The three `objects.hierarchy*` CTests cover the runnable example, complete independently authored snapshots/receipts/bytes, ten deliberate oracle corruptions, typed native math and recovery, and actual authenticated transport on legacy, agents and policy hosts. The native suite includes rational noncommuting rotation, both reparent/detach modes, multilevel and 80-level propagation, cycle/delete/stale-generation failures, staged allocation rollback against a control world, invalid scale and numerical overflow/underflow, strict parser round trips, policy admission and packet vertices derived from the cached world transform.

The preregistered tolerance for derived double fixture values is 1e-10 meters/unitless. Float packet corners use the existing 2e-6 clip-space tolerance. Identity, revision, state shape, rollback and canonical fixture bytes compare exactly. CPU packet checks do not establish physical-GPU behavior. No physical bodies exist yet, so dynamic-body parenting and inherited collider-scale restrictions remain deferred.

See [the execution plan](../docs/execution/PR-009-PLAN.md) and [handoff](../docs/execution/PR-009-HANDOFF.md) for candidate verification and delivery. No dependency or asset was added. Revert the PR for source rollback; these diagnostic bytes are not a save format or migration.

## Adopted roadmap criteria

Preserve-world and preserve-local reparent operations give independently calculated transforms.

Reject cycles and ambiguous parent deletion; stale child handles do not target reused storage. Once physical bodies exist, reject dynamic-body parenting and inherited collider scaling.

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.
