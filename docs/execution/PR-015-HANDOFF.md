# PR-015 handoff

State: in progress on `codex/pr-015-worker-lifecycle`, base `a18b9e2736c211c4397d8eb94c48c5c27acb5a83`. See [plan](PR-015-PLAN.md) and [authorization](AUTHORIZATION.md).

PR #20 is integrated with its exact reviewed tree and all six post-merge checks passed. Both native manifests are bound to the actual merge and all source/SDK/artifact bindings verified. All worktrees are preserved. Coordinator owns the ledger, example and independent oracle; the implementation helper owns isolated new SDK lifecycle/worker modules and unit tests. Independent architecture review accepted the proposal-only worker boundary and required the explicit pre-submission cancellation cut, exact-key uncertainty retention and separate observer client.

Next: retain clean-candidate evidence, obtain archive review and publish a draft with hosted Windows/Linux checks. Maintainer review/certification and manual merge remain required.

OpenSteward static and dated strict governance checks each reported `registry.unreadable` for the missing foreign-project registry. Neither is claimed passed; existing Omniweft governance remains authoritative.

Development checks: pinned Windows Release configure/build and planning validation passed. The runnable example and independent literal validation passed; the development real-process/transport oracle passed 1171 assertions plus 127 SDK assertions, and the corruption selftest rejected 38 reports. Preliminary review corrected a post-response policy-lease race in the oracle. Source review identified an unsent-key deadline edge case during client preparation; it is now reconciliation-only, with no first submission allowed by retry after expiry. Strict proposal comparison also rejects booleans masquerading as numeric values. Final evidence will bind the corrected source.

Independent architecture/source/cleanup/SDK-test/CI review closed the implementation findings. Its final evidence correction now requires the exact reviewed 127-assertion SDK summary. The earlier complete CTest attempt was interrupted after 17 passing tests and is not claimed passed. Final required runs and clean-candidate archive review remain pending.

The resumed complete Windows Release suite passed all 42 CTests (145.33 seconds), including the corrected exact-summary oracle, new SDK edge suite, prior policy/retry checks and legacy SDK regressions. Planning validation and diff whitespace checks passed. Local Linux is not_run; hosted Windows/Linux validation and clean-candidate evidence remain pending.
