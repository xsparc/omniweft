# PR-009 handoff

State: implemented and independently reviewed; draft delivery/evidence pending. Branch codex/pr-009-hierarchy; base 27552713dc95a776a21c7f5425c4cf672bc169fa. Coordinator owns all writes. Scope: [plan](PR-009-PLAN.md), [authorization](AUTHORIZATION.md), [runnable example](../../examples/objects-hierarchy.md).

PR-008 was verified merged with matching reviewed tree and all six required post-merge checks passing. PR-009 implements native preserve-world/preserve-local hierarchy with generation-bearing parents, authored local and cached world TRS, atomic iterative descendant propagation, cycle/deletion/stale-generation rejection and unchanged root-only diagnostic bytes. Positive uniform parent scale bounds the TRS representation. Existing remote/policy profiles explicitly reject hierarchy operations/world observations rather than silently changing SDK contracts. No grants, quotas, permissions, dependencies, physical-body support or save-format migration is added.

## Verification and review

Windows pinned CPU Release build passed (MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2, Python 3.12.14). The full 23-CTest run passed 22 tests; its only failure was the new test fixture incorrectly expecting the native world revision in policy capability metadata. The untouched policy ledger correctly reports revision0. After correcting only that oracle expectation, all three affected hierarchy CTests passed. Thus every current test is verified; unchanged green tests were not repeated. The native suite passed 240 assertions. Existing SDK-control/agents, policy, room, renderer structural and atomic-object regressions passed in the full run. Planning validation and whitespace checks passed.

Independent architecture/source/oracle review closed with all findings resolved: native operation added to the machine schema, common remote serializer rejects unsupported hierarchy snapshots, and test policy callbacks now use a deadline-aware serialized dispatcher. The latter missing-dispatcher fixture initially failed before producing a descriptor; it was repaired and retested. Reviewers did not replay native tests or impersonate human GitHub approval. Final retained archive audit is still pending.

Local Linux verification is not_run; the existing hosted Windows/Linux jobs now retain the new hierarchy oracle evidence. No Docker restart or unchanged GPU rerun was performed. GPU is not a required PR-009 lane; CPU packet assertions do not establish physical-GPU or physics behavior. Runtime-tested uint64 exhaustion remains outside the fixture.

Next: commit the reviewed runtime candidate with approved attribution, retain/audit clean-candidate evidence, publish its authorized draft, inspect current-head Windows/Linux checks and fix concrete failures. Keep PR-010 proposed until actual maintainer merge and post-merge verification. No automatic merge or history rewrite is authorized.
