# meshes.import_gltf

Status: **merged in PR #24 with verified Windows/Linux post-merge checks and native evidence**. Work item: **PR-017 — Bounded glTF mesh import**. Dependency PR-016 is integrated. CPU lane only. See [contract](../docs/GLTF_IMPORT.md), [plan](../docs/execution/PR-017-PLAN.md) and [handoff](../docs/execution/PR-017-HANDOFF.md).

Dependencies: PR-016. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Import a tiny generated GLB and verify known vertices, indices, transforms, bounds and material references.

The seed-7 fixture is generated first-party data under Apache-2.0. It writes and reads an actual GLB containing four strided POSITION vertices and two triangles, one explicit material and one default-material reference. Three nodes include parent-child translation/scale/180-degree rotation and a mirrored second root. The example uses the typed importer and existing Catalog, proves deduplication and distinct provenance sharing one blob, and retains complete scene and Catalog values. Detached output mutation cannot change stored source content.

## Run and independently verify

Build using the pinned [bootstrap instructions](platform-bootstrap.md). Once the new target builds:

```sh
omniweft_examples --example meshes.import_gltf --headless --seed 7 --verify --output artifacts/meshes.import_gltf
python tests/gltf_oracle.py --executable build/linux-headless/omniweft_examples --native-test build/linux-headless/gltf_native_test --evidence artifacts/gltf
python tests/gltf_oracle_test.py
```

On Windows use `build/windows-headless/omniweft_examples.exe` and `build/windows-headless/gltf_native_test.exe`. Output directories must be new. The Python oracle independently encodes the GLB and source identities and checks literal geometry, source indices, matrices, bounds, materials, complete Catalog states and artifact bytes. The integer/power-of-two fixture compares exactly; it makes no claim about arbitrary floating-point equality across platforms. Native nonexact transform tests use the preregistered 1e-12 absolute tolerance.

## Negative and recovery cases

Invalid indices, non-finite attributes, unsupported required extensions and oversized resources fail cleanly.

The example also covers stale Catalog revision, material-reference errors, external URI, cycle, mismatched declared bounds, BIN padding and truncation. Each rejected request returns no Scene and preserves the complete Catalog and input bytes, followed by successful same-source recovery. Native tests extend malformed and quota boundaries. Corruption selftests reject altered proof fields and malformed scalar types.

## Evidence and limitations

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. Retained evidence and actual passed/failed/not-run lanes belong in the [handoff](../docs/execution/PR-017-HANDOFF.md). All 48 local Windows CTests passed. [Audited clean evidence](../docs/evidence/PR-017/README.md) retains 2340 oracle and 4090 native assertions; all six hosted Windows/Linux runtime checks and both native manifest bindings passed.

This is the bounded POSITION-only GLB/TRS profile in the contract. No full glTF support, renderer, textures, World attachment, remote endpoint, cooked format, persistence migration, GPU or physics evidence. Source/license fields preserve attribution data without authenticating it. Existing Catalog and World formats remain unchanged.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.
