# Ten additional feature proposals: procedural content and gameplay tools

Status: **proposed, unimplemented, and not adopted for autonomous execution**. This fourth batch answers the maintainer's 2026-10-07 request for ten new ideas in a draft PR. It extends [FP-001–010](FEATURE_PROPOSALS.md), [FP-011–020](FEATURE_PROPOSALS_2.md) and [FP-021–030](FEATURE_PROPOSALS_3.md). It does not change the adopted [PR-001–038 roadmap](ROADMAP.md), its dependencies or release gates. Merging this document records proposals, not permission to implement them.

Baseline: main at `7d8f6711a7a9a86a2150d5e9fac627f47505a66d`. PR #27 was first squash-merged into PR #25's branch, then PR #25 integrated all thirty earlier proposals into main. The final tree matches reviewed delivery `d89763267b2f533aef7a860b5c05b24b710d4ba3`. See [README](../README.md) for actual engine support and the [fourth-batch handoff](execution/FEATURE-PROPOSALS-4-HANDOFF.md) for verified merge and delivery evidence. The earlier proposal handoffs are historical checkpoints.

## Candidate overview

IDs FP-031–040 are proposal identifiers, not new backlog items or GitHub PR numbers. All ten have status `proposed`. Effort is a relative planning estimate.

| ID | Feature | User benefit | First-slice effort / main risk |
| --- | --- | --- | --- |
| FP-031 | Symmetric object placement | Arrange opposite sides of a scene precisely | Small / placement versus geometry reflection |
| FP-032 | Explicit mesh normal generation | Rebuild a predictable smooth shading attribute | Medium / degenerate or cancelling geometry |
| FP-033 | Offline heightfield relaxation | Shape a terrain sketch with bounded erosion-like steps | Medium / integer conservation and update order |
| FP-034 | Edge-matched tile assembly | Compose a layout from compatible authored pieces | Medium / bounded search versus no-solution claims |
| FP-035 | Typed turtle drawing recipes | Generate branching line designs without scripts | Small / stack and output bounds |
| FP-036 | Bounded skeletal pose retargeting | Reuse joint rotation changes on a differently proportioned rig | Medium / rest-frame conventions |
| FP-037 | Offline sidechain audio ducking | Lower background sound while a foreground signal is active | Medium / sample timing and rounding |
| FP-038 | Branching dialogue sessions | Offer explicit choices and inspect a conversation path | Medium / stale choices and finite progression |
| FP-039 | Atomic inventory transfers | Move and split item stacks without losing or duplicating counts | Medium / conservation and owner authority |
| FP-040 | Deterministic particle emitters | Preview repeatable particle motion for authored effects | Medium / lifetime and slot reuse |

## Shared implementation boundary

Procedural generation, animation and audio are already [future tracks](FUTURE_TRACKS.md); these are additional concrete first-slice proposals, not claims of industry novelty. Each would require its own adopted plan, runnable example and independent review before implementation. Proposed ceilings below are not current API contracts.

All input and output work is bounded. Typed definitions carry a profile version and immutable content identity; mutable owners use generation-safe handles and explicit expected revisions. Reject invalid input or a failed precondition before publishing partial output or changing authoritative state. Define numeric domains, rounding, tolerances and canonical encodings before coding. Floating-point tolerance evidence does not imply bit-identical output.

Detached tools do not gain World or remote authority. World edits go through the normal typed Coordinator with existing admission, revision and quota rules. Other state owners need their own explicit typed API. Text, item kinds, recipe opcodes and content hashes never confer capabilities. No generated code, live model keys, OS input injection, new dependency, save format or remote endpoint is included.

After adoption, actual Windows/Linux CPU checks must cover success, negative and recovery examples. Rendering, audio-device, collision and physics claims require separate applicable evidence. No prototype, runtime test, benchmark, user study or device evidence exists for these proposals. The example names below describe future contracts.

## FP-031 — Symmetric object placement

**Value and distinction.** Arrange existing objects symmetrically across a chosen plane. FP-001 instantiates recipes, FP-002 aligns sockets and FP-009 scatters new objects; this tool prepares deterministic translations for existing roots.

**First slice.** Select 1–4 unique isolated, nonphysical builtin roots and an axis-aligned plane X=a, Y=a or Z=a. Reflect each root's translation across the plane; preserve its rotation and scale. A detached preview binds to every entity generation, the World identity/revision and the plane. Commit all resulting transforms in one existing atomic transaction. This is symmetric placement, not reflection of mesh shape, orientation or handedness.

**Prerequisites and contract work.** PR-003/009 transforms and PR-007 admission semantics. Specify finite plane/coordinate bounds. No creation, hierarchy mutation, mesh deformation, collision-clearance promise or persistent symmetry relationship.

**Example and oracle.** Proposed `authoring.symmetric_placement`: independently calculate positions for an off-origin plane and verify unchanged orientation/scale; a second reflection returns the original positions within the declared tolerance. One stale or parented member rejects the entire batch. Refresh/reselect the supported roots and recover; deletion and slot reuse never validate an old preview.

## FP-032 — Explicit mesh normal generation

**Value and distinction.** Produce a reproducible smooth-normal buffer from a mesh. Derived normals already appear in the World model; this proposal defines a detached weighting policy beyond PR-019's vertex mutation and FP-023's connectivity report.

**First slice.** At most 64 indexed positions and 128 triangles. For each vertex, sum incident oriented face cross-products in source face order, then normalize once: area-weighted smoothing by shared index only. Preserve topology. Reject zero-area faces, unreferenced vertices and zero-length accumulated normals under declared numeric thresholds.

**Prerequisites and contract work.** PR-017 mesh conventions, explicit winding, bounded coordinates and tolerance rules. Coincident positions with different indices remain separate. No welding, crease-angle splitting, tangent generation, automatic winding repair, importer expansion or GPU publication.

**Example and oracle.** Proposed `geometry.weighted_normals`: an independent vector oracle checks a plane and an unequal-area two-face fold. Reversed cancelling faces or an invalid index publish no attribute buffer. Correct the input and regenerate, checking unit length/direction and exact source/topology binding while preserving original positions.

## FP-033 — Offline heightfield relaxation

**Value and distinction.** Turn a stepped terrain sketch into a smoother height grid. PR-023 stores voxels and FP-024 evaluates fields; neither specifies a bounded terrain-shaping algorithm.

**First slice.** A 2–8 by 2–8 grid of nonnegative integer heights up to 4095, an integer talus threshold 0–4095 and 1–8 synchronous steps. From the old grid, each cell selects its lowest four-neighbor cell, using N/E/S/W tie order. If their difference D exceeds the threshold T, transfer floor((D-T)/2) height units to that neighbor. Apply all deltas together. Closed borders have no outside neighbors; outputs use unsigned 32-bit heights and retain every step.

**Prerequisites and contract work.** PR-016 provenance conventions; specify grid origin and neighbor directions. Mass remains at most the initial sum, so the bounded profile cannot overflow its output type. No hydraulic simulation, physical-erosion accuracy, voxel conversion or World terrain edit.

**Example and oracle.** Proposed `terrain.relax_heightfield`: independently derive every grid of a small asymmetric fixture and prove total height conservation and nonnegativity each step. Invalid dimensions/heights reject before output. Correct the fixture and rerun from its immutable initial grid; no state from the rejected attempt carries over.

## FP-034 — Edge-matched tile assembly

**Value and distinction.** Build a layout from authored adjacency rules, such as matching doorway edges. FP-005 finds paths on an existing grid and FP-009 chooses spaced points; this proposal solves tile compatibility.

**First slice.** Fill a 1–4 by 1–4 grid from 1–8 immutable tile definitions. Each has a unique integer ID and four edge labels in 0–15, with fixed orientation and unlimited reuse. Adjacent labels must match. Search row-major cells and ascending tile IDs by bounded backtracking; each attempted tile assignment counts against a caller budget of 1–4096. Return a complete layout, proven no solution, or search-budget exhaustion distinctly.

**Prerequisites and contract work.** Define boundary handling: outer edges are unconstrained initially. Bind the result to tile definitions and search profile. No random tie-breaking, rotations, model output execution, World instantiation or geometric doorway-clearance claim.

**Example and oracle.** Proposed `generation.edge_tiles`: independent exhaustive enumeration of tiny fixtures proves the selected first solution and unsatisfiable case. Duplicate tile IDs reject; an intentionally small search budget reports exhaustion without a partial layout. Retry at a sufficient allowed budget and obtain the canonical solution.

## FP-035 — Typed turtle drawing recipes

**Value and distinction.** Author a branching line pattern through inspectable drawing operations. FP-020 samples authored curves and FP-021 revolves a profile; this generates line topology from a small fixed vocabulary.

**First slice.** Up to 64 opcodes: forward by 1–16 integer grid units, left/right quarter-turn, push pose and pop pose. Start at (0,0) facing +X in an XZ plane. Push/pop restores both position and heading; limit stack depth to eight and require balance. Forward emits one segment, at most 64. Reject coordinates outside [-1024,1024]. Emit only detached ordered line segments.

**Prerequisites and contract work.** Specify left/right orientation from +Y and integer coordinate/profile encoding. No loops, recursive grammar expansion, expressions, executable scripts, line-to-mesh conversion or World objects.

**Example and oracle.** Proposed `generation.turtle_branches`: a literal branching recipe independently predicts segment endpoints and order, including return to a saved pose. Stack underflow, residual pushes or an oversized program rejects the whole recipe. Correct the program and reproduce the exact segment list with no retained interpreter state.

## FP-036 — Bounded skeletal pose retargeting

**Value and distinction.** Transfer authored joint rotation changes to a rig with different rest translations. FP-017 deforms vertices, FP-025 solves a target pose and FP-026 blends shapes; none defines cross-rig pose mapping.

**First slice.** Two detached rigid chains of 2–4 joints, with explicit bijective, parent-preserving mapping and compatible declared local joint axes. Source root pose equals its rest pose. For each nonroot joint, compute local delta = inverse(source-rest rotation) * source-pose rotation, then target-pose rotation = target-rest rotation * delta. Preserve target rest translations and the entire target rest root pose. Emit a complete local pose buffer.

**Prerequisites and contract work.** Define quaternion multiplication/normalization and frame compatibility checks; unit scale only. No name inference, root motion, foot contact, IK correction, clip playback, nonuniform scale, importer or World mutation. Native fixtures do not require adopting FP-017 first.

**Example and oracle.** Proposed `animation.retarget_chain`: independent matrices verify a known bend on differently sized chains and preserved target bone lengths. A duplicate mapping, changed source revision or incompatible frame profile rejects. Correct the mapping/profile, rebind exact rig identities and recover the expected pose.

## FP-037 — Offline sidechain audio ducking

**Value and distinction.** Reduce background signal gain while a foreground signal is loud. FP-007 defines spatial cue mixing; this adds a separate sidechain-controlled gain envelope.

**First slice.** Two equal-length mono signed PCM16 buffers, 1–48000 samples at 48 kHz: background and control. Start Q15 gain at 32768. At each sample, abs(control) >= threshold selects minimum gain (0–32768); otherwise select 32768. The threshold is 1–32767. Attack steps decrease gain and release steps increase it, clamping at the target; equal gain stays unchanged. Then output round_ties_away(background * gain / 32768). Return PCM plus the gain trace; no control audio is mixed into output.

**Prerequisites and contract work.** Define exact signed arithmetic and steps 1–32768; use widened intermediates. Generated fixtures need no decoder or FP-007 implementation. No device output, loudness/perceptual-quality claim, multibus routing or live latency promise.

**Example and oracle.** Proposed `audio.sidechain_duck`: an independent sample oracle verifies threshold equality, attack, release, negative samples (including control=-32768 with widened absolute value) and silent control. Mismatched lengths or unsupported format reject atomically. Correct the buffers and rerun from full gain, proving no envelope survives a rejected call.

## FP-038 — Branching dialogue sessions

**Value and distinction.** Let a user choose a conversation path and inspect its transcript. FP-015 drives policy-backed World proposals; this feature owns only dialogue progression and text. FP-030 supplies optional future localization, not conversation state.

**First slice.** An immutable definition has at most 16 nodes and four ordered choices per node. IDs are ASCII 1–32 bytes, node text is UTF-8 up to 256 bytes and choice labels up to 64 bytes; total text is capped at 16 KiB. Every destination must exist. A typed choose command requires the current session generation/revision and current choice ID. Loops are allowed, but after 32 accepted choices the session enters exhausted state unless the reached node has no choices. A node with no choices is terminal; terminal takes precedence on the 32nd choice. Reset invalidates old session handles.

**Prerequisites and contract work.** Define UTF-8/control-character validation, unique IDs, revision progression, transcript bounds and explicit reset semantics. No callbacks, expressions, World actions, model calls, persistence, localization integration or rendered UI.

**Example and oracle.** Proposed `dialogue.branch_session`: independently predict text, choices and revisions along two paths. Stale/unknown choices leave state unchanged; a loop reaches the exact exhaustion bound. Reset and complete a valid path while proving a pre-reset choice cannot advance the new session.

## FP-039 — Atomic inventory transfers

**Value and distinction.** Split or move item stacks without lost or duplicated quantities. This adds count-bearing storage; tags, semantic relations and selection sets do not model consumable stack quantities.

**First slice.** One native owner holds eight slots and up to four immutable item kinds with ASCII IDs of 1–16 bytes and maximum stack sizes 1–99. A trusted initializer supplies starting contents. Typed move(source,destination,count,expected_revision) transfers a positive count into an empty slot or a matching-kind stack, splitting the source when needed. Reject same-slot moves, insufficient source count, incompatible kinds or target overflow atomically.

**Prerequisites and contract work.** Define empty-slot encoding, owner identity/revision and exact per-kind conservation. No minting through transfers, crafting, equipment behavior, currency, trading, World entities, ownership-right inference, remote grants or persistence.

**Example and oracle.** Proposed `gameplay.inventory_transfer`: a separate integer ledger checks every slot and total of each kind through split/merge moves. Overflow and stale revisions leave all counts/revision unchanged. Refresh, choose a supported destination/count and recover; a handle from another inventory never applies.

## FP-040 — Deterministic particle emitters

**Value and distinction.** Reproduce a short particle-effect trajectory. FP-009 scatters static roots; this tool schedules detached births, motion and retirement without creating World objects.

**First slice.** Up to 16 slots across 1–32 ticks, with at most eight strictly ordered emission events of 1–4 births each, lifetime 1–8 ticks and integer origin components in [-32,32]. A nonzero 32-bit seed drives versioned xorshift32 (13,17,5 shifts with unsigned wrap); each velocity component is next-value modulo three minus one. Constant acceleration components are in {-1,0,1}. Each tick retires expired particles, allocates lowest free slots, then updates velocity, position and age, in that order. Births participate in that tick's update.

**Prerequisites and contract work.** Use x ^= x << 13, then x ^= x >> 17, then x ^= x << 5 with unsigned 32-bit truncation after each operation. Define stable birth IDs, generation-safe reused slots and bounded complete frame output. Capacity exhaustion rejects the whole requested run before publishing frames. No GPU, collision, general physics determinism, World mutation or global random state.

**Example and oracle.** Proposed `effects.particle_ticks`: an independent integer interpreter predicts every particle and frame, including slot reuse and exact lifetime boundaries. Zero seed, event bounds or live-slot overflow reject. Reduce the conflicting emission count and rerun from the same seed, proving deterministic complete output and unchanged prior results.

## Focused research and adoption advice

Primary-source scan: 2026-10-07. These living references motivate product ideas; they do not select dependencies, certify pinned-library compatibility or establish recent release claims.

- [Godot skeleton retargeting](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/retargeting_3d_skeletons.html) describes the significance of bone mappings and rest transforms. FP-036 deliberately restricts the mapping and coordinate profile.
- [Godot audio compression](https://docs.godotengine.org/en/stable/classes/class_audioeffectcompressor.html) documents sidechain-triggered ducking. FP-037 proposes a small integer envelope, not an implementation of that engine's compressor.
- [SideFX HeightField Erode](https://www.sidefx.com/docs/houdini/nodes/sop/heightfield_erode.html) treats erosion as terrain authoring. FP-033 begins with a much simpler conservation-testable relaxation rule, with no physical fidelity claim or tool dependency.
- [ink](https://github.com/inkle/ink) demonstrates branching narrative authoring. FP-038 uses fixed typed graph data; it neither adopts a scripting runtime nor executes imported narrative code.
- [Godot GPUParticles3D](https://docs.godotengine.org/en/stable/classes/class_gpuparticles3d.html) documents particle emission and lifetime controls. FP-040 starts with a detached integer oracle; it does not claim GPU effects or upstream-equivalent behavior.

If separately adopted, start with FP-031/032 for authoring utility and FP-035 for a small procedural example. FP-033/034 need complete bounded algorithm oracles. FP-036/037 require precise numeric conventions. FP-038/039 introduce separate state owners and deserve authority/revision review before integration. FP-040 needs a visible rendering slice before any visible-effects claim. This ordering is advisory; the original roadmap resumes at bounded PR-018 when proposal delivery and integration are complete. All forty proposals remain outside autonomous implementation until expressly adopted.
