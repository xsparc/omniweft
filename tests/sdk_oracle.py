#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Real native loopback + independent raw HTTP + separate Python SDK process oracle."""
import argparse
import copy
import json
import re
import struct
import sys
import tempfile
import time
from pathlib import Path

from sdk_test_support import (CHECKS, Evidence, Failure, Host, ROOT, canonical, encoded, exact,
                              exchange, main_guard, request_bytes, require, strict_json)

UUID = "00000007-0000-4000-8000-000000000001"
TX = [f"018f7242-4387-7c98-a114-67787915a{n}" for n in (501, 502, 503, 504, 505)]


def identity(number=1, generation=1):
    return {"world_id": "workshop", "entity_uuid": f"00000007-0000-4000-8000-{number:012x}", "generation": generation}


def transform(moved=False):
    return {"position_m": [2.5, -1.0, 3.0] if moved else [0.0, 0.0, 0.0],
            "rotation_xyzw": [0.0, 0.0, 0.0, 1.0], "scale": [1.0, 2.0, 1.0] if moved else [1.0, 1.0, 1.0]}


def snapshot(revision):
    slots = []
    if revision:
        slots.append({"entity_uuid": UUID, "generation": 1, "retired": False,
                      "entity": {"prefab": "builtin.unit_cube", "authoring_revision": min(revision, 2),
                                 "transform": transform(revision >= 2)}})
    if revision == 3:
        slots.append({"entity_uuid": identity(2)["entity_uuid"], "generation": 1, "retired": False,
                      "entity": {"prefab": "builtin.unit_cube", "authoring_revision": 3, "transform": transform()}})
    return {"format_version": 1, "world_id": "workshop", "seed": 7, "max_slots": 8,
            "world_revision": revision, "slots": slots}


def create(name="cube"):
    return {"type": "entity.create", "temporary_id": name, "prefab": "builtin.unit_cube"}


def move(target=None):
    return {"type": "transform.set", "target": identity() if target is None else target, **transform(True)}


def envelope(epoch, sequence, revision, operations=None, tx=2):
    return {"protocol_version": "0.1", "world_id": "workshop", "transaction_id": TX[tx],
            "idempotency": {"epoch": epoch, "sequence": sequence}, "expected_world_revision": revision,
            "apply_at": {"mode": "next_tick", "expires_after_ticks": 120},
            "budget": {"max_operations": len(operations or [create("intruder")]), "max_blob_bytes": 0},
            "operations": operations or [create("intruder")]}


def receipt(revision, tx, created=()):
    return {"status": "committed", "durability": "volatile", "transaction_id": TX[tx],
            "world_revision": revision, "created": list(created), "errors": []}


def error_response(response, status, code, path=None):
    require(response is not None, "structured rejection response exists")
    exact(response[0], status, "rejection HTTP status")
    value = response[1]
    require(type(value) is dict and value.keys() == {"protocol_version", "status", "error"}, "rejection exact fields")
    exact(value["protocol_version"], "0.1", "rejection protocol")
    exact(value["status"], "rejected", "rejection status")
    require(type(value["error"]) is dict and value["error"].keys() == {"code", "path"}, "rejection error fields")
    exact(value["error"]["code"], code, "rejection code")
    require(type(value["error"]["path"]) is str and re.fullmatch(r"(?:/[a-zA-Z0-9_]+)*", value["error"]["path"]) is not None,
            "rejection path contains only bounded schema components")
    if path is not None:
        exact(value["error"]["path"], path, "rejection path")
    return value


class Scenario:
    def __init__(self, host, evidence, prefix):
        self.host, self.evidence, self.prefix = host, evidence, prefix
        self.expected = snapshot(0)
        self.sequence = 1
        self.results = []

    def call(self, method, path, body=None, **kwargs):
        return exchange(self.host.descriptor, request_bytes(self.host.descriptor, method, path,
                        None if body is None else encoded(body)), **kwargs)

    def observe(self, label):
        response = self.call("POST", "/v0/observe", {"protocol_version": "0.1", "world_id": "workshop"})
        require(response is not None and response[0] == 200, label + ": authorized observation succeeds")
        value = response[1]
        require(type(value) is dict and value.keys() == {"protocol_version", "epoch", "next_sequence", "snapshot"},
                label + ": observation fields")
        exact(value["protocol_version"], "0.1", label + ": observation version")
        exact(value["epoch"], self.host.descriptor["epoch"], label + ": observation current epoch")
        exact(value["snapshot"], self.expected, label + ": complete world unchanged")
        exact(value["next_sequence"], self.sequence, label + ": next sequence unchanged")
        actual_bytes = canonical(value["snapshot"])
        require(actual_bytes == canonical(self.expected), label + ": full canonical state")
        name = self.prefix + "/" + label
        self.evidence.retain_json("snapshots/" + name + ".json", value["snapshot"])
        self.evidence.retain("canonical/" + name + ".bin", actual_bytes)
        return value

    def capabilities(self, label="capabilities"):
        response = self.call("GET", "/v0/capabilities")
        require(response is not None and response[0] == 200, label + ": negotiation succeeds")
        expected = {"protocol_version": "0.1", "epoch": self.host.descriptor["epoch"], "next_sequence": self.sequence,
                    "world_id": "workshop", "world_revision": self.expected["world_revision"],
                    "operations": ["entity.create", "transform.set", "entity.delete"],
                    "limits": {"max_header_bytes": 16384, "max_body_bytes": 1048576, "max_response_bytes": 4194304,
                               "max_operations": 256, "max_slots": 8, "request_timeout_ms": 1000,
                               "session_ttl_ms": self.host.ttl},
                    "admission": "synchronous", "durability": "volatile", "retry_mode": "resync_only"}
        exact(response[1], expected, label + ": negotiated contract")

    def submit(self, request, expected_receipt, resulting_state, label):
        response = self.call("POST", "/v0/transactions", request)
        require(response is not None and response[0] == 200, label + ": admitted HTTP status")
        self.sequence += 1
        exact(response[1], {"protocol_version": "0.1", "epoch": self.host.descriptor["epoch"],
                           "next_sequence": self.sequence, "receipt": expected_receipt}, label + ": typed receipt")
        self.expected = resulting_state
        self.evidence.retain_json("receipts/" + self.prefix + "/" + label + ".json", expected_receipt)
        self.observe(label)

    def setup(self):
        self.capabilities()
        self.observe("initial")
        first = envelope(self.host.descriptor["epoch"], 1, 0, [create()], 0)
        self.submit(first, receipt(1, 0, [{"temporary_id": "cube", **identity()}]), snapshot(1), "create")
        second = envelope(self.host.descriptor["epoch"], 2, 1, [move()], 1)
        self.submit(second, receipt(2, 1), snapshot(2), "move")

    def denied(self, label, payload, status, code, path=None, *, malformed=False, **kwargs):
        response = exchange(self.host.descriptor, payload, **kwargs)
        # Deliberately inspect real world state FIRST: an auth mutant must be caught
        # through unauthorized effects even if its HTTP response also changes.
        self.observe(label)
        if malformed and response is None:
            result = {"case": label, "transport": "closed", "state": "unchanged"}
        else:
            error_response(response, status, code, path)
            result = {"case": label, "http_status": response[0], "code": code, "state": "unchanged"}
        self.results.append(result)

    def bad_request(self, **kwargs):
        body = envelope(self.host.descriptor["epoch"], self.sequence, self.expected["world_revision"],
                        [move(), create("intruder")])
        return request_bytes(self.host.descriptor, "POST", "/v0/transactions", encoded(body), **kwargs)


def auth_and_protocol(scenario, only_auth=False):
    s = scenario
    wrong = "0" * 64 if s.host.descriptor["token"] != "0" * 64 else "1" * 64
    s.denied("wrong-token", s.bad_request(change={"authorization": "Bearer " + wrong}), 401, "NOT_AUTHORIZED", "")
    if only_auth:
        return
    cases = [
        ("missing-token", {"change": {"authorization": None}}, 401, "NOT_AUTHORIZED"),
        ("truncated-token", {"change": {"authorization": "Bearer " + s.host.descriptor["token"][:-1]}}, 401, "NOT_AUTHORIZED"),
        ("origin", {"extra": [("Origin", "http://forbidden.invalid")]}, 403, "NOT_AUTHORIZED"),
        ("origin-null", {"extra": [("origin", "null")]}, 403, "NOT_AUTHORIZED"),
        ("origin-empty", {"extra": [("Origin", "")]}, 403, "NOT_AUTHORIZED"),
        ("foreign-host", {"change": {"host": "forbidden.invalid"}}, 403, "NOT_AUTHORIZED"),
        ("header-version", {"change": {"x-omniweft-protocol": "0.2"}}, 400, "UNSUPPORTED_VERSION"),
    ]
    for label, options, status, code in cases:
        s.denied(label, s.bad_request(**options), status, code)
    for label, options in [
        ("duplicate-auth-valid-first", {"extra": [("authorization", "Bearer " + wrong)]}),
        ("duplicate-auth-valid-last", {"change": {"authorization": "Bearer " + wrong},
                                       "extra": [("Authorization", "Bearer " + s.host.descriptor["token"])]}),
        ("cookie", {"extra": [("Cookie", "session=untrusted")]}),
    ]:
        s.denied(label, s.bad_request(**options), 400, "INVALID_SCHEMA", malformed=True)
    base = envelope(s.host.descriptor["epoch"], s.sequence, 2, [create("intruder")])
    for label, field, value, status, code, path in [
        ("body-version", "protocol_version", "0.2", 400, "UNSUPPORTED_VERSION", "/protocol_version"),
        ("wrong-world", "world_id", "elsewhere", 403, "NOT_AUTHORIZED", "/world_id"),
        ("forged-principal", "principal", "administrator", 400, "INVALID_SCHEMA", None),
    ]:
        request = copy.deepcopy(base); request[field] = value
        s.denied(label, request_bytes(s.host.descriptor, "POST", "/v0/transactions", encoded(request)), status, code, path)
    for label, change, path in [
        ("sequence-gap", {"sequence": s.sequence + 1}, "/idempotency/sequence"),
        ("sequence-repeat", {"sequence": s.sequence - 1}, "/idempotency/sequence"),
        ("wrong-epoch", {"epoch": "f" * 64}, "/idempotency/epoch"),
    ]:
        request = copy.deepcopy(base); request["idempotency"].update(change)
        s.denied(label, request_bytes(s.host.descriptor, "POST", "/v0/transactions", encoded(request)),
                 409, "REQUIRES_RESYNC", path)
    old = s.host.renew()
    s.sequence = 1
    s.observe("renewal-same-world")
    request = envelope(old["epoch"], 1, 2, [create("intruder")])
    s.denied("retired-token", request_bytes(old, "POST", "/v0/transactions", encoded(request)), 401, "NOT_AUTHORIZED", "")
    s.denied("retired-epoch", request_bytes(s.host.descriptor, "POST", "/v0/transactions", encoded(request)),
             409, "REQUIRES_RESYNC", "/idempotency/epoch")


def schema_and_framing(s):
    descriptor = s.host.descriptor
    observe = b'{"protocol_version":"0.1","world_id":"workshop"}'
    body = encoded(envelope(descriptor["epoch"], s.sequence, 2, [create("intruder")]))
    malformed_bodies = [
        ("invalid-json", b"{"),
        ("json-trailing", body + b" {}"),
        ("json-nul", body + b"\x00garbage"),
        ("json-utf8", body[:-1] + b' ,"x":"\xff"}'),
        ("json-duplicate", body[:-1] + b',"protocol_version":"0.1"}'),
        ("json-escaped-duplicate", body[:-1] + b',"protocol_versi\\u006fn":"0.1"}'),
        ("json-lexical-nan", body.replace(b'"sequence":1', b'"sequence":NaN')),
    ]
    for label, value in malformed_bodies:
        s.denied(label, request_bytes(descriptor, "POST", "/v0/transactions", value), 400, "INVALID_SCHEMA")
    # Strict whole-body parser limits are enforced before any admission.
    s.denied("body-too-large", request_bytes(descriptor, "POST", "/v0/observe", observe,
              change={"content-length": "1048577"}), 413, "BUDGET_EXCEEDED", malformed=True)
    padded = observe + b" " * (1048576 - len(observe))
    response = exchange(descriptor, request_bytes(descriptor, "POST", "/v0/observe", padded))
    require(response is not None and response[0] == 200, "exact body byte limit accepted")
    exact(response[1]["snapshot"], s.expected, "exact body limit world")
    s.observe("body-limit-recovery")
    deep = b"[" * 33 + b"0" + b"]" * 33
    s.denied("body-depth", request_bytes(descriptor, "POST", "/v0/transactions", deep), 413, "BUDGET_EXCEEDED", "")
    for label, extra, change in [
        ("duplicate-length", [("Content-Length", str(len(body)))], None),
        ("conflicting-length", [("content-length", str(len(body) + 1))], None),
        ("transfer-encoding", [("Transfer-Encoding", "chunked")], None),
        ("expect", [("Expect", "100-continue")], None),
        ("content-encoding", [("Content-Encoding", "gzip")], None),
        ("negative-length", [], {"content-length": "-1"}),
        ("overflow-length", [], {"content-length": "184467440737095516160"}),
        ("missing-length", [], {"content-length": None}),
        ("bad-content-type", [], {"content-type": "text/plain"}),
    ]:
        s.denied(label, request_bytes(descriptor, "POST", "/v0/transactions", body, change=change, extra=extra),
                 400, "INVALID_SCHEMA", malformed=True)
    raw = request_bytes(descriptor, "POST", "/v0/transactions", body)
    for label, payload in [
        ("bare-lf", raw.replace(b"\r\n", b"\n")),
        ("header-nul", raw.replace(b"Connection: close", b"X-Test: bad\x00value")),
        ("header-fold", raw.replace(b"Connection: close", b"Connection: close\r\n folded")),
        ("colon-space", raw.replace(b"Host:", b"Host :")),
        ("duplicate-host", request_bytes(descriptor, "POST", "/v0/transactions", body, extra=[("HOST", "forbidden.invalid")])),
    ]:
        s.denied(label, payload, 400, "INVALID_SCHEMA", malformed=True)
    s.denied("incomplete-body", raw[:-8], 400, "INVALID_SCHEMA", malformed=True, half_close=True)
    s.denied("body-read-timeout", raw[:-8], 400, "INVALID_SCHEMA", malformed=True)
    s.denied("incomplete-header", raw.split(b"\r\n\r\n")[0][:-8], 400, "INVALID_SCHEMA", malformed=True)
    s.denied("unknown-route", request_bytes(descriptor, "GET", "/v0/not-present"), 404, "NOT_FOUND")
    s.denied("unsupported-method", request_bytes(descriptor, "DELETE", "/v0/observe"), 405, "UNSUPPORTED_OPERATION")
    s.denied("query-route", request_bytes(descriptor, "GET", "/v0/capabilities?token=untrusted"), 404, "NOT_FOUND")
    ordinary = [("X-Bounded-" + str(i), "ok") for i in range(60)]
    response = exchange(descriptor, request_bytes(descriptor, "GET", "/v0/capabilities", extra=ordinary))
    require(response is not None and response[0] == 200, "exact header count accepted")
    s.denied("header-count", request_bytes(descriptor, "GET", "/v0/capabilities", extra=ordinary + [("X-Overflow", "yes")]),
             413, "BUDGET_EXCEEDED", malformed=True)
    head = request_bytes(descriptor, "GET", "/v0/capabilities", extra=[("X-Padding", "")])
    legal = request_bytes(descriptor, "GET", "/v0/capabilities", extra=[("X-Padding", "x" * (16384 - len(head)))])
    require(len(legal) == 16384, "independent exact complete-head fixture length")
    response = exchange(descriptor, legal)
    require(response is not None and response[0] == 200, "exact complete-head byte limit accepted")
    s.denied("header-bytes", legal.replace(b"X-Padding: ", b"X-Padding: x"), 413, "BUDGET_EXCEEDED", malformed=True)
    route = "/" + "x" * (1024 - len(b"GET / HTTP/1.1\r\n"))
    legal_line = request_bytes(descriptor, "GET", route)
    require(len(legal_line.split(b"\r\n", 1)[0]) + 2 == 1024, "independent exact request-line fixture length")
    s.denied("request-line-limit", legal_line, 404, "NOT_FOUND")
    s.denied("request-line-overflow", request_bytes(descriptor, "GET", route + "x"),
             413, "BUDGET_EXCEEDED", malformed=True)


def rejected_executor(s):
    for label, operations, expected_revision, code, path, operation_index in [
        ("stale-revision", [create("intruder")], 0, "REVISION_CONFLICT", "/expected_world_revision", None),
        ("stale-handle", [move(identity(generation=2))], 2, "STALE_HANDLE", "/operations/0/target/generation", 0),
    ]:
        request = envelope(s.host.descriptor["epoch"], s.sequence, expected_revision, operations, 3)
        response = s.call("POST", "/v0/transactions", request)
        require(response is not None and response[0] == 200, label + ": executor admission returns HTTP 200")
        value = response[1]
        s.sequence += 1
        require(type(value) is dict and value.keys() == {"protocol_version", "epoch", "next_sequence", "receipt"},
                label + ": receipt wrapper fields")
        exact(value["protocol_version"], "0.1", label + ": protocol")
        exact(value["epoch"], s.host.descriptor["epoch"], label + ": current epoch")
        exact(value["next_sequence"], s.sequence, label + ": admitted rejection consumes one sequence")
        received = value["receipt"]
        require(type(received) is dict and received.keys() == {"status", "durability", "transaction_id", "world_revision", "created", "errors"},
                label + ": rejected receipt fields")
        for key, expected in (("status", "rejected"), ("durability", "volatile"), ("transaction_id", TX[3]),
                              ("world_revision", 2), ("created", [])):
            exact(received[key], expected, label + ": " + key)
        require(type(received["errors"]) is list and len(received["errors"]) == 1, label + ": one typed error")
        err = received["errors"][0]
        keys = {"code", "path", "message"} | ({"operation_index"} if operation_index is not None else set())
        require(type(err) is dict and err.keys() == keys, label + ": typed error fields")
        exact(err["code"], code, label + ": typed error code")
        exact(err["path"], path, label + ": typed error path")
        require(type(err["message"]) is str and 0 < len(err["message"]) <= 512, label + ": bounded typed message")
        if operation_index is not None:
            exact(err["operation_index"], operation_index, label + ": failing operation")
        s.observe(label)
        # Keep only independently verified fields; arbitrary error messages are never persisted.
        s.evidence.retain_json("receipts/" + s.prefix + "/" + label + ".json",
                              {key: value for key, value in received.items() if key != "errors"} |
                              {"errors": [{"code": code, "path": path}]})


def expiry(executable, evidence):
    with Host(executable, evidence, ttl=500) as host:
        s = Scenario(host, evidence, "expiry")
        s.setup()
        time.sleep(0.60)
        expired = exchange(host.descriptor, s.bad_request())
        host.renew(); s.sequence = 1
        s.observe("expired-no-mutation")
        error_response(expired, 401, "SESSION_EXPIRED", "")
        host.renew()
        raw = s.bad_request()
        split = raw.index(b"\r\n\r\n") + 4
        late = exchange(host.descriptor, raw, split=split, delay=0.65)
        host.renew()
        s.observe("expired-during-body")
        error_response(late, 401, "SESSION_EXPIRED", "")
        request = envelope(host.descriptor["epoch"], 1, 2, [create("recovery")], 2)
        s.submit(request, receipt(3, 2, [{"temporary_id": "recovery", **identity(2)}]), snapshot(3), "recovery")


def pipeline(executable, evidence):
    with Host(executable, evidence) as host:
        s = Scenario(host, evidence, "pipeline")
        first = envelope(host.descriptor["epoch"], 1, 0, [create()], 0)
        second = envelope(host.descriptor["epoch"], 2, 1, [create("intruder")], 1)
        raw = request_bytes(host.descriptor, "POST", "/v0/transactions", encoded(first))
        raw += request_bytes(host.descriptor, "POST", "/v0/transactions", encoded(second))
        response = exchange(host.descriptor, raw)
        require(response is not None and response[0] == 200, "pipeline first complete request admitted")
        exact(response[1]["receipt"], receipt(1, 0, [{"temporary_id": "cube", **identity()}]), "pipeline first receipt")
        s.expected = snapshot(1); s.sequence = 2
        s.observe("only-first-request")
        s.submit(envelope(host.descriptor["epoch"], 2, 1, [move()], 1), receipt(2, 1), snapshot(2), "recovery")


def runtime_incomplete(executable, evidence):
    started = time.monotonic()
    with Host(executable, evidence, runtime=1000) as host:
        # Leave a real authenticated HTTP body unfinished across the normal
        # host deadline. The connection's own deadline would otherwise be later.
        time.sleep(0.55)
        incomplete = request_bytes(host.descriptor, "POST", "/v0/transactions", b"{}")[:-1]
        response = exchange(host.descriptor, incomplete)
        require(response is None or response[0] == 400,
                "incomplete connection closes or rejects without an admitted receipt")
        code = host.process.wait(timeout=4)
        exact(code, 0, "incomplete connection at normal runtime expiry returns zero")
        require(time.monotonic() - started < 5.0,
                "incomplete connection runtime cleanup has a finite failure bound")
        evidence.retain_json("runtime-lifecycle.json", {"schema_version": 1,
                             "incomplete_connection_closed": True, "native_exit_code": code})


def sdk_example(executable, evidence):
    with tempfile.TemporaryDirectory(prefix="ow-sdk-example-") as temporary:
        output = Path(temporary) / "result"
        command = [sys.executable, str(ROOT / "sdk/python/examples/move_cube.py"), "--executable", str(executable),
                   "--output", str(output), "--seed", "7", "--verify"]
        result = evidence.run(command, ["<python>", "sdk/python/examples/move_cube.py", "--executable", "<executable>",
                                        "--output", "<fresh-output>", "--seed", "7", "--verify"], timeout=25)
        require(result.returncode == 0, "separate Python SDK example exits successfully")
        target = output / "result.json"
        require(target.is_file() and not target.is_symlink() and target.stat().st_size <= 65536,
                "SDK example publishes bounded result")
        value = strict_json(target.read_bytes())
        expected = {"schema_version": 1, "example": "sdk.move_cube", "seed": 7, "status": "passed",
                    "receipts": [receipt(1, 0, [{"temporary_id": "cube", **identity()}]), receipt(2, 1)],
                    "snapshots": [snapshot(0), snapshot(1), snapshot(2)]}
        exact(value, expected, "independent SDK example expected values")
        evidence.retain_json("sdk-example.json", value)
    probe = evidence.run([sys.executable, str(ROOT / "tests/sdk_client_probe.py"), "--executable", str(executable)],
                         ["<python>", "tests/sdk_client_probe.py", "--executable", "<executable>"], timeout=30)
    require(probe.returncode == 0, "separate SDK negative and uncertain-response process passes")
    result = strict_json(probe.stdout.encode("utf-8"))
    require(type(result) is dict and result.keys() == {"status", "assertions"} and result["status"] == "passed",
            "separate SDK assertion report fields")
    require(type(result["assertions"]) is list and len(result["assertions"]) > 100, "separate SDK retained assertions exist")
    for check in result["assertions"]:
        require(type(check) is dict and check.keys() == {"assertion", "passed"} and
                type(check["assertion"]) is str and 0 < len(check["assertion"]) <= 200 and check["passed"] is True,
                "separate SDK assertion is fixed and passed")
    evidence.retain_json("sdk-client-checks.json", result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path)
    parser.add_argument("--case-set", choices=("all", "auth"), default="all")
    args = parser.parse_args()
    require(not args.evidence_dir or args.case_set == "all", "retained oracle covers the complete SDK case set")
    executable = args.executable.resolve(strict=True)
    evidence = Evidence(args.evidence_dir, executable)
    evidence.manifest["case_set"] = args.case_set
    try:
        evidence.retain_json("fixture.json", {"seed": 7, "snapshots": [snapshot(i) for i in range(4)]})
        with Host(executable, evidence) as host:
            s = Scenario(host, evidence, "normal")
            s.setup()
            auth_and_protocol(s, args.case_set == "auth")
            if args.case_set == "all":
                schema_and_framing(s)
                rejected_executor(s)
                request = envelope(host.descriptor["epoch"], s.sequence, 2, [create("recovery")], 2)
                s.submit(request, receipt(3, 2, [{"temporary_id": "recovery", **identity(2)}]), snapshot(3), "recovery")
            evidence.retain_json("case-results.json", s.results)
        if args.case_set == "all":
            expiry(executable, evidence)
            pipeline(executable, evidence)
            runtime_incomplete(executable, evidence)
            sdk_example(executable, evidence)
        evidence.finish("passed")
        print(json.dumps({"status": "passed", "example": "sdk.move_cube", "assertions": len(evidence.manifest["assertions"])}))
        return 0
    except Failure as error:
        evidence.finish("failed", str(error))
        raise
    except Exception:
        evidence.finish("failed", "unexpected internal error; private details withheld")
        raise


if __name__ == "__main__":
    raise SystemExit(main_guard(main))
