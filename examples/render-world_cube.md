# render.world_cube

Status: **implemented, verified and merged in PR-004**. Work item: **PR-004 — Vulkan presentation of world objects**.

Dependencies: PR-003. Validation lanes: cpu, gpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

The optional SDL3/Vulkan renderer presents a procedural cube from a detached, committed world snapshot and then presents a subsequent typed transform. The fixture uses seed 7, a fixed orthographic camera and original first-party geometry/shaders. It covers one window, one device and one frame in flight. The default build supplies CPU packet/structural checks without SDL, Vulkan or an SDK.

## Build and run

Use the pinned compiler, CMake and Ninja setup in [platform.bootstrap](platform-bootstrap.md). Optional sources are provisioned explicitly and verified against [dependency pins](../toolchains/render-dependencies.json); configuration does not download them:

```sh
python tools/provision_render.py
cmake --preset windows-vulkan
cmake --build --preset windows-vulkan --parallel 2
ctest --preset windows-vulkan
```

On Linux, use the `linux-vulkan` presets and install the SDL window-system development headers listed in [the optional build workflow](../.github/workflows/render-quality.yml). Hosted builds run CPU checks; they do not certify physical GPU presentation. Checked-in SPIR-V includes original GLSL and tool/input/output provenance, so an ordinary build needs no shader compiler.

The Windows native fixture is:

```sh
build/windows-vulkan/omniweft_examples.exe --example render.world_cube --gpu --seed 7 --verify --output artifacts/render-native
```

Use `--headless` in place of `--gpu` for structural verification, including with the default `windows-headless` build. Add `--interactive` only with `--gpu` for explicit window inspection; interactive inspection does not replace automated evidence. Linux executables omit `.exe`.

Physical verification needs Vulkan 1.3 or newer, required formats, swapchain maintenance1 support and the Khronos validation layer with synchronization validation. Discover a workspace-local SDK's layer through a process-local `VK_LAYER_PATH`. Do not change drivers or registry settings. Every output directory must be fresh.

## Pass criteria

Present a cube from a committed snapshot and show a transformed subsequent revision; record actual device and validation output.

Run the independent oracle and deliberate native mutation proof:

```sh
python tests/render_oracle.py --executable build/windows-vulkan/omniweft_examples.exe --gpu --evidence-dir artifacts/render-public
python tests/render_mutation.py --build-dir build/windows-vulkan --evidence-dir artifacts/render-mutation-public
```

The independent ray-box oracle computes expected color, object ID and depth from fixed fixture values, without using renderer geometry or captured pixels as expectations. The [frozen plan](../docs/execution/PR-004-PLAN.md) specifies the one-pixel boundary mask, exact IDs, color error at most one channel code and depth error at most `1e-5`. It also verifies copy/presentation provenance, identity/revision receipts and resource lifetime events. A screenshot alone cannot pass.

The mutation proof builds a disposable tracked-source copy with identity TRS substituted for the committed transform. The unchanged real-GPU oracle must reject it at `initial: GPU ID mask`; source and executable preservation are checked. A passing mutation proof means the oracle caught a defect.

## Negative and recovery cases

Resize/minimize and unsupported device startup remain controlled; no use-after-free on resource retirement.

Verification observes actual resize, minimize and restore, checks submission and presentation-fence completion before retirement, and requires zero validation warnings/errors. Unsupported-driver startup uses a child-only loader filter followed by normal startup recovery. CPU tests cover unsupported capabilities, invalid geometry and allocation bounds. No global driver settings are changed.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

The [handoff](../docs/execution/PR-004-HANDOFF.md) records actual checks and exact evidence hashes. The [public Windows GPU bundle](../docs/evidence/PR-004/README.md) verifies clean runtime commit `973162f40536235c7dea24aff00bc7281cf93923`. Later evidence/docs and hosted-setup commits are recorded separately; they do not relabel that hardware run. The maintainer prioritized Windows physical-GPU checks and conservative Linux Docker attempts. The attempted Docker runtime exposed CPU llvmpipe; Linux hardware presentation remains `not_run`, and Linux desktop support is not claimed.

Record exact candidate SHA, dirty status, commands, tool/device versions, seed, assertions, result and artifact hashes using the [evidence template](../docs/templates/EVIDENCE.md). Publish only reviewed allowlisted reports and engine readbacks. Keep raw workstation build output, executable/PDB artifacts, personal/account/host names, absolute paths and device UUID/LUID/serials private.

Rollback is a revert with fixtures preserved. This slice changes no command/save schema and introduces no persistent state format. Final completion requires the adopted checks, independent review and actual maintainer merge.
