# Future game-engine and research tracks

These are explicitly outside the initial 38-PR foundation. Each needs an ADR, bounded PR decomposition and the same feature/example rule before execution. General engine maturity requires more than graphics and AI control.

| Track | Intended capability | First corresponding verification example | Admission gate |
| --- | --- | --- | --- |
| Animation | Skeletons, clips, skinning, blend graphs, AI parameter control | `animation.reach_and_blend`: known joint transforms, interpolation and skinned bounds | Stable mesh/asset pipeline; CPU/GPU skinning comparison |
| Audio | Spatial playback, buses, streaming, agent-triggered events | `audio.spatial_beacon`: timing/channel assertions and listener movement review | Output-device abstraction, licensed fixtures and no required proprietary codec |
| Navigation | Navmesh/voxel routes, moving obstacles and agent steering | `navigation.replan_bridge`: path validity before/after a terrain edit | Geometry revision events and bounded async rebuilds |
| Game scripting | Trusted scripts, lifecycle and reload; later sandboxed extensions | `scripting.reload_controller`: reload without stale entities or hidden privileges | Versioned API and explicit trust boundary |
| Player UI | Retained game UI, text, accessibility and localization | `ui.localized_menu`: keyboard/controller focus, scaling and translated layout | Input and font/asset pipeline established |
| Networking | Server authority, replication, interest management and reconciliation | `network.two_clients`: latency/loss injection and server-consistent outcomes | Explicit conflict/security design; physics determinism policy |
| Editor collaboration | Multi-user presence, branches, permissions and conflict resolution | `collaboration.shared_sculpt`: overlapping edits cannot erase unseen work | Local merge semantics and authenticated remote transport |
| Streaming and LOD | Bounded residency, mesh/voxel LOD and world partitions | `streaming.border_walk`: move across regions while checking seams and memory | Measured bottleneck and durable asset lifecycle |
| Terrain fields | Sparse SDFs, smooth extraction, mesh/voxel conversions | `fields.smooth_cave`: watertightness and declared conversion error | Block terrain baseline and an independent geometry oracle |
| Advanced physics | Constraints, vehicles, CCD cases, cloth/fluids/soft bodies | `physics.constraint_rig` then separate material-specific fixtures | Stability, determinism tier and performance measured per solver |
| Procedural generation | Rule/graph tools, constrained synthesis and provenance | `generation.seeded_village`: walkability, collision, budgets and repeatability | Catalog operations proven; generation does not bypass validation |
| Inference runtime | Optional local ONNX CPU then certified accelerators | `inference.local_policy`: known tensor IO and fallback under load | Per-model license/format review and resource isolation |
| AI evaluations | Goal success, planning efficiency, perception quality and recovery | `evaluation.builder_suite`: held-out tasks with scripted baseline | Engine correctness separate from probabilistic task quality |
| Learned physics | Surrogate predictions or control recommendations | `research.surrogate_drop`: error/stability/cost against solver baseline | No authoritative replacement without explicit evidence and fallback |
| Rendering breadth | Shadows, transparency, skinning, approved postprocess graphs | `render.shadow_room`, `pixels.composite_layers` | Named visual/GPU evidence and lifetime/synchronization checks |
| Sandboxed compute | User-supplied bounded procedural kernels/extensions | `sandbox.runaway_kernel`: timeout/quota/isolation proof | Real sandbox architecture and GPU hang recovery limitations understood |
| Distribution | Installers, crash reporting opt-in, ABI/versioning and release automation | `distribution.upgrade_project`: fresh-machine upgrade and rollback | Stable preview users, maintained compatibility and signing setup |

Research work exits with a measured comparison and a go/no-go decision, not an automatic production feature. Full-resolution per-frame generative rendering, planet-wide destruction and distributed learned simulation are expensive hypotheses. Compare them with conventional systems before committing the engine architecture to them.
