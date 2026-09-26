# PR-011 concurrent proposals and retained retry evidence

Runtime candidate: `ceaac8fc9d85fb4a8e2ed37f60f43387e26af5f6`; tree `a5cf5a43cd6f7b5fa94562f505274f3811bafead`. Merged: [PR #17](https://github.com/xsparc/omniweft/pull/17). Delivery state: [handoff](../../execution/PR-011-HANDOFF.md).

## Windows CPU proof

[windows-ceaac8f.zip](windows-ceaac8f.zip) contains 10 JSON members (manifest plus nine artifacts), 15,396 bytes, SHA-256 `784c883f4bca2f8930af2ee007fa414bd091b9d32fbf5a26a3fe7dc607cc8cf8`.

The clean-candidate oracle passed 796 assertions and retains separate native retry (96 assertions) and strict SDK (53 assertions) summaries. The manifest binds 51 source and 13 SDK file hashes to the exact candidate, three executable hashes and actual build/toolchain provenance. Executable bytes are excluded. The archive contains reviewed fixture data only; no credentials, epoch values/hashes, personal paths/host identifiers or raw HTTP/process streams.

Literal native and real two-principal SDK proposals produce one commit and one full revision-conflict receipt. Exact replay preserves both receipts and world state; canonical typed equality ignores JSON formatting. Tests cover changed transaction/revision/budget/operations, an older retained receipt after later admissions, four-entry compaction, absolute expiry without renewal, failed response delivery, discarded socket responses, retired credentials/epochs, foreign principal, sequence gap and quota saturation. Explicit reconciliation permits fresh proposals. The native suite covers payload/receipt caps and consumed keys after callback failure. Corruption tests reject 22 altered native proofs across either winner.

All 30 Windows CTests passed before final test-only refinement; all three affected contention/SDK tests and the clean-candidate oracle passed afterward. Independent source/oracle review closed. Independent archive audit passed 1,582 static checks: exact membership/integrity, clean Git/source/SDK binding, actual configured build/tool/compiler/executable binding, privacy and independently reconstructed full native/transport outcomes. No native reruns were used for this audit. Generic environment: Windows Release, MSVC 19.44.35227, CMake 3.31.6, Ninja 1.13.2, Python 3.12.14, Vulkan disabled.

## Reproduce and limits

Use the commands in [agents.contention](../../../examples/agents-contention.md) with a fresh evidence directory. Hosted native jobs run the same example/oracle and retain Windows/Linux manifests.

Local Linux is not_run. All six runtime-candidate hosted Windows/Linux checks passed; exact runs and checkout binding are recorded in the handoff. Hosted contention retained 796 Windows / 798 Linux oracle assertions plus native 96 and SDK 53 on each platform; counts vary with receive chunking. Latest delivery-head results remain available through the PR checks link. This CPU-only proof does not establish GPU, physics or persistence. The opt-in four-receipt window expires after 2,000 ms; it is volatile, not crash-safe exactly-once execution. Host metadata content caps are separate from world quota and exclude allocator/container overhead. Saturation probes require scheduling progress inside the unchanged one-second request deadline. Earlier development failures and test refinements are recorded in the handoff; successful validation does not erase that history. The maintainer merged PR #17; its matching tree and six passing post-merge jobs are recorded in the reconciled handoff.
