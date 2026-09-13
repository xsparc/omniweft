# PR-004: Vulkan presentation of committed objects

Status: adopted, in progress. Dependency PR-003 was actually merged as `e994f054d1e13ba19f714f35696ec8a1bca221a9`. Coordinator branch `codex/pr-004-vulkan-runtime`, based on contract-only PR #7 squash `b5b8def8cff7109e6e8898b2f4ccc4979346dadb`. The renderer still requires its implementation PR. [Authorization](AUTHORIZATION.md), [example](../../examples/render-world_cube.md), [renderer design](../SIMULATION_AND_RENDERING.md), [runtime ADR](../adr/0001-runtime-stack.md), [handoff](PR-004-HANDOFF.md).

## One observable behavior

Present a cube from an actual committed snapshot and then its transformed subsequent revision through an optional SDL3/Vulkan adapter. The first snapshot remains detached and unchanged. One window, one device, one graphics/present queue, one frame in flight, indexed procedural cube, depth and fixed orthographic camera. No world mutation outside PR-003's typed coordinator. No editor, physics, material system, textures, public picking API, scheduling or persistence.

Core headless targets remain GPU/network/SDK-independent. `ow_presentation` converts a detached snapshot into a bounded, deeply owned packet. Optional `ow_render` consumes that packet and owns all SDL/Vulkan resources. `OW_ENABLE_VULKAN` defaults OFF; optional configuration uses preprovisioned, hash-verified dependency sources and never downloads implicitly.

## Frozen CPU API

Header `include/omniweft/presentation.hpp`, namespace `ow::presentation`:

- `Extent { uint32_t width, height; }`.
- `Camera { array<double,3> position_m{0,0,5}; double left=-4,right=4,bottom=-3,top=3,near_m=.1,far_m=20; }`.
- `Vertex { array<float,4> clip_position; array<float,4> color; uint32_t object_id; }`.
- `Object { uint32_t object_id; string entity_uuid; uint64_t generation,authoring_revision; world::Transform transform; }`.
- `Packet { string world_id; uint64_t world_revision; Camera camera; vector<Object> objects; vector<Vertex> vertices; vector<uint32_t> indices; }`.
- `Packet make_packet(const world::Snapshot& snapshot)`; value types support equality for detached-state tests.

The camera is fixed for this slice, looking along -Z with +Y up. Clip coordinates use a positive-height Vulkan viewport, so Y is inverted during projection; normalized depth is in [0,1]. The input snapshot remains unmodified. Live slots map to ascending UUID object IDs starting at 1; background ID is 0. Each unit cube has 24 per-face vertices and 36 indices. Bound input to 1,024 slots and reject invalid/nonfinite source values or unrepresentable float/clip arithmetic with `std::invalid_argument` before returning a usable packet. Do not clamp or silently render corrupt geometry; a subsequent valid typed transform must recover.

## Frozen fixture and independent oracle

World `workshop`, seed 7, one `builtin.unit_cube`, local coordinates ±0.5. Identity `00000007-0000-4000-8000-000000000001`, generation 1.

1. Typed create/transform commits revision 1: position (-1,0,0), quaternion (0,.6,0,.8), scale (1,1,1).
2. Typed transform commits revision 2: position (1,.25,0), same quaternion, scale (.5,1.5,1).

Initial window 320×240; resize request 400×300. Camera above has orthographic X[-4,4], Y[-3,3], near .1 and far 20. Face colors are literal RGBA8: +X[255,64,64,255], -X[128,32,32,255], +Y[64,255,64,255], -Y[32,128,32,255], +Z[64,64,255,255], -Z[32,32,128,255]; clear[16,20,24,255]. No MSAA, lighting, tone mapping or sRGB conversion. Readbacks are tightly packed top-left-origin little-endian color, R32_UINT object ID and D32_SFLOAT depth.

Independent Python computes rays, inverse TRS and unit-box intersections from literal expected fixture values. It does not derive expectations from native packet geometry or captured pixels. Exclude only a fixed one-pixel Chebyshev-radius-1 band where independent expected face/background labels change. Outside that band, require exact IDs, absolute RGBA8 channel error ≤1 and normalized depth error ≤1e-5. Reject unknown IDs, nonfinite/out-of-[0,1] depth and malformed byte lengths everywhere, including the band. Thresholds precede execution and cannot be tuned after a failure.

Capture phases: `initial`, `transformed`, `resized`, `restored`. Verify exact world/object identity, generation, authoring and world revisions as integers; the corresponding typed receipts and complete snapshots establish actual committed input. Headless structural verification does not count as GPU evidence. A disposable-source mutation replaces snapshot transform conversion with identity TRS and must cause the unmodified GPU oracle to fail its pixel/ID expectation. Retain original/mutant source and binary hashes; no production hooks.

## Rendering, lifecycle and failure policy

Require Vulkan runtime ≥1.3, graphics+present queue, required color/depth/ID features, transfer-destination swapchain usage and a compatible UNORM surface format. SDL provides the loader entry point; first-party dynamic dispatch avoids a mandatory SDK/link-time Vulkan dependency in core.

Require KHR swapchain maintenance1 or equivalent EXT capability with its actual dependencies and enabled feature. Each submission and presentation carries its own completion fence. Fence completion must precede reuse, swapchain replacement and resource destruction. `vkDeviceWaitIdle` alone is not a presentation lifetime proof. Unsupported capabilities produce bounded, actionable startup failure, never a false success or fallback hardware claim.

Render to same-format, same-extent color image, copy its full contents to the acquired swapchain image with `vkCmdCopyImage`, then present that image. Capture that exact source color plus its ID/depth attachments; record source generation, swapchain generation, image index, submission serial and full-copy provenance. Review the command/barrier/fence path independently; an unrelated offscreen readback is insufficient.

Allocation limits: width and height each at most 1,024; at most 786,432 pixels; at most 64 MiB aggregate explicitly owned GPU allocations (attachments, readback, mesh and related buffers); at most three swapchain images. Check integer arithmetic, queried memory requirements and actual image counts before allocation/use. Reject an oversized actual extent without clamping or oversized allocation; a later valid run must recover.

Pump actual OS events. Observe changed pixel extent and new swapchain generation on resize. Observe actual minimized event/state, with no zero-extent allocation or presentation while minimized, then restore and present a valid revision-2 frame. SDL request success alone is not evidence. Finite verification run: at most 120 frames / 30 seconds; interactive inspection is explicit opt-in. Enable core and synchronization validation; passing GPU verification requires zero validation warnings/errors. Missing layer, denied minimize or unavailable device is recorded, not silently passed. Exercise resize with queued work and final shutdown.

Negative startup uses a child-process-only standard Vulkan loader driver filter/absent manifest, never a machine-wide registry or driver change; require controlled nonzero unsupported result and normal startup recovery. Pure CPU capability selection tests cover required feature/format failures separately.

## Report and evidence contract

Native `result.json` top level: `schema_version:1`, example `render.world_cube`, seed 7, `mode:headless|gpu`, `status:passed|failed|unsupported`, allowed device model/API/driver/type, validation enabled/errors/warnings/sanitized messages, frames, window_events and structured errors. Headless marks GPU checks `not_run`.

A GPU frame contains phase, frame_id, world_id/world_revision, object mapping with object_id/UUID/generation/authoring_revision/TRS, camera, pixel_extent, source_generation, swapchain_generation, image_index, submission_serial, present_result and attachments. Copy metadata names `vkCmdCopyImage`, source/swapchain generations and image index, extent, source/destination format and `full_extent:true`. Attachment descriptors contain relative path, format, `origin:top_left` and row_stride_bytes = width×4. Python evidence owns content SHA-256 to avoid adding a native hash dependency.

Public evidence is allowlisted and redacted before publishing: no local username, home/worktree path, hostname, device UUID/LUID/serial, full environment or unrelated process/software data. Only generic necessary device/tool versions, source/artifact hashes, relative paths, validation severity/VUID and sanitized messages. Do not reuse host/path-rich baseline manifests on the personal machine without a safe public export.

## Dependency and build provenance

SDL 3.4.16: upstream commit `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`; official source archive SHA-256 `7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68`. Vulkan-Headers SDK 1.4.357.0: `e3b1eec08173d6b825cd3ac88c885a63b621504a`, runtime minimum remains 1.3 plus required maintenance capability. Pin hashes for every consumed archive and retain upstream license/notice files before optional builds. No moving branch downloads.

Enable C only inside the optional SDL build; pin Clang C to 18.1.3 or the same installed MSVC toolset as C++. Build static SDL with only required video/events/Vulkan support and tests/examples disabled. First-party warnings-as-errors do not alter upstream source.

Checked-in original passthrough shader source and reviewed SPIR-V must have provenance/hashes and an independently validated generation path. Optional shader/validation tools are pinned and stay out of core CI. Prefer a workspace-local tool package without changing system PATH, registry, drivers or installed layers. No proprietary assets or new live-provider dependencies.

## Verification order, ownership and closeout

The maintainer selected this Windows GPU host, requested conservative Linux Docker and then prioritized Windows if necessary. First obtain actual Windows hardware readbacks and lifecycle validation. Attempt Linux container checks with bounded CPU/memory, temporary named containers and no unrelated container changes. Distinguish software Vulkan, hardware Vulkan and Linux desktop evidence; unavailable Linux GPU remains `not_run` and is documented. No blanket cross-platform graphics claim.

Native helper owns C++ headers/sources, CMake/presets, first-party shaders and optional dependency provisioning on isolated `codex/pr-004-code`. Independent test helper owns new presentation/render tests and fixtures on `codex/pr-004-tests`. Coordinator owns ledger/docs/workflows and public evidence export; integrate serially. No parallel writes to shared state.

Required checks: retained Windows/Linux CPU baseline, new structural oracle, optional native builds, Windows GPU independent oracle and deliberate mutation/lifecycle negatives, bounded Linux attempt with honest classification, plan validator/regressions, independent code/tests/evidence/privacy review. Mark done only after the applicable evidence, review and actual maintainer merge. No automatic merge authority is inferred.

Sources: [SDL 3.4.16](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.16), [SDL asynchronous minimize](https://wiki.libsdl.org/SDL3/SDL_MinimizeWindow), [Khronos presentation synchronization](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html), [maintenance extension](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_swapchain_maintenance1.html).
