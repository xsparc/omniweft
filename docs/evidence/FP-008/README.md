# FP-008 asset retention evidence

[windows-845ce05.zip](windows-845ce05.zip) contains corrected clean Windows runtime evidence for commit `845ce05412dd21d28573d1c928c56eadaaae18bb`, tree `57a84587c10fdb9cd5a7616ba9a558407a82e3eb`. The [handoff](../../execution/FP-008-HANDOFF.md#hosted-debug-test-correction) records the initial hosted Debug test failure and independently reviewed injector repair.

| Property | Observed result |
| --- | --- |
| Archive | Five JSON members, 33,670 bytes |
| SHA-256 | `57a631ddf1b2a62217a34a609249356b2029023a7d5713c430cc52afda19fa97` |
| Runtime checks | 3,895 independent oracle assertions; 973 native boundary assertions |
| Corruption selftest | 41 rejected reports in the development check |
| Local compiler | MSVC 19.44.35227.0, explicitly pinned alternate |
| Build | Release, CMake 3.31.6, Ninja 1.13.2.git.kitware.jobserver-pipe-1, Vulkan disabled |
| Bindings | 76 source files, 16 SDK files, four retained artifacts; executable hashes and build provenance |
| Independent archive audit | 14,000 static checks passed |

Members are `manifest.json`, `native/actual.json`, `native/expected.json`, `native/assertions.json` and `native/cli-recovery.json`. They retain the synthetic fixture's complete query graphs, snapshots, canonical bundle bytes, command receipts and actual collection differences from separately reconstructed Catalogs. The auditor independently derived all seven query results and nine primary transitions without using the production query or Python oracle. Stale rejection, fresh recovery, detached-result mutation and both expected CLI exit-2 rejections were verified. All member hashes/sizes, exact source/SDK bindings and privacy checks passed. No raw process streams, executable/PDB bytes, credentials or personal-machine identifiers are included.

The initial local Windows source at `90775a6` passed all 51 CTests in 128.50 seconds. After the test-only repair at `845ce05`, all four affected Release checks passed in 2.45 seconds and the corrected native test passed in both Debug and Release with 973 assertions. The full 51-test local suite was not repeated on the corrected source. Native assertion counts vary with catchable STL payload-allocation sites; the suite requires the bounded 553–6589 range and records each actual count. Fatal allocation failure in library `noexcept` bookkeeping is outside this recovery evidence. The archive records one measured Windows run, not a universal cross-platform count or performance guarantee.

Local Linux was not run. Hosted Windows/Linux evidence and required checks are tracked on [draft PR #30](https://github.com/xsparc/omniweft/pull/30) and its [current checks](https://github.com/xsparc/omniweft/pull/30/checks). Verify the actual tested checkout/tree, source/SDK bindings and all retained artifacts before calling a hosted manifest verified. The [handoff](../../execution/FP-008-HANDOFF.md) and PR body record delivery status. Documentation-only delivery commits do not change this archive's runtime binding.

This proves native read-only retention inspection under existing owner-held roots. It does not establish remote authority, World/undo integration, durable storage, GPU or physics behavior. Public bundles contain synthetic fixture bytes only; the native inspector itself returns no content/provenance bytes.

## Historical initial archive

[windows-90775a6.zip](windows-90775a6.zip) remains bound to initial runtime `90775a6dad992dbb013684aafe092c7fb0559bb5`, tree `2a98d292a48b97e797cf1ea2d339561e1d01c925`: five JSON members, 33,683 bytes, SHA-256 `503bdc91fde7fa580c1432c009018b1637b880aa0e23decdc6eafa5f827eb80c`, 3,895 oracle / 969 native assertions and 13,997 independent static audit checks. It does not validate the later corrected test. Its successful Release run does not erase the subsequent initial hosted Debug failure.
