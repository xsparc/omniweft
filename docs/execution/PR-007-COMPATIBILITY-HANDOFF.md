# PR-007 compatibility follow-up handoff

State: PR #12 merged; independent reviews, delivery checks and post-merge checks passed. See verified integration below.
Branch: codex/pr-007-sdk-compatibility. Base: d7b52696edd3e7cb9c3388972509a05f16dc6466.
Runtime: f511692699edaf867d032ccaf6286e7b6c4c4e4e; tree 87b9557ad48279eb00dd2db0ae51008e3d90c863.
Scope: [bounded plan](PR-007-COMPATIBILITY-PLAN.md). Root owns this branch and backlog; previous worktrees remain preserved.

## Reproduction and correction

Post-merge Windows native validation failed agents.sdk_compatibility; the other 17 CTests and five post-merge jobs passed. A local probe reproduced OutcomeUnknown in the retired client's mutation after renewal. An explicit split-write fixture reproduced ConnectionAbortedError five of five times while preserving world revision zero. The new unchanged public-SDK regression rejects the prior agents executable (SHA-256 4fbf6ebd9af825161c802544d111533f5b2f5110fd1e248361e8f2c6d9e391c6) with its fixed known-rejection assertion; the baseline executable remains unchanged.

The legacy rejection path now queues its existing error response, half-closes the send side and discards input into a fixed 4096-byte scratch buffer. EOF/reset, the unchanged original request/host deadline or a 1 MiB discard cap ends cleanup. Discard never parses/admit commands or consumes revision/sequence. Policy admission/leases and SDK uncertainty handling remain unchanged.

New tests require the response to start before successful separated body transmission, then exact typed 401/NOT_AUTHORIZED, complete unchanged state and fresh-client recovery. An idle rejected peer must permit an authorized observation within the existing request-deadline margin while its write side remains open. Existing deadlines/assertions remain intact.

## Verification and remaining work

Windows/Linux Release builds and all 18 CTests passed. Windows full CTest preceded a test-only one-byte response barrier refinement; both affected full SDK oracles passed afterward on the final clean runtime. Linux full CTest used that runtime. Both platforms retained passing SDK-control 6630, SDK-agents 6630, policy 1007, authentication mutation 493 and Owner dispatch/lifetime 275 assertions. The [evidence package](../evidence/PR-007-COMPATIBILITY/README.md) preserves original manifests and artifacts. Independent source review closed with no remaining finding, and final archive audit passed. The audit verified both platforms' retained proofs, full ZIP reconstruction, privacy and public/private copy equality, without replay or live container inspection; baseline failure was reviewed as retained evidence without independently rehashing the prior executable. All six runtime-head hosted checks passed; delivery-head checks remain required. Raw logs and transport details remain private; only fixture assertions and necessary version/source/artifact hashes may be published. No new dependency, permission, protocol or save-format change is introduced. GPU behavior is unchanged and no new GPU claim is made.

Next: continue PR-008 from the verified repair merge; preserve the original evidence.

## Hosted runtime validation

All six hosted checks passed on runtime f511692699edaf867d032ccaf6286e7b6c4c4e4e: [Windows/Linux native](https://github.com/xsparc/omniweft/actions/runs/35215197326), [optional renderer](https://github.com/xsparc/omniweft/actions/runs/35215197517) and [planning](https://github.com/xsparc/omniweft/actions/runs/35215197334). These later results do not relabel the archive's earlier local-capture status. Any subsequent documentation/evidence delivery commit requires its own current-head checks before integration.

## Verified integration

PR #12 was squash merged as 1c94777a9fac7869cc83e21f01b1ccc034c439a4 on 2026-09-17. Its tree 2a58baaf3e2c019c848cb0e434d0db802901d095 exactly matches reviewed delivery 851f83d47f5978609d29b56f9d44faa4bf4a763e. All six delivery checks passed. All six post-merge checks also passed: [native](https://github.com/xsparc/omniweft/actions/runs/35233666704), [renderer](https://github.com/xsparc/omniweft/actions/runs/35233666678), [planning](https://github.com/xsparc/omniweft/actions/runs/35233666645). The compatibility repair is integrated; PR-008 is now eligible. Historical evidence remains bound to its original runtime candidate.
