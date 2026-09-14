# PR-006 execution handoff

State: **in_progress**; no PR or implementation commit yet. [Adopted scope](AUTHORIZATION.md) Ã‚Â· [frozen contract](PR-006-PLAN.md) Ã‚Â· [example](../../examples/agents-mock_builder.md).

Branch codex/pr-006-fixed-step-provider starts at verified main 3ab7bf114aa4acdef52387b1709be3fcaccb17b9. PR-005 is done after actual maintainer squash merge, exact reviewed-tree verification and six passing post-merge checks; [its handoff](PR-005-HANDOFF.md) records completion.

## Current work and ownership

The coordinator reconciled PR-005 completion and integrated the runtime SDK, typed temporary targets and bounded scripted provider. The isolated native author delivered the fixed-step owner/dispatch boundary, optional agents host and live renderer. Separate test authors delivered the literal fixture/canonical oracle, exact clock cases, provider delay/recovery checks, compiled cap mutation, SDK compatibility/boundary tests, publication-fault proof and actual-owner scheduling proof. Independent source reviews found no remaining blocker.

No new third-party dependency, shader/action pin, global tool setting, permission or save format is planned. All earlier user/helper worktrees remain preserved. Only the coordinator edits the backlog and integrates owned files. No helper commits or pushes.

## Verification state

The integrated development candidate builds with the pinned Windows compiler in headless and optional Vulkan modes. All 15 CTest checks passed, including the six SDK boundary regressions and actual provider lifetime/uncertain-result cases. The independent headless oracle passed; its strengthened commit-boundary version passed 1,696 assertions in the helper. Actual Windows GPU development verification passed 4,481 assertions across the independently driven scene and public example, with real three-object color/ID/depth readbacks and complete graphics/presentation retirement. These runs use an uncommitted development candidate and are not the retained clean-candidate evidence.

Independent review found and closed a cleanup issue: a snapshot-publication exception after a claimed command could leave its gateway caller waiting until the whole-process watchdog. The corrected owner retains and completes that failed job. A separately authored compiled fault regression reproduces the old hang and verifies the fixed host closes the request without a receipt and exits with failure 4 within two seconds. Source and regression reviews found no remaining issue in this path. The four earlier SDK findings are fixed and their independent regressions pass. The actual-owner controlled-clock proof passed 33 native assertions; separate zero-due and overload guard mutants were rejected by the same oracle. Its dispatch caller samples publication immediately after return to verify snapshot availability before acknowledgment. Test sources and evidence-retention guards were independently reviewed.

The original authoring fixture, hashes and clock edges remain unchanged. Final clean-candidate clock mutation/failure proofs, hosted Windows/Linux checks, evidence privacy review and PR delivery remain unfinished. GPU device and validation details will be recorded in reviewed evidence after the final runtime commit.

Next: create the clean runtime commit, run final CPU/GPU and disposable compiled proofs, inspect hosted checks, then publish independently reviewed evidence and prepare the PR for maintainer review. No additional user authorization is currently needed.
