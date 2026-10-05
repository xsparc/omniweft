# Ten more feature proposals: scene tools and interactive content

Status: **proposed, unimplemented, and not adopted for autonomous execution**. The maintainer requested a second set of ten ideas on 2026-10-06 and clarified that the reported squash merge referred to PR #24. [PR #25](https://github.com/xsparc/omniweft/pull/25), containing [FP-001–010](FEATURE_PROPOSALS.md), remains the base draft for this stacked proposal PR. Its merge must not be presumed.

Proposal baseline: `b344154d55264b4b11a466481d0c956b6b6a43c4`. Integrated engine baseline: PR #24 merge `c9a50e6e6826d6448e09d1e7f6c1bebdf3af9fcc`. These ten additional candidates have IDs FP-011–020. None changes the adopted [38-item roadmap](ROADMAP.md), grants implementation authority, or adds a release gate. Merging either proposal document records ideas only.

## Candidate overview

| ID | Feature | User benefit | First-slice effort / main risk |
| --- | --- | --- | --- |
| FP-011 | Revision-bound measurement probes | Check distances and angles before editing a scene | Small / stale geometry and unit conventions |
| FP-012 | Declarative scene checks | Find authoring mistakes with repeatable, explainable rules | Medium / incomplete observations mistaken for success |
| FP-013 | Sampled trigger volumes | Detect objects entering and leaving authored regions | Medium / gaps, boundary cases and event ordering |
| FP-014 | Camera bookmarks | Return to a known inspection viewpoint | Small / view ownership and projection validity |
| FP-015 | Bounded state-machine behaviors | Build simple interactive responses without scripts | High / duplicate events and uncertain commits |
| FP-016 | Named selection sets | Reuse exact groups for inspection or later commands | Small / deleted and reused entity handles |
| FP-017 | Bounded skeletal skinning | Pose a small articulated mesh | High / joint spaces, weights and numerical behavior |
| FP-018 | Revision-bound mesh LOD chains | Select suitable mesh detail consistently | Medium / stale levels and threshold oscillation |
| FP-019 | Deterministic texture atlas packing | Combine small textures with exact region mappings | Medium / padding, color space and UV conventions |
| FP-020 | Authored spline paths | Describe smooth reusable paths as typed geometry | Medium / sampling bounds and degenerate tangents |

Effort estimates compare these first slices; they are not schedules or measured results. Several refine broad product intentions already in the architecture. The concrete contracts below are new proposals, not claims that the underlying ideas are novel.

## Common delivery boundary

The [first batch's acceptance contract](FEATURE_PROPOSALS.md#shared-acceptance-contract) applies. Each adopted candidate needs a separate bounded implementation PR, runnable example, independent success oracle, rejection and recovery evidence. Proposed limits below must be finalized in that slice plan. The example names below are future specifications; no new command is runnable from this document.

World mutations use the normal typed Coordinator path; other authoritative modules keep their own typed, revision-checked owner paths. A read-only proposal, semantic tag, content hash or event cannot grant authority. Remote exposure needs a separate observation/admission design. Test on Windows/Linux CPU, adding actual GPU, audio or physics evidence only when that behavior is claimed. Persistence or dependency changes need explicit compatibility/provenance work.

Current GLB profile 1 rejects skins, animation, UVs and textures and returns detached scene data. World mesh attachment, future texture resources and the editor are not supplied by that importer. Dependencies on PR-018 onward mean **future prerequisites**, not existing support. All FP identifiers in both batches remain unadopted; cross-references below are distinctions, not hidden implementation dependencies.

## FP-011 — Revision-bound measurement probes

**Value and distinction.** Measure a doorway width or the angle between two authored directions before changing a layout. PR-010 selects objects by tag/AABB; FP-002 aligns socket frames. Neither defines numerical measurement results tied to a snapshot.

**First slice.** At most eight probes over an authorized detached World snapshot: point-to-point distance and angle between two nonzero direction vectors. Points may reference a generation-safe entity and finite local offset, resolved through the snapshot's world transform. Report meters/radians, snapshot revision and all source handles. No collision-clearance, volume or physical-fit conclusion.

**Prerequisites and contract work.** PR-009/010 transform and query conventions. Define coordinate limits, zero-length rejection, angle range and a comparison tolerance. Begin with a native owner query; editor overlays require PR-033 and additional presentation evidence. Measurements do not change the World.

**Example and oracle.** Proposed `authoring.measure_fixture`: a 3-4-5 triangle and perpendicular directions have independently known answers, including a transformed child. Stale/missing handles, non-finite offsets or zero directions reject the complete probe batch. Refresh the snapshot and resubmit to recover; slot reuse must never measure a replacement object under an old handle.

## FP-012 — Declarative scene checks

**Value and distinction.** Explain missing tags, excessive object counts or objects placed outside an authored boundary. This is an end-user authoring diagnostic, separate from PR-036's adversarial test harness and FP-003's relation storage.

**First slice.** Up to eight rules over at most 32 observed builtin objects. The fixed vocabulary is `required_tag`, `max_count` and `within_authored_aabb`; rules have bounded typed parameters and no expressions, scripts or plugins. Return a stable list of rule IDs, authorized object handles and failure reasons. A truncated or unauthorized observation yields `incomplete`, never a clean pass. No automatic repairs or physical-overlap proof.

**Prerequisites and contract work.** PR-010 plus an explicit snapshot-completeness contract. Owner-only native evaluation comes first; a later restricted client must not infer hidden objects through totals or diagnostics. Rules cannot widen admission or override normal validation.

**Example and oracle.** Proposed `authoring.scene_checks`: literal fixtures produce an exact ordered violation list. Unknown rules, excess rules and incomplete snapshots reject or report incomplete without mutating World or rule state. Correct one object through normal commands, re-observe, and verify that only its expected violations disappear.

## FP-013 — Sampled trigger volumes

**Value and distinction.** Report when an authored object enters a marked interaction zone. PR-010 provides spatial queries; FP-004 proposes general revision changes. This feature compares region membership across sampled snapshots.

**First slice.** Four fixed authored AABBs and at most 16 generation-safe root objects. Compare complete, increasing snapshot samples using a documented inclusive-boundary AABB overlap predicate. Emit a bounded, sorted set of `enter`, `exit` and `invalidated` records. The first sample establishes membership without inventing an entry event. Missing/deleted identities invalidate membership; a new generation is a new object.

**Prerequisites and contract work.** PR-006/010. Define snapshot sequence, sampling cadence, region revision, maximum records and a resync result for gaps or overflow. Output events only; no callbacks, automatic actions, collision contacts or continuous-crossing detection. A fast object may cross a region entirely between samples without a detected entry.

**Example and oracle.** Proposed `observe.trigger_samples`: hand-authored inside/outside/touching sequences predict the complete event trace. Duplicate/out-of-order samples and changed region revisions reject without advancing history. After a gap, explicitly reset from a complete snapshot and verify the next transition; never silently join discontinuous histories.

## FP-014 — Camera bookmarks

**Value and distinction.** Switch between repeatable inspection viewpoints. Camera movement and focus are already editor goals; this adds named, complete view configurations rather than PR-029 observations or FP-010 dataset capture.

**First slice.** A typed in-memory store of eight bookmarks for one owner-controlled view. Each contains position, unit orientation, perspective field of view, near/far planes and a bounded name. Applying a bookmark atomically replaces a view configuration under an expected view revision; it does not mutate World or restore an old world snapshot. Aspect ratio belongs to the current viewport.

**Prerequisites and contract work.** PR-004's camera math and a separately specified versioned view-control API, which is not currently public. PR-029/033 are later observation/editor integration gates. First establish structural round-trip and matrix correctness; visible bookmark recall requires real-GPU evidence. No save-format change or file import in the first slice.

**Example and oracle.** Proposed `editor.camera_bookmarks`: eight known configurations produce independently calculated view/projection matrices. A ninth bookmark, invalid projection or stale view revision leaves the store/view unchanged. Refresh the view revision and apply again; resize the viewport and verify aspect is recomputed while the stored pose stays fixed.

## FP-015 — Bounded state-machine behaviors

**Value and distinction.** Describe an idle/active/cooldown response to explicit events. FP-006 samples a timeline, and PR-015 supervises worker lifecycle; neither defines event-driven behavior states.

**First slice.** A fixed interpreter accepts data describing four states and eight transitions keyed by a finite event vocabulary. Process at most one event per tick; each transition may emit one allowlisted typed root-transform proposal. No conditions containing code, expressions, loops, native callbacks or generated scripts. The existing parent-owned policy client validates and submits any proposal; the graph itself holds no authority.

**Prerequisites and contract work.** PR-006/007/011/015. Define a bounded event sequence, graph revision and pending-transition receipt binding. Advance a transition only after a confirmed commit, or immediately if it has no action. Rejection stops that transition; uncertainty retains the same prepared request and invokes existing reconciliation rules. Receipt expiry must lead to an explicit stopped/unknown state, not a new key or automatic resubmission. Retry TTL, leases and deadlines stay unchanged.

**Example and oracle.** Proposed `agents.state_behavior`: literal events and mocked transport outcomes cover state-machine edges. Also require a controlled lost response against the real retained-retry host through the parent policy client; independently query exact World/receipt state before transition advancement and prove that same-key reconciliation causes no second mutation. Ambiguous transitions, duplicate events and denied operations cannot double-apply actions. Test explicit reset after irrecoverable uncertainty; World state remains authoritative. Mocks alone cannot establish integration behavior. This grants no crash durability or arbitrary scripting support.

## FP-016 — Named selection sets

**Value and distinction.** Keep a precise inspection group while tags or object positions change. Tag queries select by predicates; FP-003 stores relations. A selection set records explicit membership and ordered stale-member diagnostics.

**First slice.** Four owner-local sets of at most eight World-qualified generation-safe handles, with bounded names and monotonic set revisions. Add/remove/resolve use typed operations. A read resolves all members against one authorized snapshot; deleted members are reported stale and never silently replaced or omitted. Resolving a set is not a mutation grant or an implicit bulk edit.

**Prerequisites and contract work.** PR-003/010. Define stable ordering, duplicate handling and wrong-World rejection. No cross-World membership, dynamic tag selectors, saved collections or remote sharing. Later bulk command expansion must pass ordinary atomic admission and quotas.

**Example and oracle.** Proposed `editor.selection_set`: exact membership survives unrelated edits and tag changes. Overflow, duplicate members and stale set revisions reject atomically. Delete/recreate one member, verify its stale diagnostic, then explicitly remove the old handle and add the replacement to recover.

## FP-017 — Bounded skeletal skinning

**Value and distinction.** Pose a small articulated object by moving joints. PR-019 edits vertices directly; FP-006 moves an entire root. Skinning derives vertex positions from joint transforms and weights.

**First slice.** A detached CPU fixture with at most four joints, 64 vertices and four influences per vertex. Use a declared linear-blend skinning formula, explicit inverse-bind matrices and parent-before-child joint evaluation. Validate the full joint forest and finite inputs before publishing a complete posed-position buffer. The initial profile handles positions only; no normals, lighting or rendering claim.

**Prerequisites and contract work.** PR-017 supplies coordinate/mesh conventions and PR-019 must define versioned derived mesh output before live integration. Define skeleton identity, mesh revision, rigid joint transforms, weight-sum tolerance and numerical bounds. The first fixture is typed native data. glTF skin import, animation import, World attachment, GPU skinning, collision and persistence require separate reviewed slices; profile 1 rejection remains unchanged.

**Example and oracle.** Proposed `meshes.skin_fixture`: a two-joint strip with known bind pose and quarter-turn joint pose has independently computed vertices. Invalid joint indices, cycles, bad weights or stale mesh binding publish no partial buffer. Correct weights at a new pose revision and verify recovery and unchanged source vertices.

## FP-018 — Revision-bound mesh LOD chains

**Value and distinction.** Choose between authored mesh detail levels without flickering near a threshold. PR-020 changes topology; this proposal chooses among immutable alternatives while preserving source identity.

**First slice.** A detached chain of two authored POSITION-only triangle meshes, at most 256 triangles each, bound to one source-content identity and a versioned selection policy. Quantized distance bands with explicit enter/leave thresholds determine a stable level. Return selected level, chain revision and source identity. Limit history to the previous level; invalid input preserves that state.

**Prerequisites and contract work.** PR-016/017 and PR-019's revision conventions. Define source binding, allowed bounds, thresholds and stale-chain rejection. Author-supplied levels remain distinct assets. Automatic simplification, geometric error certification, material/UV seams, live World publication and GPU resource retirement are separate gates; renderer integration requires PR-018 and explicit mesh attachment. No draw-time or memory savings are claimed before measurement.

**Example and oracle.** Proposed `meshes.lod_chain`: a fixed distance trace predicts every level switch and verifies hysteresis. Unsorted thresholds, mismatched source identities or invalid indices reject. Replace a chain, explicitly reset selection history, and verify no stale level is reused. A later GPU example must prove actual selected geometry and retirement behavior.

## FP-019 — Deterministic texture atlas packing

**Value and distinction.** Combine a few small textures into one image with auditable placement mappings. PR-021 edits image regions; this feature computes a packing layout and padded copies.

**First slice.** Up to four caller-supplied images, each at most 16 by 16 texels, packed into caller-requested atlas dimensions of 1–64 texels per axis in linear `RGBA8_UNORM`. Specify stable input ordering, no rotation, integer shelf placement, one-texel edge-duplicated padding and zero-filled unused space. Return exact texel rectangles and rational UV transforms under a documented top-left convention. Publish the entire detached atlas and mapping together. Valid images may still fail to fit a valid requested atlas size.

**Prerequisites and contract work.** PR-016 provenance and PR-021's future image-resource contract. Define zero-size rejection and capacity checks before allocation. Inputs are explicit buffers, not discovered files. First slice excludes codecs, sRGB conversion, filtering, mipmaps, compression and automatic material rebinding. GPU sampling and material/UV support require later PR-018 integration.

**Example and oracle.** Proposed `pixels.atlas_fixture`: corner-colored images predict every output byte, padding texel and mapping. Mixed formats, oversized images or packing failure publish nothing and preserve input buffers. Remove one image and retry; identical input order must reproduce identical bytes on Windows/Linux. UV tests must catch origin flips and half-texel mistakes.

## FP-020 — Authored spline paths

**Value and distinction.** Describe a smooth rail or guide curve as reusable geometry. FP-005 searches for routes, FP-009 scatters placements and FP-006 schedules motion. A spline is an authored path independent of search or playback.

**First slice.** Two connected cubic Bezier segments with finite control points and at most 65 output samples. Sample a fixed rational parameter grid, emit join points once, and retain source segment/parameter mappings. The shared join uses the outgoing segment's derivative; positional continuity is required, but tangent continuity is not promised. Bound coordinates and sample count before evaluation; publish positions and explicitly defined tangent results as one detached buffer. Uniform parameter samples do not imply equal arc-length spacing.

**Prerequisites and contract work.** PR-003/009 coordinate conventions; define a native path identity/revision and numerical tolerance. A stationary point returns a specified zero-tangent status rather than inventing an orientation. No entity movement, automatic extrusion, physical path following, mesh generation or Catalog/save integration. Those uses need independent plans.

**Example and oracle.** Proposed `geometry.spline_samples`: straight and curved fixtures have independently derived endpoints, midpoint and tangent values. Disconnected joins, non-finite points, stale path revisions and excess samples reject atomically. Repair a control point and resample; verify continuity, join deduplication and unchanged input data. Editor visualization requires PR-033 plus presentation evidence.

## Primary research and ordering advice

Focused scan date: 2026-10-06, against the proposal baseline above. Living documentation is cited for concepts only; no upstream release freshness, performance or Omniweft compatibility claim is made.

- [Godot Area3D](https://docs.godotengine.org/en/stable/classes/class_area3d.html) documents region entry/exit detection. FP-013 deliberately defines sampled authored-AABB semantics; it does not import Godot's physics behavior or dependency.
- [Khronos glTF skinning](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#skins) describes joints, inverse-bind matrices and vertex influences. This informs FP-017's required coordinate contracts; current Omniweft import support is unchanged.
- [meshoptimizer's documentation](https://github.com/zeux/meshoptimizer) discusses simplification and its constraints. FP-018 starts with authored levels, leaving automatic reduction and error metrics to a later decision. No library is selected or added.
- [Godot Curve3D](https://docs.godotengine.org/en/stable/classes/class_curve3d.html) documents 3D Bezier curves and sampled points. FP-020 uses a bounded native representation and promises only its specified parameter sampling.

Advisory order: FP-011 and FP-016 offer small inspection tools using existing object identity. FP-012 and FP-013 need clear completeness/event semantics; FP-014 needs a view-control contract. FP-020 can begin as detached geometry. FP-015 requires careful uncertainty handling. FP-017–019 should follow the mesh/image ownership prerequisites and obtain separate rendering evidence when integrated. This does not reorder the adopted roadmap or select any FP for execution.

Evidence gaps: no user study, prototype, performance measurement or additional dependency evaluation was conducted. The relative effort estimates need revision after an adopted design spike. A product goal can require more than its first bounded PR. See the [delivery plan](execution/FEATURE-PROPOSALS-2-PLAN.md) and [handoff](execution/FEATURE-PROPOSALS-2-HANDOFF.md).
