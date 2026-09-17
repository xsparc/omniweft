# PR-007 compatibility repair evidence

Runtime candidate: f511692699edaf867d032ccaf6286e7b6c4c4e4e.
Tree: 87b9557ad48279eb00dd2db0ae51008e3d90c863.
Base: d7b52696edd3e7cb9c3388972509a05f16dc6466, actual PR #11 merge.
Delivery: [draft PR #12](https://github.com/xsparc/omniweft/pull/12); not merged.

The [archive](compatibility-f511692.zip) preserves ten original clean-candidate manifests and exactly their 636 declared artifacts, plus the baseline-negative record and a supplemental validation summary. It contains 648 entries and 354,712 bytes; SHA-256 ab8e43c4218cd8d66c49c7a61f6370a7b3fc42eb993ec2f2c2948efff3ea3230. The [external summary](compatibility-f511692-summary.json) includes commands, results, toolchain/source hashes, actual bounded container settings and limitations.

| Proof | Windows assertions | Linux assertions | Artifacts per platform |
| --- | ---: | ---: | ---: |
| Complete SDK oracle against control host | 6630 | 6630 | 153 |
| Complete SDK oracle against fixed-step host | 6630 | 6630 | 153 |
| Policy oracle regression | 1007 | 1007 | 7 |
| Compiled authentication-bypass mutation | 493 | 493 | 2 |
| Owner dispatch/lifetime proof | 275 | 275 | 3 |

The 30,070 retained assertions include source/artifact provenance checks. All four 353-assertion child SDK reports (two per platform) include successful split-body transmission, exact typed rejection, complete unchanged state, allocation preservation and fresh-client recovery. Both SDK oracles also record the idle rejected peer's deadline-bounded recovery. The original proof labels remain PR-005, PR-006 or PR-007 as appropriate; these are regressions, not relabeled new features.

The same final test source rejects the unchanged prior Windows agents executable with its fixed known-rejection assertion. The baseline executable's SHA-256 and the exact final test-source hash are retained, while binary bytes stay private. No successful retry is substituted for the failing pre-fix behavior.

Windows Release used MSVC 19.44.35227 and Python 3.12.14; Linux Release used Clang 18.1.3 and Python 3.12.10. Both used CMake 3.31.6 and Ninja 1.13.2. All 18 CTests passed on each platform. Windows full CTest preceded the test-only one-byte peek refinement; both complete affected SDK oracles then passed on the final clean candidate. Linux full CTest used that final candidate and took 60.19 seconds. Linux ran with two CPUs, 3 GiB memory/swap limit, 256 processes, no network/host mounts, all capabilities dropped and no-new-privileges; the test container is stopped. Source was reconstructed from exact commit/tree objects without workstation configuration or ancestor history.

Independent source review closed after correcting the new test's packet-fragmentation assumption. Independent final archive audit passed: original manifests and artifacts, source bindings, privacy, canonical full ZIP reconstruction and public/private copy equality were verified. This was an evidence audit, without runtime replay or live container inspection; the baseline outcome was verified from its retained record, without independently rehashing the prior executable. Hosted validation is tracked by draft PR #12 and is distinct from these local proofs. GPU behavior is unchanged and was not rerun. Raw logs, actual credentials/epochs, host identifiers/paths, source-pack bytes and executables are excluded from the public package.

## Hosted runtime validation

All six hosted checks passed on runtime f511692699edaf867d032ccaf6286e7b6c4c4e4e: [Windows/Linux native](https://github.com/xsparc/omniweft/actions/runs/35215197326), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/35215197517) and [planning](https://github.com/xsparc/omniweft/actions/runs/35215197334). These later results do not relabel the archive's earlier local-capture status. Any subsequent documentation/evidence delivery commit requires its own current-head checks before integration.
