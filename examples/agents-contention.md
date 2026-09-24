# agents.contention

Status: **merged; post-merge checks passed**. Work item: **PR-011 — Concurrency and idempotent retries**.

Dependencies: PR-007, PR-010. Validation lanes: cpu.

## Behavior and scope

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Use a tiny deterministic fixture with seed 7, generated locally or covered by an explicit asset license manifest. Exercise the public command/query interfaces once available, avoiding privileged test-only mutation paths.

## Build and run

Use the pinned CPU build instructions in [platform.bootstrap](platform-bootstrap.md), then run the native example:

```sh
omniweft_examples --example agents.contention --headless --seed 7 --verify --output artifacts/agents.contention
```

The native example submits two threads through one owner and live policy leases, then verifies receipt reuse, changed-payload rejection and compaction. The independent transport oracle additionally launches `omniweft_contention`, the opt-in `policy.retry.v1` host, and races real west/east SDK requests. Either principal may win; both complete expected states are specified independently.

The profile adds a four-receipt window per principal with a 2,000 ms absolute TTL. Canonical typed envelope equality includes transaction ID, revision, budgets and operations; JSON member order/whitespace do not matter. A retained different payload returns `IDEMPOTENCY_MISMATCH`. Expired/compacted consumed keys return `REQUIRES_RESYNC`, as do old epochs presented with current credentials. Retired credentials remain unauthorized. Replay preserves the original receipt while reporting the current outer admission sequence.

Retry storage is separate bounded host metadata: at most 16,384 canonical payload bytes and 8,192 receipt string-field bytes per entry, four entries for each of two principals. These content bounds exclude container/allocation overhead and are not a global RSS guarantee. Authentication and the existing live policy lease precede replay. World sponsorship, working quotas, request limits, scopes and deadlines are unchanged. Retention expiration is reclaimed on the next window operation or rotation/destruction; replay never extends it. Failed callbacks remain consumed with no reusable receipt. Receipt moves precede response serialization, so lost responses cannot cause a second application.

The existing `omniweft_control`, `omniweft_agents`, `omniweft_policy`, `Client` and `PolicyClient` retain strict `resync_only` behavior. New `RetryPolicySession`/`RetryPolicyClient` classes negotiate the [separate extension](../schemas/retry-1.schema.json). Retain the immutable handle returned by `prepare()` and pass it to `submit()` for explicit retries after `OutcomeUnknown`. The convenience `transact()` does not return a handle on failure; its caller must explicitly resynchronize. Fresh requests stay blocked while a prepared/uncertain request is pending. Observations and capability reads alone do not clear it. No automatic replacement key or retry is performed.

## Pass criteria

Two proposals from the same revision yield one commit and one explicit conflict; duplicate identical keys reuse the receipt.

## Negative and recovery cases

A changed payload under the same key is rejected; expired keys force resync instead of guessing. After receipt compaction or epoch rotation, retry an old sequence and prove it requires resync instead of applying again.

Use independently authored expected fixture values and preserve minimized repro inputs when a check fails. If a numerical tolerance is needed, name the fixture, units, timestep/device, threshold and reason before running; do not choose it after seeing a failure.

## Evidence and completion

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md. See the [evidence template](../docs/templates/EVIDENCE.md). A screenshot alone cannot pass the feature. Missing required lanes are `not_run`, and prevent a complete claim for that lane.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

For independent retained native and real-transport evidence, use a fresh directory (add `.exe` on Windows):

```sh
python tests/contention_oracle.py --executable build/linux-headless/omniweft_examples --host build/linux-headless/omniweft_contention --native-test build/linux-headless/retry_native_test --evidence artifacts/contention
```

The oracle checks exact full receipts, world/allocator state and policy usage, including two actual response-loss cases. Native tests cover allocation bounds and consumed callback failures; strict SDK tests cover malformed negotiation, immutable requests, client binding, uncertainty and sequence rollback. No numeric tolerance is needed for the integer-position fixture. Evidence is CPU-only and volatile: restart invalidates all epochs; no persistence or crash-safe exactly-once guarantee is claimed. See [the handoff](../docs/execution/PR-011-HANDOFF.md) for actual checks and delivery state. The [roadmap](../docs/ROADMAP.md) and [backlog](../planning/backlog.json) retain the same work-item and example IDs.
