#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""SDK boundary regressions; synthetic HTTP peers do not certify native behavior."""
from __future__ import annotations

import argparse
import copy
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
from pathlib import Path
import subprocess
import sys
import threading
import time
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SDK = None
EXECUTABLE = None
TOKEN, EPOCH = "a" * 64, "b" * 64  # Synthetic credentials, never a host descriptor.


def snapshot(revision=0):
    slots = []
    if revision:
        slots = [{"entity_uuid": "00000007-0000-4000-8000-000000000001",
                  "generation": 1, "retired": False,
                  "entity": {"prefab": "builtin.unit_cube", "authoring_revision": 1,
                             "transform": {"position_m": [-2.0, 0.0, 0.0],
                                           "rotation_xyzw": [0.0, 0.6, 0.0, 0.8],
                                           "scale": [0.75, 1.0, 1.0]}}}]
    return {"format_version": 1, "world_id": "workshop", "seed": 7,
            "max_slots": 8, "world_revision": revision, "slots": slots}


def runtime():
    return {"schema_version": 1, "tick_rate_hz": 60, "max_catch_up_steps": 4,
            "simulation_tick": 1, "snapshot_sequence": 2, "overload_count": 0,
            "dropped_ticks": 0, "remainder_units": 20, "snapshot": snapshot(1),
            "presentation": {"enabled": True, "ready": True, "frame_count": 1,
                             "world_revision": 1, "snapshot_sequence": 2}}


class SyntheticSession:
    """Private descriptor fixture, not a replacement for a native session."""
    def __init__(self, port=10000):
        self.port = port

    def _provider_descriptor(self):
        return json.dumps({"schema_version": 1, "protocol_version": "0.1",
                           "host": "127.0.0.1", "port": self.port, "token": TOKEN,
                           "epoch": EPOCH, "session_ttl_ms": 300000}).encode()


class ReadOnlyPeer:
    """Only capabilities/empty observation for the real worker's barrier test."""
    def __enter__(self):
        owner = self
        self.routes = []
        self.failed = False

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *unused):
                pass

            def handle_request(self):
                self.connection.settimeout(1)
                owner.routes.append((self.command, self.path))
                if self.headers.get("Authorization") != "Bearer " + TOKEN:
                    owner.failed = True
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 <= length <= 1024:
                    owner.failed = True
                    return
                self.rfile.read(length)
                if (self.command, self.path) == ("GET", "/v0/capabilities"):
                    value = {"protocol_version": "0.1", "epoch": EPOCH,
                             "next_sequence": 1, "world_id": "workshop", "world_revision": 0,
                             "operations": ["entity.create", "transform.set", "entity.delete"],
                             "limits": {"max_header_bytes": 16384, "max_body_bytes": 1048576,
                                        "max_response_bytes": 4194304, "max_operations": 256,
                                        "max_slots": 8, "request_timeout_ms": 1000,
                                        "session_ttl_ms": 300000},
                             "admission": "synchronous", "durability": "volatile",
                             "retry_mode": "resync_only"}
                elif (self.command, self.path) == ("POST", "/v0/observe"):
                    value = {"protocol_version": "0.1", "epoch": EPOCH,
                             "next_sequence": 1, "snapshot": snapshot()}
                else:
                    owner.failed = True
                    value = {}
                data = json.dumps(value).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Cache-Control", "no-store")
                self.send_header("Connection", "close")
                self.end_headers()
                self.wfile.write(data)
                self.close_connection = True

            do_GET = handle_request
            do_POST = handle_request

        class QuietServer(HTTPServer):
            def handle_error(self, unused_request, unused_address):
                owner.failed = True

        self.server = QuietServer(("127.0.0.1", 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever,
                                       kwargs={"poll_interval": 0.05}, daemon=True)
        self.thread.start()
        return self

    @property
    def port(self):
        return self.server.server_port

    def __exit__(self, *unused):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)


class RuntimeModelTests(unittest.TestCase):
    def test_runtime_requires_consistent_stamps_and_integer_tokens(self):
        valid = runtime()
        self.assertEqual(SDK.RuntimeStatus.from_dict(valid).to_dict(), valid)
        cases = []
        for key, value in (("schema_version", True), ("simulation_tick", 1.0),
                           ("snapshot_sequence", 1), ("remainder_units", 1000000000),
                           ("overload_count", 1), ("tick_rate_hz", 59)):
            changed = copy.deepcopy(valid)
            changed[key] = value
            cases.append(changed)
        for key, value in (("world_revision", 2), ("snapshot_sequence", 3),
                           ("frame_count", True)):
            changed = copy.deepcopy(valid)
            changed["presentation"][key] = value
            cases.append(changed)
        changed = copy.deepcopy(valid)
        changed["unexpected"] = 1
        cases.append(changed)
        for index, value in enumerate(cases):
            with self.subTest(case=index), self.assertRaises(SDK.ProtocolError):
                SDK.RuntimeStatus.from_dict(value)

    def test_unpresented_revision_cannot_be_reported_as_valid(self):
        for ready in (False, True):
            value = {"enabled": True, "ready": ready, "frame_count": 0,
                     "world_revision": 1, "snapshot_sequence": 0}
            with self.subTest(ready=ready), self.assertRaises(SDK.ProtocolError):
                SDK.PresentationStatus.from_dict(value)
        for enabled, ready in ((False, False), (True, False), (True, True)):
            value = {"enabled": enabled, "ready": ready, "frame_count": 0,
                     "world_revision": 0, "snapshot_sequence": 0}
            self.assertEqual(SDK.PresentationStatus.from_dict(value).world_revision, 0)

    def test_temporary_target_uses_the_existing_wire_reference(self):
        operation = SDK.SetTransform(SDK.TemporaryTarget("cube:one"), SDK.Transform())
        self.assertEqual(operation.to_dict(), {"type": "transform.set",
                         "target": {"temporary_id": "cube:one"},
                         "position_m": [0.0, 0.0, 0.0],
                         "rotation_xyzw": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 1.0, 1.0]})


class WorkerBoundaryTests(unittest.TestCase):
    def test_decoded_worker_credential_echo_is_rejected_before_public_models(self):
        # A real subprocess injects a valid-shaped ready result. It performs no
        # native work; the target here is the parent's provider-result decoder.
        source = """import json,sys
d=json.loads(sys.stdin.buffer.readline(514))
s={'format_version':1,'world_id':d[sys.argv[1]],'seed':7,'max_slots':8,'world_revision':0,'slots':[]}
p=json.dumps({'event':'ready','receipts':[],'snapshots':[s]})
p=p.replace(d[sys.argv[1]], ''.join('\\\\u%04x'%ord(c) for c in d[sys.argv[1]]))
sys.stdout.write(p+'\\n');sys.stdout.flush()
sys.stdin.buffer.readline(9)
"""
        real_popen = subprocess.Popen
        for field in ("token", "epoch"):
            children = []
            received = []

            class RecordedPipe:
                def __init__(self, stream):
                    self.stream = stream

                def readline(self, limit):
                    data = self.stream.readline(limit)
                    received.append(data)
                    return data

                def close(self):
                    self.stream.close()

            def fixture_process(unused_command, **kwargs):
                process = real_popen([sys.executable, "-I", "-c", source, field], **kwargs)
                process.stdout = RecordedPipe(process.stdout)
                children.append(process)
                return process

            builder = SDK.ScriptedBuilder(SyntheticSession())
            try:
                with self.subTest(field=field), mock.patch("subprocess.Popen", fixture_process):
                    with self.assertRaises(SDK.ProtocolError) as caught:
                        builder.start()
                self.assertNotIn(TOKEN, repr(caught.exception))
                self.assertNotIn(EPOCH, repr(caught.exception))
                self.assertNotIn(TOKEN, repr(builder))
                self.assertIsNone(builder.initial)
                self.assertEqual(len(received), 1)
                decoded = json.loads(received[0])
                secret = TOKEN if field == "token" else EPOCH
                expected = {"event": "ready", "receipts": [],
                            "snapshots": [{**snapshot(), "world_id": secret}]}
                self.assertEqual(decoded, expected)
                self.assertEqual(SDK.Snapshot.from_dict(decoded["snapshots"][0]).world_id, secret)
                self.assertNotIn(secret.encode(), received[0])
                self.assertEqual(len(children), 1)
                self.assertIsNotNone(children[0].poll())
            finally:
                builder.close()

    def test_real_worker_expires_while_parent_keeps_the_barrier_open(self):
        # The real fixed worker remains alive at its production build barrier.
        # The peer is read-only: this test certifies worker lifetime, not native state.
        real_popen = subprocess.Popen
        children = []

        def capture(command, **kwargs):
            process = real_popen(command, **kwargs)
            children.append(process)
            return process

        with ReadOnlyPeer() as peer:
            builder = SDK.ScriptedBuilder(SyntheticSession(peer.port))
            started = time.monotonic()
            try:
                with mock.patch("subprocess.Popen", capture):
                    builder.start()
                self.assertEqual(builder.initial.snapshots[0].to_dict(), snapshot())
                self.assertEqual(len(children), 1)
                self.assertIsNone(children[0].poll())
                code = children[0].wait(timeout=35)
                elapsed = time.monotonic() - started
                self.assertEqual(code, 4)
                self.assertGreaterEqual(elapsed, 28.0)
                self.assertLess(elapsed, 35.0)
                with self.assertRaises(SDK.ProtocolError):
                    builder.close()
                self.assertFalse(peer.failed)
                self.assertEqual(peer.routes, [("GET", "/v0/capabilities"), ("POST", "/v0/observe")])
            finally:
                try:
                    builder.close()
                except SDK.ProtocolError:
                    pass

    def test_actual_committed_mutation_with_lost_result_is_not_replayed(self):
        if EXECUTABLE is None:
            self.skipTest("native executable was not supplied; no native certification claimed")
        real_popen = subprocess.Popen
        children = []

        def capture(command, **kwargs):
            process = real_popen(command, **kwargs)
            children.append(process)
            return process

        class CorruptResult:
            def __init__(self, stream):
                self.stream = stream
                self.complete_result_seen = False

            def readline(self, limit):
                raw = self.stream.readline(limit)
                value = json.loads(raw)
                self.complete_result_seen = value.get("event") == "first"
                # Corrupt only the transport result AFTER the real worker has
                # completed its public-SDK mutation and observation.
                return b'{"event":"first"\n'

            def close(self):
                self.stream.close()

        with SDK.NativeSession(EXECUTABLE) as session:
            observer = session.client()
            builder = SDK.ScriptedBuilder(session)
            try:
                with mock.patch("subprocess.Popen", capture):
                    builder.start()
                self.assertEqual(len(children), 1)
                pipe = CorruptResult(children[0].stdout)
                children[0].stdout = pipe
                with self.assertRaises(SDK.OutcomeUnknown):
                    builder.create_first()
                self.assertTrue(pipe.complete_result_seen)
                self.assertEqual(observer.observe().to_dict(), snapshot(1))
                for action in (builder.create_first, builder.finish):
                    with self.assertRaises(SDK.ProtocolError):
                        action()
                self.assertEqual(observer.observe().to_dict(), snapshot(1))
                self.assertEqual(len(children), 1)
                self.assertIsNotNone(children[0].poll())
            finally:
                builder.close()


class PrivateResult(unittest.TestResult):
    """Publish fixed test IDs/outcomes without traceback paths or pipe contents."""
    def __init__(self):
        super().__init__()
        self.failed_ids = set()

    def addFailure(self, test, error):
        super().addFailure(test, error)
        self.failed_ids.add(test.id())

    def addError(self, test, error):
        super().addError(test, error)
        self.failed_ids.add(test.id())

    def addSubTest(self, test, subtest, error):
        super().addSubTest(test, subtest, error)
        if error is not None:
            self.failed_ids.add(test.id())


def main():
    global SDK, EXECUTABLE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", type=Path, default=ROOT / "sdk/python")
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--case", choices=("all", "models", "privacy", "lifetime", "mutation"), default="all")
    args = parser.parse_args()
    try:
        sdk_root = args.sdk_root.resolve(strict=True)
        sys.path.insert(0, str(sdk_root))
        import omniweft_sdk
        SDK = omniweft_sdk
        if not Path(SDK.__file__).resolve().is_relative_to(sdk_root):
            raise RuntimeError()
        if args.executable is not None:
            EXECUTABLE = args.executable.resolve(strict=True)
        if args.case in ("all", "mutation") and EXECUTABLE is None:
            raise RuntimeError()
        selections = {"models": "RuntimeModelTests",
                      "privacy": "WorkerBoundaryTests.test_decoded_worker_credential_echo_is_rejected_before_public_models",
                      "lifetime": "WorkerBoundaryTests.test_real_worker_expires_while_parent_keeps_the_barrier_open",
                      "mutation": "WorkerBoundaryTests.test_actual_committed_mutation_with_lost_result_is_not_replayed"}
        loader = unittest.TestLoader()
        suite = (loader.loadTestsFromModule(sys.modules[__name__]) if args.case == "all"
                 else loader.loadTestsFromName(selections[args.case], sys.modules[__name__]))
        result = PrivateResult()
        suite.run(result)
        passed = result.wasSuccessful() and (args.case != "mutation" or not result.skipped)
        print(json.dumps({"status": "passed" if passed else "failed", "tests": result.testsRun,
                          "skipped": len(result.skipped), "failed_tests": sorted(result.failed_ids)}))
        return 0 if passed else 1
    except Exception:
        print('{"status":"failed","error":"SDK_BOUNDARY_TEST_SETUP_FAILED"}')
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
