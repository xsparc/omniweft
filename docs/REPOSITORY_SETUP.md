# GitHub repository setup

## Identity

- Name: **Omniweft**; slug: `omniweft`.
- Owner: [`xsparc`](https://github.com/xsparc), the authenticated account used for setup.
- Repository: [xsparc/omniweft](https://github.com/xsparc/omniweft).
- Visibility: public, matching the requested open-source project.
- Description: An open-source AI-centric world and game engine for Windows and Linux, with programmable control of objects, physics, meshes, voxels, and pixels.
- License: Apache-2.0 for first-party material; [rationale](OPEN_SOURCE.md).
- Topics: `game-engine`, `artificial-intelligence`, `world-building`, `voxel-engine`, `vulkan`, `physics-engine`, `cpp`, `windows`, `linux`, `open-source`.

## Included repository files

README and design/ADR documents; 38-item backlog and individual example specifications; LICENSE/NOTICE/third-party policy; CONTRIBUTING/DCO guidance; conduct/governance/maintainer/support/security documents; changelog; AGENTS instructions; CODEOWNERS; issue/PR templates; editor/git settings; pinned Actions planning CI; Dependabot for Actions.

## Hosted configuration target

Enable issues and discussions; use `main` as the default branch; allow squash merges and delete merged branches. Keep wiki disabled so maintained project documentation stays with code. Enable private vulnerability reporting. Keep normal PR workflow token permissions read-only and avoid enabling Actions approval privileges unnecessarily.

After the two planning checks have run, protect `main` with PR-required changes, strict current-base checks, conversation resolution, no force pushes and no deletion. Initial mandatory GitHub review count is zero for a single-maintainer project; independent technical review evidence remains required. CODEOWNERS has the real owner only. Require actual additional reviewer approvals after maintainers are appointed. Avoid allowing an agent to widen its own repository authority.

This file describes the target; [bootstrap verification](BOOTSTRAP_VERIFICATION.md) records which hosted settings were actually applied and checked. A checked-in workflow or security document alone is not evidence of hosted configuration.

## Implementation setup still required

- PR-001 introduces native toolchain pins, CMake presets and Windows/Linux CPU checks; [its handoff](execution/PR-001-HANDOFF.md) records observed results and remaining integration gates.
- GPU validation needs isolated real-device runners; no personal machine is enrolled by this bootstrap.
- Only the PR-001 headless lifecycle example is being implemented. The remaining example files are specifications.
- Appoint additional reviewers and designate a confidential project conduct contact as capacity permits.
- Release signing, package publication, telemetry and paid inference are not configured. An hourly local Codex continuation is active as recorded in [the implementation authorization](execution/AUTHORIZATION.md); no GitHub-hosted autonomous bot is installed.

The project has begun its adopted implementation scope; it is not a released engine. Use the [roadmap](ROADMAP.md) and [autonomous workflow](AUTONOMOUS_DEVELOPMENT.md) to start the first adopted slice.
