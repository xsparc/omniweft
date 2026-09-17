#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Separate Python-client checks; fake replies test only client-side failure handling."""
import argparse
import copy
from unittest import mock
from dataclasses import FrozenInstanceError
import json
import http.client
from pathlib import Path
import socket
import sys
import threading
import time

from sdk_test_support import ROOT, CHECKS, Failure, encoded, exact, main_guard, require, strict_json
from sdk_oracle import UUID, identity, snapshot, transform


TOKEN, EPOCH = "a" * 64, "b" * 64


def reply(value=None, *, status=200, body=None, length=None, extra=()):
    payload = encoded(value) if body is None else body
    size = len(payload) if length is None else length
    fields = [("Content-Type", "application/json"), ("Content-Length", str(size)),
              ("Connection", "close"), ("Cache-Control", "no-store"), *extra]
    return ("HTTP/1.1 " + str(status) + " Test\r\n" +
            "".join(k + ": " + v + "\r\n" for k, v in fields) + "\r\n").encode() + payload


def capabilities(revision=0, sequence=1):
    return {"protocol_version": "0.1", "epoch": EPOCH, "next_sequence": sequence, "world_id": "workshop",
            "world_revision": revision, "operations": ["entity.create", "transform.set", "entity.delete"],
            "limits": {"max_header_bytes": 16384, "max_body_bytes": 1048576, "max_response_bytes": 4194304,
                       "max_operations": 256, "max_slots": 8, "request_timeout_ms": 1000, "session_ttl_ms": 30000},
            "admission": "synchronous", "durability": "volatile", "retry_mode": "resync_only"}


class Drip:
    def __init__(self, data):
        head, body = data.split(b"\r\n\r\n", 1)
        self.chunks = [head + b"\r\n\r\n"] + [body[i:i+32] for i in range(0, len(body), 32)]


class Peer:
    """Bounded independent response fixture; never a substitute for the native server oracle."""
    def __init__(self, responses):
        self.responses = list(responses)
        self.requests = []
        self.error = False
        self.stop = threading.Event()

    def __enter__(self):
        self.listener = socket.socket()
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(2)
        self.listener.settimeout(0.1)
        self.port = self.listener.getsockname()[1]
        def serve():
            try:
                while not self.stop.is_set():
                    try:
                        connection, unused = self.listener.accept()
                    except socket.timeout:
                        continue
                    with connection:
                        connection.settimeout(2)
                        raw = bytearray()
                        while b"\r\n\r\n" not in raw:
                            chunk = connection.recv(4096)
                            if not chunk:
                                raise Failure("SDK request fixture truncated header")
                            raw += chunk
                            if len(raw) > 16384:
                                raise Failure("SDK request fixture header overflow")
                        header, body = bytes(raw).split(b"\r\n\r\n", 1)
                        lines = header.split(b"\r\n")
                        headers = {}
                        for line in lines[1:]:
                            key, value = line.split(b":", 1)
                            headers[key.lower()] = value.strip()
                        size = int(headers.get(b"content-length", b"0"))
                        if not 0 <= size <= 1048576:
                            raise Failure("SDK request fixture body overflow")
                        while len(body) < size:
                            chunk = connection.recv(min(65536, size - len(body)))
                            if not chunk:
                                raise Failure("SDK request fixture truncated body")
                            body += chunk
                        method, route, unused = lines[0].decode().split(" ")
                        self.requests.append({"method": method, "route": route,
                                              "body": strict_json(body) if size else None})
                        index = len(self.requests) - 1
                        if index >= len(self.responses):
                            self.error = True
                            connection.sendall(reply({"unexpected": True}, status=400))
                            continue
                        route_expected, response = self.responses[index]
                        if route != route_expected:
                            self.error = True
                        outgoing = response(self.requests[-1]) if callable(response) else response
                        if isinstance(outgoing, Drip):
                            for chunk in outgoing.chunks:
                                try:
                                    connection.sendall(chunk)
                                except OSError:
                                    break
                                time.sleep(0.04)
                        else:
                            connection.sendall(outgoing)
            except Exception:
                self.error = True
        self.thread = threading.Thread(target=serve, daemon=True)
        self.thread.start()
        return self

    def __exit__(self, kind, unused, traceback):
        self.stop.set()
        self.thread.join(timeout=3)
        self.listener.close()
        if kind is None:
            require(not self.thread.is_alive() and not self.error, "independent reply fixture completed safely")
            require(len(self.requests) == len(self.responses), "SDK issued exactly the expected HTTP requests")


def secret_safe(error):
    require(not any(secret in str(error) or secret in repr(error) for secret in (TOKEN, EPOCH)),
            "SDK exception excludes credentials and epochs")


def malformed_replies(sdk):
    wrong = encoded(capabilities())
    corpus = [
        ("truncated", reply(body=b"{}", length=123)),
        ("non-json", reply(body=b"not-json-" + TOKEN.encode())),
        ("oversized", reply(body=b"", length=4194305)),
        ("duplicate-key", reply(body=wrong[:-1] + b',"epoch":"' + EPOCH.encode() + b'"}')),
        ("nonfinite", reply(body=wrong.replace(b'"world_revision":0', b'"world_revision":1e999'))),
        ("redirect", reply(body=b"{}", status=302, extra=[("Location", "http://forbidden.invalid/")])),
        ("duplicate-length", reply(body=b"{}", extra=[("Content-Length", "2")])),
        ("large-headers", reply(body=b"{}", extra=[("X-Filler", "x" * 16384)])),
    ]
    for label, raw in corpus:
        with Peer([("/v0/capabilities", raw)]) as peer:
            info = sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000)
            client = sdk.Client(info, timeout_ms=1000)
            require(TOKEN not in repr(info) and EPOCH not in repr(info) and TOKEN not in repr(client),
                    "SDK connection and client repr hide secrets")
            try:
                client.capabilities()
            except sdk.ProtocolError as error:
                secret_safe(error)
            else:
                raise Failure("SDK rejects " + label + " response")
    denied = {"protocol_version": "0.1", "status": "rejected",
              "error": {"code": "NOT_AUTHORIZED", "path": ""}}
    with Peer([("/v0/capabilities", reply(denied, status=401))]) as peer:
        client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000))
        try:
            client.capabilities()
        except sdk.ApiError as error:
            secret_safe(error)
            exact(error.code, "NOT_AUTHORIZED", "SDK typed authentication error")
        else:
            raise Failure("SDK surfaces typed authentication failure")
    for secret in (TOKEN, EPOCH):
        reflected = copy.deepcopy(denied)
        reflected["error"]["path"] = secret
        with Peer([("/v0/capabilities", reply(reflected, status=401))]) as peer:
            client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000))
            try:
                client.capabilities()
            except sdk.ProtocolError as error:
                secret_safe(error)
            else:
                raise Failure("SDK rejects secret echo in server errors")


def uncertain_reply(sdk):
    observed = {"protocol_version": "0.1", "epoch": EPOCH, "next_sequence": 2, "snapshot": snapshot(1)}
    def moved(request):
        body = request["body"]
        require(body["idempotency"]["sequence"] == 2, "explicit recovery uses observed next sequence")
        require(body["expected_world_revision"] == 1, "explicit recovery reconciles committed state")
        exact(body["operations"], [{"type": "transform.set", "target": identity(), **transform(True)}],
              "separate SDK serializes independently specified move")
        return reply({"protocol_version": "0.1", "epoch": EPOCH, "next_sequence": 3,
                      "receipt": {"status": "committed", "durability": "volatile",
                                  "transaction_id": body["transaction_id"], "world_revision": 2,
                                  "created": [], "errors": []}})
    script = [
        ("/v0/capabilities", reply(capabilities())),
        ("/v0/transactions", reply(body=b"{", length=999)),
        ("/v0/observe", reply(observed)),
        ("/v0/capabilities", reply(capabilities(1, 2))),
        ("/v0/observe", reply(observed)),
        ("/v0/transactions", moved),
    ]
    with Peer(script) as peer:
        client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000))
        client.capabilities()
        for attempt in range(2):
            try:
                client.create_cube("cube", expected_revision=0)
            except sdk.OutcomeUnknown as error:
                secret_safe(error)
            else:
                raise Failure("uncertain mutation cannot automatically succeed or retry")
        require(len(peer.requests) == 2, "lost response causes exactly one mutation request")
        exact(client.observe().to_dict(), snapshot(1), "uncertain mutation remains observable")
        try:
            client.create_cube("cube", expected_revision=0)
        except sdk.OutcomeUnknown:
            pass
        else:
            raise Failure("observe alone cannot clear uncertain mutation state")
        require(len(peer.requests) == 3, "poisoned mutation sends no request")
        exact(client.resync().to_dict(), snapshot(1), "explicit resync observes prior effect")
        result = client.move(sdk.EntityHandle(**identity()), sdk.Transform(**transform(True)), expected_revision=1)
        require(isinstance(result, sdk.Receipt) and result.world_revision == 2,
                "explicit post-resync action returns typed receipt")
        require(sum(r["route"] == "/v0/transactions" for r in peer.requests) == 2,
                "uncertain creation is not replayed under a new sequence")


def hostile_typed_values(sdk):
    try:
        sdk.Transform(position_m=(10 ** 400, 0, 0))
    except sdk.ProtocolError as error:
        secret_safe(error)
    else:
        raise Failure("huge integer transform raises safe ProtocolError")
    for field, secret in (("prefab", TOKEN), ("prefab", EPOCH)):
        state = snapshot(1)
        state["slots"][0]["entity"][field] = secret
        response = {"protocol_version": "0.1", "epoch": EPOCH, "next_sequence": 1, "snapshot": state}
        with Peer([("/v0/capabilities", reply(capabilities())), ("/v0/observe", reply(response))]) as peer:
            client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000))
            try:
                client.observe()
            except sdk.ProtocolError as error:
                secret_safe(error)
            else:
                raise Failure("SDK excludes secret echoes from typed results")
    for secret in (TOKEN, EPOCH):
        def echoed_receipt(request):
            return reply({"protocol_version": "0.1", "epoch": EPOCH, "next_sequence": 2,
                          "receipt": {"status": "rejected", "durability": "volatile",
                                      "transaction_id": request["body"]["transaction_id"], "world_revision": 0,
                                      "created": [], "errors": [{"code": "REVISION_CONFLICT",
                                      "path": "/expected_world_revision", "message": secret}]}})
        with Peer([("/v0/capabilities", reply(capabilities())), ("/v0/transactions", echoed_receipt)]) as peer:
            client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000))
            try:
                client.create_cube("cube", expected_revision=0)
            except sdk.OutcomeUnknown as error:
                secret_safe(error)
            else:
                raise Failure("SDK excludes secret echoes from receipts")


def overall_deadlines(sdk):
    with Peer([("/v0/capabilities", Drip(reply(capabilities())))]) as peer:
        client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", peer.port, TOKEN, EPOCH, 30000), timeout_ms=100)
        started = time.monotonic()
        try:
            client.capabilities()
        except sdk.ProtocolError:
            pass
        else:
            raise Failure("incremental reply cannot extend overall SDK deadline")
        require(time.monotonic() - started < 1.0, "drip response ends within bounded deadline margin")

    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(2)
    listener.settimeout(2)
    observed = []
    errors = []
    def server():
        try:
            with listener.accept()[0] as first:
                first.settimeout(2)
                raw = bytearray()
                while b"\r\n\r\n" not in raw:
                    chunk = first.recv(4096)
                    if not chunk:
                        raise Failure("connect deadline fixture negotiation")
                    raw += chunk
                first.sendall(reply(capabilities()))
            with listener.accept()[0] as second:
                second.settimeout(2)
                observed.append(second.recv(16384))
        except Exception:
            errors.append(True)
    thread = threading.Thread(target=server, daemon=True)
    thread.start()
    try:
        client = sdk.Client(sdk.ConnectionInfo("127.0.0.1", listener.getsockname()[1], TOKEN, EPOCH, 30000), timeout_ms=100)
        client.capabilities()
        actual_connect = socket.create_connection
        connections = []
        def delayed_connect(*args, **kwargs):
            connection = actual_connect(*args, **kwargs)
            connections.append(True)
            time.sleep(0.25)
            return connection
        with mock.patch("socket.create_connection", side_effect=delayed_connect):
            for unused in range(2):
                try:
                    client.create_cube("cube", expected_revision=0)
                except sdk.OutcomeUnknown:
                    pass
                else:
                    raise Failure("connect deadline makes mutation outcome uncertain")
        thread.join(timeout=3)
        require(not thread.is_alive() and not errors, "delayed connection fixture completed")
        exact(observed, [b""], "SDK sends no request after connect deadline")
        exact(len(connections), 1, "uncertain client does not reconnect for another mutation")
    finally:
        listener.close()


def actual_client(sdk, executable):
    with sdk.NativeSession(executable, session_ttl_ms=30000) as session:
        client = session.client()
        caps = client.capabilities()
        require(isinstance(caps, sdk.Capabilities), "SDK returns typed capabilities")
        initial = client.observe()
        require(isinstance(initial, sdk.Snapshot), "SDK returns typed snapshot")
        exact(initial.to_dict(), snapshot(0), "SDK real native initial state")
        created = client.create_cube("cube", expected_revision=0)
        require(isinstance(created, sdk.Receipt), "SDK returns typed creation receipt")
        handle = created.created[0].handle()
        require(isinstance(handle, sdk.EntityHandle), "SDK resolves typed durable handle")
        exact(handle.to_dict(), identity(), "SDK real native identity")
        moved = client.move(handle, sdk.Transform(**transform(True)), expected_revision=1)
        require(isinstance(moved, sdk.Receipt) and moved.world_revision == 2, "SDK real native move receipt")
        current = client.observe()
        exact(current.to_dict(), snapshot(2), "SDK real native moved state")
        try:
            current.world_revision = 99
        except (FrozenInstanceError, AttributeError):
            pass
        else:
            raise Failure("SDK snapshots must be immutable")
        exact(initial.to_dict(), snapshot(0), "older typed SDK snapshot is detached")
        entity = client.get_entity(handle)
        exact(entity.transform.to_dict(), transform(True), "SDK typed query finds moved entity")
        fresh = session.renew()
        try:
            client.create_cube("intruder", expected_revision=2)
        except sdk.ApiError as error:
            exact(error.code, "NOT_AUTHORIZED", "retired SDK client cannot mutate")
        else:
            raise Failure("retired SDK client must reject")
        exact(fresh.observe().to_dict(), snapshot(2), "SDK renewal preserves the same native world")
        recovered = fresh.create_cube("recovery", expected_revision=2)
        exact(recovered.created[0].handle().to_dict(), identity(2), "SDK rejection leaves allocation intact")
        exact(fresh.observe().to_dict(), snapshot(3), "SDK real native recovery state")


def retired_split_request(sdk, executable):
    # The production HTTP client sends headers/body separately. Wait for a real
    # rejection before sending the body so packet coalescing cannot hide teardown.
    with sdk.NativeSession(executable) as session:
        retired = session.client()
        retired.capabilities()
        fresh = session.renew()
        original = http.client.HTTPConnection.send
        header_seen, body_sent = [], []
        def split_send(connection, data):
            original(connection, data)
            if isinstance(data, bytes) and data.startswith(b"POST ") and data.endswith(b"\r\n\r\n"):
                require(connection.sock is not None, "split request has an actual socket")
                peek = connection.sock.recv(1, socket.MSG_PEEK)
                require(peek == b"H", "retired request response starts before body transmission")
                header_seen.append(True)
                # Give an immediate-close implementation time to deliver its FIN.
                time.sleep(0.05)
            elif header_seen:
                body_sent.append(True)
        try:
            with mock.patch("http.client.HTTPConnection.send", split_send):
                retired.create_cube("denied", expected_revision=0)
        except sdk.ApiError as error:
            exact((error.status, error.code), (401, "NOT_AUTHORIZED"), "split retired request retains typed rejection")
        except sdk.OutcomeUnknown:
            raise Failure("split retired request must retain its known rejection") from None
        else:
            raise Failure("split retired request must reject")
        exact((len(header_seen), len(body_sent)), (1, 1), "split body is transmitted successfully exactly once")
        exact(fresh.observe().to_dict(), snapshot(0), "split rejected body leaves complete world unchanged")
        recovered = fresh.create_cube("recovery", expected_revision=0)
        exact(recovered.created[0].handle().to_dict(), identity(), "split rejection consumes no identity or revision")
        exact(fresh.observe().to_dict(), snapshot(1), "split rejection permits fresh-client recovery")


def natural_runtime(sdk, executable):
    for unused in range(3):
        started = time.monotonic()
        try:
            with sdk.NativeSession(executable, max_runtime_ms=1000) as session:
                # Public client access checks actual child liveness. Wait for
                # natural completion before close can send a stop command.
                ended = False
                deadline = time.monotonic() + 4.0
                while time.monotonic() < deadline:
                    try:
                        session.client()
                    except sdk.ProtocolError:
                        ended = True
                        break
                    time.sleep(0.01)
                require(ended, "native normal lifetime ends before SDK close")
        except sdk.ProtocolError:
            raise Failure("natural native runtime expiry must close successfully") from None
        exact(session.returncode, 0, "ordinary native runtime expiry returns zero")
        require(time.monotonic() - started < 5.0, "ordinary native runtime cleanup has a finite failure bound")


def close_exit_race(sdk, executable):
    # Delay after a REAL poll observes a REAL child alive; no fake process/exit result.
    session = sdk.NativeSession(executable, max_runtime_ms=1000)
    session.start()
    original_poll = __import__("subprocess").Popen.poll
    observed_alive = []
    observed_exit = []
    def delayed_poll(process):
        result = original_poll(process)
        if result is None and not observed_alive:
            observed_alive.append(True)
            deadline = time.monotonic() + 4.0
            while original_poll(process) is None:
                if time.monotonic() >= deadline:
                    raise Failure("close race child reaches a bounded natural exit")
                time.sleep(0.01)
            observed_exit.append(process.returncode)
        return result
    started = time.monotonic()
    try:
        with mock.patch("subprocess.Popen.poll", delayed_poll):
            try:
                session.close()
            except sdk.ProtocolError:
                raise Failure("SDK close reconciles natural exit between poll and stop write") from None
        require(bool(observed_alive), "close race observes the actual child alive first")
        exact(observed_exit, [0], "close race child really exits before the stop write")
        exact(session.returncode, 0, "SDK close race preserves actual successful child exit")
        require(time.monotonic() - started < 5.0, "SDK close race has a finite cleanup bound")
    finally:
        try:
            session.close()
        except sdk.ProtocolError:
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--sdk-root", type=Path, default=ROOT / "sdk/python")
    parser.add_argument("--client-only", action="store_true")
    parser.add_argument("--lifecycle-case", choices=("idle", "close-race"))
    args = parser.parse_args()
    sys.path.insert(0, str(args.sdk_root.resolve(strict=True)))
    import omniweft_sdk as sdk
    require(Path(sdk.__file__).resolve().is_relative_to(args.sdk_root.resolve()), "tested SDK imports from the declared source")
    if args.lifecycle_case:
        (natural_runtime if args.lifecycle_case == "idle" else close_exit_race)(sdk, args.executable.resolve(strict=True))
    else:
        malformed_replies(sdk)
        uncertain_reply(sdk)
        hostile_typed_values(sdk)
        overall_deadlines(sdk)
        if not args.client_only:
            retired_split_request(sdk, args.executable.resolve(strict=True))
            actual_client(sdk, args.executable.resolve(strict=True))
            natural_runtime(sdk, args.executable.resolve(strict=True))
            close_exit_race(sdk, args.executable.resolve(strict=True))
    print(json.dumps({"status": "passed", "assertions": CHECKS}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main_guard(main))
