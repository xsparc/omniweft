# PR-007 handoff

State: in_progress; implemented local candidate, not merged. Adopted scope: [authorization](AUTHORIZATION.md).
Branch: codex/pr-007-capabilities-budgets. Base: 270f67b21237c180d8c37906bd03fc31f3be0010.
PR: not opened. Exact candidate identity is to be recorded after local commit; no push is planned while hosted runs are unavailable.

## Implemented behavior

The [slice plan](PR-007-PLAN.md) defines the fixed two-principal policy profile. Native working/request admission precedes body allocation and typed serialization/staging; the shared Coordinator checks whole-cube bounds and atomic retained-memory sponsorship. A separate CPU-only fixed-step host serves three bounded transport workers. PolicySession and PolicyClient expose two private credentials, immutable grants/usage and quota-safe resynchronization. Legacy host/profile shapes and canonical world format remain unchanged. No new dependency or external asset is introduced.

The [runnable example](../../examples/policy-denied_edits.md) exercises scoped edits, denied foreign/current-destination edits, staging amplification, repeated late rollback, slot generation/sponsor reuse and renewal. Its independent oracle checks literal complete world/receipt data, a valid four-operation batch, actual 512/513-byte transmitted observation bodies, exact declared-body memory boundaries, incomplete A body admission, prompt excess-A denial, B's real authored progress, timeout refund and A recovery.

## Actual local checks

Pinned Windows Release build passed using MSVC 19.44.35227, CMake 3.31.6 and Python 3.12.14 with two build jobs. All 18 CTests passed sequentially. The subsequently tightened policy oracle passed 1007 assertions. These are working-tree checks; clean-candidate retained evidence and compiled scope-bypass proof remain pending.

Development failures were fixed before those passes: the new two-principal private descriptor needed its separate 2048-byte limit while legacy remains 513; the example required handle() conversion; independent numeric fixtures needed literal floating-point values; the raw-byte oracle delimiter needed correction. Planning initially rejected missing exact example scope/evidence/rollback/dependency/lane declarations; these were restored and all 38 planning items validated.

Independent architecture/implementation review identified admission reuse through a replacement guard, missing rejected receipt correlation and body read-ahead before reservation. All were fixed. Evidence review identified unchecked extra fields/error text and missing successful operation/actual-wire observation boundaries. Exact allowlists and literal errors now reject arbitrary metadata; real boundary tests passed. Final source/evidence review and mutation proof are pending; no self-approval or merge is claimed.

## Delivery and limitations

Hosted validation is temporarily unavailable. Hosted Windows/Linux checks are not_run. Avoid pushes/new hosted runs for now; no permissions, workflow-disable or merge-gate changes are authorized. Existing CI is prepared to run policy evidence and the compiled mutation proof when available. Local work continues; a passing Windows run does not establish Linux behavior.

Linux Docker was previously unavailable; no Docker Desktop, context, driver or global host change has been made. Policy GPU verification is not applicable to this CPU-only fixture; an unchanged renderer's prior evidence is not relabeled as policy evidence. Physical Linux GPU remains not_run.

Root owns this isolated worktree and all changes. PR-006 actual merge, exact tree match and six post-merge passes are reconciled here, with immutable PR-006 archives preserved. Next action: commit the reviewed candidate with authorized noreply author/committer, run the compiled mutation proof and retain clean-candidate local evidence; close any review findings. Prepare a reviewable local delivery while required hosted validation and maintainer merge remain pending.
