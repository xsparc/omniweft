# Ten further feature proposals: geometry, animation and scene text

Status: **proposed, unimplemented, and not adopted for autonomous execution**. These ten ideas answer the maintainer's 2026-10-06 request for another draft. They extend [FP-001–010](FEATURE_PROPOSALS.md) and [FP-011–020](FEATURE_PROPOSALS_2.md); they do not expand the adopted [38-item roadmap](ROADMAP.md) or its release gate. Merging proposal documentation does not authorize implementation.

Baseline: PR #26 was squash-merged as `b1bb3ea7c182c85c316126585335f647996da268` into the branch of still-open [PR #25](https://github.com/xsparc/omniweft/pull/25), not into main. Its tree matches reviewed PR #26 delivery `6b2fb5b25d19ebac1ecaeec872b53fdc32cea580`. This batch is a separate draft based on those combined contents. See [README](../README.md) for implemented capabilities and the [handoff](execution/FEATURE-PROPOSALS-3-HANDOFF.md) for actual delivery status.

## Candidate overview

All ten IDs below are proposed feature identifiers, not backlog entries or GitHub PR numbers. Effort is relative, not a schedule or performance claim.

| ID | Feature | User benefit | First-slice effort / main risk |
| --- | --- | --- | --- |
| FP-021 | Lathe mesh recipes | Generate vases, columns and similar rotational surfaces | Medium / winding and reproducible sampling |
| FP-022 | Planar UV projection | Give authored geometry predictable texture coordinates | Small / coordinate and source binding |
| FP-023 | Detached mesh topology audit | Locate broken connectivity before downstream work | Medium / overstating geometric validity |
| FP-024 | Bounded implicit field composition | Explore solid combinations without editing a World | Medium / confusing field values with distance |
| FP-025 | Two-bone inverse kinematics | Compute a limb pose that reaches a target | Medium / singular and unreachable poses |
| FP-026 | Morph target blending | Blend authored shape variants independently of joints | Small / topology and numeric compatibility |
| FP-027 | Texture border seam audit | Find exact edge mismatches before tiling | Small / confusing border equality with visual seamlessness |
| FP-028 | Palette-constrained texture conversion | Produce repeatable limited-color assets | Small / color-space and tie rules |
| FP-029 | Generation-safe scene annotations | Keep explanatory notes attached to authored objects | Medium / stale references and observation scope |
| FP-030 | Explicit localized text catalogs | Resolve scene labels with predictable language fallback | Medium / implying text rendering support |

## Common boundary and acceptance

These are candidate first slices, not newly discovered requirements or a claim of novelty over other engines. Animation, procedural content, fields and localization already appear in [future tracks](FUTURE_TRACKS.md). Each proposal defines an additional concrete contract distinct from the existing roadmap and previous catalogs.

Before implementation, adopt a separate bounded plan and runnable example with independent success, rejection and recovery evidence. The ceilings below are proposed. Define finite numeric ranges, overflow behavior, precision, tolerances and canonical profile versions before coding; cross-platform tolerance checks do not establish bit-identical floating-point output. Validate all inputs and output budgets before publishing a complete detached result. Rejection never modifies source assets or publishes a partial replacement.

Bind results to exact input identities/revisions and the algorithm profile. Queries consume only authorized detached data. New native owner tools confer no remote grant. Any later World mutation must use the existing typed Coordinator; other owners need explicit typed revision-checked APIs. Content hashes, labels, field expressions and translations are data, never authority or executable code. No dependency, remote endpoint, persistent format or importer profile changes are silently included.

Actual Windows/Linux CPU checks and independent review are required after adoption. Visible rendering, device or physical claims need separate applicable evidence. None of these proposals currently has a prototype, GPU result, benchmark or user-study result. Names below describe future examples, not runnable commands.

## FP-021 — Lathe mesh recipes

**Value and distinction.** Generate a rotational surface from a small cross-section. FP-001 reuses object arrangements and PR-020 edits existing topology; neither defines a parametric surface generator.

**First slice.** A typed in-memory profile has 2–8 points with strictly increasing Y and positive radius, revolved around +Y at 3–16 equal angular samples starting on +X. Emit an uncapped indexed POSITION-only surface with shared wraparound indices and outward winding, at most 128 vertices and 224 triangles. Reject axis points and duplicate heights. No UVs, normals, caps, scripts, Catalog publication or World attachment.

**Prerequisites and contract work.** PR-016/017 content and mesh conventions; specify angle direction, triangle ordering, input units and numeric tolerances. These generated outputs do not become supported GLB importer inputs by implication.

**Example and oracle.** Proposed `geometry.lathe_column`: independently enumerate a four-sided constant-radius profile's coordinates, connectivity and outward face normals, then check another nonconstant profile. Invalid height order or a seventeenth angular sample publishes nothing. Correct the profile and regenerate; the earlier immutable result stays unchanged and the new result binds to the new recipe.

## FP-022 — Planar UV projection

**Value and distinction.** Apply a simple, inspectable texture projection. FP-019 packs image rectangles into an atlas; it does not derive mesh texture coordinates.

**First slice.** Given at most 64 detached positions, explicit XY/XZ/YZ axis selection, two-component origin and positive finite scale, emit one UV per source vertex: U follows the first selected axis; V decreases along the second, with top-left UV origin. Values may lie outside [0,1]; no implicit wrapping or clamping. No seam splitting, general unwrap, material mutation, texture sampling or asset publication.

**Prerequisites and contract work.** PR-017 mesh identity conventions and a new typed detached UV-result schema. Bind to ordered source indices and exact source revision. PR-018 rendering and any UV-capable import expansion are separate prerequisites for displaying this result.

**Example and oracle.** Proposed `geometry.planar_uv`: literal asymmetric quads prove axis, origin, sign and scale choices. Zero scale, non-finite positions or a stale supplied source revision reject. Rebind to the new mesh and recompute; verify that an old coordinate buffer cannot attach to changed vertex order.

## FP-023 — Detached mesh topology audit

**Value and distinction.** Return reusable connectivity diagnostics before editing or cooking. Topology validity is already an architectural goal; this is an explicit read-only diagnostic surface beyond PR-020's command rejection.

**First slice.** Audit at most 128 indexed positions and 256 triangles. Count edges by vertex index, with no positional welding. Return sorted boundary edges, edges incident to more than two faces, same-direction pairs on two-face edges, repeated-index triangles and exact duplicate index triples independent of winding. Malformed indices reject the query; valid but problematic connectivity returns a complete diagnostic report.

**Prerequisites and contract work.** PR-017 input/provenance rules; specify deterministic face/edge identifiers and diagnostic ordering. This profile does not certify self-intersection freedom, geometric nondegeneracy, manifold vertex neighborhoods, enclosed volume, collision readiness or watertightness.

**Example and oracle.** Proposed `geometry.audit_connectivity`: independently enumerate a tetrahedron, open patch and three-face shared edge, including coincident positions with different indices. Oversized or out-of-range input rejects. Correct an index and recompute; compare the exact new diagnostic set and confirm the source was never repaired implicitly.

## FP-024 — Bounded implicit field composition

**Value and distinction.** Evaluate a small solid-design expression before choosing a mesh or voxel conversion. Fields are already a future representation; this first slice is a detached expression evaluator, not a storage replacement.

**First slice.** Up to eight acyclic typed nodes describe translated spheres, axis-aligned boxes, union via min, intersection via max and difference via max(a,-b). Evaluate at at most 64 explicit points; return finite scalar values and inside/on/outside classification under a defined zero tolerance. Positive dimensions only. No user functions, arbitrary graph execution, extraction, ray marching, voxels or World publication.

**Prerequisites and contract work.** [ADR-0003](adr/0003-hybrid-representations.md) requires a separate representation contract. Define primitive equations, bounded coordinates, graph order and sign convention. Composite values are not promised exact signed distances or a safe stepping bound.

**Example and oracle.** Proposed `fields.boolean_samples`: independent analytic points inside/outside two primitives verify all three operators and boundary handling. A cycle, negative radius or ninth node rejects with no partial values. Replace the invalid node, evaluate again and prove no stale result cache survives the expression change.

## FP-025 — Two-bone inverse kinematics

**Value and distinction.** Solve a simple reaching pose. FP-017 skins a supplied pose; FP-006 plays authored transforms. Neither computes a pose from a target.

**First slice.** One detached three-joint chain, fixed root, two positive lengths, explicit target and pole direction. Return joint positions in one declared coordinate frame. Accept only targets strictly between the inner and outer reach bounds with a noncollinear pole; unreachable, folded, fully extended and ambiguous cases return distinct rejection statuses, with no silent clamp or stretch.

**Prerequisites and contract work.** Define rest frame, bend-side convention, singularity margins and tolerances. This standalone math profile does not require FP-017 adoption; skeleton integration would. No joint rotations, constraints solver, animation scheduler, dynamic-body or World mutation.

**Example and oracle.** Proposed `animation.two_bone_reach`: an independent geometric oracle verifies both lengths, fixed root, endpoint and pole-selected bend for known poses. Exercise too-near, too-far and collinear inputs. Move the target and pole into the accepted domain and recover a complete pose without altering the last accepted result.

## FP-026 — Morph target blending

**Value and distinction.** Blend authored shape differences for expression or shape variation. This adds a weighted multi-target evaluation contract beyond PR-019's vertex edits and FP-017's joint skinning.

**First slice.** Up to 64 base positions and two position-delta targets, each bound to the exact base identity, ordered topology and revision. Each weight lies in [0,1]; evaluate base + w0*delta0 + w1*delta1 in a specified order. The weights need not sum to one. Emit a detached position buffer and recomputed bounds.

**Prerequisites and contract work.** PR-017 mesh conventions; define a native immutable target descriptor and arithmetic tolerance. No normals/tangents, clips, skinning composition, topology edits, Catalog publication or glTF morph import. The existing importer rejection remains intact.

**Example and oracle.** Proposed `animation.morph_blend`: literal deltas and several weight pairs verify all positions and bounds independently. A reordered source, mismatched target length or out-of-range weight rejects before output. Restore matching targets and valid weights, then prove the result and unchanged source bytes.

## FP-027 — Texture border seam audit

**Value and distinction.** Explain whether opposite borders match before repeating an image. PR-021 paints texels and FP-019 arranges images; neither supplies this diagnostic.

**First slice.** For one explicit 2–32 by 2–32 linear RGBA8 image, compare first/last columns and first/last rows, including alpha. Emit per-axis mismatching coordinate pairs and channel deltas in stable order; corner pairs occur once per axis. Exact byte equality is the only success criterion.

**Prerequisites and contract work.** PR-016 provenance plus a detached typed image descriptor with row stride, size and origin validation. No codec, repair, sampler, mip generation or GPU readback. Border equality does not establish derivative continuity, filtered seamlessness or visual quality.

**Example and oracle.** Proposed `pixels.border_seams`: known 3-by-4 fixtures independently predict every pair/delta. Invalid byte count or unsupported color format rejects without reading outside the buffer. Provide a corrected immutable fixture and verify both axes pass, preserving the initial report's source binding.

## FP-028 — Palette-constrained texture conversion

**Value and distinction.** Create predictable limited-color textures. PR-022 is a bounded GPU-kernel integration, not this explicit color-selection rule; FP-019 changes layout rather than color.

**First slice.** Map at most 32 by 32 opaque linear RGB8 pixels to a caller-supplied ordered palette of 1–16 RGB8 colors. Choose the smallest squared integer RGB distance, resolving equal distances to the lowest palette index. Return indices and reconstructed RGB bytes, bound to source and palette content. No automatic palette optimization, alpha, dithering, codec or GPU work.

**Prerequisites and contract work.** PR-016 content/provenance conventions and a detached typed image profile. Define integer accumulator bounds and row order. Perceptual color equivalence and sRGB conversion are unsupported rather than assumed.

**Example and oracle.** Proposed `pixels.palette_convert`: an independent exhaustive distance oracle checks every pixel, including exact ties and duplicate palette colors. Empty/oversized palettes or an unsupported alpha-bearing image format reject at the typed descriptor boundary before publication. Supply a supported palette/image and recover exact output without changing the prior image.

## FP-029 — Generation-safe scene annotations

**Value and distinction.** Attach explanatory labels to stable scene locations. Tags classify, FP-003 relates entities and FP-016 selects sets; annotations add owner-local text and a geometric anchor.

**First slice.** A separate revisioned owner holds at most eight ASCII labels of at most 80 characters, each attached to a World-qualified generation-safe entity handle and local point. Resolve all anchors against one authorized detached snapshot using its world transforms. Missing, reused or unobserved entities yield the same unresolved status; no hidden-entity existence probe.

**Prerequisites and contract work.** PR-009/010 snapshots and a new native typed annotation-owner API. Creation/update validates expected annotation revision and an authorized current snapshot. No URLs, rich text, persistence, remote sharing, automatic deletion, screen placement or font/rendering support. Later visibility requires its own renderer evidence.

**Example and oracle.** Proposed `authoring.annotation_anchors`: independent transforms verify resolved points under translation/rotation. A stale owner revision or ninth label rejects atomically. Delete/reuse an entity and prove its old annotation never follows the replacement; explicitly rebind at fresh revisions to recover.

## FP-030 — Explicit localized text catalogs

**Value and distinction.** Resolve authored labels in a requested language with inspectable fallback. General UI localization is a future track; this is a bounded data lookup contract, separate from FP-029 anchor geometry and existing editor plans.

**First slice.** One immutable in-memory table contains at most four explicitly declared ASCII language tokens of 1–16 bytes and 16 ASCII message keys of 1–64 bytes, with at most 256 UTF-8 bytes per translation. Each language has zero or one explicit fallback to another declared language; the graph must be acyclic. Query a declared language/key and return exact stored bytes plus the language that supplied them, or an explicit missing result. No OS locale detection or implicit regional fallback.

**Prerequisites and contract work.** PR-016 provenance conventions; define strict UTF-8 validation, unique keys/tokens and text control-character policy before coding. Content is not evaluated or normalized implicitly. No placeholders, plural rules, translation service, import format, font/shaping, bidi layout or automatic attachment to existing labels.

**Example and oracle.** Proposed `ui.localized_lookup`: generated licensed strings verify direct hits, two-hop fallback and missing keys byte-for-byte. Cyclic or undeclared fallbacks, oversized keys/tokens, duplicate entries or invalid UTF-8 reject the whole table. Correct the table and re-query; earlier immutable catalogs still produce their own version-bound results. A successful lookup is not evidence that the text can be rendered.

## Primary research and ordering advice

Focused primary-source scan: 2026-10-06. These living references motivate boundaries; no upstream release-recency, benchmark or compatibility claim is made, and no dependency is selected.

- [Godot procedural geometry](https://docs.godotengine.org/en/stable/tutorials/3d/procedural_geometry/index.html) distinguishes geometry-building approaches. FP-021/022 propose much smaller detached generation/projection contracts.
- [CGAL polygon mesh processing](https://doc.cgal.org/latest/Polygon_mesh_processing/index.html) covers distinct repair, orientation and intersection operations. FP-023 deliberately reports index connectivity without presenting that as a complete geometric certificate.
- [Khronos glTF morph targets](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes) describes weighted attribute deltas; FP-026 starts with native position-only buffers and does not widen Omniweft's importer.
- [Godot TwoBoneIK3D](https://docs.godotengine.org/en/stable/classes/class_twoboneik3d.html) documents two-bone posing; FP-025 makes singular-case rejection explicit in a detached math profile.
- [Godot internationalization](https://docs.godotengine.org/en/stable/tutorials/i18n/internationalizing_games.html) distinguishes translation lookup from font, layout and language concerns. FP-030 addresses lookup only.

If separately adopted, FP-023 and FP-027 offer small diagnostic slices, followed by FP-021/022/028 for content authoring. FP-026 and FP-025 need numeric pose contracts; FP-029/030 need owner and text-boundary decisions. FP-024 remains exploratory until a concrete use justifies its separate field representation. This advice does not displace PR-018–038. Open questions include which asset-authoring tasks users repeat, whether posed buffers need rendering first, and whether scene notes should eventually be shared. No audience demand or runtime benefit has been measured.
