# PR-010 bounded native query evidence

Runtime candidate: `3b32f812232608af0203d579a9cb89ce9f71173c`; tree `9fe30bc84a4d24a4304f805f487edbde7aa6bbe3`. Draft: [PR #16](https://github.com/xsparc/omniweft/pull/16). Contract and current delivery state: [handoff](../../execution/PR-010-HANDOFF.md).

## Windows CPU proof

[windows-3b32f81.zip](windows-3b32f81.zip) is 17,850 bytes with 15 JSON members (manifest plus 14 artifacts). SHA-256: `f6cb79f4fbefba26ce8d6ddd204fcd480e863c2732e075bb18b4e70b0e8cb3b4`.

Independent archive audit passed exact membership, artifact hashes, clean source/tree/executable/toolchain binding and privacy. Static replay passed 1,641 literal query/tag/receipt/transport checks without rerunning native processes. No personal/account details, host paths/IDs, credentials or epoch payloads, raw streams or executable/PDB bytes are included.


The clean-candidate oracle passed 1,650 assertions and retains the separate native suite's 162-assertion result. Literal expected/actual pages cover AND tag filters, inclusive AABB contact, combined predicates, inherited/rotated/signed-scale bounds, full grant-region containment, replay across mutation, fresh replacement, response rejection, expiry and recovery. Native tests additionally verify hidden-row noninterference, exact/short byte caps, stale generations, immutable grants, invalid host bounds and nonrenewable deadlines.

Retained typed tag fixtures include complete before/after state, exact receipts, independently encoded OWOBJ003/OWOBJ001 bytes, late rejection and allocator recovery. Real legacy/agents/policy gateways reject native tag operations without consuming sequence or changing state, then accept supported same-sequence recovery; policy resources return to their expected levels. Tagged observe/runtime callbacks are rejected.

Windows used Release, MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2 and Python 3.12.14, with Vulkan disabled. All 44 source and 12 SDK file hashes match runtime Git blobs; seven executable hashes and actual build/toolchain provenance are retained. Executable bytes are excluded.

## Reproduce

Use the command in [observe.semantic_query](../../../examples/observe-semantic_query.md) after a pinned native build. Use a fresh evidence directory. CTest also runs the native suite, complete fixture/transport oracle and corruption/schema selftests. Existing hosted native jobs retain corresponding Windows/Linux evidence.

## Limits

All 26 Windows CTests were verified across the full run and affected-test rerun described in the handoff. Local Linux is not_run; hosted checks are separate and must pass on the latest PR head. This is CPU-only evidence, with no GPU, physics, save migration or remote query claim. Hosts bound session count; projected payload caps are not a global service/RSS bound. World must outlive its session. Actual maintainer review/certification and merge remain required for completion.
