# Open-source and license policy

## Recommendation

Use **Apache-2.0** for first-party engine code, SDKs, tools, example code and Markdown documentation. It permits broad reuse, including commercial games, and includes an explicit contributor patent grant with defined conditions. Preserve its redistribution and notice obligations. This is a project recommendation; it does not establish rights to every third-party asset, model or patent. [Apache license text](https://www.apache.org/licenses/LICENSE-2.0)

| License | Tradeoff for this project |
| --- | --- |
| Apache-2.0 — selected | Permissive reuse with explicit contributor patent terms; suitable for a reusable engine ecosystem |
| MIT | Short permissive text and notice preservation; no express patent clause in its text. [MIT](https://opensource.org/license/mit) |
| MPL-2.0 | File-level copyleft: distributing modified covered files creates source obligations; separate files may use other licenses. Prefer this if requiring shared engine-file improvements is the priority. [Mozilla FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/) |

Apache-2.0 does not require commercial users to publish their games or distributed engine modifications. It also does not grant trademark rights. License changes require a public governance proposal and review of existing contribution rights; maintainers cannot assume they can relicense everyone else's work.

## Content boundaries

- First-party source, SDKs, tools, Markdown and original procedural example fixtures use Apache-2.0 unless explicitly marked otherwise.
- Third-party code retains its upstream terms. Record exact version/commit, origin, SPDX expression, notices, modifications and source/binary shipping scope in [THIRD_PARTY.md](../THIRD_PARTY.md) and the implementation inventory.
- Downloaded art, fonts, audio, textures, datasets, weights and adapters each require their own provenance record. Do not silently relicense them under the engine's license.
- Prefer locally generated geometric fixtures and tiny original textures for mandatory tests. An optional asset pack may use an appropriate separate license with a per-asset manifest.
- Optional model integrations document download origin, hash, format, terms, resource needs and whether redistribution is allowed. A model runtime license is not a model-weight license.
- Unknown-license, research-only or noncommercial-only material cannot enter the normal open-source release bundle. Keep any permitted optional integration separate and clearly documented.
- Generated content records tool/model/version where known, source inputs and human review. No automatic claim of copyright ownership or training-data clearance follows from generation.

The selected candidate dependencies have permissive foundations, but the actual pinned tree and subcomponents must be reviewed: [SDL license](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt), [Jolt license](https://github.com/jrouwe/JoltPhysics/blob/master/LICENSE), and [Vulkan-Headers license information](https://github.com/KhronosGroup/Vulkan-Headers/blob/main/LICENSE.md). Do not treat every component in a Vulkan SDK installation as identically licensed.

## Contributions and attribution

Use the Developer Certificate of Origin 1.1 rather than a bespoke CLA for normal contributions. Contributors certify the rights they actually hold with a sign-off; an AI agent must not invent a person's identity or make a legal certification on their behalf. [DCO](https://developercertificate.org/)

See [CONTRIBUTING.md](../CONTRIBUTING.md) for the submission process. The initial AI-authored planning import is explicitly recorded as a bootstrap in the changelog; it does not fabricate a human DCO sign-off. Normal contribution certification starts after that import.

Maintain copyright notices and relevant upstream attribution. `NOTICE` contains accurate project attribution and expands when dependency obligations require it. Code headers can use `SPDX-License-Identifier: Apache-2.0`; adopt REUSE metadata when assets/source arrive, and claim conformance only after checking it. [REUSE specification](https://reuse.software/spec/)

## Community and release foundations

The repository supplies contribution, conduct, governance, maintainer, support, security and changelog documents, issue/PR templates, CODEOWNERS, a planning CI workflow and a dependency-update configuration. Hosted security features and branch protection must be enabled separately; a Markdown file cannot enforce them.

Every release includes the license, required notices, dependency inventory/SBOM, asset manifests, checksums, reproducible build instructions and exact supported-platform results. Signing and publishing credentials require a trusted release path; the planning bootstrap installs no release automation or secrets.
