# Optional renderer dependencies

SDL 3.4.16 is unmodified upstream source at `fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`, distributed under its [Zlib notice](SDL-LICENSE.txt). The explicitly provisioned archive retains all embedded third-party notices.

Vulkan-Headers 1.4.357.0 is unmodified upstream source at `e3b1eec08173d6b825cd3ac88c885a63b621504a`. Preserve its [license summary](Vulkan-Headers-LICENSE.md), [Apache-2.0 text](Vulkan-Headers-Apache-2.0.txt), [MIT text](Vulkan-Headers-MIT.txt) and per-file SPDX notices. No Vulkan driver or loader is redistributed.

Exact upstream URLs, archive/source-manifest hashes and build roles are recorded in `toolchains/render-dependencies.json`. Configuration checks consumed source bytes against those pins. Provisioning is explicit; headless builds do not consume these dependencies.

The cube geometry, camera, GLSL and generated SPIR-V are original first-party Apache-2.0 fixture material. Shader generation uses separately provisioned SDK 1.4.357.0 tools; `shaders/provenance.json` records actual tool/input/output hashes. glslang and SPIRV-Tools are authoring/validation tools, not linked or redistributed runtime dependencies. Their upstream SDK revisions are pinned in the dependency inventory; upstream tool distributions retain their own notices.
