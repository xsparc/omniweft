# PR-007 handoff

State: **PR #11 merged; post-merge SDK compatibility repair in progress**.
Adopted scope: [authorization](AUTHORIZATION.md).
Branch: codex/pr-007-capabilities-budgets. Base: 270f67b21237c180d8c37906bd03fc31f3be0010.
Runtime candidate: 3055ada3604f7df879454d71f44956437496dd56.
Runtime tree: 66a46165fe9b7e4797f4758dfa6a161ef54a684d.
PR URL: [draft #11](https://github.com/xsparc/omniweft/pull/11). The maintainer explicitly requested draft publication on 2026-09-17. Maintainer certification/review and actual merge remain required.

## Result

The [plan](PR-007-PLAN.md) and [runnable example](../../examples/policy-denied_edits.md) define the fixed two-principal behavior. The shared typed Coordinator checks complete cube bounds and atomically commits World and retained-memory sponsorship. Native admission and header-only policy ingress precede body allocation/serialization/staging. A separate CPU-only fixed-step host has three bounded transport workers; a stable request lease covers body reception, Owner dispatch and response completion. PolicySession/PolicyClient expose private principal credentials, immutable grants/usage and quota-safe resynchronization. Canonical world bytes and default host/profile shapes remain compatible.

All scopes, operation counts, retained/working charges and observations use the versioned fixture contract. Memory figures are deterministic charges, not heap/RSS guarantees. Whole-world reads are explicitly granted to both principals. General fairness, confidential filtering, rates, grant administration, persistence and new representations are outside the slice. No new dependency or external asset is introduced.

## Actual verification

- Pinned Windows Release build passed with two build jobs.
- All 18 Windows CTests passed sequentially. The later test-only raw-wire refinement reran through the clean-candidate policy oracle; production binaries stayed unchanged.
- Native policy tests passed 843 assertions: scope, prefix amplification, repeated rollback, sponsor/identity recovery and native admission lifetime.
- Clean runtime policy oracle passed 1007 retained assertions, including literal complete state and real HTTP timeout/isolation/recovery.
- A compiled native destination-scope bypass was detected: 480 retained assertions.
- Existing native authentication-bypass regression passed 479 retained assertions; existing Owner lifetime/failure regression passed 268, including its 33 native checks.
- Planning validation passed all 38 items. This is documentation/graph validation, not a runtime or GPU test.
- On 2026-09-16 the same clean runtime passed local Ubuntu 24.04 container validation: pinned Release build, 18/18 CTests, native policy 843 assertions, policy oracle 1007, scope mutation 480, authentication mutation 479 and Owner dispatch/lifetime proof 268. All 32 planning regression tests also passed. The retained Linux package and provisioning recovery are described in the evidence README; independent archive integrity/privacy audit passed with no blocking finding, without runtime replay or independent live-container inspection.

[Public-safe evidence](../evidence/PR-007/README.md) contains the original clean-candidate manifests and hashed fixture artifacts. Its archive is bound to runtime 3055ada, not relabeled as a later documentation delivery. Independent read-only archive audit passed with no blocking finding. Full ZIP structure/bytes, all inventories/hashes, exact candidate/tree and source bindings, privacy allowlists and compiled mutation failures were verified. The audit did not replay the separately reported CTests or native suite.

Independent architecture, implementation and oracle review closed with no remaining source blocker. Findings fixed before the final passes included admission reuse across replacement guards, missing receipt correlation on early rejection, pre-admission body read-ahead, unchecked evidence metadata and missing successful operation/actual-wire observation boundaries. The oracle now rejects unexpected fields and compares every fixed error field literally. Windows proof manifests record actual commands/results and preserved baseline hashes.

## Remaining checks and delivery

Hosted validation was unavailable at local evidence capture. After the maintainer requested draft publication, initial Windows/Linux planning checks passed and native/optional-renderer jobs started. Check the PR for current results; running jobs are not passes. Docker became available and the maintainer requested resumption. Bounded local Linux CPU validation now passes using two CPUs and 3 GiB RAM, with no network or host mounts. The initial missing dependency scanner was repaired inside the disposable toolchain image; no host/global configuration changed. GPU testing is not applicable to this CPU-only policy host; physical Linux GPU remains not_run. The Linux container result establishes local headless CPU behavior only; hosted Windows/Linux checks remain outstanding.

The explicit draft request supersedes the previous publication hold for this slice. Existing workflow changes prepare policy oracle/mutation artifacts for eventual Windows/Linux CI; no required checks or permissions have been weakened. Inspect the latest PR head and required checks, resolve any failures, then prepare the bounded PR for maintainer certification/review and squash merge. Never mark this work done from local checks alone.

Root owns this worktree, delivery docs and backlog. The original user checkout was verified clean and unchanged. PR-006 actual merge/tree/six post-merge checks are reconciled in this branch, and its existing archives are unchanged. Preserve this reviewed candidate and proof archives through hosted validation and maintainer integration.

## Actual merge and follow-up, 2026-09-17

The maintainer merged PR #11 at 10:57:32 UTC as d7b52696edd3e7cb9c3388972509a05f16dc6466. Its tree fe56a4c09194af701614d3b7373da68a75c8537c exactly matches reviewed delivery 8105be42e0f122606d806bda5f0678960168d127. All six required PR checks passed. Five post-merge checks passed, but [Windows native validation](https://github.com/xsparc/omniweft/actions/runs/35213168811/job/105175308927) failed agents.sdk_compatibility. The generic child-probe failure was reproduced locally and narrowed to early authentication rejection racing a separately sent request body after credential renewal.

The preceding sections retain the original delivery/evidence checkpoint. Current follow-up state is recorded in the [compatibility handoff](PR-007-COMPATIBILITY-HANDOFF.md). This regression is not waived or hidden by a successful rerun; PR-008 remains proposed while the bounded repair is validated and reviewed.
