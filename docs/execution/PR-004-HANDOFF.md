# PR-004 execution handoff

State: **in_progress**. [GitHub PR #7](https://github.com/xsparc/omniweft/pull/7) merged the contract only. Active implementation branch `codex/pr-004-vulkan-runtime`, base `b5b8def8cff7109e6e8898b2f4ccc4979346dadb`; a follow-up implementation PR is pending. Adopted scope and Windows/privacy clarification: [authorization](AUTHORIZATION.md). Contract: [plan](PR-004-PLAN.md). Runnable commands: [render.world_cube](../../examples/render-world_cube.md).

PR-003 is done from the actual maintainer squash merge of PR #6 at `e994f054d1e13ba19f714f35696ec8a1bca221a9`, with its reviewed tree and final/post-merge checks verified. No later slice is claimed complete.

## Source and ownership

The maintainer merged contract checkpoint `1d7bab387d9f9b9e91fd1f1d1b8e7f8313680774` as squash `b5b8def8cff7109e6e8898b2f4ccc4979346dadb` at 2026-09-13T05:44:38Z. Its eight documentation/ledger changes contain no renderer runtime, renderer tests, optional workflow or dependency pins. **Implementation, final oracle fixes and coordinator integration are included in the runtime candidate being prepared for publication.** The earlier Windows results below are historical dirty-checkout evidence; clean-candidate checks are next. Passing checks on the remote contract head do not certify this implementation.

The coordinator integrated 22 native files and six independent-test files; byte hashes were checked against the owners' final worktree copies. The native implementation supplies deeply owned CPU presentation packets, optional SDL3/Vulkan presentation, indexed original geometry/shaders, bounded allocations, required device/format checks, presentation/submission fences, actual window event recovery and controlled unsupported startup. Tests independently compute expected ray-box color/ID/depth, check typed receipts and lifetime provenance, and prove sensitivity with a native identity-transform mutation.

Coordinator-owned changes cover the optional Windows/Linux build workflow, public-evidence exporter and tests, dependency/usage/status documentation, planning ledger and the generated-cache Markdown exclusion/regression. Existing worktrees are preserved; machine-local locations are retained only in the private task. The coordinator remains the sole backlog owner.

## Actual local checks

These are **preliminary dirty-source results from the original contract checkout**, not certification of a merged renderer or a final committed candidate. The later branch reconciliation preserved every implementation/test byte; no new GPU result is claimed for the new base SHA.

| Check | Actual result |
| --- | --- |
| Windows optional Vulkan configure/build | Passed with two-way build parallelism |
| Windows optional CTest | 9/9 passed |
| Windows default headless configure/build | Passed without consuming optional dependencies |
| Windows default CTest | 9/9 passed |
| Integrated independent Windows GPU oracle | 1,961 assertions passed, none failed; four required capture phases, actual resize/minimize/restore, typed state, ID/color/depth, copy provenance, fence retirement and unsupported-driver recovery |
| Integrated real-GPU transform mutation proof | 345 assertions passed; deliberately broken identity TRS rejected at `initial: GPU ID mask`; original source/executable preserved |
| Oracle/privacy/lifecycle harness regressions | 18 passed |
| Planning validator | Passed: 38 linked work items and dependency/authorization checks |
| Planning/privacy tool regressions | 32 tests completed: 31 passed, one Windows symlink-creation test skipped |
| Shader generation/validation and provenance | Actual tools passed; independent review matched GLSL/SPIR-V and recorded hashes |
| Linux Docker runtime attempt | CPU llvmpipe only; software runtime discovery |
| Linux hardware presentation | **not_run**: attempted container exposes no physical Vulkan ICD |
| Hosted optional renderer checks on implementation | **not_run**: implementation has not been committed/pushed |
| Final clean-candidate checks and immutable independent approval | **pending** |
| Maintainer merge/contribution certification | **pending** |

Actual integrated commands were `cmake --build --preset windows-vulkan --parallel 2`, `ctest --preset windows-vulkan`, the two independent GPU commands in the example, and configure/build/CTest with `windows-headless`. The GPU commands used fresh private evidence directories. Validation-layer discovery and build-tool paths were process-local.

Observed environment: Windows, NVIDIA GeForce RTX 5070, Vulkan 1.4.351, driver 616.64; MSVC 19.44.35227.0, Python 3.12.14, CMake 3.31.6 and Ninja `1.13.2.git.kitware.jobserver-pipe-1`. Core and synchronization validation reported zero warnings/errors in the passing GPU runs. The signed SDK 1.4.357.0 archive was verified against SHA-256 `81f474711e9042f4cd22b31b2f7a8870db2e428b21586fb43dd80150be97310d` and extracted in documented copy-only mode. No driver, registry, permission or global-tool installation changes were made.

The first automatically removed Docker probe had network disabled, one CPU, 128 MiB memory, a 64-process limit and dropped capabilities. A second automatically removed attempt used one CPU, 512 MiB and a 128-process limit to provision Vulkan tools/Mesa. It observed llvmpipe, Vulkan 1.3.230 and Mesa 22.3.6. Unrelated containers were untouched. These attempts establish neither Linux physical-GPU presentation nor Linux desktop support. The maintainer's Windows-first priority remains binding.

## Retained evidence and privacy

Integrated evidence is retained privately under the coordinator's ignored `.cache` directory. Both manifests identify base `1d7bab387d9f9b9e91fd1f1d1b8e7f8313680774` with `dirty=true`; they must not be relabeled as final candidate passes.

- `render-oracle-integrated-001/manifest.json`: SHA-256 `b540f96eb2258c129ebb5c0efa48f62a87b730945fe4cd5c8a0dbe2a363e7f01`, 32 retained artifacts.
- `render-mutation-integrated-001/manifest.json`: SHA-256 `f15641eb7ee9b7a0f8ab7e8f8f7f0ad0e2993ba3789541284f4d27e6f292f5de`, two retained artifacts.
- Both tested executable SHA-256: `f35f5d8663e3448d17bed72c3d38e321e890affa6800ee90e44a021d94981f6b`.
- Mutation original presentation source SHA-256: `542d098f3a42f97e91d66b1ac81650e82cfe051209bacba47e39d134baec1af1`; mutant source SHA-256: `6e281d6a9155f17b521a1246adcd1c53c5edb1def8f1fbf27e15d7f4ee452c35`.

Artifact digests were rechecked after execution. Allowlisted JSON scans found no local account names, absolute personal paths, personal email, hostname or device UUID/LUID/serial fields. Raw workstation logs, executable/PDB bytes and environment inventories remain private. Public-exporter SHA-256 `4289471996a6bf451289c6debd4ca38179c2cb8c9d58b06616ec825fe3bafe12` received independent privacy/integrity review. It preserves checked assertions and commitments while copying only an exact known bootstrap result schema; render evidence uses its own allowlisted recorder. No new local evidence has been publicly uploaded at this checkpoint.

## Independent review and compatibility

Independent draft review found no remaining blocker after fixing per-device KHR/EXT capability fallback, binding captures/completions to the owning swapchain generation, allowing safe retirement of an unused generation, rejecting duplicate/non-finite JSON, binding retained binaries to the actual source checkout/tool metadata, and preventing partial/software runs from claiming a full hardware pass. Focused regressions accompany oracle fixes. Review also checked shader/dependency/license bytes, public evidence privacy, optional-build isolation and the bounded hosted workflow. These are technical reviews of draft bytes, not a fabricated human approval or final immutable-candidate sign-off.

Original shader/fixture material is Apache-2.0. Optional SDL/Vulkan-Headers source and notice pins are recorded in [the inventory](../../toolchains/render-dependencies.json) and [retained notices](../../third_party/render/NOTICE.md). No loader or graphics driver is redistributed. Command/save schemas remain unchanged; no persistent format is introduced. Rollback is a revert with fixtures retained.

## Attribution authorization and next action

The earlier automatic approval-review attribution blocker was resolved on 2026-09-13 when the maintainer explicitly replied, “I authorize this for Omniweft commits.” The [authorization record](AUTHORIZATION.md#commit-attribution-authorization) binds that consent to the exact public GitHub handle and noreply identity, including future increments. Verify author/committer metadata before every push; retain the prohibition on personal-email defaults and fabricated DCO sign-offs.

Proceed with the authorized commit, follow-up draft PR, both hosted operating-system lanes and local GPU oracle/mutation verification on the clean exact candidate. Review public evidence and immutable changes before marking the PR ready. Final delivery requires actual maintainer merge under existing contribution policy. Keep PR-005 unclaimed until PR-004 is done.

The hourly automation now carries this standing attribution authorization, Windows priority and public-data exclusions. Unchanged pending state stays quiet.

The historical source-byte inventory from before contract-merge reconciliation is retained at `pr004-review/source-checkpoint.json` under the ignored cache: SHA-256 `c205922e460fc64f2c012036bee25e34b47e15cd594f844fbb1f3575bb2b194d`, covering 40 changed files and excluding this handoff to avoid a self-reference. It is a dirty draft inventory, not an immutable commit approval. A scan of all 41 pending changed files found no actual workstation account/host token, personal attribution email or personal absolute-path marker.

The optional whitespace audit `git diff --cached --check` returned nonzero for unchanged upstream SDL license whitespace and extra blank lines at EOF in several new first-party source/test/shader files. Those bytes remain unchanged to retain the reviewed upstream/shader hashes; this audit is not a functional pass. Planning and functional results above are unaffected.

Before contract-merge reconciliation, final independent draft review rechecked both manifest hashes, all 34 retained artifacts and sizes, the current executable, and all 40 source-checkpoint entries. It found no private identity/path markers in JSON or pending patches. Initial/transformed color readbacks were decoded and inspected in memory and contained only the intended fixture/background; ID/depth spot checks were valid. No blocker remained in the final docs/evidence review. The reviewer did not rerun GPU execution or supply immutable-candidate approval.

## Contract-only merge reconciliation

On 2026-09-13, independent review verified the actual PR #7 merge by the maintainer, sole parent `e994f054d1e13ba19f714f35696ec8a1bca221a9`, and tree `b1a80f1f5d9b295a5e70636d8c927774cd4c060d`. This tree exactly matches the original contract head. The complete main tree contains no PR-004 renderer source, oracle/mutation tests, optional workflow or renderer dependency pins. Post-merge [native run 34741046114](https://github.com/xsparc/omniweft/actions/runs/34741046114) and [planning run 34741046181](https://github.com/xsparc/omniweft/actions/runs/34741046181) passed on both OSes; they certify only the contract and existing headless functionality. No formal review or inline finding was present; the automated contract review completed without an actionable finding.

The coordinator fetched the merge and created `codex/pr-004-vulkan-runtime` at that exact identical base tree, carrying the existing uncommitted implementation. Before/after hashes verified all 41 pending files and the complete staged index were unchanged. No commit or push was performed. The original branch and helper worktrees are preserved. The ledger records PR #7 under `contract_checkpoint`, leaving the runtime PR URL unset and PR-004 in progress.

At the contract-merge checkpoint, attribution authorization was still pending; the merge itself did not supply it. The maintainer subsequently authorized attribution explicitly, as recorded above. Open the follow-up draft so hosted checks can run; mark it ready only after clean-candidate verification and independent review. Continue the same bounded PR-004 behavior; no downstream item is claimed complete or started.

The current post-reconciliation source inventory is retained under the ignored cache at `pr004-review/source-checkpoint-contract-merge.json`, SHA-256 `f557b7d436e0edf5d5a0c5985ee827b2bd88bc249e288a20519d9964d5b92bdb`: 40 changed files, excluding this handoff. Its base is the actual contract squash; the plan and ledger changes are intentional, while native/test bytes remain unchanged. A privacy scan of all 41 pending files passed. The historical inventory and GPU reports above are preserved with their original hashes. Planning validation and 32 tool regressions completed after reconciliation: 31 passed, one Windows symlink case skipped.

The merged PR #7 title and description were corrected to describe its actual documentation-only contents, with exact post-merge CI links and an explicit statement that runtime delivery requires a follow-up. Its squash commit and shared history were left unchanged. The automation was updated at reconciliation to resume the follow-up branch and distinguish the contract merge from renderer completion; its former attribution pause was subsequently replaced with the explicit authorization above.

Independent reconciliation review closed both wording findings: historical inventory/review provenance is explicit, and draft creation follows attribution while ready status follows final checks and review. No further issue was found in that narrow review; it did not rerun functional tests.

Before the runtime commit, the six test files were normalized to their already-reviewed staged LF bytes. Four retained upstream render licenses received explicit byte-preservation attributes. The optional renderer workflow now runs and uploads allowlisted structural evidence on both hosted OSes with seven-day retention, using the existing pinned upload action. Runtime behavior and frozen oracle thresholds were unchanged.
