# Bounded native GLB import

PR-017 adds `ow_gltf` and the typed native `ow::gltf::import_glb(Catalog&, ImportRequest)` API. Implementation and validation status are recorded in [the handoff](execution/PR-017-HANDOFF.md); the [example](../examples/meshes-import_gltf.md) specifies the fixture. Profile 1 is a small supported subset of glTF 2.0.

The request supplies original GLB bytes, printable source/license provenance and the expected Catalog revision. The importer validates and prepares the entire detached Scene and a source-asset bundle before calling the existing typed Catalog Import command once. A successful result contains the normal Catalog receipt and Scene; a rejection contains the current revision, fixed error code and no Scene. The caller's input and complete Catalog remain unchanged on rejection. Successful deduplication still increments the Catalog revision once. The source asset's media type is `model/gltf-binary`; its full manifest identity retains provenance, while identical bytes share one content blob. Equivalent geometry encoded differently need not deduplicate.

Scene values preserve source mesh/primitive/node/material order and indices. A missing primitive material is an absent reference to the glTF default material. Node matrices are column-major binary64 values with local `T * R * S` and parent-world times local composition. Node bounds cover only that node's own mesh, using transformed vertices; Scene bounds aggregate mesh instances reachable from the selected scene roots. The entire forest is validated and has computed matrices, including unused nodes. A selected scene containing no mesh primitives rejects. Returned values are detached; modifying them grants no World or Catalog authority. Catalog roots remain explicit owner-held sets, and import does not install roots.

## Supported profile

One embedded BIN buffer and one scene; indexed triangle primitives with float32 VEC3 POSITION and unsigned 8/16/32-bit scalar indices; TRS nodes; material names, base-color, metallic/roughness factors, double-sided and opaque mode. Finite positions, declared extrema, valid references, accessor spans/alignment/strides, index ranges and primitive-restart sentinels are checked. BIN padding is outside the declared buffer and cannot satisfy accessor reads. Unknown trailing chunk types may be skipped under the file budget; duplicate JSON/BIN chunks reject.

Other vertex attributes, sparse/normalized accessors, matrix-authored nodes, non-triangle modes, skins, morph targets, animations, cameras, images, textures, samplers, external/data URIs and unsupported extensions reject. No input triggers file or network resolution. Bounded JSON validation rejects duplicate decoded keys before DOM construction. All declared records are validated, including unused ones; neither unused records nor shared accessors evade resource limits.

| Resource | Maximum |
| --- | ---: |
| Complete GLB / JSON chunk | 65536 / 16384 bytes |
| JSON depth / SAX events / decoded key or string | 16 / 4096 / 256 bytes |
| Buffer views / accessors | 8 / 8 |
| Meshes / total primitives / nodes / materials | 4 / 4 / 8 / 4 |
| Vertices / indices per primitive | 256 / 768 |
| Unique decoded vertices / indices | 1024 / 3072 |
| Materialized primitive vertices / indices | 1024 / 3072 |

Absolute source position/translation components are bounded at 1e6 and scale components at 1e3. Negative and zero scale are accepted without winding repair. Quaternion squared norm must be within 1e-6 of one, followed by one explicit normalization. Composed values must be finite and transformed world positions stay within 1e12. These limits are importer policy; they do not assert World hierarchy compatibility.

Fixed errors include `INVALID_GLB`, `INVALID_SCHEMA`, `UNSUPPORTED_PROFILE`, `UNSUPPORTED_REQUIRED_EXTENSION`, `INVALID_INDEX`, `NONFINITE_VALUE`, `INVALID_TRANSFORM`, `BUDGET_EXCEEDED`, and existing Catalog errors such as `REVISION_CONFLICT`. Errors contain no imported strings. Integer fields exclude booleans, fractions, negatives and overflow; mathematically integral JSON numbers remain integers for format validation.

## Compatibility and provenance

The source uses the existing immutable asset manifest and bundle format. No cooked mesh serialization, automatic retention root, World attachment, remote import endpoint, save migration, new dependency or licensing authentication is introduced. The scene is an in-memory interpretation tagged with profile version 1 and its full source asset identity. Future rendering/material/geometry work must define its own publication and lifetime contracts.

The runnable fixture is generated first-party data under Apache-2.0. The importer uses already pinned nlohmann/json. The [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) defines the external container, accessor and transformation rules; the supported subset and budgets here are Omniweft-specific. No GPU, physics or general bitwise floating-point determinism is established by CPU import checks.
