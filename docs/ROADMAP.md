# Incremental implementation roadmap

All 38 items are **proposed**, not authorized engine implementation in this documentation task. PR IDs are stable work-item IDs; they are not GitHub pull-request numbers. The dependency graph permits parallel independent work; number order is a reading aid. The initial documentation bootstrap precedes PR-001.

## M1: Offline AI-addressable scene

**Exit gate:** PR-001 through PR-008 and their CPU/GPU examples pass on both supported platforms.

| Work item | One deliverable | Depends on | Corresponding example |
| --- | --- | --- | --- |
| PR-001 | Buildable Windows/Linux project shell | Bootstrap | [platform.bootstrap](../examples/platform-bootstrap.md) |
| PR-002 | Versioned command schema | PR-001 | [protocol.reject_invalid](../examples/protocol-reject_invalid.md) |
| PR-003 | Atomic object lifecycle and transforms | PR-002 | [objects.atomic](../examples/objects-atomic.md) |
| PR-004 | Vulkan presentation of world objects | PR-003 | [render.world_cube](../examples/render-world_cube.md) |
| PR-005 | Authenticated local Python SDK | PR-003 | [sdk.move_cube](../examples/sdk-move_cube.md) |
| PR-006 | Fixed-step loop and scripted provider | PR-004, PR-005 | [agents.mock_builder](../examples/agents-mock_builder.md) |
| PR-007 | Capabilities and resource budgets | PR-005, PR-006 | [policy.denied_edits](../examples/policy-denied_edits.md) |
| PR-008 | Offline AI blocks showcase | PR-004, PR-006, PR-007 | [showcase.ai_blocks](../examples/showcase-ai_blocks.md) |

## M2: Reliable authoring and recovery

**Exit gate:** Queries, concurrent edits, authoring undo, replay, durable recovery and worker failures pass.

| Work item | One deliverable | Depends on | Corresponding example |
| --- | --- | --- | --- |
| PR-009 | Hierarchy and reparenting | PR-003 | [objects.hierarchy](../examples/objects-hierarchy.md) |
| PR-010 | Bounded semantic and spatial queries | PR-007, PR-009 | [observe.semantic_query](../examples/observe-semantic_query.md) |
| PR-011 | Concurrency and idempotent retries | PR-007, PR-010 | [agents.contention](../examples/agents-contention.md) |
| PR-012 | Authoring undo and redo | PR-009, PR-011 | [world.undo_chain](../examples/world-undo_chain.md) |
| PR-013 | Recorded command replay | PR-006, PR-011, PR-012 | [world.replay](../examples/world-replay.md) |
| PR-014 | Crash-safe persistence and recovery | PR-013 | [world.crash_recovery](../examples/world-crash_recovery.md) |
| PR-015 | Agent lifecycle and cancellation | PR-007, PR-011, PR-014 | [agents.worker_failure](../examples/agents-worker_failure.md) |

## M3: Geometry and pixel control

**Exit gate:** Imported meshes, bounded topology, exact texels and cross-boundary voxels have independent correctness oracles.

| Work item | One deliverable | Depends on | Corresponding example |
| --- | --- | --- | --- |
| PR-016 | Content-addressed assets and provenance | PR-003, PR-014 | [assets.roundtrip](../examples/assets-roundtrip.md) |
| PR-017 | Bounded glTF mesh import | PR-016 | [meshes.import_gltf](../examples/meshes-import_gltf.md) |
| PR-018 | Materials and texture rendering | PR-004, PR-016, PR-017 | [render.materials](../examples/render-materials.md) |
| PR-019 | Versioned mesh vertex edits | PR-017 | [meshes.deform](../examples/meshes-deform.md) |
| PR-020 | Bounded mesh topology edits | PR-012, PR-019 | [meshes.cut_and_restore](../examples/meshes-cut_and_restore.md) |
| PR-021 | Exact texture region edits | PR-016, PR-018 | [pixels.paint_region](../examples/pixels-paint_region.md) |
| PR-022 | Approved GPU image kernel | PR-021 | [pixels.compute_filter](../examples/pixels-compute_filter.md) |
| PR-023 | Sparse voxel storage and edits | PR-003, PR-016 | [voxels.edit_boundary](../examples/voxels-edit_boundary.md) |
| PR-024 | Voxel surface extraction | PR-004, PR-023 | [voxels.surface_update](../examples/voxels-surface_update.md) |

## M4: Physical edits and observations

**Exit gate:** Supported physical geometry publishes coherently; observations and offline provider-adapter validation pass.

| Work item | One deliverable | Depends on | Corresponding example |
| --- | --- | --- | --- |
| PR-025 | Rigid-body solver adapter | PR-003, PR-013 | [physics.drop_stack](../examples/physics-drop_stack.md) |
| PR-026 | Forces, impulses and body state | PR-007, PR-025 | [physics.agent_push](../examples/physics-agent_push.md) |
| PR-027 | Versioned asynchronous collider cooking | PR-017, PR-020, PR-024, PR-025 | [physics.cook_versions](../examples/physics-cook_versions.md) |
| PR-028 | Atomic publication of physical edits | PR-012, PR-026, PR-027 | [physics.edit_active_body](../examples/physics-edit_active_body.md) |
| PR-029 | Camera, depth and object observations | PR-010, PR-018, PR-024 | [observe.camera_frame](../examples/observe-camera_frame.md) |
| PR-030 | Optional model-provider adapter | PR-005, PR-007, PR-015, PR-029 | [agents.provider_bridge](../examples/agents-provider_bridge.md) |

## M5: Developer preview v0.1

**Exit gate:** Living Workshop, recovery/fuzz checks, measured performance report and clean-machine CPU/GPU package checks pass.

| Work item | One deliverable | Depends on | Corresponding example |
| --- | --- | --- | --- |
| PR-031 | Isolated speculative world branches | PR-014, PR-016, PR-028 | [world.speculate](../examples/world-speculate.md) |
| PR-032 | Explicit branch diff and merge | PR-011, PR-031 | [world.merge_conflict](../examples/world-merge_conflict.md) |
| PR-033 | Minimal inspector and plan preview | PR-008, PR-010, PR-032 | [editor.inspect_transaction](../examples/editor-inspect_transaction.md) |
| PR-034 | Human interaction through command API | PR-004, PR-009, PR-025, PR-029 | [interaction.pick_and_move](../examples/interaction-pick_and_move.md) |
| PR-035 | Reproducible performance harness | PR-022, PR-024, PR-028, PR-029 | [bench.core_workloads](../examples/bench-core_workloads.md) |
| PR-036 | Adversarial operation and recovery harness | PR-014, PR-015, PR-020, PR-021, PR-023, PR-028, PR-032 | [quality.hostile_session](../examples/quality-hostile_session.md) |
| PR-037 | Living Workshop integration | PR-028, PR-030, PR-032, PR-033, PR-034, PR-036 | [showcase.world_workshop](../examples/showcase-world_workshop.md) |
| PR-038 | Developer-preview packaging | PR-035, PR-037 | [release.clean_machine](../examples/release-clean_machine.md) |

## How to execute the plan

Each linked example specification contains the success oracle, negative case, proposed command, validation lanes and rollback/evidence contract for its PR. The [machine-readable backlog](../planning/backlog.json) is the source for IDs, dependencies, acceptance and coverage; update both it and the linked specification when scope changes. The planning validator checks their alignment.

The merge queue normally contains one independently reviewed PR per behavior. Separate agent worktrees may advance independent dependency-ready items, but may not race on schemas, ownership or generated files. Before executing an item, adopt an execution scope and record its ready state under the [autonomous workflow](AUTONOMOUS_DEVELOPMENT.md). Approval can cover a milestone or a listed batch; it does not require interrupting the user for every routine substep.

At kickoff, refine any oversized item into bounded children that keep a stable parent ID and explicit evidence. Import, renderer, geometry-publication, editor and packaging items are likely split candidates if integration work expands. Keep the same example contract, but add separate acceptance evidence for each child. Do not hide multiple architectural changes in a nominally small PR.

PR-030 verifies an adapter with recorded/offline inputs; live provider credentials or quality are not a v0.1 gate. PR-028 supports static terrain and tested primitive/convex replacements; arbitrary dynamic concave deformation stays out of scope. Simulation branches in M5 are bounded local worlds, not multiplayer replication.

## Critical path and checkpoints

The first visible result follows PR-001 -> PR-002 -> PR-003 -> renderer/SDK -> fixed-step provider -> policy -> PR-008. After M2, mesh/asset, voxel and physics work can overlap when shared contracts are stable. Physical geometry publication joins those tracks at PR-027/028. M5 joins reliability, editor and representation work into a usable developer preview.

Re-estimate after M1 and the first voxel/collider prototype. Track actual PR lead time, failed examples, review backlog and GPU availability. No calendar estimate is an acceptance criterion. If a milestone is too broad, release a clearly labeled earlier sandbox milestone instead of claiming unverified v0.1 coverage.

## Definition of done for every engine feature

- The public API/schema and its address/revision semantics are documented.
- The named example runs from a clean checkout and through public interfaces.
- Success, rejection and relevant recovery assertions pass; visual features include real-GPU evidence.
- Both target OSes pass the applicable CPU lanes, with GPU limitations recorded separately.
- An independent review checks implementation and the example's ability to catch a regression.
- Exact-commit evidence, dependency/asset notices and migration notes accompany the PR.
- Required checks and adopted merge policy pass; the merged commit and handoff are recorded.

See [future tracks](FUTURE_TRACKS.md) for general game-engine breadth and research features beyond this foundation.
