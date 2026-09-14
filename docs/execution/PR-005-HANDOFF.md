# PR-005 execution handoff

State: **done**, merged by the maintainer at 2026-09-13T12:18:32Z as `3ab7bf114aa4acdef52387b1709be3fcaccb17b9`. [PR #9](https://github.com/xsparc/omniweft/pull/9), branch `codex/pr-005-authenticated-sdk`, base `ba80bb8392206a32810c8de989245e80cdfe055d`. Retained tested runtime is `8ce2f06c6f9df626558a59bc694626d91c561a47`, tree `738aa2cf26537aec74b82cb76b2e210e8cdac175`. [Authorization](AUTHORIZATION.md) · [contract](PR-005-PLAN.md) · [example](../../examples/sdk-move_cube.md).

## Verified completion

Final delivered head `5b89546baaf8e0437abe809d855b0afcc5ba5f39` and the actual squash merge have identical tree `6a31fa40a5fa5bc25683e9d821cde3fa53385c03`. The squash has the expected sole parent `ba80bb8392206a32810c8de989245e80cdfe055d`. The final additional automated review completed without new findings; the earlier lifecycle thread is resolved and outdated. Two independent checks of the live GitHub state confirmed these facts.

All six post-merge jobs passed: [Windows/Linux planning](https://github.com/xsparc/omniweft/actions/runs/34756733068), [Windows/Linux native](https://github.com/xsparc/omniweft/actions/runs/34756733065) and [Windows/Linux optional renderer build](https://github.com/xsparc/omniweft/actions/runs/34756733062). Completion records merge and checks; it does not invent a separate DCO sign-off or human review identity.

## Implemented behavior

The native `omniweft_control` process owns one World and admits bounded authenticated loopback requests through the existing typed command coordinator. The separate standard-library Python SDK negotiates capabilities, creates/moves/deletes typed objects, observes complete snapshots, renews private sessions and requires explicit resynchronization after uncertain mutation responses. See [the wire schema](../../schemas/control-0.1.schema.json) and [SDK usage](../../sdk/python/README.md).

All World callbacks run on the owner thread. Admission is synchronous and volatile; scoped multi-principal policies, filtered queries, replay/retention, remote endpoints and persistence remain later roadmap work. Existing rendering runtime, shaders, dependency/action pins and CI permissions are unchanged.

## Runtime-expiry correction

The additional automated review on earlier delivery `4abb3f4483af22bd645cbddecee9ea8ccf6f837d` found that normal runtime expiry and emergency termination shared one deadline. Independent review confirmed the race; the original local binary returned failure 4 in nine of ten idle 1000 ms lifetimes. PR #9 was returned to draft while the issue was corrected.

The configured max_runtime_ms remains the admission, network-I/O and private-renewal cutoff. A fixed additional 1000 ms allows orderly cleanup before the still-armed watchdog terminates a stalled process with exit 4. SDK shutdown reconciles a failed stop write by performing a bounded wait and accepting only a verified real exit 0; nonzero exits and timeouts remain failures.

Independent new regressions observe natural completion before requesting SDK cleanup, leave an actual HTTP body incomplete across process expiry, and force a real child exit between the SDK's alive poll and stop write. They fail against the preserved original native binary and, separately, the prior SDK with the corrected native binary. The corrected combination passes. No production test hook or fabricated child exit code is used.

## Actual validation and evidence

With the pinned Windows toolchain configured, run from the repository root:

~~~sh
cmake --build --preset windows-headless
ctest --preset windows-headless --output-on-failure
python tests/sdk_oracle.py --executable build/windows-headless/omniweft_control.exe --evidence-dir artifacts/sdk
python tests/sdk_mutation.py --build-dir build/windows-headless --evidence-dir artifacts/sdk-mutation
python tools/validate_plan.py
~~~

- Pinned local Windows build and all 11 CTest checks passed; the SDK check passed again after strengthening natural-exit observation.
- Current clean retained oracle: 6,502 assertions, 150 artifacts, including 310 separate SDK client assertions and the real lifetime-expiry outcome.
- Current compiled authentication-mutation proof: 401 assertions; unauthorized full-world mutation detected and original source/executable preserved.
- Planning validation passed 38 items. The unchanged tooling suite previously passed 31 tests with one skip for unavailable local symlink creation (32 collected).
- All six hosted jobs passed for runtime `8ce2f06`: [planning](https://github.com/xsparc/omniweft/actions/runs/34755540239), [native](https://github.com/xsparc/omniweft/actions/runs/34755540240), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34755540235).
- Hosted merge candidate `8674aea8b5f323b1ce54066c6afab384216ac58b` has the runtime tree above and parents `ba80bb8392206a32810c8de989245e80cdfe055d` and `8ce2f06c6f9df626558a59bc694626d91c561a47`. Both operating systems retain actual SDK and mutation evidence.
- [Current durable evidence and historical predecessor](../evidence/PR-005/README.md). Previous `3d23b2d` evidence remains explicitly historical and does not certify the lifecycle fix.

Final documentation/evidence commits require their own current-head hosted checks; the PR records that exact delivery head and its checks. Runtime evidence remains bound to the clean candidate above. PR-005 GPU testing is `not_applicable`; it adds no GPU or performance claim.

## Independent review and provenance

Separate native and test authors worked in isolated worktrees; the coordinator alone integrated files and edited the ledger. Independent native/SDK security, integration/schema, oracle/mutation, lifecycle-test and retained-artifact reviews passed after findings were addressed.

Earlier resolved SDK findings covered decoded credential echoes, large-number overflow and the whole-request deadline. The schema now reuses the authoritative temporary-ID definition. The lifecycle correction and real-process regressions were separately reviewed. Current and historical archives contain only reviewed fixed fixture/domain data and allowlisted metadata; raw private channels, personal paths and credential values/hashes are excluded.

No third-party runtime dependency, action pin, permission, save format or command schema was changed by the lifecycle correction. Every new commit uses the [authorized noreply author and committer](AUTHORIZATION.md#commit-attribution-authorization). No human approval, DCO sign-off or automatic merge authority is invented.

## Prior merge and next action

PR-004 merged through [PR #8](https://github.com/xsparc/omniweft/pull/8) at 2026-09-13T10:08:36Z as `ba80bb8392206a32810c8de989245e80cdfe055d`; its tree matches reviewed delivery `8e03add6e01b53e991fedd2b8c4e82346e69347a` and all six post-merge checks passed. Windows GPU evidence remains bound to `973162f40536235c7dea24aff00bc7281cf93923`; Linux physical GPU remains `not_run`.

The coordinator owns delivery docs and the ledger; all earlier user/helper worktrees remain preserved. PR #9 is closed and must receive no further pushes. PR-006 is dependency-ready under the existing authorization; its new isolated worktree starts from the verified squash above. Resume the PR-006 contract and handoff once claimed.
