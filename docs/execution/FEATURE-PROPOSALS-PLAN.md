# Ten-feature proposal slice plan

Authority: the maintainer's 2026-09-30 request, "Think of 10 new features and have it in a draft PR", following confirmation of PR #24's squash merge. Existing commit attribution, privacy and manual-merge rules in [AUTHORIZATION.md](AUTHORIZATION.md) apply.

One deliverable: a reviewable catalog of exactly ten distinct feature proposals, each with value, a bounded first slice, dependencies, compatibility limits, independent success oracle, negative case and recovery plan. This is a documentation-only planning slice. Proposal publication does not adopt implementation scope.

Baseline: `c9a50e6e6826d6448e09d1e7f6c1bebdf3af9fcc`; branch `codex/feature-proposals-10`. The coordinator owns the catalog, shared ledger reconciliation and delivery records. Independent review is read-only.

Expected paths: [proposal catalog](../FEATURE_PROPOSALS.md), README/roadmap navigation, this plan and [handoff](FEATURE-PROPOSALS-HANDOFF.md), plus PR-017 status in its existing handoff/example/evidence/ledger after actual merge verification. Preserve all worktrees and helper work.

Non-scope: no engine/runtime, dependencies, schemas, API, persistent formats, CI permissions or validation thresholds change. No new proposal enters the adopted 38-item ledger, receives an execution claim or becomes a release commitment. PR-018 remains the next existing roadmap candidate.

Validation: current primary-source research; independent distinctness/dependency/privacy review; `python tools/validate_plan.py`; the existing planning-gate unit suite; `git diff --check`; exact count and scope audit. Run the OpenSteward static and dated strict checks and report any foreign-registry incompatibility honestly. Let existing hosted workflows evaluate the draft normally; do not rerun unchanged local engine tests or start Docker. Post-merge PR-017 verification includes its exact reviewed tree, all six checks and both native manifest bindings.

Risks: confusing a proposal with authorization, duplicating an adopted behavior, assuming remote/asset/physics support that is not implemented, overstating research evidence, or leaking machine details. Mitigations are explicit proposed status, separate FP identifiers, per-feature scope/exclusions, primary citations and public diff review. Existing Omniweft governance remains authoritative; do not invent a parallel registry.
