# showcase.ai_blocks

Status: **planned specification; not implemented**. Work item: **PR-008 — Offline AI blocks showcase**.

Dependencies: PR-004, PR-006, PR-007. Validation lanes: cpu, gpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Proposed run command

This command becomes available only when this work item is implemented:

```sh
omniweft_examples --example showcase.ai_blocks --gpu --seed 7 --verify --output artifacts/showcase.ai_blocks
```

Also supply a headless structural verification mode and an interactive inspection mode; neither substitutes for the real-GPU lane.

## Pass criteria

A scripted agent builds and rearranges a visible room through the public SDK; exact object assertions and GPU evidence agree.

## Negative and recovery cases

Reject one intentional invalid plan and continue the scene with the previous state intact.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
