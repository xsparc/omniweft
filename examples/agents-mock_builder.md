# agents.mock_builder

Status: **implementation in progress; required runtime/GPU verification and merge pending**. Work item: **PR-006 — Fixed-step loop and scripted provider**.

Dependencies: PR-004, PR-005. Validation lanes: cpu, gpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Use the pinned build instructions in [platform.bootstrap](platform-bootstrap.md). The SDK/provider uses only Python's standard library; no provider key or package install is required.

~~~sh
cmake --preset windows-headless
cmake --build --preset windows-headless --parallel 2
python sdk/python/examples/mock_builder.py --executable build/windows-headless/omniweft_agents.exe --headless --seed 7 --verify --output artifacts/agents.mock_builder
~~~

For actual GPU verification, build the optional Vulkan adapter using the pinned [rendering instructions](render-world_cube.md), select that build's omniweft_agents executable and use --gpu in place of --headless. Add --interactive for inspection until window close or the bounded host lifetime. The independent oracle, rather than inspection alone, establishes the GPU lane. Linux uses its corresponding preset and unsuffixed executable.

The Python launcher owns a separate fixed scripted-provider process and a native agents host. It holds the provider at explicit barriers while an independent client observes fixed-step progress, then releases three SDK create/transform batches. Live GPU inspection presents the same committed state. This actual Python entry point replaces the earlier proposed native example command.

See the [frozen execution contract](../docs/execution/PR-006-PLAN.md) for exact fixture values, canonical hashes, timing arithmetic, privacy boundaries and compatibility limits. [The handoff](../docs/execution/PR-006-HANDOFF.md) records current verification; commands here are not proof that the candidate passed.

## Pass criteria

A fixed seed builds the same known object arrangement and canonical authoring hashes through the public SDK.

## Negative and recovery cases

Provider delay does not stop fixed-step progress; capped catch-up reports overload.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
