# PR-004 Windows GPU evidence

This bundle verifies runtime commit `973162f40536235c7dea24aff00bc7281cf93923` from a clean checkout. It is independent of later documentation/evidence commits. The hosted workflow subsequently received YAML quoting and Linux XTest-package fixes; no runtime, test, shader, dependency pin or build-tool source changed in those fixes.

- [Compressed reports and readbacks](windows-gpu-973162f.zip): 36,882 bytes, 36 entries, 6,866,317 bytes uncompressed.
- ZIP SHA-256: `c07f0706259d747386ed33cd60f2a964cc0925f353b2546047cd57c0ace4b7c6`.
- [Machine-readable summary](windows-gpu-973162f-summary.json).
- Oracle manifest SHA-256: `cd2075ac2b3638ec3e613bbc28eb9a348ce7d6525735e26348b0d5503c20360e`.
- Mutation manifest SHA-256: `db706e129602fd0f7a720cd15f3ad2cea29f6758586ce93e1a2ea9136f25e43e`.

The independent hardware oracle passed 1,961 assertions. The deliberate native identity-transform mutation was rejected at `initial: GPU ID mask`, and its proof passed 351 assertions. Both manifests record `dirty: false`, the exact runtime commit above and the same original executable SHA-256 `f35f5d8663e3448d17bed72c3d38e321e890affa6800ee90e44a021d94981f6b`. The executable itself is excluded.

The archive has fixed `oracle/` and `mutation/` roots containing each manifest and exactly its inventoried artifacts. Six normal/recovery frame captures include color, object-ID and depth readbacks. These are tightly packed raw buffers with extent, format, origin and row stride recorded in the accompanying reports. The two evidence manifests retain commands, exit codes, assertions, tool/device versions and artifact hashes.

A separate reviewer checked every archive entry/hash/size, re-evaluated retained structural, pixel and lifecycle evidence, and verified privacy exclusions. The archive contains 18 allowlisted JSON/patch entries and 18 canonical raw readbacks. There are no workstation executables, PDBs, raw build logs, account/host names, personal paths, device UUID/LUID/serial fields or archive comments.

Observed hardware: NVIDIA GeForce RTX 5070 on Windows, Vulkan 1.4.351; validation warnings/errors were zero. This is one Windows hardware result. Linux physical-GPU presentation remains `not_run`; the conservative Docker probe exposed software llvmpipe only.

See [the runnable example](../../../examples/render-world_cube.md), [frozen oracle contract](../../execution/PR-004-PLAN.md) and [execution handoff](../../execution/PR-004-HANDOFF.md). Hosted CPU/optional-build checks and their retained artifacts are attached to [PR #8](https://github.com/xsparc/omniweft/pull/8).
