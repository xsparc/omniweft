# Omniweft

**An open-source, AI-centric world and game engine for Windows and Linux, with programmable control of objects, physics, meshes, voxels, and pixels.**

Omniweft treats a virtual world as structured data that people, game systems, and AI agents can inspect and change through the same interfaces. AI can compose a scene, sculpt terrain, modify geometry, paint textures, apply physical actions, and inspect the result. The engine validates and schedules those changes while simulation and rendering continue.

**Status: headless bootstrap, typed objects, Vulkan presentation and authenticated Python SDK merged (PR-001 through PR-005).** The native runner provides a C++20 start/step/stop example and structural command validation with round-trip serialization. PR-003 adds bounded, synchronous create/transform/delete transactions, detached snapshots and complete rollback for in-memory root objects. Its Windows/Linux checks and actual merge are recorded in [the object execution handoff](docs/execution/PR-003-HANDOFF.md). PR-004 adds optional SDL3/Vulkan presentation with verified Windows hardware evidence and passing hosted Windows/Linux builds. PR-005 adds a native authenticated loopback host and separate typed Python SDK; its runtime-expiry correction, independent review and Windows/Linux checks passed. Its maintainer squash merge and all six post-merge checks are verified in [the SDK handoff](docs/execution/PR-005-HANDOFF.md). Physics, persistence and performance targets below remain planned. See the runnable [bootstrap](examples/platform-bootstrap.md), [protocol](examples/protocol-reject_invalid.md) and [atomic object](examples/objects-atomic.md) examples.

## Start here

| Document | Purpose |
| --- | --- |
| [Project brief](docs/PROJECT_BRIEF.md) | Product vision, scope, users, first release, success criteria |
| [Architecture](docs/ARCHITECTURE.md) | Runtime modules, data flow, stack and platform decisions |
| [World model](docs/WORLD_MODEL.md) | Objects, meshes, voxels, pixels, persistence and identity |
| [AI control protocol](docs/AI_CONTROL_PROTOCOL.md) | Observation, transactions, capabilities, concurrency and recovery |
| [Simulation and rendering](docs/SIMULATION_AND_RENDERING.md) | Physics, graphics, collision synchronization and frame budgets |
| [Editor and workflows](docs/EDITOR_AND_WORKFLOWS.md) | Human/AI interaction and an integrated demonstration |
| [Roadmap](docs/ROADMAP.md) | Dependency-ordered implementation PRs and release gates |
| [Examples](examples/README.md) | Feature-by-feature verification contracts |
| [Validation](docs/VALIDATION.md) | Windows/Linux CI, GPU evidence, replay and performance methodology |
| [Autonomous development](docs/AUTONOMOUS_DEVELOPMENT.md) | One-slice PR workflow, review and durable handoff |
| [Open source and licensing](docs/OPEN_SOURCE.md) | Apache-2.0 recommendation, governance and dependency policy |
| [Repository setup](docs/REPOSITORY_SETUP.md) | GitHub configuration and remaining implementation setup |
| [Decisions](docs/adr/README.md) | Architectural choices and reconsideration triggers |
| [Risks](docs/RISKS.md) | Feasibility boundaries and research work |
| [Sources](docs/SOURCES.md) | Primary sources informing the design |

## What “AI control” means

- **Objects:** create, query, move, compose and delete versioned entities and components.
- **Physics:** apply forces, impulses, constraints and approved parameter changes through a conventional solver.
- **Meshes:** edit bounded vertex/attribute selections and, later, topology with explicit revision changes.
- **Voxels:** query, fill, carve and paint sparse regions without creating a rigid body for every cell.
- **Pixels:** address exact texture texels; inspect view pixels; control approved compute and compositing operations.
- **World building:** convert a goal into a previewable, budgeted plan and a sequence of validated changes.

Pixel control means defined image coordinates and programmable bulk operations. A screen pixel belongs to a particular view and frame; it is not a permanent world entity. The design does not require one model call per pixel or put a language model inside the frame loop.

## First usable target

A local sandbox on Windows 11 x64 and Ubuntu 24.04 x64: a minimal editor, rigid bodies, basic physically based materials, editable static voxel terrain, mesh deformation, texture painting, a Python agent interface, and save/replay. All core examples work offline with a scripted AI provider. Cloud providers and downloadable models are optional.

Proposed stack: **C++20, CMake, SDL3, Vulkan 1.3, Jolt Physics and an out-of-process Python SDK**. PR-001 pins the toolchain and actual dependencies of the headless bootstrap. PR-004 pins optional SDL3/Vulkan-Headers sources and notices; Jolt remains deferred until its integration slice. See the [stack decision](docs/adr/0001-runtime-stack.md).

## Build and validate

Build the native shell using the platform-specific instructions in [platform.bootstrap](examples/platform-bootstrap.md). It requires the pinned tools in `toolchains/bootstrap.json`; the executable itself needs no GPU, Python interpreter, provider key or network.

For the optional renderer, use the build and independent verification commands in [render.world_cube](examples/render-world_cube.md); current limitations and evidence are recorded in [its handoff](docs/execution/PR-004-HANDOFF.md).

Planning validation separately requires Python 3.11 or newer and no third-party Python packages:

```sh
python tools/validate_plan.py
```

This checks local Markdown links, required repository files, the roadmap dependency graph, and example coverage. It does not build or validate an engine. Future build/run commands in the planning documents are explicitly marked as proposed.

For the authenticated local SDK candidate, see [sdk.move_cube](examples/sdk-move_cube.md), [SDK usage](sdk/python/README.md) and [its execution handoff](docs/execution/PR-005-HANDOFF.md).

## Contribute

Read [CONTRIBUTING.md](CONTRIBUTING.md), [AGENTS.md](AGENTS.md), and the [Code of Conduct](CODE_OF_CONDUCT.md). Start from an eligible item in the [roadmap](docs/ROADMAP.md). Each core feature ships with its corresponding runnable example, assertions, failure cases, and evidence in the same PR.

The project uses the [Apache License 2.0](LICENSE). First-party examples and documentation use the same license unless explicitly marked otherwise. Models, datasets and third-party assets retain their own licenses. See [THIRD_PARTY.md](THIRD_PARTY.md).
