# PR-014 handoff

State: implementation and local validation complete; clean-candidate evidence and draft delivery pending on `codex/pr-014-crash-recovery`, base `a3223cdacfe58a4a04c66c9ee21fb9626ce09de1`. See [plan](PR-014-PLAN.md) and [authorization](AUTHORIZATION.md).

PR #19 merged with the exact reviewed delivery tree; required post-merge workflows passed. Both post-merge native evidence manifests are verified. All prior worktrees are preserved. Runtime implementation is isolated; coordinator owns shared ledger and example/oracle. The repaired Windows Release build and all 39 CTests passed. Next: retain and independently audit clean-candidate evidence, publish the authorized draft, and verify current-head Windows/Linux hosted checks. Manual maintainer review/certification and merge remain required.

OpenSteward static and dated strict checks each reported `registry.unreadable` for its missing foreign-project registry. They are not claimed passed; existing Omniweft governance remains authoritative. The independent oracle selftest now rejects 33 altered reports; selftest results alone do not establish runtime persistence.

## Development validation and findings

Initial integrated Windows crash oracle passed 5105 assertions plus the 262-assertion native suite. Earlier sandboxed attempts failed before baseline execution in system temporary storage; a workspace diagnostic and authorized execution outside the sandbox isolated the environment restriction. These are not relabeled as passing runs.

Independent review then found that a corrupted frame length could be mistaken for a torn tail, risking rollback of an acknowledged watermark. The unpublished format was repaired to authenticate its fixed header before any length-based tail classification. Native and independent corruption fixtures require fixed-code rejection and unchanged bytes for length, high-water and header-digest corruption. Final evidence is bound to the repaired runtime; the initial green run does not establish this correction.

Review also corrected the hosted persistence evidence step to use the existing headless build directories. Earlier example/oracle findings corrected actual acknowledgement delivery, private storage separation, private epoch comparisons, interrupted compaction after receipt eviction and retry of the interrupted sequence. Those review-driven refinements are not hidden by the aggregate pass count.

The fixed-header integrity repair passed 272 native assertions. Independent source, I/O, native-test, oracle and CI review closed with no remaining blockers. The oracle selftest rejects 33 corruptions. Integrated Windows Release build and all 39 CTests passed (126.57 seconds), including the repaired crash oracle. Clean-candidate evidence and hosted Windows/Linux checks remain pending. Local Linux is not_run.
