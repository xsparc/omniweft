# PR-005 execution plan

State: **in_progress**. Adopted scope: [authorization](AUTHORIZATION.md). Dependency PR-003 actually merged at `e994f054d1e13ba19f714f35696ec8a1bca221a9`. Base: PR-004's verified squash `ba80bb8392206a32810c8de989245e80cdfe055d`, tree `5baf923a0972fc099cfbc7c5082e5590b8ced98c`. Owner: Codex coordinator; branch `codex/pr-005-authenticated-sdk`. Roadmap authorization makes this bounded item eligible; it was promoted from proposed to ready and claimed on 2026-09-13.

## Outcome and scope

Implement [sdk.move_cube](../../examples/sdk-move_cube.md): a separate Python process negotiates protocol 0.1 and creates, moves and queries a native world through authenticated loopback HTTP. Only typed envelopes reach the existing single-owner transaction coordinator. Ship the SDK, native control host, independent oracle, negative/recovery cases and retained privacy-reviewed CPU evidence together.

This is the synchronous, single-world, single-active-session subset of [the control protocol](../AI_CONTROL_PROTOCOL.md) and [architecture](../ARCHITECTURE.md), preserving [ADR-0001](../adr/0001-runtime-stack.md) and [ADR-0002](../adr/0002-authoritative-transactions.md). No engine renderer/physics change, general HTTP server, remote binding/TLS, browser integration, provider execution, multi-principal/region policies, asynchronous scheduling, receipt retention/replay, filtered pagination, blobs/events or persistence. PR-007 owns richer policy/quotas; PR-010 owns filtered queries; PR-011 owns duplicate-receipt retry semantics. Command schema 0.1 and existing save/canonical meanings remain unchanged.

## Native host and secret channel

New `omniweft_control` executable owns one `World` and invokes `Coordinator::apply_at_boundary` only through authenticated admission on its owner thread. `ow_control` handles bounded framing/session state; core world/commands remain independent of transport and Python. Use private snapshot/apply callbacks rather than adding mutable world access to SDK or transport.

Bind IPv4 `127.0.0.1` to an OS-assigned port only. Generate independent 32-byte bearer token and epoch with OS CSPRNG (Windows BCryptGenRandom, Linux getrandom); fail closed on entropy failure. There is no token/epoch/host override, ambient cookie authentication or credential argument/environment/file. Require private stdin/stdout pipes. Stdout carries only bounded launch/renewal descriptors, never logs; stderr contains fixed non-reflective diagnostic codes.

The descriptor is at most 512 UTF-8 bytes plus newline, exactly:
`{"schema_version":1,"protocol_version":"0.1","host":"127.0.0.1","port":<1..65535>,"token":<64 lowercase hex>,"epoch":<independent 64 lowercase hex>,"session_ttl_ms":<integer>}`.

The launcher exclusively owns the private pipe. Fixed `renew\n` rotates token and epoch, resets next_sequence to 1, and preserves the same World. Old credentials immediately cease to authorize. Fixed `stop\n` or EOF stops the host. Process host commands between bounded network requests; no public session-issuance endpoint or test-only world mutation. Invalid/oversized private commands fail closed. At most 64 renewals per process; hard runtime fallback prevents detached survivors.

Host CLI accepts bounded nonsecret configuration: `--world workshop`, `--seed 7`, `--max-slots` (1..1024, default 1024), `--session-ttl-ms` (50..300000, default 30000), `--max-runtime-ms` (1000..600000, default 60000), `--max-requests` (1..4096, default 1024). Fixture uses eight slots. No host/address/token flags. The configured max_runtime_ms is the normal admission, network-I/O and private-renewal cutoff; ordinary bounded completion returns exit 0. A separate hard watchdog stays armed through cleanup and exits 4 only if the process remains stalled for a fixed additional 1000 ms. This cleanup allowance grants no further request or renewal time. SDK shutdown performs a bounded wait and accepts a failed stop-pipe write only when the real child is subsequently verified to have exited 0.

## Frozen HTTP subset

One HTTP/1.1 request per connection; always close after response. Support only exact origin-form routes below, no query/fragment, redirects, proxies or percent-decoding. This is a deliberately restricted Content-Length API, not a general HTTP/1.1 compliance claim. See [RFC 9112 framing](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3).

Bound request line including its CRLF to 1024 bytes, the complete request head (request line, header fields and final CRLF) to 16384 bytes, body to 1048576 bytes, response to 4194304 bytes, header count to 64, and entire connection read/write lifetime to 1000 ms using monotonic deadlines. One active connection and backlog eight. Reject bare-LF, control/NUL bytes, obsolete folding, whitespace before a header colon, duplicate header names case-insensitively, any Transfer-Encoding/Expect/Content-Encoding/Cookie, invalid/overflow Content-Length and incomplete messages. GET permits absent or zero Content-Length; POST requires exactly one digits-only Content-Length and Content-Type application/json. Unknown ordinary well-formed headers may be ignored.

Every endpoint requires exactly one `Authorization: Bearer <token>`, exact `Host: 127.0.0.1:<actual-port>` and `X-Omniweft-Protocol: 0.1`. Reject every Origin header including null, and reject foreign hosts. Authenticate before exposing world/epoch/revision. Check expiry again immediately before synchronous transaction admission after receiving/validating the body. Whole-body size/depth/duplicate-key/UTF-8/finite-number validation remains mandatory. Never reflect credentials or arbitrary request content into diagnostics.

All responses use bounded Content-Length, application/json, Cache-Control:no-store and Connection:close, with no CORS headers. Malformed/incomplete transport may close without response; it never reaches admission. A complete first pipelined request may execute once; subsequent bytes are never executed.

| Route | Request | Successful response |
| --- | --- | --- |
| GET /v0/capabilities | No body | capabilities below |
| POST /v0/observe | Exactly `{"protocol_version":"0.1","world_id":<granted world>}` | `{"protocol_version":"0.1","epoch":<epoch>,"next_sequence":<uint64>,"snapshot":<detached Snapshot>}` |
| POST /v0/transactions | Existing strict Envelope 0.1 unchanged | `{"protocol_version":"0.1","epoch":<epoch>,"next_sequence":<uint64>,"receipt":<existing Receipt>}` |

Capabilities has exactly protocol_version, epoch, next_sequence, world_id, world_revision, operations (entity.create, transform.set, entity.delete), limits, admission:"synchronous", durability:"volatile", retry_mode:"resync_only". Limits has max_header_bytes, max_body_bytes, max_response_bytes, max_operations (256), max_slots, request_timeout_ms and session_ttl_ms.

Snapshot/Receipt wire fields match the existing typed domain serialization in objects.atomic: snapshot format_version, world_id, seed, max_slots, world_revision and complete slots (entity_uuid, generation, retired, entity); entity is null or prefab, authoring_revision and transform with position_m/rotation_xyzw/scale. Receipt contains status, durability, transaction_id, world_revision, created and errors; errors include code/path/message and optional operation_index.

Pre-admission errors have exactly `{"protocol_version":"0.1","status":"rejected","error":{"code":<fixed code>,"path":<schema path>}}`. Codes/statuses: 401 NOT_AUTHORIZED or SESSION_EXPIRED; 403 NOT_AUTHORIZED for Origin/Host; 400 INVALID_SCHEMA or UNSUPPORTED_VERSION; 413 BUDGET_EXCEEDED; 409 REQUIRES_RESYNC; 404 NOT_FOUND; 405 UNSUPPORTED_OPERATION. Authorized wrong-world requests reject without exposing another world. Admitted executor outcomes, including typed rejection, return HTTP 200 and the receipt wrapper.

## Admission and client behavior

One host-issued active epoch, next sequence starting at 1; no client principal/grant field. Only exact epoch and next sequence are admitted. Every repeat, gap, wrong/retired epoch or exhausted counter requires resync; no replay is treated as a fresh mutation. Invalid auth/framing/schema/version/world/epoch/sequence does not consume sequence. Once admitted, consume exactly one sequence even if the existing executor rejects revision/resource/operation preconditions. Retain no receipt cache in this slice. Host bounds override client budgets. `apply_at.next_tick` means the next immediate synchronous host boundary; no queued tick/deadline execution or physics tick advancement is claimed.

The standard-library Python SDK provides typed immutable handles, transforms, capabilities, snapshots and receipts. NativeSession owns safe subprocess argument arrays and the secret pipes; direct HTTPConnection uses validated numeric loopback/port, without environment proxies or redirects. Serialize admissions with a lock. Do not automatically retry a mutation after transport uncertainty or advance under a guessed new sequence; require explicit observe/capabilities resynchronization first. Renewing a session is an explicit host action and never replays an uncertain mutation. Tokens/epochs must be excluded from repr/errors and public evidence; credential hashes are not evidence.

The runnable example is a Python SDK entry point that launches the separate native host. Replace the earlier proposed C++ command with its actual Python command when implemented; no embedded Python or generated code execution is introduced.

## Frozen fixture, verification and ownership

Fixture: world workshop, seed 7, eight slots. Create cube at revision 1; move to position [2.5,-1,3], rotation [0,0,0,1], scale [1,2,1] at revision 2. UUID is `00000007-0000-4000-8000-000000000001`, generation 1, entity authoring revision 2. Independent oracle derives exact expected snapshots/canonical bytes without importing server serialization or deriving expectations from responses.

Negative requests must leave the same server's full snapshot and allocator state unchanged. Cover missing/wrong tokens, duplicate auth, any Origin, foreign Host, incompatible header/body versions, expiry, retired token/epoch, sequence gaps/repeats, invalid schema/framing/limits, stale revision/handle and bounded partial-input timeouts. Renew through the production private host channel after expiry, then observe the same World; a valid recovery create must use slot 2/generation 1 at revision 3. Cover truncated/non-JSON/oversized replies and secret-safe SDK errors. Mutation proof disables a real native auth guard in a disposable copy; unchanged independent oracle must observe unauthorized state change before failing on status, and original source/executable must be preserved.

Coordinator owns docs, schemas, SDK implementation, CMake/CI integration, public exporter and ledger. Native helper owns new include/omniweft/control.hpp and src/control* files only, in an isolated worktree. Test helper independently owns new tests/sdk* files only, in another worktree. Architecture helper reviews contract, trust boundary and final immutable implementation/evidence. No helper commits or pushes; root integrates reviewed bytes.

Required lanes: actual local Windows CPU build/CTest, fresh hosted Windows/Linux CPU and optional-build regression CI, SDK oracle, deliberate native auth mutation and planning/public-export checks. No new GPU functionality; PR-004's physical evidence stays bound to its original runtime. Retain clean candidate SHA/tree, executable/client hashes, sanitized commands, assertions, snapshots/receipts and payload hashes. Publish no private descriptors, credentials/epochs, raw headers/logs, personal paths or device/process identifiers.

No new runtime third-party dependency is planned: existing pinned nlohmann JSON, C++20, OS socket/CSPRNG APIs and Python standard library. No global installation, host trust change or paid resource. Rollback is revert with fixtures retained; no persistent migration or package publication. Final completion requires independent source/security/evidence review, current checks, maintainer certification and actual merge. Unrun checks remain not_run until execution; this plan proves no functionality.
