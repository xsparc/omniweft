# PR-002 execution handoff

- State: **in_progress**. Draft [GitHub PR #5](https://github.com/xsparc/omniweft/pull/5). No merge or completed functionality is claimed yet.
- Work item: PR-002 — Versioned command schema; example `protocol.reject_invalid`.
- Task: `01a09699-3e74-7103-80e4-63d80a8e978e`; coordinator branch `codex/pr-002-command-schema`.
- Base: PR-001 squash merge `7283b8ad02b96195bb9e828b9945dc4c6c975c9a`, verified through GitHub after the user's merge confirmation. PR-001 is recorded done in the shared ledger.
- Authority: [adopted implementation scope](AUTHORIZATION.md). Routine future merging/certification policy is unchanged.
- Contract: [slice plan](PR-002-PLAN.md) and [schema 0.1](../../schemas/protocol-0.1.schema.json). Structural success is `schema_valid`; the pure module has no world mutation/admission interface.
- Contract review: independent architecture reviewer found no actionable issues at `4a90580e384291b1097006607277abd1763b4f75`.
- Ownership: coordinator owns schema, ledger, docs and workflow. Native implementation uses `codex/pr-002-code`; independent tests use `codex/pr-002-tests`, both starting at the reviewed contract commit. Integration is serialized through Git trees and non-forced branch updates.
- Implementation source: native helper commit `a4cfb4b5a451f9937d5ef4cf791adc650e682ec9`, independent test helper commit `a343caa1de608cbbd098781c85fa8e932b9c5d3f`. An independent review found the JSON dependency treated raw NUL as end-of-input; native validation now explicitly rejects it, with independent raw-NUL suffix and subsequent recovery fixtures.
- Local execution: **not_run** for this slice. Local command calls did not return, including a read-only diagnostic. GitHub API preparation and hosted Windows/Linux checks are the available fallback; existing local worktrees remain preserved.

## Observed checks

The contract-only draft at `4a90580e384291b1097006607277abd1763b4f75` passed existing bootstrap native checks in [run 34723748677](https://github.com/xsparc/omniweft/actions/runs/34723748677). This does not establish protocol functionality.

[Planning run 34723748693](https://github.com/xsparc/omniweft/actions/runs/34723748693) passed document/ledger validation on both platforms but failed one of 13 regression tests: its false-completion fixture inherited PR-001's newly legitimate merge evidence. The fixture now explicitly removes that evidence from its temporary test copy. Both original rejection assertions and the production validator remain intact; independent review accepted this correction. Rerun results remain pending.

Protocol build, independent oracle, typed serialization, mutation proof and full implementation review are pending. Hosted artifacts will record the exact tested candidate, executable/source hashes, commands, assertions and lane limitations.

## Resume

1. Inspect the live PR head and latest Windows/Linux native and planning checks.
2. Integrate only the owned files from each helper branch, retaining dependency bytes and provenance.
3. Resolve compiler/test failures, run the real-validator mutation check, and obtain independent review of implementation and evidence.
4. Record actual current-candidate results here and in the PR before requesting maintainer integration.
5. Mark done only after GitHub verifies an actual merge; then claim dependency-ready PR-003.
