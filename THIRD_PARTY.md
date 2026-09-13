# Third-party inventory and provenance

The native headless bootstrap uses the platform C++ standard library/runtime. PR-002 vendors nlohmann/json for structural envelope parsing and serialization; no models, datasets or art are included. The exact build-tool inventory is `toolchains/bootstrap.json`; `toolchains/build-tools.txt` pins provisioned Python wheels by version and SHA-256. The default headless build downloads or links no SDL, Vulkan or Jolt dependency. PR-004 adds explicitly provisioned SDL3 and Vulkan-Headers only when the optional renderer is enabled. The Apache license text is included verbatim in [LICENSE](LICENSE). Actual source dependencies are pinned independently of future integrations.

| Actual dependency | Pin and role | License and provenance |
| --- | --- | --- |
| nlohmann/json 3.12.0 | `55f93686c01528224f448c19128836e7df245f72`; native JSON parse/serialize | [MIT notice](third_party/nlohmann/LICENSE.MIT), embedded notices retained in the unmodified header; [byte hashes and source inventory](third_party/dependencies.json) |
| SDL 3.4.16 (optional) | `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`; window/platform adapter | [Zlib notice](third_party/render/SDL-LICENSE.txt), unmodified source; [archive and source-manifest pins](toolchains/render-dependencies.json) |
| Vulkan-Headers 1.4.357.0 (optional) | `e3b1eec08173d6b825cd3ac88c885a63b621504a`; dynamic Vulkan API declarations | [License summary and retained notices](third_party/render/NOTICE.md), per-file Apache-2.0 or MIT; [exact source inventory](toolchains/render-dependencies.json) |

CMake checks the vendored header and license SHA-256 values before compilation. Git attributes preserve their upstream bytes on both Windows and Linux. The header is a source dependency compiled into native consumers; it introduces no network service or configure-time download. The build is offline after pinned tool provisioning. The remaining candidate dependencies below are not yet pinned, downloaded or audited for shipping.

| Candidate | Intended role | Upstream |
| --- | --- | --- |
| Jolt Physics | Rigid-body solver | [Jolt](https://github.com/jrouwe/JoltPhysics) |
| Dear ImGui | Initial editor UI, subject to spike | [Dear ImGui](https://github.com/ocornut/imgui) |
| ONNX Runtime, optional later | Model inference adapter | [ONNX Runtime](https://github.com/microsoft/onnxruntime) |

PR-001 supplies the pinned baseline tools. PR-004 separately pins shader-authoring/validation tools and retains generated shader provenance; these tools are not linked into or redistributed with the engine. The installed graphics runtime supplies the dynamically loaded Vulkan loader and driver; neither is bundled. Optional-source archive and extracted-byte checks run before compilation, with embedded upstream notices retained. Candidate dependencies above remain deferred. Each entry must include name, version/commit, source URL, content hash, SPDX license expression, notice paths, modifications, transitive components and whether shipped in source or binaries. Generate an SBOM from actual build inputs before packaging; this candidate table is not an SBOM.

The planning CI invokes official `actions/checkout` and `actions/setup-python` at immutable source commits named in its workflow. Dependabot proposes updates; review changed code and licenses before merging. These services/actions are build tooling, not vendored engine dependencies.

Asset/model manifests also record author/source, license URL, attribution, allowed distribution scope, generation inputs/model where applicable and hash. Preserve source notices. See the [license policy](docs/OPEN_SOURCE.md).
