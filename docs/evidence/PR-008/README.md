# PR-008 room showcase evidence

Runtime: de3d52e8a4937a1eae59c3a41b22f791fdd00149; tree 98579c6e5933b6f0b3b46966dfb8526ed3f23f37.
Delivery: [draft PR #13](https://github.com/xsparc/omniweft/pull/13); not merged.

The [Windows archive](windows-de3d52e.zip) contains four unchanged passing manifests, their 106 declared artifacts, and one supplemental validation summary: 111 entries, 136850 bytes, SHA-256 a05e26e2c3a9c4176905d7a5eb63a053326f01b78377fa29f0682bafa11bc79f. The [external summary](windows-de3d52e-summary.json) adds the archive identity. Original work-item labels are preserved for regression proofs.

| Retained proof | Assertions | Artifacts |
| --- | ---: | ---: |
| Room CPU, complete states/receipts and independent observer barriers | 2045 | 7 |
| Room physical GPU, two complete color/ID/depth captures | 3069 | 15 |
| Existing agents physical-GPU regression | 4359 | 52 |
| Existing renderer physical-GPU regression | 1961 | 32 |

The 11434 assertions include metadata/provenance checks. The showcase verifies the complete five-object room, exact rejected receipt, rollback of a valid prefix, unchanged identities/generations, sequence consumption, idle quota recovery and corrected rearrangement. Expected values were authored separately from the example. Readback byte artifacts permit re-evaluation of the object masks, colors and projected depth, with actual publication/submission/fence provenance.

Windows used pinned Release builds. All 20 CTests passed (80.85 seconds) before the runtime commit with the same final runtime/test source; retained CPU/GPU and regression proofs then passed on the clean commit. The oracle selftests reject identity/revision corruption, leaked rollback state, and blank color/ID/depth buffers. Source hashes record actual tested Windows bytes; Git line-ending normalization applies to the selftest file. This is not a claim that every raw working-file hash equals its Git blob hash.

Independent source/oracle review passed. Final independent archive audit passed: original manifests and all artifact bytes, source/toolchain/binary bindings, privacy, public-copy equality and complete ZIP framing were verified. Independent retained-data re-evaluation passed 10179 checks across all 12 GPU captures plus state, quota, rollback and lifecycle evidence. No native/GPU workload or Docker run was repeated by that reviewer. Archive summary review/hosted fields describe capture time; later delivery outcomes belong in this README and the [handoff](../../execution/PR-008-HANDOFF.md), without rewriting original manifests. Local Linux container checks are not_run because the engine was unavailable; hosted Linux validation is tracked separately. Linux physical-GPU verification is not_run and no Linux desktop GPU support is inferred.

Only allowlisted fixture data, generic tool/device versions, source/artifact hashes and validated attachment bytes are public. Credentials/epochs, account/host identifiers, local paths, raw process/network logs and executable/PDB bytes remain excluded. No new assets, dependencies, grants, quotas, protocol or save-format changes are introduced.

## Hosted runtime checks

All six checks passed on runtime de3d52e8a4937a1eae59c3a41b22f791fdd00149: [Windows/Linux native](https://github.com/xsparc/omniweft/actions/runs/35339087519), [optional renderer builds](https://github.com/xsparc/omniweft/actions/runs/35339087374), and [planning](https://github.com/xsparc/omniweft/actions/runs/35339087375). These are hosted build/test results, not Linux physical-GPU evidence. The subsequent documentation/evidence delivery commit still requires its own current-head checks before integration.
