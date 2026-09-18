# showcase.ai_blocks

Status: **implementation candidate; final validation and review pending**. Work item: **PR-008 — Offline AI blocks showcase**.

Dependencies: PR-004, PR-006, PR-007. Validation lanes: cpu, gpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Run the candidate

Build `omniweft_policy` with the pinned tools as described in [platform.bootstrap](platform-bootstrap.md); add `OW_ENABLE_VULKAN=ON` and the pinned graphics dependencies from [render.world_cube](render-world_cube.md) for the GPU lane. The Python example is the scripted agent in a separate process from the native host. It uses the existing east policy grant without widening permissions.

```sh
python sdk/python/examples/ai_blocks.py --executable build/linux-headless/omniweft_policy --headless --seed 7 --output artifacts/room
python tests/showcase_oracle.py --executable build/linux-headless/omniweft_policy --headless --evidence artifacts/room-proof
```

On Windows, use the built `omniweft_policy.exe`. For the Vulkan build, replace `--headless` with `--gpu`; add `--interactive` to the example to keep the final scene visible until window close or the bounded host lifetime ends. Every output directory must be fresh. The example performs the scenario; the separate oracle establishes its independent correctness. Headless structural checks and interactive inspection do not substitute for physical-GPU evidence.

The fixture contains a floor, back wall, side wall, table and stool in the existing east region. Three transactions assemble revision 3; one rejected transaction stages a valid table move before a forbidden stool move; a corrected two-operation transaction produces revision 4. Every identity, generation, authoring revision and transform has an independently specified expectation. Five live cubes use 1920 of the existing 2048 retained charged bytes. Idle policy queries must show zero outstanding requests/working bytes.

GPU captures hold revisions 3 and 4. Independent pixel-centre rays check every expected interior object/background pixel, excluding only a one-pixel band derived from expected face boundaries. Fixed tolerances are one UNORM color byte and 1e-5 normalized clip depth; they account for raster conversion precision, not changed geometry. IDs and fixture authoring values are exact. Frame metadata must agree with actual immutable publication and completed graphics/presentation work.

## Pass criteria

A scripted agent builds and rearranges a visible room through the public SDK; exact object assertions and GPU evidence agree.

## Negative and recovery cases

Reject one intentional invalid plan and continue the scene with the previous state intact.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Update this document with actual build/run instructions and observed evidence when the implementation merges. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
