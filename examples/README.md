# Example verification catalog

**PR-001 supplies the first headless runner and platform.bootstrap fixture. Other entries remain planned specifications.** Each implementation PR must supply its matching executable/SDK fixture and keep its specification current. Consult each example and its evidence for implementation state; a specification alone never establishes feature completion.

The common interface for later examples remains proposed:

```sh
omniweft_examples --example objects.atomic --headless --seed 7 --verify --output artifacts/objects.atomic
omniweft_examples --example showcase.world_workshop --interactive --seed 7
```

PR-001 implements the runner shell; each later PR adds its named example. GUI-only behavior uses `--interactive` and a manual scenario alongside automated structural checks. GPU verification uses `--gpu --verify` on a supported real device; `--headless` does not mean GPU coverage. Each runtime result includes explicit `passed`, `failed`, or `not_run` for each lane, plus its environment and exact commit.

| Feature PR | Example specification | Verification lanes |
| --- | --- | --- |
| PR-001 | [platform.bootstrap](platform-bootstrap.md) | cpu |
| PR-002 | [protocol.reject_invalid](protocol-reject_invalid.md) | cpu |
| PR-003 | [objects.atomic](objects-atomic.md) | cpu |
| PR-004 | [render.world_cube](render-world_cube.md) | cpu, gpu |
| PR-005 | [sdk.move_cube](sdk-move_cube.md) | cpu |
| PR-006 | [agents.mock_builder](agents-mock_builder.md) | cpu, gpu |
| PR-007 | [policy.denied_edits](policy-denied_edits.md) | cpu |
| PR-008 | [showcase.ai_blocks](showcase-ai_blocks.md) | cpu, gpu |
| PR-009 | [objects.hierarchy](objects-hierarchy.md) | cpu |
| PR-010 | [observe.semantic_query](observe-semantic_query.md) | cpu |
| PR-011 | [agents.contention](agents-contention.md) | cpu |
| PR-012 | [world.undo_chain](world-undo_chain.md) | cpu |
| PR-013 | [world.replay](world-replay.md) | cpu |
| PR-014 | [world.crash_recovery](world-crash_recovery.md) | cpu |
| PR-015 | [agents.worker_failure](agents-worker_failure.md) | cpu |
| PR-016 | [assets.roundtrip](assets-roundtrip.md) | cpu |
| PR-017 | [meshes.import_gltf](meshes-import_gltf.md) | cpu |
| PR-018 | [render.materials](render-materials.md) | cpu, gpu |
| PR-019 | [meshes.deform](meshes-deform.md) | cpu |
| PR-020 | [meshes.cut_and_restore](meshes-cut_and_restore.md) | cpu |
| PR-021 | [pixels.paint_region](pixels-paint_region.md) | cpu, gpu |
| PR-022 | [pixels.compute_filter](pixels-compute_filter.md) | cpu, gpu |
| PR-023 | [voxels.edit_boundary](voxels-edit_boundary.md) | cpu |
| PR-024 | [voxels.surface_update](voxels-surface_update.md) | cpu, gpu |
| PR-025 | [physics.drop_stack](physics-drop_stack.md) | cpu |
| PR-026 | [physics.agent_push](physics-agent_push.md) | cpu |
| PR-027 | [physics.cook_versions](physics-cook_versions.md) | cpu |
| PR-028 | [physics.edit_active_body](physics-edit_active_body.md) | cpu, gpu |
| PR-029 | [observe.camera_frame](observe-camera_frame.md) | cpu, gpu |
| PR-030 | [agents.provider_bridge](agents-provider_bridge.md) | cpu, optional-provider |
| PR-031 | [world.speculate](world-speculate.md) | cpu |
| PR-032 | [world.merge_conflict](world-merge_conflict.md) | cpu |
| PR-033 | [editor.inspect_transaction](editor-inspect_transaction.md) | cpu, gpu, manual |
| PR-034 | [interaction.pick_and_move](interaction-pick_and_move.md) | cpu, gpu, manual |
| PR-035 | [bench.core_workloads](bench-core_workloads.md) | cpu, gpu, benchmark |
| PR-036 | [quality.hostile_session](quality-hostile_session.md) | cpu |
| PR-037 | [showcase.world_workshop](showcase-world_workshop.md) | cpu, gpu, manual |
| PR-038 | [release.clean_machine](release-clean_machine.md) | cpu, gpu, manual |

Examples use fixed seeds, procedural or explicitly redistributable fixtures, no mandatory credentials, and independent expected values. Assertions must exercise public behavior, not merely repeat implementation formulas. All behavior examples include at least one negative test. Recovery assertions are required whenever the feature adds persistence, asynchronous work or external execution.

Evidence format and lane definitions are in [validation](../docs/VALIDATION.md) and the [evidence template](../docs/templates/EVIDENCE.md). New core capabilities must add catalog entries in the same PR; a general demo is insufficient if it cannot isolate the behavior being added.
