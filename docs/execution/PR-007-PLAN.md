# PR-007 slice plan

State: blocked on required hosted validation after local implementation and review. Adopted scope: [authorization](AUTHORIZATION.md).
Owner: Codex coordinator. Branch: codex/pr-007-capabilities-budgets.
Base: 270f67b21237c180d8c37906bd03fc31f3be0010.
PR-005 and PR-006 are merged; the PR-006 handoff records exact tree and post-merge verification.

## Observable behavior

Two authenticated fixture principals can edit cubes only inside their host-issued write regions while independent operation, retained-resource, working-resource, observation and data-plane request limits remain enforced. The example is policy.denied_edits. All mutations retain the typed Coordinator and fixed-step owner boundary.

The fixture is workshop, seed 7, eight slots. West has inclusive write bounds X [-8,-1], east [1,8]; both have Y/Z [-4,4]. Scope means the complete rotated/scaled unit-cube AABB, including negative scales. Derived nonfinite bounds fail closed. Both principals explicitly have whole-world observation grants; scoped writes do not provide confidential observations. Creates are initially unplaced within staging and must be placed before surviving commit. Every explicit placement is checked; existing foreign cubes cannot be moved or deleted.

## Resource contract

The separate policy version 1 profile is authoritative. Legacy capability shapes and default hosts remain compatible. Limits are four operations per transaction (also constrained by the client/global ceilings), one data-plane request per principal and two globally. A request remains charged through body reception, owner waiting/claim/publication and response completion. At least three bounded transport workers permit prompt excess-A rejection and B progress while A holds an incomplete body. This demonstrates data-plane saturation isolation, not general principal fairness. Empty-body capabilities and policy status are exempt from principal data-plane/observation limits but globally bounded.

Memory uses deterministic charged-resource-bytes-v1 accounting, not physical heap/RSS guarantees. Every slot skeleton costs 128 bytes; a live cube adds 256. Sponsors survive deletion and transfer atomically on slot reuse. West retained limit is 512 bytes and east 2048. Check every staging prefix, including create/delete amplification and sponsor rollback.

Working charge is 65536 + 1024 * max_slots + 8 * declared_body_bytes, reserved before body allocation/wait. Body cap is 16384 bytes, west working limit 98304 and east 262144: the exact west declared-byte boundary is 3072. The fixed base charges bounded fixture staging, publication and response work without claiming to measure allocator overhead. Native typed entry must independently preflight bounded input and obtain an unforgeable admission reservation before validation/cloning; no unverified caller-supplied charge can bypass this check.

Observation limits are complete UTF-8 JSON body bytes: west 512, east 4096, applying to observe and runtime. They are size/concurrency limits, not cumulative egress rates. West runtime may exceed its limit even when the world is empty as counters grow; exempt status supplies revision/sequence resynchronization.

Owner-stage semantic rejections consume exactly one sequence; authentication, framing and pre-admission resource rejections consume none. Renewal changes credentials/sequences but never resets retained quota.

## Implementation and verification

Root owns policy module, checked Coordinator staging, opt-in transport/owner, Python SDK session/profile, runnable example, independent oracle, native and HTTP failure tests, CI integration and shared ledger. No parallel writers.

Windows and Linux CPU lanes are required. Reuse pinned tools; existing regression checks remain required. GPU rendering changes are not intended; report any physical GPU lane not run honestly. No new dependency, assets, format migration, capability-administration API, rate quota, filtered query, pagination, persistence or asynchronous job API is included. Existing canonical world bytes retain their format; sponsor accounting is volatile host metadata. Rollback is a PR revert and process restart, preserving existing source fixtures.

Tests will cover scoped success, foreign current/target regions, rotated footprint and overflow, unplaced create rejection, temporary targets, per-prefix amplification, repeated late rollback of world/generations/sponsors, exact body/operation/observation boundaries, retained quota across renewal, independent principal quotas and actual transport saturation. A real incomplete A body plus excess A and B traffic must prove rejection, progress, timeout refund and A recovery. Native global-limit-one tests supplement real transport evidence. A compiled scope-check mutant must fail the independent oracle.

Independent architecture and implementation/oracle reviews found no remaining source blocker after the documented fixes. Actual results and evidence-review disposition belong to the handoff; this plan does not establish runtime passes by itself.
