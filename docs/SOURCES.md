# Design sources

Reviewed for this design on 2026-09-13. These primary sources support specific choices and constraints; the proposed Omniweft architecture, quotas, milestones and performance targets are project design judgments, not claims made by these sources. Verify exact pinned dependency versions and terms during implementation.

| Source | Design use |
| --- | --- |
| [SDL supported platforms](https://wiki.libsdl.org/SDL3/README-platforms) | Windows/Linux platform-layer suitability |
| [SDL license](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt) | Candidate dependency license review |
| [Vulkan development environment](https://docs.vulkan.org/tutorial/latest/02_Development_environment.html) | Platform/tooling setup; exact engine baseline still needs a build spike |
| [Vulkan compute](https://docs.vulkan.org/tutorial/latest/11_Compute_Shader.html) | Storage-image/compute operations and graphics synchronization |
| [Vulkan-Headers licensing](https://github.com/KhronosGroup/Vulkan-Headers/blob/main/LICENSE.md) | Component-level licensing; SDK is not one uniform license |
| [Jolt architecture](https://jrouwe.github.io/JoltPhysics/) | Solver integration, deterministic configurations, ordering and state-restore limitations |
| [Jolt license](https://github.com/jrouwe/JoltPhysics/blob/master/LICENSE) | Candidate solver license review |
| [glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) | Mesh/material exchange, coordinates and units; separate native world semantics |
| [ONNX execution providers](https://onnxruntime.ai/docs/execution-providers/) | Optional provider-specific acceleration matrix |
| [ONNX DirectML status](https://onnxruntime.ai/docs/execution-providers/DirectML-ExecutionProvider.html) | Avoid assuming a universal Windows/Linux accelerator path |
| [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) | Selected first-party license and explicit patent/notice provisions |
| [MIT license](https://opensource.org/license/mit) | Permissive alternative comparison |
| [MPL FAQ](https://www.mozilla.org/en-US/MPL/2.0/FAQ/) | File-level copyleft alternative comparison |
| [Developer Certificate of Origin](https://developercertificate.org/) | Normal contribution certification |
| [REUSE specification](https://reuse.software/spec/) | Future per-file license/provenance metadata |
| [GitHub community files](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/creating-a-default-community-health-file) | Contribution/security/support repository foundations |
| [GitHub CODEOWNERS](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners) | Review ownership configuration |
| [GitHub secure Actions use](https://docs.github.com/en/actions/reference/security/secure-use) | Read-only PR jobs, immutable action pins and untrusted-input boundaries |
| [GitHub rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets) | Enforceable branch/merge controls |
| [GitHub private reporting setup](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository) | Hosted vulnerability reporting must be enabled separately |

No competitive feature claims, measured performance comparison or trademark clearance is inferred from this reference list.
