# sdk.move_cube

Status: **verified on Windows/Linux in the PR-005 candidate; maintainer merge pending**. Work item: **PR-005 — Authenticated local Python SDK**.

Dependencies: PR-003. Validation lanes: cpu.

## Behavior

Implement only the named behavior, its public contract, corresponding runnable example and directly required tests/docs. Split the item before execution if it cannot be reviewed as one behavior.

A separate Python process negotiates protocol 0.1 with the native loopback control host, creates a cube, moves it and observes typed receipts and detached snapshots. The host owns one workshop world and a single active session. Every mutation uses the existing typed command path, strict admission sequence and synchronous transaction boundary.

The deterministic fixture uses seed 7 and eight slots. Creation yields world revision 1 and entity `00000007-0000-4000-8000-000000000001`, generation 1. A typed transform sets position `[2.5,-1,3]`, identity rotation and scale `[1,2,1]` at world/entity authoring revision 2.

## Build and run

Use the pinned native build instructions in [platform.bootstrap](platform-bootstrap.md). The SDK uses Python's standard library and requires no provider key or package install.

```sh
cmake --preset windows-headless
cmake --build --preset windows-headless --parallel 2
python sdk/python/examples/move_cube.py --executable build/windows-headless/omniweft_control.exe --seed 7 --verify --output artifacts/sdk.move_cube
```

On Linux use the `linux-headless` preset and `build/linux-headless/omniweft_control` executable. The output directory must be fresh. This actual Python entry point replaces the earlier proposed native example command; Python remains outside the engine process.

For independent verification:

```sh
python tests/sdk_oracle.py --executable build/windows-headless/omniweft_control.exe --evidence-dir artifacts/sdk-oracle
python tests/sdk_mutation.py --build-dir build/windows-headless --evidence-dir artifacts/sdk-mutation
```

CTest includes the independent oracle and its regression tests. A passed example's self-check is not a substitute for this independent evidence.

## Pass criteria

Negotiate protocol, create/move/query an object through a separate Python process, and receive typed receipts.

## Rejection and recovery

Missing/wrong token, forbidden Origin, incompatible version and an expired session cannot mutate the world.

The oracle independently constructs expected full snapshots and canonical bytes. Missing/wrong tokens, forbidden Origin, foreign Host, incompatible protocol, expired sessions and malformed/oversized framing cannot mutate the same live world. Rejected pre-admission requests do not advance its sequence or consume identity slots. Admitted executor rejections do consume a sequence while preserving atomic world state.

After actual session expiry, the private host channel renews credentials while preserving the World. An authenticated observer then verifies the exact pre-attempt snapshot; a valid recovery create must use slot 2/generation 1 at revision 3. Old credentials remain invalid. The deliberate auth mutation changes a disposable native build; the unchanged oracle must detect unauthorized world mutation. Original source and executable must remain intact.

## API limits and privacy

[The frozen contract](../docs/execution/PR-005-PLAN.md) and [control wire schema](../schemas/control-0.1.schema.json) define three authenticated endpoints, private credential delivery, one request per connection, body/header/response limits and monotonic session/request deadlines. Browser origins, cookie authentication, proxy/remote binding, asynchronous execution and receipt replay are unsupported.

The SDK performs no automatic mutation retry. A lost or malformed response after submission yields `OutcomeUnknown` and disables further mutations until explicit observation/resynchronization. A new epoch never automatically replays an uncertain action.

[SDK usage](../sdk/python/README.md) explains the exclusively owned private pipes. Never retain launch descriptors, credentials/epochs, raw headers, process identifiers, workstation paths or exception dumps in public evidence. The [handoff](../docs/execution/PR-005-HANDOFF.md) records actual checks, reviewed artifacts, failures and remaining gates.

## Completion and compatibility

Record exact candidate SHA, commands, environment, seed, assertions, results, artifact hashes and limitations using docs/templates/EVIDENCE.md.

Revert the PR; preserve source fixtures and earlier save data. Any persistent schema change needs a versioned migration and recovery fixture before merge.

No command schema or persistent-format change. Receipt durability is volatile; restarting the host creates a new empty fixture world and invalidates old credentials. Rollback is a revert with fixtures preserved. Only current required checks, independent review and actual maintainer merge can mark PR-005 done.
