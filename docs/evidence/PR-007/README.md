# PR-007 local Windows evidence

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

Build: Windows Release, MSVC 19.44.35227, CMake 3.31.6, Python 3.12.14. Linux container checks were not_run because the engine was unavailable. The policy fixture is CPU-only; prior renderer/GPU evidence is not relabeled as policy validation. See the [handoff](../../execution/PR-007-HANDOFF.md) for delivery state and next action.
