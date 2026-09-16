# PR-007 local Windows and Linux evidence

Runtime candidate: 3055ada3604f7df879454d71f44956437496dd56.
Tree: 66a46165fe9b7e4797f4758dfa6a161ef54a684d.
This is an unmerged local candidate. Hosted Windows/Linux checks remain not_run; no merge gate is waived.

The [archive](windows-policy-3055ada.zip) contains the original four clean-candidate manifests, exactly their 14 declared artifacts, and a supplemental validation summary. The [external summary](windows-policy-3055ada-summary.json) records hashes and limitations. The archive is 21,872 bytes with 19 entries; SHA-256 is b26ad98823ec2b3a5b1f227bdca31302b94b594b9a957b15d99a33bd07ed0c76.

| Proof | Assertions | Declared artifacts | Result |
| --- | ---: | ---: | --- |
| Policy literal-state and real transport oracle | 1007 | 7 | Passed |
| Compiled native destination-scope bypass | 480 | 2 | Detected by unchanged oracle |
| Existing SDK authentication-bypass regression | 479 | 2 | Detected by unchanged oracle |
| Existing Owner lifetime/failure regression | 268 | 3 | Passed |

The four manifests retain 2234 assertions, including provenance and artifact checks. The SDK and Owner manifests retain their original PR-005/PR-006 labels; they are regressions run on this candidate, not new feature claims.

Separately, the local policy native suite passed 843 assertions and all 18 Windows CTests passed sequentially. The full CTest run preceded the final test-only wire-byte oracle refinement; the refined policy oracle reran against the clean runtime candidate. Production binaries remained unchanged. These supplemental results are not represented as additional independently replayed archive manifests.

Evidence includes exact fixture identities/revisions/transforms, canonical world bytes before and after rejection, sponsor recovery, a valid four-operation batch, actual 512/513-byte wire observations, 3072/3073-byte west working boundaries, 16384/16385-byte global body boundaries, and real incomplete-request saturation/timeout/recovery. Mutation proofs compile changed native code in disposable source copies and preserve the baseline source and executable.

No credential/epoch values or hashes, raw HTTP/process streams, account or machine paths, host/device identifiers or executable/PDB bytes are included. Generic tool versions, source/artifact hashes and allowlisted fixture data are retained. Independent read-only archive integrity/privacy audit passed with no blocking finding. It verified the complete canonical ZIP bytes, all hashes and allowlists, clean candidate/tree bindings, 79 relevant committed source files and the exact compiled mutation failures; it did not rerun runtime tests.

Build: Windows Release, MSVC 19.44.35227, CMake 3.31.6, Python 3.12.14. At Windows evidence capture, Linux container checks were not_run because the engine was unavailable; the later Linux run is recorded below. The policy fixture is CPU-only; prior renderer/GPU evidence is not relabeled as policy validation. See the [handoff](../../execution/PR-007-HANDOFF.md) for delivery state and next action.

## Linux container validation, 2026-09-16

The same clean runtime candidate and tree passed a fresh local Ubuntu 24.04 x86_64 Release build with Clang 18.1.3, Python 3.12.10, CMake 3.31.6 and Ninja 1.13.2. All 18 CTests passed (56.62 seconds), the native policy suite passed 843 assertions, planning validated all 38 items and all 32 planning regression tests passed. Original proof manifests retain another 2234 assertions: policy oracle 1007, compiled scope-bypass mutation 480, SDK authentication mutation 479 and Owner dispatch/lifetime proof 268. The last proof uses agents_owner_test.py; it does not claim a separate agents_failure.py retained run.

The [Linux archive](linux-policy-3055ada.zip) contains four original manifests, exactly their 14 declared artifacts and a supplemental validation summary. It has 19 entries and 22,907 bytes; SHA-256 is 06dfa347063205211e2b309512c318fa5e4e874925cac11d0eb47ec2cfabe072. The [external summary](linux-policy-3055ada-summary.json) records commands, results, versions, image/tool/source-pack hashes and limitations. The earlier Windows archive remains unchanged and records availability at its own capture time.

The container used two CPUs, a 3 GiB memory/swap ceiling, 256-process limit, no network, no host mounts, all capabilities dropped and no-new-privileges. Source was reconstructed from the exact runtime commit and tree objects, without ancestor history, remotes or workstation Git configuration. Official Ubuntu and Python base images are pinned by digest in the summary; the image installs matching clang-18/clang-tools-18 and the repository's hash-locked CMake/Ninja wheels. Tests ran sequentially and builds used two jobs. An initial image lacked clang-scan-deps and its build exited 127 before any test ran; installing clang-tools-18 and starting a fresh container resolved that provisioning failure without changing project source.

This establishes local Linux headless CPU behavior. It does not establish Linux desktop/GPU presentation or hosted CI success. Required hosted checks and maintainer integration remain pending. Raw logs, container/host identities, source-pack bytes and executables remain outside the public package. Independent read-only archive integrity/privacy audit passed with no blocking finding. It checked complete ZIP bytes, original manifests/artifacts, exact candidate source bindings and specific mutation/recovery outcomes. It did not rerun the tests or independently inspect the live container; container isolation is supported by the coordinator's observed, sanitized record.
