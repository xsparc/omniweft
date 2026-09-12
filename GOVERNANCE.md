# Governance

Omniweft begins as a maintainer-led open-source project. `@xsparc`, the repository owner, is the initial project steward. There is no foundation, elected board or separate security team at launch.

Routine contributions use small PRs, example evidence, independent technical review and required CI. Architectural changes use an ADR. Product/scope changes use a public proposal explaining the problem, alternatives, compatibility impact and verification plan. The steward resolves decisions after considering evidence and contributor input, and records the rationale.

License changes, public format/protocol breaks, CI permissions, release authority, safety/trust boundaries and verification-policy changes require steward review. Automatic merge authority is not assumed; adopt it explicitly under the [autonomous workflow](docs/AUTONOMOUS_DEVELOPMENT.md).

Maintainers may be added when they demonstrate sustained useful contribution, review judgment and willingness to own a subsystem. Publish the responsibility and accepted appointment in [MAINTAINERS.md](MAINTAINERS.md) before granting privileges. Access may be reduced for inactivity, departure, abuse or compromised credentials; document non-sensitive reasons and preserve project continuity.

Disagreements should cite concrete behavior, evidence and tradeoffs. Request reconsideration through an issue or discussion, linking the decision and new evidence. Conduct disputes follow [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). Security reports follow the private route in [SECURITY.md](SECURITY.md).

The initial bootstrap imports planning documents directly to an empty repository. Subsequent changes use PRs. With one maintainer, branch protection initially requires checks and conversation resolution but zero formal GitHub approvals; independent review is recorded in the PR. Add enforceable approval requirements when real additional reviewers exist.

No fixed support SLA, roadmap deadline, compatibility guarantee or release cadence is promised before the corresponding policy and capacity exist.
