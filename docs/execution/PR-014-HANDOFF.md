# PR-014 handoff

State: audited runtime delivered as draft [PR #20](https://github.com/xsparc/omniweft/pull/20); final evidence/documentation delivery checks tracked through the PR on `codex/pr-014-crash-recovery`, base `a3223cdacfe58a4a04c66c9ee21fb9626ce09de1`. See [plan](PR-014-PLAN.md) and [authorization](AUTHORIZATION.md).

PR #19 merged with the exact reviewed delivery tree; required post-merge workflows passed. Both post-merge native evidence manifests are verified. All prior worktrees are preserved. Runtime implementation is isolated; coordinator owns shared ledger and example/oracle. The repaired Windows Release build and all 39 CTests passed. Next: verify final delivery-head checks, then await maintainer review/certification and manual merge. After actual merge verify matching reviewed tree and all required post-merge jobs before advancing. Manual maintainer review/certification and merge remain required.

OpenSteward static and dated strict checks each reported `registry.unreadable` for its missing foreign-project registry. They are not claimed passed; existing Omniweft governance remains authoritative. The independent oracle selftest now rejects 33 altered reports; selftest results alone do not establish runtime persistence.

## Development validation and findings

Initial integrated Windows crash oracle passed 5105 assertions plus the 262-assertion native suite. Earlier sandboxed attempts failed before baseline execution in system temporary storage; a workspace diagnostic and authorized execution outside the sandbox isolated the environment restriction. These are not relabeled as passing runs.

Independent review then found that a corrupted frame length could be mistaken for a torn tail, risking rollback of an acknowledged watermark. The unpublished format was repaired to authenticate its fixed header before any length-based tail classification. Native and independent corruption fixtures require fixed-code rejection and unchanged bytes for length, high-water and header-digest corruption. Final evidence is bound to the repaired runtime; the initial green run does not establish this correction.

Review also corrected the hosted persistence evidence step to use the existing headless build directories. Earlier example/oracle findings corrected actual acknowledgement delivery, private storage separation, private epoch comparisons, interrupted compaction after receipt eviction and retry of the interrupted sequence. Those review-driven refinements are not hidden by the aggregate pass count.

The fixed-header integrity repair passed 272 native assertions. Independent source, I/O, native-test, oracle and CI review closed with no remaining blockers. The oracle selftest rejects 33 corruptions. Integrated Windows Release build and all 39 CTests passed (126.57 seconds), including the repaired crash oracle. Clean-candidate evidence is audited and hosted Windows/Linux runtime checks passed. Local Linux is not_run.

## Audited runtime delivery

Runtime `727877c8804e42565ef9b9f3efeb16296c4dc939`, tree `dd158295b35c1b85ed627b8a6915a8065dbaab15`, passed all six required hosted checks: [native Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36319934155), [renderer Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36319934156), and [planning Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36319934170).

Clean-candidate Windows evidence passed 5575 independent oracle assertions plus 272 native assertions. [Retained evidence](../evidence/PR-014/README.md) contains six JSON members, 27,911 bytes, SHA-256 `107e565cbe6b8386175eba387a648be9cfc08a91f83fb1f1f163e2053b393fad`. Independent archive audit passed 53,227 static checks covering full literal snapshots/bytes/receipts/watermarks, exact identities, 11 crash boundaries, eight corruption outcomes, 63 source and 13 SDK bindings, actual build/binary provenance and privacy. No runtime rerun was used for the archive audit.

Final evidence/documentation commits do not change runtime or tests. Inspect [fresh PR checks](https://github.com/xsparc/omniweft/pull/20/checks) for their exact delivery-head results; successful runtime-head checks do not substitute for final-head checks. PR-014 remains in progress until actual maintainer integration; downstream work remains proposed.

Both hosted native manifests use checkout `1f6f742f1e0c3c847447c7b08b093dcf2aadba85` with the identical runtime tree. Each passed 5575 oracle plus 272 native assertions, and all 63 source, 13 SDK and retained artifact bindings were verified. Windows used MSVC19.44.35229.0; Linux used Clang18.1.3.

Independent final delivery-document review closed without blockers.
