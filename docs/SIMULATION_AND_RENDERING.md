# Simulation, geometry and rendering

Status: design targets, not benchmarks.

## Simulation clock

Begin with a fixed 60 Hz simulation step, rendering independently with interpolation. Use an accumulator and a capped catch-up policy. If overloaded, report lag and shed authoring work; do not silently change physics timestep or execute unbounded catch-up. Paused editing still has ordered commit barriers without advancing physical time.

Tick order is explicit: admit ready commands; commit validated authoring/physical actions; update kinematic targets; simulate physics; collect/sort relevant events; publish the world snapshot; append replay/checkpoint metadata. User input, agent actions and asynchronous preparation results enter through deterministic ordering records.

## Physics authority

Jolt owns the numerical rigid-body solve behind an adapter. Engine IDs map to private solver IDs; never serialize raw solver pointers or expose them to agents. Start with static triangle terrain and dynamic primitives/convex shapes. Include collision layers, triggers, sleeping, contacts and raycasts before advanced constraints. Continuous collision detection is enabled only for the supported fast-body cases and is demonstrated separately.

Use analytical expectations for isolated force/impulse tests, contact/penetration bounds for stacks, and fixed configuration traces for replay. Numerical thresholds belong to a named fixture and timestep. A result resembling plausible motion is not enough evidence.

Physics planning may use a speculative world or cheap predictor, but predictions carry model/version/error metadata. Neural physics replacement, differentiable simulation and arbitrary learned contact resolution remain research tracks until compared against conventional baselines for stability, error and cost.

## Editing physical geometry

1. Validate the proposed mesh/voxel edit and collider support policy.
2. Stage a new immutable geometry revision and calculate affected neighbors/bodies.
3. Prepare new render resources for a graphical session and cook collision off the simulation thread. A headless session stages canonical geometry plus collision without any GPU dependency.
4. Reject stale, cancelled, invalid or over-budget cook results.
5. At a tick barrier, recheck source revisions and publish the new geometry/collider pair.
6. Apply declared mass/inertia, velocity and wake-state policy; invalidate relevant contact/query caches.
7. Retire the previous resources after solver jobs and GPU fences permit it.

The initial supported dynamic-shape edit is replacement of a primitive/convex collider, with explicit mass recomputation and default velocity reset. Preserving momentum across inertia changes requires its own tested policy. Static terrain edits can affect nearby bodies; detect overlap and apply a documented rejection or bounded depenetration policy, not an unbounded teleport.

For long preparation, continue displaying and simulating the previous committed pair. Receipts distinguish `preparing`, authoritative revision, collision revision and presented frame/revision. This avoids a visible hole whose old collider still blocks the player. A mixed physics/render revision is a test failure for the edited resource, although different frames may legally show earlier complete snapshots.

Physics ticks do not invalidate authored-geometry preconditions. Revalidate authored configuration at commit and then evaluate the current body/overlap policy. An attached or recovered renderer reconstructs resources from a committed snapshot before presenting that revision; a headless commit never waits for a nonexistent render device.

## Renderer

Start with a single Vulkan device, indexed triangle rasterization, depth, camera transforms and a minimal forward path. Add basic metallic/roughness materials, textures and lighting incrementally. Shadowing, transparency ordering, skeletal skinning, post-processing and more elaborate lighting each require explicit examples before becoming public capabilities.

Use a small render graph with declared resource reads/writes, barriers and lifetimes. Geometry and pixel edits update dirty regions, not entire buffers by default. GPU allocations, staging uploads and readbacks have budgets. Resource handles include generation/revision; GPU completion controls lifetime. Device creation validates required features and reports unsupported devices clearly.

For pixel control, v0.1 permits exact texture writes and one predefined compute filter with bounded parameters. Later approved compositing graphs can modify a view's output using storage images and explicit synchronization. The [Vulkan compute guide](https://docs.vulkan.org/tutorial/latest/11_Compute_Shader.html) informs this mechanism. Arbitrary generated shader source is quarantined and never automatically compiled/executed in the default path.

Screen observations include view/frame ID, dimensions, origin, color space, near/far convention, camera matrices, simulation tick and world revision. Depth linearization is explicit. A screenshot does not establish exact underlying world state. Object-ID picking and CPU queries provide independent structural checks.

## Determinism tiers

| Tier | Promise | Evidence |
| --- | --- | --- |
| D0: canonical authoring | Same accepted commands, assets and schema yield the same canonical nonphysics state | Exact hashes with sorted keys, stable IDs and defined numeric encoding |
| D1: reference simulation | Replay on the same pinned build/configuration meets fixed state checks | Fixed timestep/seeds, stable mutation order, supported state restore |
| D2: cross-platform simulation | Only configurations explicitly certified by trace comparison qualify | Windows/Linux compilers, CPU features, Jolt defines and fixtures recorded |
| D3: presentation | Semantic/frame invariants and bounded numerical/image differences | Real-GPU readbacks, tolerant image comparisons, validation output |

D2 is not a v0.1 blanket guarantee. GPU output and external model inference have no universal bitwise guarantee. Jolt documents determinism conditions and ordering caveats in its [architecture documentation](https://jrouwe.github.io/JoltPhysics/). Physical checkpointing must include body lifecycle and engine-owned properties.

## Provisional performance envelope

First nominate reference machines during PR-001: Windows 11 and Ubuntu 24.04, x64, at least 6 physical CPU cores, 16 GiB RAM, SSD and Vulkan 1.3-capable dedicated GPU with 6 GiB VRAM. Record exact hardware/driver details before establishing a baseline. These are proposed development targets, not published minimum requirements.

PR-035 uses a fixed workload: 1080p, 10,000 resident entities with 2,000 visible simple mesh instances, 500 active rigid bodies, 256 resident voxel chunks at 32 cubed, and two editing agents issuing at most 10 bounded authoring batches per second combined. Instance/triangle/material counts are recorded in the fixture; no undocumented procedural complexity.

Initial hypotheses: median frame at or below 16.7 ms, p95 at or below 25 ms; physics p95 below 4 ms; main-thread authoring admission below 1 ms p95; resident process memory below 4 GiB and GPU allocations below 3 GiB. Measure CPU/GPU overlap rather than adding all timings. Keep slow geometry cooking off the frame thread; report completion latency separately.

Warm up for 30 seconds, measure 120 seconds over 5 repetitions with a fixed seed, report p50/p95/p99 and peak memory. Compare only equivalent hardware/configurations. If targets fail, profile and reduce scope or change algorithms through review; never edit thresholds merely to make CI green. AI inference latency and cost are a separate asynchronous budget.
