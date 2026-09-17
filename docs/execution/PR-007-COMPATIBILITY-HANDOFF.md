# PR-007 compatibility follow-up handoff

State: implementation under validation and independent review; not published or merged.
Branch: codex/pr-007-sdk-compatibility. Base: d7b52696edd3e7cb9c3388972509a05f16dc6466.
Scope: [bounded plan](PR-007-COMPATIBILITY-PLAN.md). Root owns this branch and backlog; previous worktrees remain preserved.

## Reproduction and correction

Post-merge Windows native validation failed agents.sdk_compatibility; the other 17 CTests and five post-merge jobs passed. A local probe reproduced OutcomeUnknown in the retired client's mutation after renewal. An explicit split-write fixture reproduced ConnectionAbortedError five of five times while preserving world revision zero. The new unchanged public-SDK regression rejects the prior agents executable (SHA-256 4fbf6ebd9af825161c802544d111533f5b2f5110fd1e248361e8f2c6d9e391c6) with its fixed known-rejection assertion; the baseline executable remains unchanged.

The legacy rejection path now queues its existing error response, half-closes the send side and discards input into a fixed 4096-byte scratch buffer. EOF/reset, the unchanged original request/host deadline or a 1 MiB discard cap ends cleanup. Discard never parses/admit commands or consumes revision/sequence. Policy admission/leases and SDK uncertainty handling remain unchanged.

New tests require real early 401 before successful separated body transmission, exact typed NOT_AUTHORIZED, complete unchanged state and fresh-client recovery. An idle rejected peer must permit an authorized observation within the existing request-deadline margin while its write side remains open. Existing deadlines/assertions remain intact.

## Verification and remaining work

Windows Release build and CPU tests are running. Linux checks, retained current-candidate evidence, independent source/evidence review and hosted checks remain pending. Raw logs and transport details remain private; only fixture assertions and necessary version/source/artifact hashes may be published. No new dependency, permission, protocol or save-format change is introduced. GPU behavior is unchanged and no new GPU claim is made.

Next: resolve actual validation findings, retain clean candidate evidence, complete independent review and open one bounded follow-up draft for maintainer review/certification and squash merge. Do not start PR-008 before this correction integrates.
