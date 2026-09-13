# PR-005 execution handoff

State: **in_review**, not merged. [PR #9](https://github.com/xsparc/omniweft/pull/9), branch `codex/pr-005-authenticated-sdk`, base `ba80bb8392206a32810c8de989245e80cdfe055d`. Runtime candidate `3d23b2dfb089ecefbb69196fe5c779bdabb68dfa`, tree `01b9fe13202d7c03f8759ec0d6b7f58ff638f4ef`. [Authorization](AUTHORIZATION.md) · [frozen slice plan](PR-005-PLAN.md) · [example](../../examples/sdk-move_cube.md).

## Implemented behavior

The native `omniweft_control` process owns one World and admits authenticated loopback requests through the existing typed command coordinator. The separate standard-library Python SDK negotiates capabilities, creates/moves/deletes typed objects, observes complete snapshots, renews private sessions and requires explicit resynchronization after uncertain mutation responses. Request framing, credentials, expiry, world identity, revisions and sequence admission are bounded and validated. See [the wire contract](../../schemas/control-0.1.schema.json) and [SDK usage](../../sdk/python/README.md).

The slice adds native control files, Python SDK/example, five independent oracle/support files, the public schema, CMake/CPU CI integration and delivery documentation. Existing rendering runtime, shaders, dependency pins and permissions are unchanged. All native mutation callbacks run on the World owner thread. Admission is synchronous and volatile; there is no queued physics tick, receipt replay cache, scoped multi-principal policy, remote endpoint or persistent-format migration.

## Actual validation and evidence

From the repository root, with the pinned Windows toolchain configured:

~~~sh
cmake --build --preset windows-headless
ctest --preset windows-headless --output-on-failure
python tests/sdk_oracle.py --executable build/windows-headless/omniweft_control.exe --evidence-dir artifacts/sdk
python tests/sdk_mutation.py --build-dir build/windows-headless --evidence-dir artifacts/sdk-mutation
python tools/validate_plan.py
python -m unittest discover -s tools -p 'test_*.py'
~~~

- Windows configure/build and all 11 integrated CTest checks passed, including the separate-process SDK example and 10 oracle selftests.
- Clean retained local SDK oracle: 6,467 assertions, 149 artifacts; separate SDK client report: 296 assertions.
- Actual compiled authentication-bypass proof: 395 assertions; unauthorized full-world mutation detected; original source/executable unchanged.
- Planning validation passed all 38 items. Tooling suite: 31 passed, one skip because local symlink creation was unavailable (32 collected).
- All six hosted jobs passed for runtime head `3d23b2d`: [planning](https://github.com/xsparc/omniweft/actions/runs/34753912412), [native](https://github.com/xsparc/omniweft/actions/runs/34753912427), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34753912390).
- Hosted merge candidate `3623856bd79849c179f6477fdd59e6064c4bd05d` has exactly the runtime tree above and parents `ba80bb8392206a32810c8de989245e80cdfe055d` and `3d23b2dfb089ecefbb69196fe5c779bdabb68dfa`. Both OS artifacts bind that candidate/tree, report clean passed oracle and mutation runs, and match the reviewed Python source hashes.
- [Durable local evidence archive and independent audit](../evidence/PR-005/README.md). Raw local process/network streams and private session descriptors are excluded.

Later documentation/evidence commits require their own current-head hosted checks before delivery; the PR records the final exact head and check links. Runtime evidence remains explicitly bound to the clean candidate above. PR-005 GPU verification is `not_applicable`; no GPU support or performance claim is added by these CPU checks.

## Independent review and dispositions

Separate implementation and oracle authors used isolated worktrees; the coordinator alone integrated changes and edited the ledger. Independent native/SDK source, architecture/security, integration/CI, oracle/mutation and retained-artifact reviews completed with no unresolved blocking finding.

Three SDK findings were fixed and re-reviewed: reject decoded credential echoes before constructing public models; normalize arbitrarily large numeric inputs into typed protocol failures; enforce the whole-request deadline across connection establishment and I/O. Corresponding malformed-response, privacy and deadline regressions passed. One schema finding was fixed by reusing the authoritative command temporary-ID definition, preserving valid colon-containing IDs. A separate reviewer verified every public archive entry against the raw reviewed evidence, including exact hashes and privacy constraints.

No dependency or action pins changed, no new third-party runtime library was added, and no human approval or DCO sign-off is fabricated. Every new commit uses the [standing authorized noreply author and committer](AUTHORIZATION.md#commit-attribution-authorization).

## Prior merge and next action

PR-004 actually merged through [PR #8](https://github.com/xsparc/omniweft/pull/8) at 2026-09-13T10:08:36Z as `ba80bb8392206a32810c8de989245e80cdfe055d`. Its tree equals reviewed delivery `8e03add6e01b53e991fedd2b8c4e82346e69347a`; all six post-merge jobs passed: [planning](https://github.com/xsparc/omniweft/actions/runs/34751087947), [native](https://github.com/xsparc/omniweft/actions/runs/34751087956), [renderer](https://github.com/xsparc/omniweft/actions/runs/34751087968). Windows GPU evidence remains bound to `973162f40536235c7dea24aff00bc7281cf93923`; Linux physical GPU remains `not_run`.

The coordinator owns final delivery documentation and the ledger. Prior user/checkouts and helper worktrees remain preserved; machine-local paths stay in the private task. Finish current-head CI and review-thread disposition, make PR #9 ready, then wait for maintainer review/certification and squash merge. Automatic merge remains disabled. On the next wakeup, inspect the actual remote head/checks and merge state before claiming PR-006; record the actual merge before marking PR-005 done. No further routine commit-attribution permission is needed.
