# Omniweft local Python SDK

The PR-005 candidate connects a separate Python process to a native `omniweft_control` host. It implements the bounded synchronous control profile in [the slice plan](../../docs/execution/PR-005-PLAN.md) and [wire schema](../../schemas/control-0.1.schema.json). Python 3.12 is used by the pinned verification lanes; no third-party Python package is required.

## Run the example

From the repository root, build the native host and run:

```sh
python sdk/python/examples/move_cube.py --executable build/windows-headless/omniweft_control.exe --seed 7 --verify --output artifacts/sdk.move_cube
```

Use `build/linux-headless/omniweft_control` for Linux. See [the independent example contract](../../examples/sdk-move_cube.md) for oracle and mutation-proof commands.

For application imports, add this `sdk/python` directory to the application's Python module search path. An installation/release package is not published by this slice.

```python
from omniweft_sdk import NativeSession, Transform

with NativeSession("build/windows-headless/omniweft_control.exe") as host:
    client = host.client()
    capabilities = client.capabilities()
    before = client.observe()
    created = client.create_cube("cube", before.world_revision)
    if created.status != "committed":
        raise RuntimeError("creation rejected")
    handle = created.created[0].handle()
    moved = client.move(handle, Transform(position_m=(2.5, -1, 3)),
                        expected_revision=created.world_revision)
    current = client.get_entity(handle)
```

`Snapshot`, `Receipt`, `EntityHandle` and `Transform` are immutable typed values. `to_dict()` on snapshots/receipts produces their domain wire fields. A receipt with `status="rejected"` is an admitted but unsuccessful atomic transaction, distinct from a pre-admission `ApiError`.

## Session ownership and failures

`NativeSession` explicitly launches the trusted native executable with argument arrays and private stdin/stdout pipes. The OS chooses a numeric IPv4 loopback port. Fresh token and epoch values travel only through the private descriptor channel; the SDK does not discover proxies or follow redirects. This is a local execution boundary, not an OS sandbox for arbitrary supplied executables.

`host.renew()` returns a new `Client` after rotating the sole active credentials while keeping the same native World. Existing clients keep their retired credentials and are denied. `close()`, context-manager exit or owner pipe EOF stops the host; bounded termination handles stalled children. Host configuration can reduce session TTL, slot capacity, request count and normal runtime within the advertised bounds. Natural runtime expiry completes successfully; request admission, network I/O and renewals stop at that deadline. An emergency watchdog allows a fixed additional 1,000 ms for cleanup before failing a stalled process. The SDK verifies the actual child exit when normal completion races its stop-pipe write.

`ApiError` exposes a fixed structured rejection code/status. `ProtocolError` reports malformed or unavailable control transport without echoing received content. `OutcomeUnknown` means a mutation might have committed before its response was lost or invalid; do not replay it. Inspect current state, then deliberately call `client.resync()` before a subsequent mutation. The SDK never substitutes a fresh sequence or epoch to retry automatically.

Capabilities declare `retry_mode="resync_only"`. Duplicate receipts, concurrent-client retry coordination and retention/compaction belong to PR-011. Scoped multi-principal grants/fair quotas, filters/pagination, blobs/events, remote binding/TLS and persistence are also outside this slice.

## Evidence hygiene

Credential descriptors and live client metadata are private control data, not log material. Token and epoch fields are suppressed from representations; decoded responses echoing those secrets are rejected before public models are constructed. Publish only the independently reviewed fixed fixture/receipt/snapshot evidence, never raw HTTP or process streams, local paths, IDs or credential hashes.
