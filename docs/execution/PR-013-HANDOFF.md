# PR-013 handoff

State: audited runtime delivered as draft [PR #19](https://github.com/xsparc/omniweft/pull/19); final evidence/documentation delivery checks tracked through the PR on `codex/pr-013-command-replay`, base `d8f49dc97d97ef62600b58b299c24527b94bbb4c`. [Plan](PR-013-PLAN.md), [authorization](AUTHORIZATION.md), [example](../../examples/world-replay.md).

PR-012 merged with matching reviewed tree and all six post-merge checks passed. MSVC19.44.35229 was actually exercised by successful native and renderer jobs, resolving the prior limitation. Existing worktrees are preserved. Independent preliminary architecture review accepted an opt-in bounded recorder, trusted prepublication observer and private replay with explicit identity mappings. Independent source/oracle/CI reviews closed after the refinements recorded below.

Next: verify final delivery-head checks, then await maintainer review/certification and manual merge. After actual merge, verify matching reviewed tree and all required post-merge jobs before advancing. No automatic ready transition, merge or certification. Keep PR-014 proposed until integration.

Initial targeted validation passed 86 native assertions. Asset identity review then required a bounded exact content descriptor/digest; compiled CPU mesh binding and wrong-content tests brought the suite to 249 assertions. Further review added transient same-batch create/delete/recreate identities, combined observer/guard rejection, hierarchy replay and literal direct-example verification. The independent full-report oracle passed 1090 assertions and rejects 18 altered reports. Final local and hosted runtime validation are recorded below.

OpenSteward's static and dated strict checks (2026-09-27) each reported `registry.unreadable` for the missing foreign-project registry. They are not claimed passed; Omniweft's adopted backlog, handoff and validation gates remain authoritative. No parallel governance authority or gate bypass is introduced.

Final Windows Release build and all 36 CTests passed (MSVC19.44.35227, Python3.12.14, CMake3.31.6, Ninja1.13.2, Vulkan disabled). Native replay passed 260 assertions; the retained oracle minimum now enforces that final count. Source/oracle/CI review closed with the asset-identity and transient-binding findings resolved. Planning and diff checks passed. Local Linux is not_run; hosted Windows/Linux passed. Independent clean-candidate archive audit passed 11,016 static checks. No runtime failure was observed in these development runs; validation was strengthened through review rather than relabeling failed checks.

During builtin descriptor authoring, source inspection found a cyclically equivalent triangle index sequence rather than the exact compiled sequence. The descriptor was corrected to `[0,1,2,2,3,0]` before the mesh-binding test was executed; this was a fixture correction, not a failing runtime result.

## Audited runtime delivery

Runtime `973d72ab168856273d563cda4051c7c641f59807`, tree `f08bbb5ce2d3784a2a2d449adafdc155962a8d0d`, passed all six required hosted checks: [native Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36252869415), [renderer Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36252869397), and [planning Windows/Linux](https://github.com/xsparc/omniweft/actions/runs/36252869369).

[Retained evidence](../evidence/PR-013/README.md) contains the audited four-member JSON archive: 15,422 bytes, SHA-256 `acd279f6c5d680747bdf7e0ed3e197638e7c17953ea73e52448add9f9f0018c3`. Independent audit passed 11,016 static checks, including full literal states, accepted operations, identities, canonical bytes, exact asset content, rejection/recovery, 56 source and 13 SDK bindings, actual binary/build provenance and privacy. No runtime rerun was used for the audit.

Final evidence/documentation commits do not change runtime or tests. Inspect [fresh PR checks](https://github.com/xsparc/omniweft/pull/19/checks) for their exact delivery-head status; successful runtime-head checks above do not substitute for final-head checks. PR-013 remains in progress until actual maintainer integration; PR-014 remains proposed.

Both hosted native manifests use checkout `97b14a42fa5000b85db314a3da50951ed94a1125` with the identical runtime tree. Each passed 1090 oracle and 260 native assertions; all 56 source, 13 SDK and retained artifact bindings were verified. Windows used MSVC19.44.35229.0; Linux used Clang18.1.3. Independent final delivery-document review closed without blockers.
