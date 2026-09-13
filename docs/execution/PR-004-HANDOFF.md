# PR-004 execution handoff

State: **in_progress**. Branch `codex/pr-004-vulkan-presentation`, base `e994f054d1e13ba19f714f35696ec8a1bca221a9`. [Plan](PR-004-PLAN.md), [authorization](AUTHORIZATION.md), [example](../../examples/render-world_cube.md).

PR-003 completion was independently verified from the actual PR #6 squash merge, identical reviewed tree, final checks and post-merge checks. The ledger now records its real merge; this is the sole newly claimed item.

Native, architecture and independent-oracle agents agreed the bounded packet/renderer fixture, exact readback thresholds, presentation-fence lifetime requirement and privacy policy before implementation. This checkpoint claims no PR-004 native build, render or GPU test result.

Local read-only sandbox calls stalled, but authorized execution outside that sandbox works. Existing user worktrees are preserved. The Windows host exposes a Vulkan 1.4-capable NVIDIA discrete GPU with swapchain maintenance1; the validation layer is not currently available. Docker Desktop uses a Linux WSL2 backend. Detailed local paths, host identifiers, installed software and raw inventory remain private. No driver, registry, permission or unrelated-container changes were made.

The maintainer requested Windows verification on this machine, conservative Linux Docker use, Windows priority if necessary, and no personal details in public project material. Provision workspace-local pinned dependencies/tools; complete Windows hardware verification first, then bounded Linux checks. Report any remaining Linux hardware lane explicitly as `not_run`.

Next: implement owned native and independent-test files, perform builds and actual validation, sanitize public evidence, obtain independent review and update this handoff with exact commits/results. Final delivery remains subject to actual maintainer merge. Rollback is a revert with retained fixtures; no persistent world state format is introduced.
