# FP-008 asset retention evidence

This is the historical initial Release archive. Subsequent hosted Windows Debug failure and the reviewed test-injector correction are documented in the [handoff](../../execution/FP-008-HANDOFF.md#hosted-debug-test-correction). The original archive remains bound to its actual source; it does not validate the corrected test. Current delivery evidence must use the corrected candidate.

[windows-90775a6.zip](windows-90775a6.zip) contains the clean Windows runtime evidence for commit `90775a6dad992dbb013684aafe092c7fb0559bb5`, tree `2a98d292a48b97e797cf1ea2d339561e1d01c925`.

| Property | Observed result |
| --- | --- |
| Archive | Five JSON members, 33,683 bytes |
| SHA-256 | `503bdc91fde7fa580c1432c009018b1637b880aa0e23decdc6eafa5f827eb80c` |
| Runtime checks | 3,895 independent oracle assertions; 969 native boundary assertions |
| Corruption selftest | 41 rejected reports in the development check |
| Local compiler | MSVC 19.44.35227.0, explicitly pinned alternate |
| Build | Release, CMake 3.31.6, Ninja 1.13.2.git.kitware.jobserver-pipe-1, Vulkan disabled |
| Bindings | 76 source files, 16 SDK files, four retained artifacts; executable hashes and build provenance |
| Independent archive audit | 13,997 static checks passed |

Members are `manifest.json`, `native/actual.json`, `native/expected.json`, `native/assertions.json` and `native/cli-recovery.json`. They retain the synthetic fixture's complete query graphs, snapshots, canonical bundle bytes, command receipts and actual collection differences from separately reconstructed Catalogs. The auditor independently derived all seven query results and nine primary transitions without using the production query or Python oracle. Stale rejection, fresh recovery, detached-result mutation and both expected CLI exit-2 rejections were verified. All member hashes/sizes, exact source/SDK bindings and privacy checks passed. No raw process streams, executable/PDB bytes, credentials or personal-machine identifiers are included.

The full local Windows suite separately passed all 51 CTests in 128.50 seconds. That suite result is not inferred from this archive. Native assertion counts vary with STL allocation-failure sites; the suite requires the bounded 549–6585 range and records each actual count. The archive records one measured Windows run, not a universal cross-platform count or performance guarantee.

Local Linux was not run. Hosted Windows/Linux evidence and required checks are tracked on [draft PR #30](https://github.com/xsparc/omniweft/pull/30) and its [current checks](https://github.com/xsparc/omniweft/pull/30/checks). Verify the actual tested checkout/tree, source/SDK bindings and all retained artifacts before calling a hosted manifest verified. The [handoff](../../execution/FP-008-HANDOFF.md) and PR body record delivery status. Documentation-only delivery commits do not change this archive's runtime binding.

This proves native read-only retention inspection under existing owner-held roots. It does not establish remote authority, World/undo integration, durable storage, GPU or physics behavior. Public bundles contain synthetic fixture bytes only; the native inspector itself returns no content/provenance bytes.
