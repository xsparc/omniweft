# Contributing to Omniweft

Omniweft is implementing its first native headless bootstrap, with the remaining engine features specified in the roadmap. The existing backlog is [adopted for incremental execution](docs/execution/AUTHORIZATION.md). Contribute one dependency-ready behavior with its example, actual checks and independent review.

1. Read the [project brief](docs/PROJECT_BRIEF.md), relevant [architecture decisions](docs/adr/README.md), [Code of Conduct](CODE_OF_CONDUCT.md) and [agent instructions](AGENTS.md).
2. Choose an eligible [work item](docs/ROADMAP.md), or propose a change in an issue with a concrete problem and acceptance criteria. Coordinate ownership before overlapping changes.
3. Work on a branch. Keep one behavior per PR. New core behavior includes a runnable example, independent assertions, negative cases and relevant recovery checks in the same PR.
4. Run `python tools/validate_plan.py` for documentation/planning changes. Once runtime code exists, follow the example's platform-specific commands and required lanes.
5. Open a PR using the supplied template. Include exact tested commit, commands, actual outcomes, remaining limitations and source/asset provenance.
6. Address independent review and keep the PR description aligned with its final implementation. Do not claim tests or GPU support you did not verify.

## Contribution rights and AI assistance

Contributions use [Apache-2.0](LICENSE), subject to explicit third-party exceptions. Read the [Developer Certificate of Origin 1.1](https://developercertificate.org/) and add your own `Signed-off-by` line when you can make that certification:

```sh
git commit -s
```

Use an identity/email you are comfortable publishing. Sign-off is a contributor certification, not cryptographic commit signing. Do not sign for another person. Disclose material AI assistance and retain provenance for copied/imported/generated assets or code where relevant. Review generated work yourself; AI assistance does not remove contributor responsibility. No custom CLA or copyright assignment is required.

The initial AI-authored design import is a documented bootstrap exception to sign-off; agents must not use that exception for later contributions. Automated contributions need an adopted project bot identity and an authorized certification policy, or a submitting contributor who reviews and certifies the work.

## Style and compatibility

Use clear Markdown, relative repository links and explicit status labels. Keep proposed APIs separate from runnable instructions. Preserve stable work-item/example IDs. Public API or save-format changes require an ADR, migration/compatibility statement and appropriate fixture. Dependency changes pin exact versions and update notices.

Future C++ conventions are established in PR-001: explicit ownership, bounded allocation at trust boundaries, warnings enabled, formatting and meaningful sanitizer coverage. Avoid speculative frameworks or generalization without a corresponding tested use case.

Report vulnerabilities through [SECURITY.md](SECURITY.md). Use [SUPPORT.md](SUPPORT.md) for questions and [GOVERNANCE.md](GOVERNANCE.md) for disputes or project decisions.
