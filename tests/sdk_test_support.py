# SPDX-License-Identifier: Apache-2.0
"""Independent SDK test primitives; private process/network data never becomes evidence."""
import hashlib
import json
import math
import queue
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

from render_test_support import Evidence as BaseEvidence, Failure, CHECKS, ROOT, exact, require, digest, timestamp


def encoded(value):
    return json.dumps(value, ensure_ascii=True, allow_nan=False, separators=(",", ":")).encode("ascii")


def strict_json(data):
    def pairs(items):
        out = {}
        for key, value in items:
            if key in out:
                raise Failure("JSON duplicate key")
            out[key] = value
        return out
    def number(value):
        result = float(value)
        if not math.isfinite(result):
            raise Failure("JSON nonfinite number")
        return result
    def constant(unused):
        raise Failure("JSON nonfinite number")
    try:
        return json.loads(data.decode("utf-8"), object_pairs_hook=pairs, parse_float=number, parse_constant=constant)
    except (ValueError, UnicodeError):
        raise Failure("JSON syntax or encoding") from None


def canonical(snapshot):
    def string(value):
        raw = value.encode("ascii")
        return struct.pack("<I", len(raw)) + raw
    result = bytearray(b"OWOBJ001") + string(snapshot["world_id"])
    result += struct.pack("<IIQI", snapshot["seed"], snapshot["max_slots"], snapshot["world_revision"], len(snapshot["slots"]))
    for slot in snapshot["slots"]:
        result += slot["entity_uuid"].encode("ascii")
        result += struct.pack("<QBB", slot["generation"], slot["retired"], slot["entity"] is not None)
        if slot["entity"] is not None:
            entity = slot["entity"]
            result += string(entity["prefab"]) + struct.pack("<Q", entity["authoring_revision"])
            transform = entity["transform"]
            values = transform["position_m"] + transform["rotation_xyzw"] + transform["scale"]
            result += struct.pack("<10d", *(0.0 if value == 0 else value for value in values))
    return bytes(result)


class Evidence(BaseEvidence):
    def __init__(self, directory, executable, proof="independent_oracle"):
        self.secrets = set()
        super().__init__(directory, executable, "cpu", proof)
        tree = subprocess.run(["git", "rev-parse", "HEAD^{tree}"], cwd=ROOT, capture_output=True, text=True)
        require(tree.returncode == 0 and re.fullmatch("[0-9a-f]{40}", tree.stdout.strip()), "candidate tree is known")
        self.manifest.update(work_item="PR-005", example="sdk.move_cube", candidate_tree=tree.stdout.strip(),
                             lanes={"cpu": "running", "gpu": "not_applicable"})
        files = [p for p in (ROOT / "sdk/python").rglob("*.py") if "__pycache__" not in p.parts]
        self.client_files = {str(p.relative_to(ROOT)).replace("\\", "/"): digest(p.read_bytes()) for p in files}
        self.manifest["client_source_sha256"] = self.client_files

    def register(self, descriptor):
        self.secrets.update((descriptor["token"], descriptor["epoch"]))

    def private_free(self, payload):
        require(not any(secret.encode("ascii") in payload for secret in self.secrets),
                "public evidence contains no credentials or epochs")

    def retain(self, relative, payload):
        self.private_free(payload)
        super().retain(relative, payload)

    def finish(self, status, failure=None):
        if status == "passed":
            require(all(digest((ROOT / path).read_bytes()) == sha for path, sha in self.client_files.items()),
                    "tested Python client source remains unchanged")
        self.manifest["lanes"]["cpu"] = status
        self.private_free(encoded(self.manifest))
        super().finish(status, failure)


class Host:
    """A real native subprocess, controlled exclusively through its production pipes."""
    def __init__(self, executable, evidence, *, ttl=30000, max_requests=1024):
        self.evidence = evidence
        self.ttl = ttl
        self.lines = queue.Queue(maxsize=66)
        self.pipe_error = False
        self.stderr = bytearray()
        self.process = None
        self.descriptor = None
        self.args = [str(executable), "--world", "workshop", "--seed", "7", "--max-slots", "8",
                     "--session-ttl-ms", str(ttl), "--max-runtime-ms", "60000", "--max-requests", str(max_requests)]
        self.record = {"command": ["<executable>", *self.args[1:]], "exit_code": None, "started_at": timestamp()}
        evidence.manifest["commands"].append(self.record)

    def __enter__(self):
        self.process = subprocess.Popen(self.args, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE, close_fds=True,
                                        creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0)
        require(self.process.pid != __import__("os").getpid(), "native host is a distinct process")
        def read_lines():
            line = bytearray()
            while True:
                byte = self.process.stdout.read(1)
                if not byte:
                    self.lines.put(None)
                    return
                line += byte
                if len(line) > 513:
                    self.pipe_error = True
                    self.lines.put(None)
                    return
                if byte == b"\n":
                    self.lines.put(bytes(line))
                    line.clear()
        def read_errors():
            while True:
                data = self.process.stderr.read(256)
                if not data:
                    return
                if len(self.stderr) + len(data) > 16384:
                    self.pipe_error = True
                    return
                self.stderr.extend(data)
        self.threads = [threading.Thread(target=read_lines, daemon=True), threading.Thread(target=read_errors, daemon=True)]
        for thread in self.threads:
            thread.start()
        try:
            self.descriptor = self.read_descriptor()
        except BaseException:
            self.close(check=False)
            raise
        return self

    def read_descriptor(self):
        try:
            line = self.lines.get(timeout=5)
        except queue.Empty:
            raise Failure("native private descriptor deadline") from None
        require(line is not None and not self.pipe_error and len(line) <= 513, "bounded private descriptor received")
        result = strict_json(line)
        require(type(result) is dict and result.keys() == {"schema_version", "protocol_version", "host", "port", "token", "epoch", "session_ttl_ms"},
                "private descriptor exact fields")
        exact(result["schema_version"], 1, "descriptor schema")
        exact(result["protocol_version"], "0.1", "descriptor protocol")
        exact(result["host"], "127.0.0.1", "descriptor numeric loopback")
        require(type(result["port"]) is int and 1 <= result["port"] <= 65535, "descriptor bounded port")
        exact(result["session_ttl_ms"], self.ttl, "descriptor configured lifetime")
        for field in ("token", "epoch"):
            require(type(result[field]) is str and re.fullmatch("[0-9a-f]{64}", result[field]), "descriptor secret format")
        require(result["token"] != result["epoch"], "token and epoch are independent values")
        self.evidence.register(result)
        return result

    def renew(self):
        old = self.descriptor
        self.process.stdin.write(b"renew\n")
        self.process.stdin.flush()
        self.descriptor = self.read_descriptor()
        require(old["token"] != self.descriptor["token"] and old["epoch"] != self.descriptor["epoch"],
                "renewal rotates both credentials")
        exact(self.descriptor["port"], old["port"], "renewal preserves listener")
        return old

    def close(self, check=True):
        if self.process is None:
            return
        try:
            if self.process.poll() is None:
                self.process.stdin.write(b"stop\n")
                self.process.stdin.flush()
            code = self.process.wait(timeout=4)
        except (OSError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait(timeout=3)
            if check:
                raise Failure("native host failed bounded stop") from None
            code = self.process.returncode
        self.record["exit_code"] = code
        self.record["finished_at"] = timestamp()
        for thread in self.threads:
            thread.join(timeout=1)
        if check:
            require(code == 0, "native host exits successfully through production stop")
            require(not self.pipe_error and not any(thread.is_alive() for thread in self.threads), "native secret pipes remain bounded")
            require(not any(s.encode() in self.stderr for s in self.evidence.secrets),
                    "native diagnostics exclude credentials and epochs")
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()

    def __exit__(self, kind, unused, traceback):
        self.close(check=kind is None)


def request_bytes(descriptor, method, route, body=None, *, change=None, extra=()):
    headers = [("Host", "127.0.0.1:" + str(descriptor["port"])),
               ("Authorization", "Bearer " + descriptor["token"]), ("X-Omniweft-Protocol", "0.1")]
    if body is not None:
        headers += [("Content-Length", str(len(body))), ("Content-Type", "application/json")]
    headers += [("Connection", "close")]
    if change:
        headers = [(key, change.get(key.lower(), value)) for key, value in headers
                   if change.get(key.lower(), value) is not None]
    headers += list(extra)
    return (method + " " + route + " HTTP/1.1\r\n" +
            "".join(key + ": " + value + "\r\n" for key, value in headers) + "\r\n").encode("ascii") + (body or b"")


def parse_response(raw):
    if raw == b"":
        return None
    require(len(raw) <= 4194304 + 16384, "HTTP response is bounded")
    require(b"\r\n\r\n" in raw, "HTTP response has complete headers")
    header, body = raw.split(b"\r\n\r\n", 1)
    lines = header.split(b"\r\n")
    require(re.fullmatch(rb"HTTP/1\.1 [1-5][0-9]{2} [ -~]+", lines[0]), "HTTP response status line")
    status = int(lines[0].split(b" ")[1])
    fields = {}
    for line in lines[1:]:
        require(b":" in line, "HTTP response header syntax")
        key, value = line.split(b":", 1)
        key = key.lower()
        require(key not in fields, "HTTP response has no duplicate header")
        fields[key] = value.strip().lower()
    require(fields.get(b"content-type") == b"application/json", "HTTP response JSON media type")
    require(fields.get(b"connection") == b"close", "HTTP response closes connection")
    require(fields.get(b"cache-control") == b"no-store", "HTTP response disables caching")
    require(not any(key.startswith(b"access-control-") for key in fields), "HTTP response grants no browser origin")
    require(b"transfer-encoding" not in fields and re.fullmatch(rb"[0-9]+", fields.get(b"content-length", b"")),
            "HTTP response has unambiguous length")
    require(int(fields[b"content-length"]) == len(body) <= 4194304, "HTTP response body exactly matches declared bounded length")
    return status, strict_json(body)


def exchange(descriptor, payload, *, split=None, delay=0, half_close=False):
    received = bytearray()
    with socket.create_connection(("127.0.0.1", descriptor["port"]), timeout=3) as connection:
        connection.settimeout(3)
        try:
            if split is None:
                connection.sendall(payload)
            else:
                connection.sendall(payload[:split])
                time.sleep(delay)
                connection.sendall(payload[split:])
            if half_close:
                connection.shutdown(socket.SHUT_WR)
        except (BrokenPipeError, ConnectionResetError):
            pass
        try:
            while True:
                data = connection.recv(65536)
                if not data:
                    break
                received += data
                require(len(received) <= 4194304 + 16384, "HTTP response receive budget")
        except ConnectionResetError:
            pass
        except socket.timeout:
            raise Failure("native HTTP connection exceeded bounded response deadline") from None
    return parse_response(bytes(received))


def main_guard(main):
    try:
        return main()
    except Failure as error:
        print("sdk oracle failed: " + str(error), file=sys.stderr)
        return 1
    except Exception:
        print("sdk oracle failed: unexpected internal error; private details withheld", file=sys.stderr)
        return 1
