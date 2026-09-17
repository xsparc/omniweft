# PR-006 execution plan

State: **done**; actual maintainer merge and post-merge checks are recorded in [the handoff](PR-006-HANDOFF.md). [PR #10](https://github.com/xsparc/omniweft/pull/10) and [reviewed evidence](../evidence/PR-006/README.md) are complete; the original contract below remains unchanged. Adopted scope: [authorization](AUTHORIZATION.md). Dependency PR-004 merged as ba80bb8392206a32810c8de989245e80cdfe055d and PR-005 merged as 3ab7bf114aa4acdef52387b1709be3fcaccb17b9. Base is that verified PR-005 squash, tree 6a31fa40a5fa5bc25683e9d821cde3fa53385c03. The coordinator promoted the adopted, dependency-ready item to ready and claimed it on 2026-09-13, branch codex/pr-006-fixed-step-provider.

## Outcome and ownership

Implement [agents.mock_builder](../../examples/agents-mock_builder.md): a separate, repository-authored scripted provider builds a known three-cube arrangement through the public SDK while the native world advances fixed steps. Actual Vulkan presentation consumes live detached publications from that same world. Include the runnable Python launcher, headless structural mode, bounded interactive inspection, independent oracle, negative/recovery cases and reviewed evidence in this PR.

The coordinator owns SDK/provider code, schemas, CMake/CI, docs, public evidence and the shared backlog. One isolated native author owns control dispatch, the pure clock, new native agents host and live renderer adapter. A separate isolated test author owns agents fixtures/oracles/mutation proof and the native clock test. An independent architect reviews implementation and retained evidence. Earlier user/helper worktrees are preserved; helpers do not commit or push.

This is fixed-step scheduling and deterministic authoring, with no physics solver, physical determinism, model service/key, general plugin execution, remote endpoint, persistence, interpolation, recorded replay or larger policy/queue system. Existing world/transaction/presentation contracts, action/dependency pins, shader provenance and permissions remain intact.

## Frozen clock and owner boundary

Use integer rational 60 Hz arithmetic: elapsed nanoseconds contribute 60 accumulator units each; one due tick consumes 1,000,000,000 units. Use quotient/remainder arithmetic to support uint64 elapsed input without multiplication overflow. Execute at most four due callbacks per advance, count and discard excess whole debt, preserve the fractional remainder, and increment overload_count once per overloaded advance. simulation_tick counts actual executed callbacks only. Reject cumulative counter overflow before executing callbacks or changing clock state.

Independent preregistered edges include 16,666,666 ns -> zero steps, then 1 ns -> one step with remainder 20; 83,333,333 ns -> four steps, no dropped step, remainder 999,999,980; 83,333,334 ns -> four executed and one dropped, remainder 40; 1 s -> four executed and 56 dropped, followed by 16,666,667 ns -> one executed and remainder 20. Synthetic elapsed-time policy tests are identified separately from real process timing evidence.

The gateway retains bounded framing, authentication and private session I/O on its own thread. A dedicated owner thread exclusively owns the World and simulation clock. SDL/Vulkan runs on the main thread and consumes detached publications. One mutex-protected pending dispatch slot serializes pending -> claimed -> completed or cancellation before claim. Gateway callers retain task/session lifetime until claimed work completes. No task may outlive captured request state.

Dispatch the FINAL request-deadline, session-expiry, epoch and sequence checks together with sequence consumption and typed application to an actual simulation boundary. Cancellation before claim never mutates or consumes a sequence. Defer pending authoring throughout an overloaded advance; only a later non-overloaded actual tick can apply it. A zero-due poll cannot admit authoring. Read tasks may run between ticks and never advance time or authoring revision.

Each completed tick publishes its full detached snapshot before returning the committed receipt or servicing subsequent reads. snapshot_sequence is simulation_tick + 1, including initial publication 1 at tick 0. The renderer may lag; its own frame/publication stamps identify the source it actually presented. Sample telemetry coherently so a reported presentation never points ahead of its enclosing runtime publication.

Use a shared normal-stop deadline across gateway/simulation/rendering, not separately shifted deadlines after startup. Normal runtime remains the cutoff for admission and I/O. Keep an outer whole-process watchdog armed through every thread join and GPU cleanup with the established additional 1000 ms emergency grace. Exit 0 is orderly completion; actual stalled cleanup is failure 4. The grace authorizes no extra serving time.

## Public compatibility

Retain exact PR-005 private descriptors, capabilities, observe snapshots, transaction envelopes/receipts and error shapes. Synchronous admission still means the response follows committed/rejected execution, now at a real tick in the opt-in agents host. apply_at.expires_after_ticks remains deferred metadata, as in PR-005: this slice enforces HTTP, session and normal process deadlines, and does not claim a separately enforced tick-expiry policy.

The new omniweft_agents executable accepts the same six bounded base configuration pairs as omniweft_control, plus optional --gpu 0|1, --interactive 0|1 and --output PATH. The old executable and default NativeSession argument list remain compatible. Output must be fresh and contains only bounded domain data. GPU verification is bounded to 600 frames/30 seconds; interactive inspection remains bounded by the configured host lifetime.

An optional native callback enables authenticated GET /v0/runtime only on the new host. Legacy hosts return NOT_FOUND. The response has exactly protocol_version, epoch, next_sequence and runtime; authentication and session validation remain identical to existing routes. See the separate [runtime schema](../../schemas/runtime-1.schema.json).

runtime contains exactly schema_version:1, tick_rate_hz:60, max_catch_up_steps:4, simulation_tick, snapshot_sequence, overload_count, dropped_ticks, remainder_units (0..999999999), snapshot (the complete existing domain snapshot) and presentation. presentation has exactly enabled, ready, frame_count, world_revision and snapshot_sequence; counters start at zero and remain zero when disabled. The SDK validates these immutable models and world/session bindings. Private wrapper credentials/epochs never enter exported runtime evidence.

A fixed bundled worker receives credentials only on a private pipe and emits bounded typed receipts/snapshots. Explicit ready/first/finish barriers permit an independent observer to pause the real provider process. A fixed 30-second worker watchdog bounds abandoned barriers and pipe writes; active parent result reads have a three-second deadline and 64 KiB cap. The worker executes only the three checked-in SDK batches; no generated code, user-provided program or model output is evaluated.

## Frozen authoring fixture

World workshop, seed 7, eight slots. Three sequential transactions, IDs 018f7242-4387-7c98-a114-67787915a601, 018f7242-4387-7c98-a114-67787915a602 and 018f7242-4387-7c98-a114-67787915a603. Each uses typed CreateCube plus SetTransform with the corresponding temporary target.

| Name | Position (metres) | Scale | Rotation xyzw | Authoring revision |
| --- | --- | --- | --- | --- |
| left | [-2,0,0] | [0.75,1,1] | [0,0.6,0,0.8] | 1 |
| centre | [0,0.5,0] | [1,0.5,1] | [0,0.6,0,0.8] | 2 |
| right | [2,-0.5,0] | [0.5,1.5,1] | [0,0.6,0,0.8] | 3 |

Identities use seed-7 slots 1, 2 and 3 with generation 1. World revisions are 0 through 3. Independently preregistered OWOBJ001 byte lengths/hashes:

| Revision | Bytes | SHA-256 |
| --- | --- | --- |
| 0 | 40 | 85b1d8cd51e10301ffb78f9aba4116c09ea48b41a4cb9786fef0870bdd2ba780 |
| 1 | 195 | 4e7ee3306d01bc46098f63977d39d12022e4fcb4286d7bb08b2e6168536b0cd2 |
| 2 | 350 | a3c35848d0ac4512f9586958e0a935471ec039ef26eb6fe9000a9a4bb7d0ebec |
| 3 | 505 | 75d3387aa74edb50e43287305c5581d3c0cf0fef49df8b598ae19aa5d9b06918 |

Runtime ticks/publication/frame counters, delays and credentials are excluded from canonical authoring encoding. Arrival ticks may vary while accepted authoring state remains exact. The independent oracle owns its literal fixture/encoder and does not derive expected state from SDK responses or import worker fixture values.

## Verification and evidence

CPU: pinned local Windows build and all existing CTest regressions; new clock/oracle/self-tests; real subprocess barrier delay while independent authenticated runtime observations advance ticks and retain unchanged complete world state; release and finish the same provider; repeated runs compare exact authoring hashes. Cover incomplete network input without stalled ticks, dispatch cancellation/expiry, normal lifetime and shutdown, unsupported runtime route, strict SDK profile parsing, overload/debt recovery and actual callback execution. Compile a disposable native catch-up-cap 4 -> 5 mutant and require the unchanged independent callback-count test to fail, preserving the original candidate. Compile the actual Owner with a controlled clock alias in a disposable test copy; verify a pending typed create remains deferred during zero-due polls and every callback of a 100 ms overloaded advance, then applies exactly once on the next normal tick. A separate real request deadline cancels a frozen pending job before claim; later ticks cannot apply it. Real waits and a watchdog bound these synthetic-time tests. Inject a revision-one publication allocation failure in another disposable copy and require request closure without a receipt plus failure exit 4 within two seconds, before the five-second normal cutoff.

GPU: verify actual live revision 1 before releasing later provider batches, then capture revision 3 from the same native owner. Frame captures include actual color/ID/depth attachments, packet identity/geometry, tick/publication stamps and existing fenced lifecycle metadata. Independently check all three objects with fixed preregistered thresholds and actual Vulkan device/validation evidence. Bounded interactive inspection is supplementary. The existing render.world_cube regressions remain required after renderer changes.

The native report uses schema_version 1, example agents.mock_builder, status, runtime, device, validation, frames, window_events, lifecycle_events and errors. GPU phases are initial (revision 1) and final (revision 3); each frame includes simulation_tick and snapshot_sequence in addition to established renderer metadata. SDK reports own sanitized receipts and full snapshots. Neither includes private descriptors/epochs.

Fresh hosted Windows/Linux headless and optional-renderer build checks are required for the final candidate. Real Windows hardware is the authorized GPU priority. Attempt conservative Linux container verification where useful; software rendering is identified explicitly and unavailable physical Linux GPU remains not_run. Earlier PR-004 evidence cannot certify this changed runtime.

Retain clean candidate SHA/tree, exact commands, generic tool/device versions, source/executable hashes, independent assertions and allowlisted domain artifacts. Exclude personal identities/paths, host/device/process identifiers, credentials/epoch values or hashes, raw process/HTTP logs and executable/PDB files. Evidence is reviewed independently before publication. No capability claim follows from planning validation alone.

Run python tools/validate_plan.py after documentation/ledger changes. Completion requires actual required checks, independent implementation/evidence review, maintainer review/certification and a verified squash merge. Rollback is revert with fixtures retained; no persistent migration or release publication.
