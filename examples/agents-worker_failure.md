# agents.worker_failure

Status: **merged; restart-status correction integrated and verified**. Work item: **PR-015 — Agent lifecycle and cancellation**. Dependencies: PR-007, PR-011, PR-014. Validation lanes: cpu.

## Behavior

An opt-in `WorkerSupervisor` launches the fixed bundled proposal worker. Typed cube goals carry a captured world revision, request identity and generation. The parent retains the only mutation client and sends accepted proposals through the existing `policy.retry.v1` grant and fixed-step owner. The worker receives no session token, epoch or prepared transaction. This process separation is not an OS sandbox.

Each supervisor retains at most eight request statuses without eviction and permits one active request, with at most two bounded children during old-result draining. IPC frames are capped at 16 KiB. Deadlines are absolute from request start. A ready proposal expires when a public method checks it; there is no background notification service. A deadline that expires while the parent prepares an unsent key permits reconciliation only, never a first submission through retry. `status(request_id)` reports immutable local state without waiting on provider or mutation I/O.

Cancellation, timeout and supersession before submission prevent mutation. A late result cannot change a terminal status or become a new proposal. Explicit submission crosses the cancellation boundary: `cancel` then returns `too_late`, and replacement authoring is blocked until the outcome is known or explicitly reconciled. A worker restart never clears a pending transaction.

An uncertain submission keeps its exact `PreparedTransaction` for explicit retry. Existing four-receipt/2,000 ms retention remains unchanged. After expiry, retry requires explicit reconciliation; it never silently allocates a replacement key. This contract covers provider restart while the supervisor and native host survive. It does not make local statuses persistent or make the volatile policy host crash-safe.

## Run

Build the pinned headless targets as described in [platform.bootstrap](platform-bootstrap.md), then run the Python example with the resulting contention host:

```sh
python sdk/python/examples/worker_failure.py --executable build/linux-headless/omniweft_contention --headless --seed 7 --verify --output artifacts/agents.worker_failure
python tests/lifecycle_oracle.py --executable build/linux-headless/omniweft_contention --evidence artifacts/lifecycle
```

On Windows use `build/windows-headless/omniweft_contention.exe`. Choose fresh output directories. The example emits sanitized `result.json`; the oracle separately checks full literal snapshots, receipts, policy usage, request generations and live tick progress. The independent transport case loses an actual post-commit response and proves same-key recovery without duplicate creation or revision increments, then verifies expiry and explicit reconciliation. Core tests use only repository-authored scripted data and need no live provider or keys.

The fixture times out a delayed worker, cancels and drains a second worker, observes a real child crash, supersedes an actual delayed result, creates one cube at x=3, restarts the provider and moves the same generation-safe entity to x=4. World revisions remain zero through all prevented proposals, then advance to one and two. The native simulation keeps advancing throughout pending worker work. Malformed/oversized output, identity mismatches, cleanup and capacity are covered by the SDK edge suite.

## Evidence and limits

Original integration is recorded in [the handoff](../docs/execution/PR-015-HANDOFF.md); the [restart-expiry correction](../docs/execution/PR-015-RESTART-HANDOFF.md) carries current follow-up validation. A passing state-machine unit test alone does not establish real process or native transport behavior. Physical GPU, physics, model inference, host/supervisor restart durability and performance targets are outside this slice. Existing legacy SDK profiles, policy grants/leases, native request deadlines and persistence formats remain unchanged. Revert through a normal PR; no save migration is required.

## Adopted acceptance and recovery contract

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

Timeout, cancel, crash and restart a worker while simulation remains responsive and statuses remain queryable.

Late results from a superseded request never commit; retry after uncertain disconnect does not duplicate work.

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.
