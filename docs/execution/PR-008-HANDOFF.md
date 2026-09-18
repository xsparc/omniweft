# PR-008 handoff

State: in review; [draft PR #13](https://github.com/xsparc/omniweft/pull/13) open, not merged. Owner: Codex coordinator.
Branch: codex/pr-008-ai-blocks. Base: 1c94777a9fac7869cc83e21f01b1ccc034c439a4.
Runtime: de3d52e8a4937a1eae59c3a41b22f791fdd00149; tree 98579c6e5933b6f0b3b46966dfb8526ed3f23f37.
Scope: [slice plan](PR-008-PLAN.md), [authorization](AUTHORIZATION.md).

The PR-007 repair is integrated with six passing delivery checks and six passing post-merge checks. The new example builds and rearranges a five-block room through the existing east PolicyClient grant. A rejected mixed-validity plan leaves every authoritative byte intact before a corrected plan succeeds. The policy host optionally renders detached snapshots; existing owner scheduling, quotas and admission/lease behavior remain intact. Shared report serialization and host-owned capture barriers preserve existing agents defaults. No dependency, wire shape, grant or save-format changes.

Run the example and independent oracle using [showcase.ai_blocks](../../examples/showcase-ai_blocks.md). Windows pinned CPU/Vulkan Release builds passed. All 20 CTests passed on final source before commit; clean-candidate room CPU/GPU and existing agents/render physical-GPU proofs passed afterward. See [retained evidence](../evidence/PR-008/README.md) for exact counts, hashes and limitations. Oracle corruption selftests passed. Local Docker validation is not_run because the Linux engine was unavailable; hosted Linux checks are tracked separately. Linux physical GPU remains not_run.

Independent architecture, source and oracle review completed. Findings resolved: policy runtime presentation telemetry, actual readback retention, numeric privacy bounds and explicit build/shader source binding. Reviewers did not replay native tests. Final independent archive audit passed, including exact public/archive bytes, source/toolchain/binary bindings, privacy and 10179 retained-data checks over all 12 captures and domain/lifecycle proofs. All six runtime-head hosted checks passed; delivery-head checks remain required. Original source/evidence is preserved; documentation/evidence delivery changes are coordinator-owned.

Next: inspect all required checks on the actual current draft head and resolve concrete failures. Await maintainer contribution review/certification and actual squash merge. Then verify merge tree/post-merge checks before starting the next eligible roadmap slice. No automatic merge is authorized.

## Hosted runtime checks

All six checks passed on runtime de3d52e8a4937a1eae59c3a41b22f791fdd00149: [Windows/Linux native](https://github.com/xsparc/omniweft/actions/runs/35339087519), [optional renderer builds](https://github.com/xsparc/omniweft/actions/runs/35339087374), and [planning](https://github.com/xsparc/omniweft/actions/runs/35339087375). These are hosted build/test results, not Linux physical-GPU evidence. The subsequent documentation/evidence delivery commit still requires its own current-head checks before integration.
