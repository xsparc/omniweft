# PR-006 Windows runtime evidence

These bundles verify clean runtime commit dc604ebd83a08ba0d23dcac473d5f10c7e74a362, tree 5e0cd4e71935d82cdc4ce0eb7087819a8b45270e. They cover the public scripted-provider example, actual fixed-step owner, live Windows Vulkan presentation and existing rendering regression.

- [Provider/runtime reports and readbacks](windows-agents-dc604eb.zip): 80,649 bytes, 96 entries, 4,588,635 bytes uncompressed. SHA-256: 179c9b84155ffbadee7e331d6ba1e75a27e1a3394a2fc82905869e726e83b4f7.
- [Provider/runtime summary](windows-agents-dc604eb-summary.json): exact manifest, toolchain, executable and inventory hashes.
- [Existing renderer regression reports and readbacks](windows-render-dc604eb.zip): 34,258 bytes, 33 entries, 6,824,795 bytes uncompressed. SHA-256: fc7b111cd2de4a786d299b8a0ac0ecb4e5cc23a21e0e7d2b863c971d614f4bc9.
- [Renderer regression summary](windows-render-dc604eb-summary.json): its untouched manifest and exactly 32 inventoried artifacts.

| Provider/runtime root | Passed assertions | Artifacts | Meaning |
| --- | ---: | ---: | --- |
| cpu | 1,606 | 30 | Real separate provider/native/observer processes, delayed barriers, continued ticks, exact authoring and repeated public-example results |
| gpu | 4,065 | 53 | Two actual GPU runs, revision 1 then revision 3; four color/ID/depth captures, geometry, publication stamps and fenced retirement |
| clock-mutation | 455 | 3 | Synthetic elapsed-time clock policy; compiled cap 4-to-5 mutant rejected by actual callback-count oracle |
| publication-fault | 465 | 2 | Actual authenticated commit followed by injected publication failure; no receipt, connection closure and exit 4 within two seconds |
| owner-dispatch | 253 | 3 | Actual Owner with controlled clock; 33 native assertions on cancellation, zero-due/overloaded deferral, recovery and publication before acknowledgment |

The main archive contains 6,844 recorded assertions and 91 domain artifacts, plus five untouched manifests. Assertion totals include provenance and structure checks. Controlled-time proofs are distinct from actual process-delay and physical-GPU evidence. Exact canonical authoring hashes exclude timing, presentation and credentials.

The separate existing render.world_cube regression passed 1,961 assertions with six independently rechecked GPU captures, startup rejection/recovery and resize/minimize/resource-retirement evidence. Its manifest SHA-256 is b01583e3f3f433dfdc5cc478752189ceef3ee9ccaf0441b7a9bbc97318f85537.

Hardware: NVIDIA GeForce RTX 5070, Vulkan 1.4.351, driver 2584739840. Validation was enabled; recorded validation error/warning assertions were zero. Captures came from actual submitted GPU work. Toolchain: Windows, Python 3.12.14, MSVC 19.44.35227.0, CMake 3.31.6, Ninja 1.13.2.git.kitware.jobserver-pipe-1, Debug.

Independent review bound all 205 tracked source files to the runtime commit and checked exact inventory, hashes, sizes, strict JSON, complete canonical ZIP bytes, fixture snapshots, readbacks and lifecycle records. Entry timestamps are fixed and comments are empty. Neither archive contains personal machine paths/identifiers, private descriptors, token/epoch values or credential hashes, raw process/HTTP streams, executables or PDBs.

The local proof is immutable. Follow-up test commit 989a88002b3049bd7f1194aeaf4b390436096134 adds one canonical-root assignment to each publication/owner test helper so hosted Windows short temporary paths pass the same containment checks. Actual short-path reproduction, both complete helpers and independent escape-rejection checks passed. Runtime, SDK, clock, renderer, fixtures and oracle thresholds did not change. Later hosted evidence binds its own exact merge candidate and updated helpers; it does not relabel these archives.

All 16 local CTests passed sequentially. An initial run concurrent with disposable compilations had one legacy SDK child-probe failure; the isolated probe passed 310 assertions, the full sequential suite passed, and the initial hosted Windows suite also passed all 16. No repeatable SDK defect was established; no test was weakened or silently retried in CI.

Physical Linux GPU and local Docker checks are not_run; the Docker engine was unavailable. All six hosted planning/native/optional-renderer jobs passed on both Windows and Linux at corrected-helper head 989a88002b3049bd7f1194aeaf4b390436096134; their exact runs and independently verified merge candidate are in the handoff. Hosted Linux CPU and optional-renderer builds are separate checks. There is no physics, persistence or cross-device determinism claim.

See [the example](../../../examples/agents-mock_builder.md), [contract](../../execution/PR-006-PLAN.md), [handoff and hosted checks](../../execution/PR-006-HANDOFF.md), and [PR #10](https://github.com/xsparc/omniweft/pull/10). Maintainer squash merge and six post-merge checks are verified in the handoff; this evidence remains bound to its original runtime candidate.
