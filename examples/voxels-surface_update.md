# voxels.surface_update

Status: **planned specification; not implemented**. Work item: **PR-024 — Voxel surface extraction**.

Dependencies: PR-004, PR-023. Validation lanes: cpu, gpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Proposed run command

This command becomes available only when this work item is implemented:

```sh
omniweft_examples --example voxels.surface_update --gpu --seed 7 --verify --output artifacts/voxels.surface_update
```

Also supply a headless structural verification mode and an interactive inspection mode; neither substitutes for the real-GPU lane.

## Pass criteria

Update affected chunk/neighbor surfaces and display them; fixture faces and seams match known occupancy.

## Negative and recovery cases

Boundary edit cannot leave stale neighbor faces; cancellation drops obsolete meshing output.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
