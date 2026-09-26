# PR-013 recorded command replay evidence

Runtime candidate `973d72ab168856273d563cda4051c7c641f59807`, tree `f08bbb5ce2d3784a2a2d449adafdc155962a8d0d`. Draft [PR #19](https://github.com/xsparc/omniweft/pull/19); [delivery handoff](../../execution/PR-013-HANDOFF.md).

[windows-973d72a.zip](windows-973d72a.zip) contains exactly four JSON members, 15,422 bytes, SHA-256 `acd279f6c5d680747bdf7e0ed3e197638e7c17953ea73e52448add9f9f0018c3`. The clean-candidate oracle passed 1090 assertions and retained the separate 260-assertion native suite. Independent archive audit passed 11,016 static checks without rerunning native tests.

The audit independently reconstructs all four complete accepted snapshots, operations, receipts, resolved generation mappings, canonical checkpoint bytes, the exact 592-byte builtin asset descriptor and digest, six negative outcomes and recovery. Public archive members are byte-identical to retained private reports. Eighteen altered reports fail the oracle corruption selftest. Native coverage additionally includes transient same-batch create/delete/recreate identities, hierarchy replay, bounded capacity, external edits and prepublication allocation/guard rejection.

The manifest binds 56 source files and 13 SDK files, actual example/native executable hashes and configured tool/build provenance. Thirteen source bindings differ from Git blobs only by CRLF/LF conversion. Local build: Windows Release, MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2, Python 3.12.14, Vulkan disabled. All 36 Windows CTests passed. Only reviewed fixture data and generic versions/hashes are included; no private account/host/path data, credentials or epochs, raw process/HTTP streams, executable or PDB bytes.

Local Linux is not_run. Hosted current-head checks are separate and linked through the PR and handoff. This is bounded typed in-memory CPU replay, not a persisted journal, remote replay, physics or GPU evidence. Reproduce using [world.replay](../../../examples/world-replay.md).

All six runtime-head hosted checks passed, linked in the handoff. Both hosted native manifests use checkout `97b14a42fa5000b85db314a3da50951ed94a1125` with the identical runtime tree. Each passed 1090 oracle and 260 native assertions; all 56 source, 13 SDK and retained artifact bindings were verified. Windows used MSVC19.44.35229.0; Linux used Clang18.1.3. Final delivery-head status remains linked through the PR.
