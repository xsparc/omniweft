# PR-005 Windows SDK evidence

The current bundle verifies the runtime-expiry correction at clean commit `8ce2f06c6f9df626558a59bc694626d91c561a47`, tree `738aa2cf26537aec74b82cb76b2e210e8cdac175`.

- [Current reports and canonical snapshots](windows-sdk-8ce2f06.zip): 77,865 bytes, 154 entries, 890,548 bytes uncompressed.
- ZIP SHA-256: `9f94859d21770fdee65b7250c51818666bbf8f5f336cf6d940d056a85fbd4d87`.
- [Current machine-readable summary](windows-sdk-8ce2f06-summary.json).
- Oracle manifest SHA-256: `02178d1a038cd182147ffff6d5cf625aa6c8c8533be911b0e78af098b7e54e6f`.
- Mutation manifest SHA-256: `957546589bda136a34b56fb4a74a32dc50566cd8e0579462713fca824b659c85`.

The independent oracle passed 6,502 assertions and retained 150 artifacts. It compares complete fixed snapshots and independently serialized canonical bytes after creation, movement, rejected requests, session expiry/renewal and recovery. The separate SDK report contains 310 assertions, including three natural runtime expiries observed before cleanup and a real child-exit/stop-pipe race. The additional runtime-lifecycle.json records an incomplete connection closing at process lifetime expiry with actual native exit 0. The new regressions reject the earlier native binary and, separately, the previous SDK with the corrected native binary.

The deliberate mutation disables exactly the real native authentication guard in a disposable tracked-source copy, compiles a distinct executable, and requires the independent oracle to reject unauthorized full-world mutation. The proof passed 401 assertions and retains its exact patch and fixed failure. Original source and executable remained byte-identical. Both manifests record `dirty: false` and original executable SHA-256 `c27419eb3086b98ba71d9cb747531a1685054962f97f2826992816cfe4efc83d`; no executable is published.

Fixed `oracle/` and `mutation/` roots contain each manifest and exactly its inventoried artifacts. A separate reviewer checked entries, hashes, sizes, candidate/source binding, expected states and privacy. The archive contains no descriptor, token, epoch value, credential hash, raw HTTP/process stream, PDB, workstation executable or personal machine path/identifier. Entry timestamps are fixed; comments are empty.

Environment: Windows, Python 3.12.14, MSVC 19.44.35227.0, CMake 3.31.6, Ninja 1.13.2.git.kitware.jobserver-pipe-1, Debug, Vulkan disabled. PR-005 requires CPU checks; GPU is `not_applicable`.

All six hosted jobs passed for this runtime head: [planning](https://github.com/xsparc/omniweft/actions/runs/34755540239), [native](https://github.com/xsparc/omniweft/actions/runs/34755540240), [optional renderer builds](https://github.com/xsparc/omniweft/actions/runs/34755540235). Their tested merge candidate `8674aea8b5f323b1ce54066c6afab384216ac58b` has the same tree and expected main-base/runtime-head parents. Hosted artifacts retain separate SDK and compiled mutation evidence for both operating systems.

The [earlier archive](windows-sdk-3d23b2d.zip) and [summary](windows-sdk-3d23b2d-summary.json) are immutable historical evidence for runtime `3d23b2dfb089ecefbb69196fe5c779bdabb68dfa`, before the lifecycle correction. They do not certify the corrected runtime or new lifecycle checks. Its 153-entry archive SHA-256 is `93881b2f4253540ff2d99dcb05cf375441214a6510fde3bfa7ee6280b349bfcb`.

See [the example](../../../examples/sdk-move_cube.md), [frozen contract](../../execution/PR-005-PLAN.md), [SDK usage](../../../sdk/python/README.md) and [handoff](../../execution/PR-005-HANDOFF.md). [PR #9](https://github.com/xsparc/omniweft/pull/9) remains subject to final-head checks, review disposition and maintainer merge.
