# PR-006 execution handoff

Merged by maintainer xsparc through [PR #10](https://github.com/xsparc/omniweft/pull/10) at 2026-09-14T10:45:10Z as 270f67b21237c180d8c37906bd03fc31f3be0010. Its tree 02be55d4f26810a8a24ad3915dd09ffb8d89f7dc exactly matches reviewed delivery d83be1c1fa5245a734d5dfd8b274ccb813ce4234, with sole parent 3ab7bf114aa4acdef52387b1709be3fcaccb17b9. Both OS jobs passed in all post-merge workflows: [planning](https://github.com/xsparc/omniweft/actions/runs/34834737476), [native](https://github.com/xsparc/omniweft/actions/runs/34834737539), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34834737458). PR-006 is **done**.

All six final-delivery checks also passed: [planning](https://github.com/xsparc/omniweft/actions/runs/34834213071), [native](https://github.com/xsparc/omniweft/actions/runs/34834213059), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34834212957). Eight retained Windows/Linux proofs passed with verified hashes; tested merge candidate f7f85c0be2ff17e8fbec9a7db84fc71f2b6acf14 has the reviewed delivery tree. Runtime/GPU evidence remains bound to dc604ebd83a08ba0d23dcac473d5f10c7e74a362.

The delivery record below is historical and preserves its original review state. The next dependency-ready roadmap item is PR-007, capabilities and resource budgets.

State: **in_review**. [PR #10](https://github.com/xsparc/omniweft/pull/10), branch codex/pr-006-fixed-step-provider, base 3ab7bf114aa4acdef52387b1709be3fcaccb17b9. [Adopted scope and attribution](AUTHORIZATION.md) · [contract](PR-006-PLAN.md) · [example](../../examples/agents-mock_builder.md) · [public evidence](../evidence/PR-006/README.md).

## Delivered behavior

The opt-in agents host advances a dedicated world owner at rational 60 Hz, caps catch-up at four executed ticks and reports discarded whole debt. A separate fixed scripted worker builds the preregistered three-cube scene through the public SDK while real observer calls demonstrate continued tick progress during provider delays. Authoring executes only on a non-overloaded actual tick; committed snapshots publish before acknowledgment. Vulkan presentation consumes those detached publications and reports the frames it actually completed.

The SDK adds immutable runtime observations, typed temporary targets and a bounded provider with private barriers, strict outputs and no replay after unknown outcomes. Legacy host arguments, capabilities, observations, authenticated commands and receipts remain compatible. Tick-expiry metadata remains deferred; request/session/process deadlines are enforced. Physics, persistence, external model execution and future-track features are excluded.

## Candidate and evidence binding

Clean runtime commit: dc604ebd83a08ba0d23dcac473d5f10c7e74a362; tree: 5e0cd4e71935d82cdc4ce0eb7087819a8b45270e. Both pinned Windows builds completed before retained checks, with clean source and unchanged executable bytes throughout.

[Two public archives](../evidence/PR-006/README.md) retain five provider/runtime manifests with exactly 91 domain artifacts and the existing renderer regression manifest with exactly 32 artifacts. The summaries include precise provenance, hashes and generic environment details. No private workstation streams or identifiers are published.

Test-only follow-up 989a88002b3049bd7f1194aeaf4b390436096134 resolves each disposable source root immediately after creation. Hosted Windows exposed a short-path-versus-resolved-path containment mismatch after all 16 CTests passed. Real Windows short-path reproduction verified the old rejection, valid fixed copies, unchanged outside bytes and continued traversal/absolute/sibling escape rejection. Both complete patched proofs passed. Runtime, SDK, fixtures and GPU inputs remain byte-identical to the retained runtime candidate.

## Actual verification

| Check | Result |
| --- | --- |
| Pinned Windows headless and optional Vulkan builds | Passed |
| Local sequential CTest suite | All 16 passed |
| Independent CPU provider oracle | 1,606 assertions; real separate-process delay/recovery and canonical state |
| Actual Windows agents GPU oracle | 4,065 assertions; four captures from two runs, initial revision 1 and final revision 3 |
| Pure-clock compiled cap mutation | Passed detection; original clock source/executable preserved |
| Post-commit publication-fault proof | Passed; injected marker, no receipt, prompt connection closure and natural failure exit 4 |
| Actual-owner controlled-clock proof | 33 native assertions; cancellation, zero-due and every-overload-tick deferral, later admission and publication-before-ack |
| Owner admission guard mutants | Both actual compiled defects rejected by the unchanged native oracle |
| Existing render.world_cube physical-GPU regression | 1,961 assertions, six rechecked captures and startup/resize/minimize/recovery evidence |
| Planning validator | Passed all 38 linked work items |
| Physical Linux GPU / local Docker | not_run; Docker engine unavailable |

GPU: NVIDIA GeForce RTX 5070, Vulkan 1.4.351, driver 2584739840. Validation was enabled; error/warning assertions were zero. Generic tool versions and full hashes are in the public summaries. Synthetic clock/owner tests do not establish physics determinism.

One initial local CTest run concurrent with disposable compilations failed the legacy SDK child-probe gate. The isolated probe then passed 310 assertions, the full sequential suite passed, and the initial hosted Windows suite passed all 16. No repeatable SDK defect was established and no check was weakened.

Initial hosted runs at the runtime head: [planning](https://github.com/xsparc/omniweft/actions/runs/34832555884), [native](https://github.com/xsparc/omniweft/actions/runs/34832555802), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34832555775). Linux native and both optional-renderer jobs passed. Windows native stopped at the temporary-path helper after its 16 CTests passed. All six jobs subsequently passed at corrected-helper head 989a88002b3049bd7f1194aeaf4b390436096134: [planning](https://github.com/xsparc/omniweft/actions/runs/34833363616), [native](https://github.com/xsparc/omniweft/actions/runs/34833363557), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/34833363690). Retained Windows/Linux provider, clock mutation, publication-fault and owner-dispatch manifests all passed with verified artifact hashes. Their actual merge candidate fbd009bb90d7ff1619cfb9ac3d8667b8e44bcf20 has tree 91cb8bc4c10d52898e27c8ba306c620abdbd56cc and the expected main/runtime-follow-up parents. Final documentation/evidence updates require their own current-head checks; inspect the PR's exact head and results before merge. Hosted jobs use GitHub's actual PR merge candidate and are not physical-GPU evidence.

## Review and ownership

Separate native, SDK, oracle and delivery reviewers inspected implementation and evidence. Resolved findings covered strict SDK outputs, uncertain results, claimed-job cleanup after publication failure, publication sampling at dispatch return and temporary-path aliases. The real publication-fault proof rejects the preserved buggy source and passes the correction. Independent evidence review checked both exact archives, four agents GPU captures and six existing renderer captures without claiming another GPU run.

The coordinator owns integration, CMake/CI, schemas, documentation, evidence and the shared backlog. Helpers used isolated worktrees; earlier user/helper worktrees remain preserved. Every new commit uses the authorized public name and noreply address for author and committer; no human approval or DCO sign-off is fabricated.

## Next action and rollback

Finish current-head checks/review, then mark PR #10 ready for maintainer review, contribution certification and squash merge. PR-006 remains in_review until actual merge and required evidence are verified. Do not start dependent work or push to closed earlier PRs. After merge, reconcile the actual squash tree and post-merge checks before selecting the next eligible item.

Rollback is a normal revert with fixtures retained. No persistent migration, permission change, release publication or automatic merge is included. Hourly continuation remains active; no further routine user authorization is needed.
