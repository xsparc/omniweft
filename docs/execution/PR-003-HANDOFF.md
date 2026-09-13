# PR-003 execution handoff

State: **done**. [GitHub PR #6](https://github.com/xsparc/omniweft/pull/6) was squash-merged by xsparc at 2026-09-13T00:14:29Z as `e994f054d1e13ba19f714f35696ec8a1bca221a9`. Final reviewed head `825cdf3f3bb3d0e6bc68be22cc68ffb9e1e7c4e8`; branch `codex/pr-003-atomic-objects`; base `32e32ae19b5138231159fe961246dbcf5c8a88c2`. [Adopted scope](AUTHORIZATION.md) and [slice plan](PR-003-PLAN.md).

PR #5 was independently verified merged by xsparc at 2026-09-12T23:11:17Z, with squash commit `32e32ae19b5138231159fe961246dbcf5c8a88c2`. Its final Windows/Linux run 34724489627 and planning run 34724489605 passed. PR-002 is done; PR-003 is done; PR-004 is the next claimed item.

## Delivered behavior

The trusted host applies one typed batch at a synchronous boundary. The coordinator validates the envelope, world and revision; stages complete bounded authoring state and provisional identities; creates the receipt; then publishes once through a nonthrowing swap. Failure discards all staged changes. Create, transform and root deletion preserve UUID/generation identity, transaction-local references and revision contracts. Slots reuse the lowest available index, with generation increment and retirement instead of wrap.

`ow_world` owns storage and detached snapshots without a command dependency. `ow_transactions` owns mutation. The additive schema 0.1 `entity.delete` operation requires `child_policy: "reject_if_children"`. Previously valid envelopes remain valid; older readers reject the new operation as unsupported. There are no new dependencies or toolchain changes. Diagnostic canonical format 1 is not a save-file compatibility promise.

All objects are bare roots using `builtin.unit_cube`. The host owns one thread and must supply the execution boundary. Receipts are volatile. Transport authentication, epoch admission, receipt deduplication, scheduling, persistence, hierarchy, renderer and physics remain later work.

## Actual verification

Initial integrated head: `fe4e684605268a1b1874722203fe2c2ea3e40b79`. Tested GitHub merge candidate: `88030420557cb7c60a1d25e02442bf97e2d1b612`, with parents equal to that head and the base above. Candidate and head share tree `37a0fced9019746e14cddd3473f33217fdfb0ffe`.

- [Native run 34726759921](https://github.com/xsparc/omniweft/actions/runs/34726759921): configure, build and CTest **6/6 passed on both Windows and Linux**, including all existing bootstrap/protocol tests and new objects.native/objects.atomic.
- [Planning run 34726759918](https://github.com/xsparc/omniweft/actions/runs/34726759918): planning validation and 13 validator regression tests passed.
- Independent object oracle: **27 stateful transactions**, a two-transaction control world, three built-in transactions repeated, output preservation and missing-input cases. Initial retained manifest: **3,357 assertions / 9 commands per platform**.
- Existing bootstrap: **75 assertions / 24 commands** per platform. Additive protocol coverage: **158 cases / 19 stable round-trips / 3 built-in cases**, **1,701 assertions / 11 commands** per platform.
- Both deliberate mutation proofs passed: **16 assertions / 6 commands each** per platform. The object proof swaps a rejected staged prefix into a disposable source copy; the unmodified independent oracle then exits 1 at `staged-prefix-rollback: full-state after`. Original source hashes remain unchanged.

The follow-up in this handoff commit integrates independently reviewed test helper `8b2cdff210cbff4154f96a1bef4f5c2bfe1c8245`, adding exact integer-token assertions to the oracle. **The run above predates that strengthening.** Current-head Windows/Linux and planning checks must pass before marking the PR ready. Final candidate, assertion totals and immutable run links belong in the PR readiness record, avoiding a commit hash that refers to itself. Verify those links against the actual current head before merge; the preceding run alone cannot establish final readiness.

Commands used by CI (replace PLATFORM with the pinned platform build directory):

```sh
python tools/bootstrap.py
python tests/bootstrap_oracle.py --executable build/PLATFORM/omniweft_examples --evidence-dir artifacts/bootstrap
python tests/protocol_oracle.py --executable build/PLATFORM/omniweft_examples --evidence-dir artifacts/protocol
python tests/objects_oracle.py --executable build/PLATFORM/omniweft_examples --evidence-dir artifacts/objects
python tests/protocol_mutation.py --build-dir build/PLATFORM --evidence-dir artifacts/protocol-mutation
python tests/objects_mutation.py --build-dir build/PLATFORM --evidence-dir artifacts/objects-mutation
python tools/validate_plan.py
python -m unittest discover -s tools -p "test_*.py"
```

Windows uses `windows-headless/omniweft_examples.exe` and the pinned developer shell; Linux uses `linux-headless/omniweft_examples`. Actual command arrays, exit codes, input/output logs and hashes are retained in manifests. CTest LastTest.log is uploaded as well.

Environment: Ubuntu 24.04 / Clang 18.1.3; Windows Server 2022 / MSVC 19.44.35228.0 with toolset 14.44.35207. Both native lanes used Python 3.12.10, CMake 3.31.6 and Ninja executable 1.13.2.git.kitware.jobserver-pipe-1 from wheel 1.13.2. Seed 7; actual source manifests reported clean. No local PR-003 execution is claimed: one local main status read was clean, but later read-only metadata/worktree calls did not return. Existing local worktrees are preserved.

Initial object report SHA-256 on both platforms: `8388e74e5118f423ec508213e514e656bfe5f9c39d52913283737348da24fa8e`. Independent final canonical authoring state SHA-256: `1670d0cbf493dbd54e1b4f6eb59ea3b080027f6742312386b28d6a43a7e1506f`. The control world confirms allocation after a failed batch matches the successful-only history. Full JSON state and independently encoded canonical bytes are checked before and after every batch, including identity/generation, live/deleted/retired slots, authoring revisions and all transform components.

Initial native artifacts (seven-day retention, unexpired when reviewed):

| Platform | Artifact ID | ZIP SHA-256 |
| --- | --- | --- |
| Linux | 10308250273 | `c77e969f65e9d3a85e16652328b077cccf6125248901564f3490d8f221bf8202` |
| Windows | 10307769006 | `735bbea4fc939d71251459a11655f49fdb0007aaa312bda72847ce4ef880dd67` |

## Independent review

Agent `bootstrap_architecture` reviewed native helper `e31552c55acc183242f826647478bd5c0be576d4`, the stricter object oracle at `8b2cdff210cbff4154f96a1bef4f5c2bfe1c8245`, other tests at `2c0a1a5128bae4485033b64a90d3c20a94e99885`, and integrated workflow at `fe4e684605268a1b1874722203fe2c2ea3e40b79`: no actionable findings remained. The reviewer independently checked both hosted job logs, planning results, artifact metadata and matching cross-platform hashes above. The integer-token strengthening still needs current-head hosted evidence.

The oracle constructs expected state and canonical bytes independently of native output, checks a literal empty-state fixture, rejects stale generations and temporary references, and exercises real rollback after modifying an existing transform, creation and deletion. It uses no privileged state injection or production test hooks. Revision overflow and generation retirement guards received source review only; exhaustive 64-bit exhaustion is **not run**. This review is an independent agent review, not a fabricated human GitHub approval or contributor sign-off.

## Resume, merge and rollback

Before merge, verify final current-head planning and both native checks, evidence artifacts and independent review in the PR readiness record. Resolve any review conversations. Only a verified actual maintainer merge may change PR-003 to done. No automatic merge or contributor certification policy has been adopted.

The hourly task resumes from remote state. After merge, record the squash SHA/time, preserve this evidence and claim the next dependency-ready bounded item, starting with PR-004. Recheck its GPU/device evidence requirements before implementation; CPU checks cannot prove presentation or hardware support.

Rollback: revert this PR, retaining source fixtures and previous evidence. No persistent world data or save format is introduced. Do not discard user changes, alter protections, fabricate sign-offs or publish a release.

## Final checks and actual merge

The strict integer-token follow-up passed [native run 34727171603](https://github.com/xsparc/omniweft/actions/runs/34727171603) and [planning run 34727171597](https://github.com/xsparc/omniweft/actions/runs/34727171597) before merge. Candidate `5597f168bab98a24ce18855bd5d6fc1864af263f` has parents equal to the base and final head above; its tree `ae3f0f8b06c7d57c16667f8937e89d4314711b55` also equals the squash commit's tree. Both native lanes passed CTest 6/6 and the final object oracle recorded **4,126 assertions / 9 commands**; all earlier object hashes and other suite totals above stayed unchanged. Both planning lanes passed validation and 13 regression tests. Independent final source/test/docs/log/artifact review found no actionable findings. No unresolved GitHub review conversations existed at merge.

Final retained artifacts: Linux 10307829293, ZIP SHA-256 `450a6742c7badd6fa68d44631c8109e1184df4b1e947e7bd48131108afbcaf33`; Windows 10307744552, ZIP SHA-256 `6ec66ab1fb845f245542a2dad5f2fbb41efdf9d27718696ddad7c7668a199500`. Both were unexpired when checked. Post-merge [native run 34727459583](https://github.com/xsparc/omniweft/actions/runs/34727459583) and [planning run 34727459619](https://github.com/xsparc/omniweft/actions/runs/34727459619) passed on the actual squash SHA. Earlier pending-check wording is the historical checkpoint, superseded by this final verified result.
