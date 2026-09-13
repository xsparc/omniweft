# PR-005 Windows SDK evidence

This bundle verifies runtime commit `3d23b2dfb089ecefbb69196fe5c779bdabb68dfa` and tree `01b9fe13202d7c03f8759ec0d6b7f58ff638f4ef` from a clean checkout. Later delivery documentation and archive commits do not change the tested runtime, SDK or oracle sources.

- [Compressed reports and canonical snapshots](windows-sdk-3d23b2d.zip): 77,151 bytes, 153 entries, 884,104 bytes uncompressed.
- ZIP SHA-256: `93881b2f4253540ff2d99dcb05cf375441214a6510fde3bfa7ee6280b349bfcb`.
- [Machine-readable summary](windows-sdk-3d23b2d-summary.json).
- Oracle manifest SHA-256: `4c91f4db04c058e644b78746cc556d776bede6305284bb260845c382e508e3ee`.
- Mutation manifest SHA-256: `3870b3e57932b38a639576e3583dc880868c57e656da9ea5a436bc9c3324b8a2`.

The independent local oracle passed 6,467 assertions and retained 149 artifacts, including 296 separate SDK client assertions. It compares complete fixed snapshots and independently serialized canonical bytes after creation, movement, denials, expiry, session renewal and recovery. The separate Python example's typed receipts and snapshots match independently constructed expectations.

The deliberate native mutation disables exactly the real authentication guard in a disposable copy of the tracked candidate, compiles a distinct executable, and requires the independent oracle to reject it at `wrong-token: complete world unchanged`. Its proof passed 395 assertions and retains the exact patch and fixed oracle failure. Both original source and executable remained byte-identical. The two manifests record `dirty: false` and original executable SHA-256 `45b401c22ad078ee6771747984feeabab343de3b6f5f0de9b8a71362bdd818d7`; executables themselves are excluded.

The archive has fixed `oracle/` and `mutation/` roots containing each manifest and exactly its inventoried artifacts. A separate reviewer checked all entries, hashes, byte sizes, fixture semantics, candidate/source binding and privacy exclusions. It contains no private descriptor, token, epoch value, credential hash, raw HTTP/process stream, PDB, workstation executable or personal machine path/identifier. Archive entry timestamps are fixed and comments are empty.

Environment: Windows, Python 3.12.14, MSVC 19.44.35227.0, CMake 3.31.6, Ninja 1.13.2.git.kitware.jobserver-pipe-1, Debug, Vulkan disabled. This SDK slice requires CPU checks; its GPU lane is `not_applicable`.

Hosted [Windows/Linux native checks](https://github.com/xsparc/omniweft/actions/runs/34753912427), [planning](https://github.com/xsparc/omniweft/actions/runs/34753912412) and [optional renderer builds](https://github.com/xsparc/omniweft/actions/runs/34753912390) passed for runtime head above. Their tested merge candidate `3623856bd79849c179f6477fdd59e6064c4bd05d` has the same tree, with the recorded main base and runtime head as its parents. Hosted artifacts retain separate native SDK oracle and compiled mutation evidence for both operating systems.

See [the runnable example](../../../examples/sdk-move_cube.md), [frozen contract](../../execution/PR-005-PLAN.md), [SDK usage](../../../sdk/python/README.md) and [execution handoff](../../execution/PR-005-HANDOFF.md). [PR #9](https://github.com/xsparc/omniweft/pull/9) remains subject to current-candidate checks and maintainer merge.
