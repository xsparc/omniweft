# PR-004 execution handoff

## Actual maintainer merge

Merged through [PR #8](https://github.com/xsparc/omniweft/pull/8) at 2026-09-13T10:08:36Z as `ba80bb8392206a32810c8de989245e80cdfe055d`. The squash tree equals the independently reviewed delivery tree `5baf923a0972fc099cfbc7c5082e5590b8ced98c`. All six post-merge planning/native/optional-render jobs passed; their run links and next slice are recorded in [PR-005 handoff](PR-005-HANDOFF.md). PR-004 is now **done**. The delivery record below is historical and preserves its original pre-merge status, evidence SHAs and limitations.

State: **in_review**. [Implementation PR #8](https://github.com/xsparc/omniweft/pull/8), branch `codex/pr-004-vulkan-runtime`, base `b5b8def8cff7109e6e8898b2f4ccc4979346dadb`. Adopted scope and standing noreply attribution: [authorization](AUTHORIZATION.md). [Plan](PR-004-PLAN.md) · [runnable example](../../examples/render-world_cube.md) · [public GPU evidence](../evidence/PR-004/README.md).

PR #7 merged only the eight-file contract checkpoint. PR-004 is not done: the runtime implementation is in PR #8 and requires actual maintainer merge. PR-003's actual merge remains `e994f054d1e13ba19f714f35696ec8a1bca221a9`.

## Implementation and immutable candidates

Runtime commit **`973162f40536235c7dea24aff00bc7281cf93923`**, tree `ce67686ebba2d78d06d35bc31c6d5f6336b342d3`, includes the renderer, runnable example, independent tests, provenance, privacy exporter and explicit attribution record. Source review approved this exact commit. Clean local Windows verification below ran on this commit.

The adapter converts detached committed snapshots into deeply owned presentation packets and renders a bounded original cube fixture through optional SDL3/Vulkan. It verifies required capabilities/formats, uses one frame in flight, copies the exact captured color source into the presented image, and retires resources only after graphics and presentation completion. Typed transforms, identity/revision receipts, actual resize/minimize/restore and unsupported startup/recovery are exercised. The default headless build consumes no optional graphics dependency.

Two subsequent commits change hosted setup only: `3c67025a10dd45df8b31c8445518673da712b29c` wraps the unchanged pip command in a YAML block scalar, and `08d8367a71d4c1a317381c928f935a25f255489f` installs the required Linux SDL XTest development package. No runtime, test, shader, dependency-pin or build-tool source differs from the tested runtime commit. Later evidence/documentation commits must preserve those exact files; their SHA is distinct from the runtime SHA.

## Actual verification

| Check | Result and evidence |
| --- | --- |
| Local Windows default and optional builds | Passed; CTest **9/9** for each configuration |
| Clean Windows hardware oracle | **1,961 assertions passed**, zero failures; both manifests identify runtime `973162f` with `dirty:false` |
| Clean native transform-mutation proof | **351 assertions passed**; independent oracle rejected identity TRS at `initial: GPU ID mask`; original source/executable preserved |
| Oracle/privacy/lifecycle harness | **18 passed**, included in CTest |
| Planning and public-export regressions | Planning validator passed; **31 passed, one Windows symlink case skipped** out of 32 tool tests |
| Hosted planning, both OSes | [Run 34749008421](https://github.com/xsparc/omniweft/actions/runs/34749008421): passed at branch head `08d8367` |
| Hosted native, both OSes | [Run 34749008453](https://github.com/xsparc/omniweft/actions/runs/34749008453): passed at branch head `08d8367` |
| Hosted optional renderer, both OSes | [Run 34749008417](https://github.com/xsparc/omniweft/actions/runs/34749008417): passed at branch head `08d8367`, with retained allowlisted headless structural evidence |
| Linux hardware presentation | **not_run**; bounded Docker attempts exposed CPU llvmpipe rather than a physical Vulkan ICD |
| Final branch status and maintainer merge | Current checks/review are attached to [PR #8](https://github.com/xsparc/omniweft/pull/8); merge is still required |

Hosted runs above tested merge candidate `e954efa92b7b82234e5af782de8f6f101af8317f`, whose parents are base `b5b8def8cff7109e6e8898b2f4ccc4979346dadb` and head `08d8367a71d4c1a317381c928f935a25f255489f`. Its tree `2eb440e2d7ce106b3ef525e3980a83a941bdd498` equals that head's tree. Independent review verified all six jobs, all four downloaded artifact ZIP digests and all 462 inventoried payload hashes/sizes. Retained manifests report `dirty:false`. Hosted planning passed 32/32 on both OSes, both native and optional CTest passed 9/9, and the structural renderer oracle passed 529 assertions per OS with GPU explicitly `not_run`. Both native mutation proofs detected their intended defects.

| Retained hosted artifact | GitHub artifact ID | ZIP SHA-256 |
| --- | --- | --- |
| Native Linux | `10315157094` | `1b16bf537a4ec854c3eda54a7924547ff3ee2cc7f90f41999948dbddc6a7de84` |
| Native Windows | `10314688430` | `6fbae1a9e195677e076f18e4da342d04a59a90ce7de51353d2e92d50fdfdf7a2` |
| Structural Linux | `10315576071` | `d4ca71681c6312e0b140e75836185e2c09a9e94103bb1b1733c68dcbce0b904b` |
| Structural Windows | `10315311747` | `6b3b6a9abbe11aad59fe810895b0c0f23c9a3f3a014a669574354a2067f603ef` |

Hosted jobs check out GitHub's PR merge candidate; record its actual SHA/tree from retained artifacts when assessing later checks. A passing hosted build is CPU evidence, not physical-GPU certification. Documentation/evidence-only updates require current hosted checks and source-identity review, without relabeling the earlier GPU run as a different commit.

Local tools were MSVC 19.44.35227.0, Python 3.12.14, CMake 3.31.6 and Ninja `1.13.2.git.kitware.jobserver-pipe-1`. Hardware was NVIDIA GeForce RTX 5070, Vulkan 1.4.351, driver 616.64. Core and synchronization validation produced zero warnings/errors. Build/layer settings were process-local; no host driver, registry, permission or global-tool changes were made.

Commands are recorded in the manifests and [example](../../examples/render-world_cube.md): configure/build/CTest for both Windows presets, `render_oracle.py --gpu` with the full case set, and `render_mutation.py` against the same built executable. Output directories were fresh; Git status was clean before retained runs.

## Public evidence and independent review

[The 36,882-byte archive and summary](../evidence/PR-004/README.md) retain both manifests and exactly their inventoried artifacts under fixed `oracle/` and `mutation/` roots. ZIP SHA-256: `c07f0706259d747386ed33cd60f2a964cc0925f353b2546047cd57c0ace4b7c6`. Oracle manifest: `cd2075ac2b3638ec3e613bbc28eb9a348ce7d6525735e26348b0d5503c20360e`; mutation manifest: `db706e129602fd0f7a720cd15f3ad2cea29f6758586ce93e1a2ea9136f25e43e`.

Both manifests bind the same original executable SHA-256 `f35f5d8663e3448d17bed72c3d38e321e890affa6800ee90e44a021d94981f6b`; executable/PDB bytes are excluded. Independent evidence review recomputed all 36 archive-entry hashes/sizes and rechecked six retained frames, typed structure, pixels and lifetimes with 1,824 audit assertions. The bundle contains only 18 allowlisted JSON/patch entries and 18 canonical raw readbacks. No private identity/path/device-ID fields, raw workstation logs, unrelated data or archive comments were found.

Independent source review verified all 22 native blobs, six normalized test blobs, fixed oracle thresholds, shader/SPIR-V provenance, dependency/archive/license bytes and public-exporter integrity. All findings were resolved. Hosted review and immutable evidence approval are technical checks; they do not fabricate human approval or contributor certification.

## Failures, limitations and history

The first optional workflow attempt failed YAML parsing at the pip command's colon-space sequence ([run 34748756773](https://github.com/xsparc/omniweft/actions/runs/34748756773)). After syntax repair, Linux configuration exposed missing XTest headers ([run 34748908771](https://github.com/xsparc/omniweft/actions/runs/34748908771)). Both causes were repaired; run 34749008417 passed both optional builds. No checks or thresholds were disabled.

Conservative Docker probes were temporary and automatically removed, bounded to one CPU and at most 512 MiB/128 processes. They observed llvmpipe, Mesa 22.3.6 and Vulkan 1.3.230. Unrelated containers were untouched. Linux desktop/physical-driver support and performance are not claimed.

Earlier dirty-checkout GPU reports and local source inventories are historical, retained privately with their original hashes. They are superseded for runtime verification by the clean `973162f` bundle. The optional whitespace audit reports preserved upstream SDL license whitespace and extra EOF blank lines in new first-party files; it is not claimed as a passing functional check.

## Compatibility and next action

Original fixture/shader material is Apache-2.0. [Optional dependency pins](../../toolchains/render-dependencies.json) and [retained notices](../../third_party/render/NOTICE.md) cover SDL/Vulkan-Headers; no graphics driver or loader is redistributed. Command/save schemas remain unchanged. Rollback is a revert with fixtures preserved; no persistent format migration is introduced.

The maintainer explicitly authorized the public GitHub noreply identity for current and future Omniweft commits. The earlier attribution pause is resolved; do not ask again for routine attribution. Verify author/committer fields before pushing and retain all existing privacy, contribution and merge rules.

After the evidence/documentation update, verify the latest PR head's required hosted checks and unchanged runtime inputs, obtain final independent review and mark the implementation PR ready. Only actual maintainer merge may mark PR-004 done. Then record the real merge SHA/tree/checks and select the next eligible bounded item. Until then, resume PR #8 and keep downstream work unclaimed. The coordinator owns the backlog; existing user/helper worktrees remain preserved.
