# PR-013 handoff

State: implementation and local validation complete; clean-candidate evidence and draft delivery pending on `codex/pr-013-command-replay`, base `d8f49dc97d97ef62600b58b299c24527b94bbb4c`. [Plan](PR-013-PLAN.md), [authorization](AUTHORIZATION.md), [example](../../examples/world-replay.md).

PR-012 merged with matching reviewed tree and all six post-merge checks passed. MSVC19.44.35229 was actually exercised by successful native and renderer jobs, resolving the prior limitation. Existing worktrees are preserved. Independent preliminary architecture review accepted an opt-in bounded recorder, trusted prepublication observer and private replay with explicit identity mappings. Independent source/oracle/CI reviews closed after the refinements recorded below.

Next: retain and independently audit clean-candidate evidence, publish the authorized draft and verify all six required hosted checks. No automatic ready transition, merge or certification. Keep PR-014 proposed until integration.

Initial targeted validation passed 86 native assertions. Asset identity review then required a bounded exact content descriptor/digest; compiled CPU mesh binding and wrong-content tests brought the suite to 249 assertions. Further review added transient same-batch create/delete/recreate identities, combined observer/guard rejection, hierarchy replay and literal direct-example verification. The independent full-report oracle passed 1090 assertions and rejects 18 altered reports. Final local validation is recorded below; hosted validation remains pending.

OpenSteward's static and dated strict checks (2026-09-27) each reported `registry.unreadable` for the missing foreign-project registry. They are not claimed passed; Omniweft's adopted backlog, handoff and validation gates remain authoritative. No parallel governance authority or gate bypass is introduced.

Final Windows Release build and all 36 CTests passed (MSVC19.44.35227, Python3.12.14, CMake3.31.6, Ninja1.13.2, Vulkan disabled). Native replay passed 260 assertions; the retained oracle minimum now enforces that final count. Source/oracle/CI review closed with the asset-identity and transient-binding findings resolved. Planning and diff checks passed. Local Linux is not_run; hosted Windows/Linux and clean-candidate archive audit remain pending. No runtime failure was observed in these development runs; validation was strengthened through review rather than relabeling failed checks.

During builtin descriptor authoring, source inspection found a cyclically equivalent triangle index sequence rather than the exact compiled sequence. The descriptor was corrected to `[0,1,2,2,3,0]` before the mesh-binding test was executed; this was a fixture correction, not a failing runtime result.
