# policy.denied_edits

Status: **implemented candidate; not merged**. Work item: **PR-007 - Capabilities and resource budgets**.
Dependencies: PR-005, PR-006. Validation lanes: cpu.
Both dependencies are merged; Windows/Linux CPU checks remain required.
See the [plan](../docs/execution/PR-007-PLAN.md) and [handoff](../docs/execution/PR-007-HANDOFF.md).

## Run

Build the pinned headless project as described in [platform.bootstrap](platform-bootstrap.md). The executable is omniweft_policy (omniweft_policy.exe on Windows). Python uses only the standard library; no provider keys, external assets or GPU are required.

    python sdk/python/examples/denied_edits.py --executable build/linux-headless/omniweft_policy --output artifacts/policy-example
    python tests/policy_oracle.py --executable build/linux-headless/omniweft_policy --evidence-dir artifacts/policy
    python tests/policy_mutation.py --build-dir build/linux-headless --evidence-dir artifacts/policy-mutation

Use the Windows executable/build paths on Windows. Every output directory must be fresh. The public example produces policy.json with allowlisted fixture profiles, receipts and complete world snapshots. The independent oracle checks literal identities, revisions, transforms, quotas, canonical rollback bytes and recovery; it does not trust an example-reported pass flag. Real HTTP tests cover incomplete-body reservation, prompt excess-principal rejection, another principal's authored progress, timeout refund, exact operation/body/observation boundaries and renewal. The mutation proof compiles a disabled native destination-scope check in isolated source and requires the unchanged oracle to reject its unauthorized commit.

## Behavior

Permit a scoped edit while enforcing per-principal operations, memory, observation and queue limits. Foreign-region edits, allocation amplification and repeated rejected batches neither mutate nor leak quota.

The fixture has eight slots, world workshop and seed 7. Fixed grants are west X [-8,-1] and east X [1,8], each Y/Z [-4,4]. The complete rotated/scaled unit cube must fit its inclusive region. Both principals explicitly have whole-world read grants. Create plus temporary-target transform can place a new cube; every surviving create needs placement. Current foreign cubes and foreign destination bounds are rejected by the shared typed Coordinator.

The separate policy.v1 profile reports four operations per transaction, one data-plane request per principal and two globally. Three bounded transport workers allow B to proceed while A holds an incomplete body; this is data-plane saturation isolation, not general fairness or throughput. Empty-body capabilities and policy status are globally bounded control-plane exemptions.

Resource accounting uses charged-resource-bytes-v1, not allocator/RSS measurements. Slot skeletons cost 128 bytes and live cubes add 256. West/east retained limits are 512/2048 bytes; every staging prefix is checked. Sponsors survive deletion and transfer atomically on reuse. Working charge is 73728 + 8 * declared_body_bytes, with limits 98304/262144 and a 16384-byte body cap. Policy headers stop at the delimiter before body reservation/allocation. Global framing storage is separate and bounded. Native typed admission independently sums bounded string fields before serialization; the fixed base accounts for bounded numeric, JSON, staging and response work.

Complete observe/runtime UTF-8 response bodies are limited to 512/4096 bytes. These are size/concurrency limits, not cumulative bandwidth quotas. West runtime can exceed its limit with no entities as counters grow. PolicyClient.resync_policy() recovers revision and sequence without requiring an oversized observation. Owner-stage rejected receipts consume one sequence; pre-admission denials and incomplete-body timeouts do not. Renewal invalidates credentials and resets sequences while preserving retained quota.

## Limits and evidence

No capability administration, confidential filtering, pagination, rate quotas, persistence or generalized entity representations are claimed. Canonical world bytes retain their format; sponsorship is volatile host metadata. The policy host is CPU-only; default renderer/SDK behavior remains subject to regression checks. Actual passes, development failures, independent findings and pending delivery are recorded in the handoff. Hosted validation is tracked in [draft PR #11](https://github.com/xsparc/omniweft/pull/11); no required check is waived.

## Execution contract

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.
