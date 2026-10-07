# Fourth feature-proposal handoff

State: documentation-only FP-031–040 prepared for review and draft delivery. See [catalog](../FEATURE_PROPOSALS_4.md) and [plan](FEATURE-PROPOSALS-4-PLAN.md). Coordinator branch `codex/feature-proposals-40`, based on main `7d8f6711a7a9a86a2150d5e9fac627f47505a66d`. Inspect the [fourth-batch PR](https://github.com/xsparc/omniweft/pulls?q=is%3Apr+head%3Acodex%2Ffeature-proposals-40) for actual latest delivery head/checks.

## Prior proposal integration

PR #27 squash-merged into PR #25's branch as `065213653a760af1c2b12c715a3bad8a2ea2dc33`. PR #25 then squash-merged into main as `7d8f6711a7a9a86a2150d5e9fac627f47505a66d`, tree `ec91956b6d90e7b785a44e71cd4ebe631ff3f67d`, matching reviewed third-batch delivery `d89763267b2f533aef7a860b5c05b24b710d4ba3`. All thirty earlier proposals are integrated documentation and remain unimplemented/unadopted. This supersedes prior handoffs' open-stack checkpoints.

All six actual-merge checks passed: [native](https://github.com/xsparc/omniweft/actions/runs/37463473729), [renderer](https://github.com/xsparc/omniweft/actions/runs/37463473598), [planning](https://github.com/xsparc/omniweft/actions/runs/37463473637). Both native manifests use the actual merge SHA/tree; each passed the existing GLB regression with 2340 oracle/4090 native assertions, all 74 source/16 SDK/3 artifact bindings verified. Debug compilers: Windows MSVC19.44.35229.0 and Linux Clang18.1.3. The prior branch compiler timeout and runner-acquisition failure were resolved by their recorded targeted retries; no further retries were needed for integration.

## Current proposal scope and validation

Exactly ten additional candidates; five documentation paths only. Earlier catalogs and every ledger item are unchanged. No runtime, SDK, CI, dependency, authority, protocol or save-format changes. PR-017 remains done and PR-018 remains proposed while this requested proposal draft is delivered. All forty FP items require express implementation adoption.

Planning validation passed. The existing planning/export suite ran 32 tests in 5.465 seconds: 31 passed and one symlink-creation test skipped. Process-local TEMP/TMP used a fresh ignored task directory; no test, gate or host/global setting changed. Independent source/contract/privacy review closed after specifying audio threshold equality, gain direction and exact Q15 output rounding with the PCM16 minimum-value boundary case. Target rest-root preservation, dialogue terminal precedence and particle PRNG direction were also clarified. This is technical review, not human approval or contribution certification.

OpenSteward static and dated strict checks on 2026-10-07 both returned `registry.unreadable` for the absent foreign registry. Neither is claimed passed; existing Omniweft governance remains authoritative. Primary research is advisory and cited in the catalog. No new executable, runtime archive, GPU/physics/audio-device claim, benchmark or prototype. Existing hosted engine checks assess regressions only.

The coordinator owns the public docs and private checkpoint. Every worktree and unrelated PR is preserved. Exact proposal count, changed-path scope, unchanged prior catalogs/ledger, whitespace and publication privacy checks passed. Next: publish the reviewed draft against main and verify actual current-head hosted checks plus both native manifests. Record final observations in its PR body/private checkpoint without repeatedly committing evidence-only status changes.

After delivery verification, await actual maintainer review/certification and manual merge. Then verify matching reviewed merge tree, all six post-merge checks and both native manifests before resuming bounded PR-018 under the original roadmap. No automatic ready transition/merge, fake human approval/DCO, history rewrite, purchase, trust change or release is authorized. Keep the continuation active; unchanged pending maintainer state stays quiet.
