# Adopted autonomous implementation scope

Date: 2026-09-13. Source: the maintainer's Codex task, **Bootstrap autonomous PR workflow** (`01a09699-3e74-7103-80e4-63d80a8e978e`).

> Documents are already in place I want you to bootstrap and perpetually develop using a multi PR autonomous incremental workflow.

This instruction supersedes the earlier planning-only scope. It adopts incremental implementation of the existing PR-001 through PR-038 roadmap, including their examples, tests, fixes and delivery documentation. It authorizes local branches/worktrees, commits prepared for review, PR creation and recurring continuation. Dependency ordering and each item's acceptance criteria remain binding. It does not mark the whole backlog ready: the coordinator claims one eligible behavior at a time and records its execution state in [the backlog](../../planning/backlog.json).

The scope excludes future-track expansion, paid services or runner purchases, release publication/signing, secrets, repository permission changes, weakened checks, protocol breaks and persistent-format migrations requiring designated maintainer review. Packaging preparation in PR-038 is distinct from publishing a release.

## Integration and contribution gates

The repository's existing [merge authority](../AUTONOMOUS_DEVELOPMENT.md#merge-authority) and [contribution certification](../../CONTRIBUTING.md#contribution-rights-and-ai-assistance) remain in effect. The implementation request does not invent a bot identity, a human DCO certification or a GitHub approval. Work may reach a tested, independently reviewed PR while a maintainer decision remains pending.

Before unattended integration can begin, the maintainer must explicitly adopt routine merge authority and either an actual project bot identity with an authorized certification policy, or review and certify each submitted contribution. Proposed routine merge conditions are: adopted scope, completed dependencies, current Windows/Linux functional and planning checks, independent technical review with findings resolved, valid contribution certification, no unresolved conversations, and ordinary protected-branch integration without bypass. Sensitive changes continue to require designated maintainer review.

## Recurring execution

The Codex app automation **Develop Omniweft incrementally** (`develop-omniweft-incrementally`) was created active on 2026-09-13 with an hourly cadence, attached to the existing task. It resumes active PRs before claiming new work, persists a handoff, and reports meaningful completions, new failures or required maintainer action. Unchanged blocked state stays quiet. This is a local Codex scheduler, not a GitHub-hosted bot or a promise of uninterrupted execution while the host is unavailable.

Initial coordinator worktree: `D:\Projects\Local\agent_works\omniweft-pr-001`, branch `codex/pr-001-platform-bootstrap`. The implementation helper uses its own worktree and branch; the coordinator integrates reviewed changes and is the only writer of the shared backlog. Resume from the latest [PR-001 handoff](PR-001-HANDOFF.md), inspect live git/PR state, and reconcile actual merges before starting dependent work.

At roadmap completion, report the measured outcome and keep additional ideas proposed until scope is adopted. The user may pause or change the automation at any time.
