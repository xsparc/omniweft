# Validation and evidence

## Current repository checks

The documentation bootstrap supplies `python tools/validate_plan.py`, running on Windows and Linux in GitHub Actions. It checks required project files, local Markdown paths/anchors, roadmap IDs, acyclic dependencies, authorized execution states, and feature-to-example coverage. This is planning validation only. PR-001 adds a separate CMake/CTest headless native lane; see [its example](../examples/platform-bootstrap.md) and [actual execution evidence](execution/PR-001-HANDOFF.md). PR-002 extends the native lane with the independent command-envelope oracle and typed non-finite serialization checks; see [the protocol example](../examples/protocol-reject_invalid.md) and [current evidence](execution/PR-002-HANDOFF.md). PR-003 adds atomic object execution, detached canonical snapshots, deletion/generation reuse and a deliberate rollback-mutation proof; see [its example](../examples/objects-atomic.md) and [current evidence](execution/PR-003-HANDOFF.md). No renderer or physics behavior is verified by these CPU examples.

## Implementation lanes

| Lane | Execution environment | Gate and evidence |
| --- | --- | --- |
| `cpu` | Windows 11/MSVC and Ubuntu 24.04/Clang or GCC; exact versions pinned in PR-001 | Compile, unit/integration tests, public-protocol examples, canonical nonphysics hashes |
| `gpu` | Named real Vulkan-capable Windows and Linux machines | Device/driver manifest, validation-layer output, readbacks, supported example assertions |
| `manual` | Desktop editor on both target OSes | Repeatable input/preview/recovery steps, result and screen recording with environment |
| `benchmark` | Fixed dedicated reference hardware | Frozen workload, warm-up/repetitions, p50/p95/p99, peak memory, comparable baseline |
| `optional-provider` | Explicitly enabled trusted integration environment | Live adapter smoke test, provider/model, redacted results and cost/latency ceiling |

Hosted CI operating-system images are not a Windows 11 desktop or a physical Vulkan GPU certification. Add clean-machine Windows 11 validation and Linux desktop X11/Wayland coverage before claiming supported desktop releases. A software Vulkan implementation may catch API errors but cannot establish real-driver behavior or performance.

Core tests and provider-adapter contract tests require no API keys, network calls, proprietary assets or model downloads. Initial provisioning of the pinned build tools can require network access; subsequent headless configure/build/test does not fetch engine dependencies. The `optional-provider` lane is not required to merge an otherwise verified offline adapter or release the offline engine.

## Evidence contract

Each PR attaches a [manifest](templates/EVIDENCE.md) with exact candidate SHA, work item, example, command, timestamp, exit code, environment, seed, assertions, results, limitations and artifact hashes. CI execution evidence for the final code commit may live in Actions/PR artifacts rather than being committed into that same commit: this avoids a self-referential evidence hash. A subsequent ledger update can record the merge SHA.

Capture JSON results, logs, command sequence, state checkpoints, and graphics artifacts when relevant. Use explicit `passed`, `failed`, `not_run` or `not_applicable`; never turn a skipped GPU job into a “GPU passed” badge. Redact credentials and sensitive provider payloads. Keep minimal reproducible fixtures under version control; store bulky CI artifacts with bounded retention.

An independent reviewer checks that the test would fail if the claimed behavior were absent or wrong. For example, a pixel test checks untouched neighbors as well as the painted region; an atomicity test places an invalid operation after a valid one; a voxel test crosses negative chunk coordinates and tests adjacent surfaces. Avoid tests that simply call the same implementation helper to manufacture expected values.

## Test families

- **Schema/property tests:** bounds, finite numbers, unknown fields/version policy, stable ordering and round trips.
- **Transactions:** all-or-nothing, duplicate keys, stale handles/revisions, permission denial, quota refunds and client disconnect.
- **Geometry:** known mesh bounds/indices, voxel occupancy and boundary surfaces, image bytes and region isolation.
- **Physics:** isolated analytical fixtures and tolerance-based contact/penetration constraints. Keep gravity, timestep, units and solver config recorded.
- **Persistence:** forced interruption at defined write stages, corrupted input, missing blobs, migration copy/recovery and idempotency after restart.
- **Graphics:** validation-layer cleanliness, resource lifetimes, resize/minimize, synchronization, exact format readbacks and tolerant display images.
- **Resilience:** timed-out providers, late cook completion, queue exhaustion, cancellation races and renderer device loss where supported.
- **Security:** protocol/import fuzzing, unauthorized cross-world operations and observation filtering; supported sanitizers on trusted CI.

## Numerical and visual thresholds

Use exact integer/byte comparisons for IDs, voxel materials, selected RGBA8 storage operations and canonical nonphysics data. Floating-point behavior uses fixture-specific tolerances chosen before results are evaluated. PR-025 should start with a named stack fixture and a penetration tolerance expressed in meters; it must explain expected solver settling and duration.

Visual comparisons fix scene, camera, resolution, seed and postprocess settings. Masks and per-channel/aggregate thresholds are versioned with rationale. A perceptual score alone cannot prove the correct object was edited; pair images with world/ID-buffer assertions. GPU golden changes require reviewed before/after artifacts and reason, not automatic baseline updates.

Performance targets in [simulation and rendering](SIMULATION_AND_RENDERING.md) are hypotheses until PR-035 measures named machines. Thereafter fail only comparable benchmark regressions under the adopted policy, initially a sustained >10% p95 regression across repeated runs or budget breach requiring investigation. Do not use noisy hosted-runner timings as hard graphics performance gates.

## CI rollout

1. Bootstrap: planning checks on `ubuntu-24.04` and `windows-2022` hosted runners.
2. PR-001: pinned toolchains, CMake presets, CTest, warning policy, CPU runner and license inventory.
3. PR-004: controlled Vulkan smoke lane on both OSes; no pretend GPU pass when runners are absent.
4. As features land: add each example to an explicit registry with applicable lanes and timeouts.
5. PR-036: bounded fuzz corpus runs per PR plus scheduled longer trusted runs.
6. PR-038: fresh-machine packaging, notices/SBOM, checksums and real-GPU certification for preview release.

Use read-only tokens for untrusted PR jobs, pin third-party Actions to reviewed full commit SHAs, and avoid privileged execution of fork code. GPU runners must be disposable/isolated and only run approved jobs without production credentials. See [GitHub secure-use guidance](https://docs.github.com/en/actions/reference/security/secure-use).

Required status checks are added to branch protection only after their exact names have run. A PR cannot bypass a failed functional lane by changing workflow filters or rewriting acceptance thresholds. Missing infrastructure is a precise blocker for the affected claim; independent CPU work may continue.
