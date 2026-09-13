# Adopted autonomous implementation scope

Date: 2026-09-13. Source: the maintainer's Codex task, **Bootstrap autonomous PR workflow**.

> Documents are already in place I want you to bootstrap and perpetually develop using a multi PR autonomous incremental workflow.

This instruction supersedes the earlier planning-only scope. It adopts incremental implementation of the existing PR-001 through PR-038 roadmap, including their examples, tests, fixes and delivery documentation. It authorizes local branches/worktrees, commits prepared for review, PR creation and recurring continuation. Dependency ordering and each item's acceptance criteria remain binding. It does not mark the whole backlog ready: the coordinator claims one eligible behavior at a time and records its execution state in [the backlog](../../planning/backlog.json).

The scope excludes future-track expansion, paid services or runner purchases, release publication/signing, secrets, repository permission changes, weakened checks, protocol breaks and persistent-format migrations requiring designated maintainer review. Packaging preparation in PR-038 is distinct from publishing a release.

## Integration and contribution gates

The repository's existing [merge authority](../AUTONOMOUS_DEVELOPMENT.md#merge-authority) and [contribution certification](../../CONTRIBUTING.md#contribution-rights-and-ai-assistance) remain in effect. The implementation request does not invent a bot identity, a human DCO certification or a GitHub approval. Work may reach a tested, independently reviewed PR while a maintainer decision remains pending.

Before unattended integration can begin, the maintainer must explicitly adopt routine merge authority and either an actual project bot identity with an authorized certification policy, or review and certify each submitted contribution. Proposed routine merge conditions are: adopted scope, completed dependencies, current Windows/Linux functional and planning checks, independent technical review with findings resolved, valid contribution certification, no unresolved conversations, and ordinary protected-branch integration without bypass. Sensitive changes continue to require designated maintainer review.

## Recurring execution

The Codex app automation **Develop Omniweft incrementally** (`develop-omniweft-incrementally`) was created active on 2026-09-13 with an hourly cadence, attached to the existing task. It resumes active PRs before claiming new work, persists a handoff, and reports meaningful completions, new failures or required maintainer action. Unchanged blocked state stays quiet. This is a local Codex scheduler, not a GitHub-hosted bot or a promise of uninterrupted execution while the host is unavailable.

Initial coordinator branch `codex/pr-001-platform-bootstrap`. Machine-local worktree locations remain in the private task. The implementation helper uses its own worktree and branch; the coordinator integrates reviewed changes and is the only writer of the shared backlog. Resume from the active backlog item's handoff (initially [PR-001](PR-001-HANDOFF.md)), inspect live git/PR state, and reconcile actual merges before starting dependent work.

At roadmap completion, report the measured outcome and keep additional ideas proposed until scope is adopted. The user may pause or change the automation at any time.

## Rendering verification and privacy clarification

For PR-004, the maintainer designated the current Windows GPU machine, requested conservative Linux Docker use, and said to prioritize Windows verification if necessary. Windows physical-GPU evidence is the immediate required target; attempt bounded Linux container checks and report whether they use hardware or software, with any unavailable Linux GPU lane explicitly `not_run`. Do not claim Linux desktop support from a container or silently replace a hardware test with software rendering. This changes verification priority, not authority to spend, modify trust or publish releases.

The maintainer also explicitly required protection of personal details on the Windows machine. Public commits, PRs, logs and artifacts must omit local account names, home/worktree paths, hostnames, device UUID/LUID/serials, environment dumps and unrelated local software/processes. Retain necessary generic GPU model, driver/API/tool versions, source hashes, relative artifact paths and independently verified results. Redact before publishing; keep machine-specific orchestration details only in the private task.

## Commit attribution authorization

On 2026-09-13, the maintainer explicitly replied:

> I authorize this for Omniweft commits.

This answers the preceding request to use public GitHub identity `xsparc <10508303+xsparc@users.noreply.github.com>` for agent-generated Omniweft commits, including future increments. Use that exact name and noreply address for author and committer through per-command Git configuration/environment. Verify both fields before pushing. Do not use a connector that silently substitutes a personal email, or change global Git configuration.

The earlier automatic approval-review attribution blocker is resolved by this explicit authorization. This is commit-attribution permission, not a DCO sign-off, human code-review approval, new merge authority or permission to rewrite shared history. Existing privacy, contribution, merge and release policies remain binding.
