# Second feature-proposal slice plan

Authority: the maintainer requested ten more feature ideas in a draft PR on 2026-10-06. A clarification confirmed the reported squash merge referred to PR #24. PR #25 remains open; this proposal PR is stacked on its reviewed head `b344154d55264b4b11a466481d0c956b6b6a43c4`, branch `codex/feature-proposals-10`. New branch: `codex/feature-proposals-20`.

Deliverable: [FP-011–020](../FEATURE_PROPOSALS_2.md), exactly ten additional proposals, each distinct from FP-001–010 and the adopted 38-item roadmap, with bounded first slice, prerequisites, success oracle, negative case, recovery and explicit unsupported scope. Proposal authorship/publication is authorized; implementation is not adopted by this request or by merging proposal documentation.

Expected paths: the new catalog, this plan, [handoff](FEATURE-PROPOSALS-2-HANDOFF.md), and README/roadmap navigation. Keep the first catalog and `planning/backlog.json` unchanged. Coordinator owns all document writes; independent review is read-only. Preserve all worktrees.

Validation: inspect live base/head state, browse primary sources for the focused research, independently review distinctness/contracts/privacy, run `python tools/validate_plan.py` and the existing planning/export unit suite using process-local ignored workspace temporary storage. Audit exactly ten new IDs, unchanged ledger/first catalog, local links and `git diff --check`. Run OpenSteward static and dated strict checks, reporting foreign-registry incompatibility honestly. Existing Omniweft governance remains authoritative.

No runtime, SDK, workflows, dependencies, protocols or saves change. No unchanged local engine/GPU tests or Docker runs are needed. Inspect actual hosted checks on the published stacked head. After PR #25 actually merges, verify its reviewed tree, post-merge checks and manifests before retargeting this PR to main and validating the resulting candidate. Do not assume a retargeted PR retains earlier check evidence.

Risks: accidentally presenting old ideas as new implementation, interpreting proposal merge as adoption, hiding a dependency on the open base PR, assuming unsupported skin/UV/World attachment or weakening uncertain-commit behavior. Mitigate with explicit status, baseline/stack records, bounded typed contracts and independent review. Existing attribution, privacy and manual-merge policy in [AUTHORIZATION.md](AUTHORIZATION.md) remain binding.
