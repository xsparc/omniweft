# Project brief

Decision date: 2026-09-13. Status: proposed product baseline for implementation.

## Identity and purpose

**Name:** Omniweft. “Weft” describes the threads that form a fabric; the engine exposes the many representations woven into a world. Working repository: `xsparc/omniweft`. A preliminary exact-name web search found no results; this is not trademark clearance.

**GitHub description:** An open-source AI-centric world and game engine for Windows and Linux, with programmable control of objects, physics, meshes, voxels, and pixels.

**Mission:** let humans and AI systems construct, simulate, inspect and revise interactive worlds through a coherent, inspectable interface.

The long-term ambition is a general world engine. The deliverable begins as a bounded developer sandbox. “Ultimate” describes the direction of travel; release gates define what the software actually supports.

## Users and jobs

| User | Job | Observable success |
| --- | --- | --- |
| Game developer | Build AI-assisted tools and gameplay without rewriting engine internals | An agent and a player modify the same scene through supported APIs |
| Technical artist | Generate, inspect and revise mesh, terrain and texture operations | Exact selections, preview, validation feedback and undo are visible |
| AI researcher | Connect a controller to a reproducible environment | Headless observations, action receipts, seeds and replay are available |
| Open-source contributor | Add one capability without learning the whole engine | A module boundary, small PR task, example and test oracle exist |
| Tool builder | Use a local SDK independent of any model vendor | Offline provider and remote-provider adapters use the same protocol |

## Product principles

1. Every supported representation has a documented address, read operation, write operation and validation rule.
2. Human editor commands, scripted gameplay and AI proposals converge on the same mutation path.
3. World state remains authoritative; render buffers, collision caches and semantic indexes are derived views.
4. Agents propose bounded work. The engine decides whether, when and with which resources it can commit.
5. Offline use is complete for the core engine. No account, telemetry or paid model is required.
6. Examples are executable specifications, maintained with the feature they demonstrate.
7. An optional provider failure cannot freeze the simulation or corrupt the saved world.
8. Claims distinguish measured results, design targets and research hypotheses.

## First release scope

The v0.1 developer preview supports one local world and one active editor, stable entity handles, a typed component model, primitive and glTF static meshes, selected vertex edits, sparse block terrain, exact RGBA8 texture edits, rigid-body physics, a camera and basic materials, transactional control, a scripted provider, a Python SDK, local save/load and replay.

World-building starts with a small catalog of procedural primitives, material presets and terrain operations. Natural-language generation is an optional adapter once the reliable control path exists. Imported or generated content is quarantined until validated.

The integrated example is **Living Workshop**: an agent builds a courtyard, sculpts a ramp, places rigid-body blocks, paints a sign and deforms a decorative mesh. The user inspects each operation, drops a ball down the ramp, edits the plan, restores a checkpoint and replays the session. This is a verification scenario, not a complete commercial game.

The full v0.1 roadmap also includes bounded face add/remove operations, local speculative branches and conflict-aware branch merging before the final inspector. These are deliberately limited operations, not general mesh CSG or distributed collaboration. M1 is the earlier usable AI-blocks demonstration; if v0.1 scope must shrink, change its dependency graph and release claims explicitly rather than skipping required predecessors.

## Release acceptance

- A clean checkout builds on both named operating systems using documented presets.
- Living Workshop completes offline through the public interfaces with machine-readable receipts.
- Each included core feature has a headless correctness oracle and a visual example where relevant.
- Rejected, conflicting, over-budget and timed-out operations leave the world consistent.
- Save/load preserves entity identity, assets, terrain and supported physics state; replay has a declared determinism tier.
- Package contents, dependency notices and asset provenance are auditable.
- Required CPU and real-GPU release lanes pass; an unavailable GPU lane blocks the release claim.

See [validation](VALIDATION.md) for thresholds and [roadmap](ROADMAP.md) for milestone gates.

## Deferred scope

Planet-scale streaming, production multiplayer, distributed simulation, fluid/cloth generality, arbitrary live concave-body deformation, neural replacement of collision or integration, generated native plugins, unrestricted generated shaders, path-traced photorealism, consoles, mobile, browser deployment, VR and marketplace services are outside v0.1. Each requires its own evidence-producing research or delivery track. Game audio, animation and packaging basics are planned after the geometry/control foundation; v1.0 cannot claim general game-engine maturity without them.

## Resourcing and decision gates

Suggested responsibility coverage is runtime/graphics, simulation/geometry, tools/AI, and integration/quality, with an independent reviewer. People may cover multiple areas. Agent concurrency is limited by ownership boundaries and available reviewers, not by the number of backlog items.

Do not promise a calendar delivery date before the build, renderer and collision-editing spikes establish throughput. Re-estimate after PR-008 and PR-023 using actual cycle time, defects, GPU access and review capacity. If general engine work overwhelms the team, preserve the AI-addressable sandbox as a useful standalone release and defer broader engine features through an ADR.
