# platform.bootstrap

Status: **implementation under review; see the [execution handoff](../docs/execution/PR-001-HANDOFF.md) for actual checks and merge state**. Work item: **PR-001 — Buildable Windows/Linux project shell**.

Dependencies: documentation bootstrap. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Use Python **3.12.10** (the CI baseline; **3.12.14** is an explicitly pinned local alternate), CMake **3.31.6** and Ninja **1.13.2**. Install the exact hash-locked build tools with:

```sh
python -m pip install --require-hashes --only-binary=:all: --no-deps -r toolchains/build-tools.txt
```

On Windows, use an x64 Visual Studio developer shell selecting MSVC toolset **14.44.35207** (CI compiler **19.44.35228**, local alternate **19.44.35227**). On Ubuntu 24.04, install/select `clang++-18` (**18.1.3**). `toolchains/bootstrap.json` specifies the exact accepted compiler versions. Drift is an actionable failure, not silently accepted. Provisioning tools can use the network; configuring, building, running and testing the headless shell requires no engine downloads, model or GPU.

From the repository root:

```sh
python tools/bootstrap.py --check
python tools/bootstrap.py
```

The second command configures, builds and runs CTest. The corresponding explicit presets are `windows-headless` and `linux-headless`:

```sh
cmake --preset linux-headless
cmake --build --preset linux-headless
ctest --preset linux-headless
```

On Windows substitute `windows-headless`. The executable is `build/windows-headless/omniweft_examples.exe` or `build/linux-headless/omniweft_examples`. Run it by its full path, or add that directory to PATH:

```sh
omniweft_examples --example platform.bootstrap --headless --seed 7 --verify --output artifacts/platform.bootstrap
```

Use a fresh output directory for every invocation. The runner claims that directory exclusively; any existing directory is preserved and causes a clear refusal, even if empty. Choose a new directory to retry. No delete or overwrite step is needed.

For retained evidence after a build, run `python tests/bootstrap_oracle.py --executable build/linux-headless/omniweft_examples --evidence-dir artifacts/bootstrap-evidence` (substitute the Windows executable path as appropriate). The evidence directory must also be new.

The seed-7 fixture reports lifecycle `created`, `running`, `stopped`, exactly one executed step and checksum `1282168116`. The independent Python subprocess oracle checks literal expected values, exit status and output artifacts. A successful `--verify` is a lifecycle assertion for this fixture, not world-engine or graphics certification.

## Pass criteria

Configure, build and run a headless start/exit example on both target OSes; pin compiler/dependency versions and licenses.

## Negative and recovery cases

Missing tools and unsupported graphics capabilities produce actionable diagnostics; headless build needs no GPU.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

Actual build/run instructions are above; [the handoff](../docs/execution/PR-001-HANDOFF.md) records current validation and integration status. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
