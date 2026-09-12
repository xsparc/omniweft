# World model and representation contracts

Status: proposed schema design. The initial schema version is `0.1`; serialization is not implemented.

## Authority and identity

A world owns entities, typed components, asset references and spatial resources. Semantic labels help agents find things; labels do not grant permission and are never unique identity. Durable entity identity is `(world_id, entity_uuid, generation)`. Destroying an entity invalidates its handles; reusing storage does not resurrect them. Asset identity is a content hash plus an explicitly versioned asset manifest.

Component schemas contain units, legal ranges, serialization version, mutation capability and observation exposure. Mutable runtime components include transform, hierarchy, renderable, rigid body, collider, light, camera, semantic tags and agent ownership. Extensions register typed schemas; an unvalidated arbitrary JSON map cannot alter solver or GPU state.

Every committed authoring transaction increments a monotonic authoring revision (`world_revision`) and updates affected authored-resource revisions. Simulation separately advances `simulation_tick` and a snapshot/state sequence; physics integration does not increment the authoring revision. A resource has distinct authored-configuration and dynamic-state versions when both apply. v0.1 uses a whole-world expected authoring revision for simple editor transactions; high-frequency physical actions use a tick window, body generation and authored-configuration preconditions. Concurrent authors are supported conservatively: intervening authoring edits conflict even when their resources are disjoint. Per-resource optimistic concurrency is a later optimization requiring an ADR and disjoint-resource example. Geometry preparation revalidates authored geometry/configuration, not every intervening physics transform; commit separately rechecks current overlap/body-state policies.

## Addressability matrix

| Level | Address | Read/observe | Supported write | Invalidations |
| --- | --- | --- | --- | --- |
| Entity | World + UUID + generation | Typed components, bounds, tags | Create, component update, reparent, delete | Delete invalidates handle; cycles rejected |
| Mesh instance | Entity + mesh asset revision | Bounds and instance transform | Instance transform, material assignment | Asset edit creates a new revision |
| Mesh element | Mesh ID + topology revision + element ID | Vertex/face/attribute selection | Batched vertex/attribute changes; later topology | Topology edits return a remap or invalidate all old element IDs |
| Voxel | Volume ID + signed chunk coordinate + local cell | Material/occupancy in bounded regions | Region fill, carve, paint | Chunk revision changes; mesh/collider derived caches rebuild |
| Texture texel | Image ID + revision + mip + layer + x/y | Typed texel/region read | Exact writes, brush tiles, approved kernels | Image revision changes; GPU upload fence tracked |
| Screen pixel | View ID + frame ID + x/y | Color, depth, object ID when available | Transient overlay or approved postprocess | Valid only for that frame; no durable world identity |
| Physics resource | Entity/constraint ID + revision | Transform, velocity, contacts, queries | Force, impulse, constraints, explicit teleport | Body type/shape changes require recooking and state policy |

“Every unit” means every element in a supported, resident or explicitly loadable resource can be addressed within quotas. It does not mean unlimited world size, permanent triangle indices across remeshing, or direct access to arbitrary GPU memory.

## Objects and lifecycle

Object deletion specifies child handling (`reject_if_children`, `reparent`, or `recursive`) and must fit a declared mutation budget. The default rejects ambiguous cascading deletion. Creation can reference temporary IDs scoped to a transaction; commit returns durable IDs. Parent transforms are resolved in a stable order. NaN/Inf transforms, singular required inverses and hierarchy cycles are rejected.

The canonical journal records the resolved UUID/generation mapping for every created temporary ID; replay reuses it instead of drawing new random IDs. Canonical nonphysics hashing sorts map/entity keys, uses schema-defined little-endian numeric encodings, normalizes negative zero, rejects non-finite numbers and excludes wall-clock timestamps, diagnostics, cache handles and provider prose. Schema changes to this encoding change the hash-format version.

Dynamic body transform changes distinguish `teleport` from kinematic movement and physical impulses. Teleport defines whether velocity is preserved or reset and performs the documented overlap check. AI cannot silently bypass physical validation by writing a render transform.

For v0.1, dynamic bodies are hierarchy roots with unit scale. Reparenting a dynamic body, creating a dynamic body under a parent, or applying inherited/nonuniform scale to an active collider is rejected. Visual children may follow a body's transform, but cannot move it. Static/kinematic collider changes still require the physical mutation path and recooking policy. More flexible frames require a separate tested contract.

## Meshes

Start with indexed triangles, positions, normals, UVs and material slots. Static imports and procedural primitives precede deformation. Batched vertex edits use a known topology revision and bounded selection. Validate indices, finite attributes, resource limits and supported geometry properties. Recompute derived bounds/normals according to an explicit policy. An invalid operation rejects the batch.

Topology edits are a later capability with before/after element maps, manifold diagnostics and declared collision support. Render meshes and collision proxies are distinct assets linked to the same committed geometry revision. Simplifying a collider cannot silently replace the visible mesh.

## Voxels

v0.1 uses sparse fixed-size `32 x 32 x 32` chunks, integer material IDs and occupancy, with a per-volume voxel size. This is a design starting point to benchmark, not a proven optimum. Chunk coordinates are signed; floor division/modulo semantics for negative coordinates must be identical on all platforms. A region operation includes bounds, expected chunk revisions, cell count and resulting memory estimate.

Block surface extraction with greedy meshing is the initial visual path. Neighbor invalidation includes every face-sharing chunk affected by a boundary edit. A volume has no implicit rigid body per cell. Terrain collision uses static chunk geometry with bounded cooking jobs. An active graphical world retains the previous visual/collision pair until the complete new pair is ready to publish. A headless world stages canonical geometry and collision only; it never requires GPU resources. An attached or recovered renderer builds a matching snapshot and acknowledges presentation separately.

Smooth signed-distance fields, octrees, clipmaps and procedural sparse volume backends are separate future representations. Conversion between meshes, block voxels and fields is lossy; it must report resolution, provenance and error bounds rather than pretending the representations are equivalent.

## Images and pixels

Define image format, color space, origin, dimensions, mip/layer, row pitch and sampling policy. The first exact path is uncompressed linear `RGBA8_UNORM`, top-left origin, mip 0 and a single layer. Editing an sRGB asset requires a documented conversion; exact byte comparison applies to stored bytes, not an arbitrary filtered display.

Texture painting has persistent world meaning. Frame compositing is a view operation with a limited lifetime. Picking returns the source view/frame and stable entity identity plus an optional mesh hit with topology revision; reject attempts to mutate from obsolete selection data. Readbacks are asynchronous, ROI-limited and budgeted. A full screenshot is not the default observation.

## Physics and semantics

Physical components carry explicit mass/inertia policy, body type, material, layers and supported collider. AI may propose solver configuration changes only through a distinct administrative capability, generally while paused. Runtime controllers use forces, impulses and kinematic targets. Learned models may suggest control or approximate off-line results; canonical collision/integration remain conventional in v0.1.

Semantic metadata can describe affordances such as `walkable` or `graspable`, but claims are checked against geometry and physics where relevant. Agent ownership tags are separate from enforced capability grants.

## Persistence and migration

A native world package contains a versioned manifest, schema versions, world/entity IDs, typed component data, sparse volume chunks, immutable mesh/image blobs, controller configuration, dependency hashes and optional replay/checkpoint records. glTF imports are references or cooked assets; glTF is not the save format.

Writes go to a temporary package, validate all referenced hashes, flush according to platform policy, then atomically replace the previous manifest. Recovery retains a last-known-good manifest. Import/export rejects path traversal, decompression bombs, hash mismatch, unknown required schema versions and missing mandatory blobs.

Visible in-memory commits and durable acknowledgements are distinct. Referenced blobs become durable before their journal commit record is flushed; only then may the engine report `durable_revision`. Recovery replays the complete durable journal prefix beyond the last checkpoint, including resolved IDs, retry epoch and receipts. A receipt marked merely committed can be lost on a crash before flush. See the [protocol durability contract](AI_CONTROL_PROTOCOL.md) for client behavior and interruption tests.

Checkpointing includes entity/body lifecycle and supported properties in addition to solver state. Jolt `SaveState` alone is insufficient to recover arbitrary creation/deletion or property changes; see [Jolt documentation](https://jrouwe.github.io/JoltPhysics/). Migrations run on a copy, preserve the source, and emit a version/provenance report. Cross-version replay requires an explicit compatibility adapter; it is not automatically promised.
