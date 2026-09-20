# PR-009 handoff

State: merged and verified; see verified integration below. Branch codex/pr-009-hierarchy; base 27552713dc95a776a21c7f5425c4cf672bc169fa. Coordinator owns all writes. Scope: [plan](PR-009-PLAN.md), [authorization](AUTHORIZATION.md), [runnable example](../../examples/objects-hierarchy.md).

PR-008 was verified merged with matching reviewed tree and all six required post-merge checks passing. PR-009 implements native preserve-world/preserve-local hierarchy with generation-bearing parents, authored local and cached world TRS, atomic iterative descendant propagation, cycle/deletion/stale-generation rejection and unchanged root-only diagnostic bytes. Positive uniform parent scale bounds the TRS representation. Existing remote/policy profiles explicitly reject hierarchy operations/world observations rather than silently changing SDK contracts. No grants, quotas, permissions, dependencies, physical-body support or save-format migration is added.

## Verification and review

Windows pinned CPU Release build passed (MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2, Python 3.12.14). The full 23-CTest run passed 22 tests; its only failure was the new test fixture incorrectly expecting the native world revision in policy capability metadata. The untouched policy ledger correctly reports revision0. After correcting only that oracle expectation, all three affected hierarchy CTests passed. Thus every current test is verified; unchanged green tests were not repeated. The native suite passed 240 assertions. Existing SDK-control/agents, policy, room, renderer structural and atomic-object regressions passed in the full run. Planning validation and whitespace checks passed.

Independent architecture/source/oracle review closed with all findings resolved: native operation added to the machine schema, common remote serializer rejects unsupported hierarchy snapshots, and test policy callbacks now use a deadline-aware serialized dispatcher. The latter missing-dispatcher fixture initially failed before producing a descriptor; it was repaired and retested. Reviewers did not replay native tests or impersonate human GitHub approval. Final retained archive audit passed, including complete membership, 840 static oracle checks, source/build/binary bindings and privacy. See [retained evidence](../evidence/PR-009/README.md).

Local Linux verification is not_run; the existing hosted Windows/Linux jobs now retain the new hierarchy oracle evidence. No Docker restart or unchanged GPU rerun was performed. GPU is not a required PR-009 lane; CPU packet assertions do not establish physical-GPU or physics behavior. Runtime-tested uint64 exhaustion remains outside the fixture.

Next: inspect current-head Windows/Linux checks and fix concrete failures within this bounded slice; request maintainer integration only when required checks pass. Keep PR-010 proposed until actual maintainer merge and post-merge verification. No automatic merge or history rewrite is authorized.

## Hosted compiler correction

The first hosted Linux native job rejected a copied string loop variable in the new native test under Clang's -Werror,-Wrange-loop-construct. The test now binds the loop variable by const reference. Production runtime behavior is unchanged; this is an ordinary follow-up commit, not a history rewrite. Current-head hosted checks and retained evidence must bind the corrected candidate.

## Reviewed delivery

Corrected runtime: 9630e3bf8dd934d7b91b6f06d0e458272d5d6e1c; tree 64ac352e998819c76ae968d320089f73a5b97acd. Clean Windows retained proof passed 1849 oracle assertions plus native240, with 12 artifacts. The independently audited archive is published under docs/evidence/PR-009. Production source is unchanged from reviewed implementation1c3eefd; the later runtime candidate changes a test string loop to const reference and records the compiler correction. No history was rewritten.

Both corrected-head planning checks passed while the four native/renderer jobs were running. The documentation/evidence delivery requires its own current-head checks; consult actual PR state instead of treating this checkpoint as a green CI claim. The active continuation tracks this branch/PR. PR-010 remains proposed until actual merge and post-merge verification.

## Verified integration

PR #15 merged as 4996aca01f7bfa76cacc50eb75aa6ffe0f042b3a, tree baabb21c90cdf3b9948a13e9001813d454dcd1a8, matching reviewed delivery f2b9d616584255c365e9eb1d375732ac6776392f. All six delivery and all six required post-merge checks passed: [native](https://github.com/xsparc/omniweft/actions/runs/35442078880), [renderer](https://github.com/xsparc/omniweft/actions/runs/35442078864), [planning](https://github.com/xsparc/omniweft/actions/runs/35442078906). Evidence remains bound to corrected runtime9630e3b. PR-009 is done; PR-010 is now eligible.
