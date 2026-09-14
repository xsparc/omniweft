# PR-007 handoff

State: **blocked on required hosted validation; locally implemented and reviewed, not merged**.
Adopted scope: [authorization](AUTHORIZATION.md).
Branch: codex/pr-007-capabilities-budgets. Base: 270f67b21237c180d8c37906bd03fc31f3be0010.
Runtime candidate: 3055ada3604f7df879454d71f44956437496dd56.
Runtime tree: 66a46165fe9b7e4797f4758dfa6a161ef54a684d.
PR URL: not opened; branch remains local. Maintainer certification/review and actual merge remain required.

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

[Public-safe evidence](../evidence/PR-007/README.md) contains the original clean-candidate manifests and hashed fixture artifacts. Its archive is bound to runtime 3055ada, not relabeled as a later documentation delivery. Independent read-only archive audit passed with no blocking finding. Full ZIP structure/bytes, all inventories/hashes, exact candidate/tree and source bindings, privacy allowlists and compiled mutation failures were verified. The audit did not replay the separately reported CTests or native suite.

Independent architecture, implementation and oracle review closed with no remaining source blocker. Findings fixed before the final passes included admission reuse across replacement guards, missing receipt correlation on early rejection, pre-admission body read-ahead, unchecked evidence metadata and missing successful operation/actual-wire observation boundaries. The oracle now rejects unexpected fields and compares every fixed error field literally. Windows proof manifests record actual commands/results and preserved baseline hashes.

## Remaining checks and delivery

Hosted Windows/Linux checks are not_run because hosted validation is temporarily unavailable. Local Docker's engine was unavailable during the latest bounded read-only probe; no engine, context or global configuration was changed. GPU testing is not applicable to this CPU-only policy host; physical Linux GPU remains not_run. Passing local Windows tests does not establish Linux behavior.

Do not push or trigger new hosted runs while this restriction remains. Existing workflow changes prepare policy oracle/mutation artifacts for eventual Windows/Linux CI; no required checks or permissions have been weakened. Once hosted validation becomes available, inspect the current remote base, reconcile changes as needed, run current required checks and prepare the bounded PR for maintainer certification/review and squash merge. Never mark this work done from local checks alone.

Root owns this worktree, delivery docs and backlog. The original user checkout was verified clean and unchanged. PR-006 actual merge/tree/six post-merge checks are reconciled in this branch, and its existing archives are unchanged. Keep this local candidate, proof archives and durable state until hosted validation and maintainer integration can resume.
