# PR-009 hierarchy evidence

Corrected runtime candidate: `9630e3bf8dd934d7b91b6f06d0e458272d5d6e1c`; tree `64ac352e998819c76ae968d320089f73a5b97acd`. Merged: [PR #15](https://github.com/xsparc/omniweft/pull/15). Source contract and verification history: [handoff](../../execution/PR-009-HANDOFF.md).

## Retained Windows CPU proof

[windows-9630e3b.zip](windows-9630e3b.zip) is 17,885 bytes with 13 members (manifest plus 12 artifacts). SHA-256: `d3912b09031bb879f65d8e7a7977f5c65ee664624a31ee9a633a59b0a8943302`.

The clean-candidate oracle passed 1,849 assertions and includes the separate native suite's 240-assertion result. Exact expected/actual native fixture state includes all parents, local/world transforms, revisions, receipts and independently encoded OWOBJ001/OWOBJ002 bytes. Retained remote proofs cover legacy control, agents and policy rejection without sequence/state consumption, successful same-sequence recovery, policy resource refund, and rejection of hierarchy observe/runtime callbacks. The native suite additionally covers noncommuting rotations, both detach modes, deep propagation, cycles, ambiguous deletion, stale child/parent slot reuse, numerical rejection, full staged rollback/allocation recovery and actual packet vertices.

The Windows build used Release, MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2 and Python 3.12.14 with Vulkan disabled. Seven actual executable hashes and build/toolchain provenance are retained; executable bytes are not. All 37 source and 12 SDK file hashes match the corrected runtime Git blobs exactly. The native runtime itself is unchanged from the preceding reviewed implementation; the corrected candidate fixes a Clang warning in one test loop.

Independent archive audit passed: exact membership/bytes/artifact hashes, candidate/tree/source/executable/toolchain binding and privacy. The reviewer replayed 840 literal fixture/canonical assertions without rerunning native processes; unchanged semantic artifacts match the first independently audited capture. No personal/account details, host paths/IDs, credentials or epoch payloads, raw HTTP/process streams or executable/PDB bytes are included. Earlier pre-correction archives remain private.

## Reproduce

After a pinned headless build, run the following, adding `.exe` to executable names on Windows and replacing `<build>` with the build directory:

```sh
python tests/hierarchy_oracle.py --executable <build>/omniweft_examples --control <build>/omniweft_control --agents <build>/omniweft_agents --policy <build>/omniweft_policy --hierarchy-legacy <build>/hierarchy_legacy_fixture --hierarchy-policy <build>/hierarchy_policy_fixture --native-test <build>/hierarchy_native_test --evidence artifacts/hierarchy
```

Use a fresh evidence directory. `ctest` also runs the native suite, complete fixture/transport oracle and corruption selftests. Existing native hosted jobs retain corresponding Windows/Linux evidence. Check the actual latest PR head; local proof does not make pending hosted jobs pass.

## Limits

Local Windows verified all 23 CTests across the full run and affected-test reruns described in the handoff. Local Linux is not_run; hosted Windows/Linux results are separate. This is a CPU-only feature: packet checks do not establish GPU or physics support. Native hierarchy is unsupported over the current SDK/policy profiles. No save parser, persistence or migration is implemented. Runtime uint64 exhaustion is not tested. Actual maintainer merge and post-merge checks are verified below.

## Verified integration

PR #15 merged as 4996aca01f7bfa76cacc50eb75aa6ffe0f042b3a, tree baabb21c90cdf3b9948a13e9001813d454dcd1a8, matching reviewed delivery f2b9d616584255c365e9eb1d375732ac6776392f. All six delivery and all six required post-merge checks passed: [native](https://github.com/xsparc/omniweft/actions/runs/35442078880), [renderer](https://github.com/xsparc/omniweft/actions/runs/35442078864), [planning](https://github.com/xsparc/omniweft/actions/runs/35442078906). Evidence remains bound to corrected runtime 9630e3b. PR-009 is done; PR-010 is now eligible.
