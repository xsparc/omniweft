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

- PR-001 pins and verifies native build dependencies, compiler versions and CMake presets on Windows/Linux.
- GPU validation needs isolated real-device runners; no personal machine is enrolled by this bootstrap.
- Mandatory examples need their runtime implementations; current example files are specifications.
- Appoint additional reviewers and designate a confidential project conduct contact as capacity permits.
- Release signing, package publication, telemetry, paid inference and background autonomous execution are not configured.

The project is ready for a scoped implementation kickoff; it is not a released engine. Use the [roadmap](ROADMAP.md) and [autonomous workflow](AUTONOMOUS_DEVELOPMENT.md) to start the first adopted slice.
