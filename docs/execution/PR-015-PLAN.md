# PR-015: bounded provider lifecycle and cancellation

Adopted scope: [authorization](AUTHORIZATION.md), [agents.worker_failure](../../examples/agents-worker_failure.md), ADR-0002. Coordinator branch `codex/pr-015-worker-lifecycle` starts at verified PR #20 merge `a18b9e2736c211c4397d8eb94c48c5c27acb5a83`. PR-007, PR-011 and PR-014 are integrated. All six PR-014 post-merge checks and both native evidence manifests passed and match the reviewed delivery tree.

## Behavior and boundary

Add an opt-in Python SDK supervisor for one active bounded scripted proposal. A fixed bundled child receives typed goal data only, with no token, epoch, prepared transaction or direct mutation authority. The parent owns a dedicated `RetryPolicyClient`; all accepted mutations use the existing policy.retry.v1 host and its fixed-step owner, grants, leases, quotas, revision checks and retained receipts. Legacy ScriptedBuilder behavior remains unchanged.

Each request binds a monotonic request ID, worker generation and captured revision. A proposal must match these identities and the original absolute deadline when received and before explicit submission. Running/ready work may cancel, time out or be superseded. Late child output is discarded. Once submission starts, cancellation is too late and cannot claim that mutation was prevented. Submitting or uncertain requests block replacement authoring. Queryable immutable local status never waits on network I/O, pipe reads or process cleanup under its state lock.

The same exact prepared request remains available for explicit retry after uncertain response delivery, including provider restart. Receipt expiry requires explicit reconciliation; it cannot become a new-key replay. Restart means restarting the provider while supervisor and native host survive. There is no supervisor/host crash durability claim and no new persistent format, endpoint, grant, dependency, OS sandbox, GPU or physics capability.

Bound eight requests/status records per supervisor lifetime, one active proposal, at most two live children during old-result draining, at most four operations and 16 KiB per IPC frame. Scripted goals build or move a cube; fixed failure fixture modes exercise delayed, crashed, malformed and overlong output without accepting executable code or arbitrary worker paths. Absolute deadlines and bounded cleanup prevent abandoned children from accumulating.

## Ownership and verification

Implementation helper owns new lifecycle/worker modules and SDK state tests in an isolated worktree. Coordinator owns exports, runnable example, independent oracle, CI and all shared documentation/ledger files. Independent architecture/source/oracle/archive review is required; it is not a human GitHub approval or contribution certification.

The runnable Python example uses the existing native contention host. Independently authored literal full snapshots, generation identities, receipts, revision/admission counters and policy charges verify timeout, cancellation, crash/restart, actual superseded late output and valid recovery. Runtime samples must show advancing native ticks while worker work is pending; terminal statuses stay queryable. Real post-commit response loss must produce uncertainty and exact-key receipt recovery with no duplicate entity/revision. Expired recovery must require explicit reconciliation without reapplication. Strict parser/capacity/cleanup tests and evidence-corruption selftests supplement the real process/transport oracle.

Run targeted tests, complete Windows CTest and required hosted Windows/Linux CPU checks. Retain clean-candidate source/SDK/build/artifact hashes and reviewed JSON fixtures only; never retain credentials, epoch values/hashes, raw streams, personal paths or executable bytes. Local Linux may be not_run when hosted Linux provides the required CPU lane. No Docker or physical GPU test is needed for this SDK-only behavior. Rollback is a normal PR revert; no save migration. PR-016 stays proposed until actual PR-015 integration.
