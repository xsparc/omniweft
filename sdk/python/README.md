# Omniweft local Python SDK

The merged PR-005 SDK connects a separate Python process to a native `omniweft_control` host. It implements the bounded synchronous control profile in [the slice plan](../../docs/execution/PR-005-PLAN.md) and [wire schema](../../schemas/control-0.1.schema.json). Python 3.12 is used by the pinned verification lanes; no third-party Python package is required.

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


## Fixed-step runtime and scripted builder

The PR-006 candidate adds an opt-in native omniweft_agents host. Its world advances fixed 60 Hz steps on a dedicated owner thread while gateway I/O and the fixed provider process run separately. The existing omniweft_control executable and control 0.1 response shapes remain supported.

~~~python
from omniweft_sdk import NativeSession, ScriptedBuilder

with NativeSession("build/windows-headless/omniweft_agents") as host:
    observer = host.client()
    with ScriptedBuilder(host) as provider:
        initial = provider.initial.snapshots[0]
        first = provider.create_first()
        progress = observer.runtime()
        completed = provider.finish()
~~~

On Windows use the .exe filename. The fixed builder creates the seed-7 three-cube fixture; its ready and first barriers permit deliberate provider delay while another client observes simulation progress. It is a checked-in provider implementation and does not evaluate model output or arbitrary code. A separate 30-second worker lifetime bounds abandoned private barriers and pipe writes. Each active result read is bounded to three seconds and 64 KiB.

Client.runtime() returns immutable profile-1 metadata and a complete detached snapshot. Authoring revision, executed tick and presentation source stamps are distinct. The clock executes at most four catch-up steps per advance, reports overload and dropped whole debt, and retains the fractional remainder. This does not establish physics determinism. The [runtime schema](../../schemas/runtime-1.schema.json) is separate from the unchanged legacy route schemas; older hosts reject the new route.

Typed TemporaryTarget references allow CreateCube and SetTransform in one public SDK transaction. The same native atomic coordinator validates and applies these operations. A ScriptedBuilder whose build/finish result is lost becomes unusable and raises OutcomeUnknown; inspect the world and explicitly resynchronize before deciding on further mutations. The SDK never replays the uncertain builder batch.

The [runnable example](../../examples/agents-mock_builder.md) includes headless, real GPU and bounded interactive modes. NativeSession accepts optional gpu, interactive and output parameters for the new agents executable. Credentials stay on private pipes, and public progress objects contain only reviewed domain data. Verification state and limitations are recorded in the [PR-006 handoff](../../docs/execution/PR-006-HANDOFF.md).

## Scoped policy fixture

PR-007's opt-in PolicySession launches omniweft_policy with two independent private credentials. Obtain session.client("west") or session.client("east"); existing typed transact/move/delete operations retain their receipt contract. Create and place a cube atomically with CreateCube and a TemporaryTarget transform because an unplaced cube cannot survive commit.

PolicyClient.policy_status() returns an immutable version 1 profile with host-issued write bounds, an explicit whole-world read grant, limits and usage. These effective limits are separate from unchanged legacy global capabilities. PolicyClient.resync_policy() recovers revision/sequence after an uncertain outcome or oversized observation without retrying mutations. Renewal rotates both principals' credentials and retains world/quota sponsorship. Default NativeSession behavior remains unchanged.

See [policy.denied_edits](../../examples/policy-denied_edits.md) for accounting, response-size limits, request recovery and commands.
