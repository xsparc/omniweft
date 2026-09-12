# Third-party inventory and provenance

The native headless bootstrap uses the platform C++ standard library/runtime. PR-002 vendors nlohmann/json for structural envelope parsing and serialization; no models, datasets or art are included. The exact build-tool inventory is `toolchains/bootstrap.json`; `toolchains/build-tools.txt` pins provisioned Python wheels by version and SHA-256. No SDL, Vulkan or Jolt library is downloaded or linked by this slice. The Apache license text is included verbatim in [LICENSE](LICENSE). The following actual source dependency is pinned independently of future engine integrations.

| Actual dependency | Pin and role | License and provenance |
| --- | --- | --- |
| nlohmann/json 3.12.0 | `55f93686c01528224f448c19128836e7df245f72`; native JSON parse/serialize | [MIT notice](third_party/nlohmann/LICENSE.MIT), embedded notices retained in the unmodified header; [byte hashes and source inventory](third_party/dependencies.json) |

CMake checks the vendored header and license SHA-256 values before compilation. Git attributes preserve their upstream bytes on both Windows and Linux. The header is a source dependency compiled into native consumers; it introduces no network service or configure-time download. The build is offline after pinned tool provisioning. The remaining candidate dependencies below are not yet pinned, downloaded or audited for shipping.

| Candidate | Intended role | Upstream |
| --- | --- | --- |
| SDL3 | Window/input/platform | [SDL](https://github.com/libsdl-org/SDL) |
| Vulkan-Headers/Loader and selected SDK tools | Rendering/compute | [Khronos](https://github.com/KhronosGroup) |
| Jolt Physics | Rigid-body solver | [Jolt](https://github.com/jrouwe/JoltPhysics) |
| Dear ImGui | Initial editor UI, subject to spike | [Dear ImGui](https://github.com/ocornut/imgui) |
| ONNX Runtime, optional later | Model inference adapter | [ONNX Runtime](https://github.com/microsoft/onnxruntime) |

PR-001 supplies an actual pinned inventory for tools it introduces; candidate engine dependencies above remain deferred. Each entry must include name, version/commit, source URL, content hash, SPDX license expression, notice paths, modifications, transitive components and whether shipped in source or binaries. Generate an SBOM from actual build inputs before packaging; this candidate table is not an SBOM.

The planning CI invokes official `actions/checkout` and `actions/setup-python` at immutable source commits named in its workflow. Dependabot proposes updates; review changed code and licenses before merging. These services/actions are build tooling, not vendored engine dependencies.

Asset/model manifests also record author/source, license URL, attribution, allowed distribution scope, generation inputs/model where applicable and hash. Preserve source notices. See the [license policy](docs/OPEN_SOURCE.md).
