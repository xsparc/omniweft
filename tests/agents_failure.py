#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Inject a post-commit publication failure and require prompt claimed-job cleanup."""
import argparse
import difflib
from pathlib import Path, PurePosixPath
import queue
import re
import socket
import subprocess
import sys
import tempfile
import threading
import time

from agents_test_support import Evidence, Failure, ROOT, digest, main_guard, require
from sdk_test_support import Host, encoded, request_bytes, timestamp

TARGET = "src/agents_main.cpp"
PUBLICATION = "          published_.store(std::make_shared<const Published>(Published{completed_tick,world_.snapshot()}));"
MARKER = b"OW_TEST_POST_COMMIT_PUBLICATION_FAILURE"
INJECTION = (
    "          if(world_.snapshot().world_revision==1) {\n"
    '            std::fputs("OW_TEST_POST_COMMIT_PUBLICATION_FAILURE\\n",stderr);\n'
    "            std::fflush(stderr);\n"
    "            throw std::bad_alloc();\n"
    "          }\n"
)
COMPLETION_SECONDS = 2.0


class FailureHost(Host):
    """Reuse production descriptor validation; keep pipe data private and bounded."""
    def __enter__(self):
        self.marker = threading.Event()
        self.started = time.monotonic()
        self.process = subprocess.Popen(
            self.args, cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, close_fds=True,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0)

        def drain(stream, descriptor=False):
            line = bytearray()
            total = 0
            first = True
            try:
                while True:
                    byte = stream.read(1)
                    if not byte:
                        if descriptor and first:
                            self.lines.put_nowait(None)
                        return
                    total += 1
                    line += byte
                    if total > 16384 or len(line) > 513:
                        self.pipe_error = True
                        if descriptor and first:
                            self.lines.put_nowait(None)
                        return
                    if byte == b"\n":
                        if descriptor and first:
                            self.lines.put_nowait(bytes(line))
                            first = False
                        elif not descriptor and bytes(line).rstrip(b"\r\n") == MARKER:
                            self.marker.set()
                        line.clear()
            except (OSError, ValueError, queue.Full):
                self.pipe_error = True

        self.threads = [threading.Thread(target=drain, args=(self.process.stdout, True), daemon=True),
                        threading.Thread(target=drain, args=(self.process.stderr,), daemon=True)]
        for thread in self.threads:
            thread.start()
        try:
            # Host.read_descriptor validates exact loopback, types and secret
            # formats. Its five-second wait is narrowed here before delegation.
            try:
                line = self.lines.get(timeout=2)
            except queue.Empty:
                raise Failure("failure-injected host starts within two seconds") from None
            self.lines.put_nowait(line)
            self.descriptor = self.read_descriptor()
            require(time.monotonic() - self.started < 2,
                    "request can precede the normal five-second host cutoff")
            return self
        except BaseException:
            self.close(check=False)
            raise

    def close(self, check=False):
        if self.process is None:
            return
        # A failing old implementation must be terminated at the test deadline;
        # never wait for its six-second emergency watchdog to manufacture a pass.
        if self.process.poll() is None:
            self.process.kill()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            raise Failure("failure-injected process cleanup remains bounded") from None
        self.record.update(exit_code=self.process.returncode, finished_at=timestamp())
        for thread in self.threads:
            thread.join(timeout=1)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()
        require(not self.pipe_error and not any(thread.is_alive() for thread in self.threads),
                "failure-injected private pipes drain within fixed bounds")


def exercise(executable, evidence):
    result = {"marker_observed": False, "prompt_exit": False,
              "exit_code_4": False, "closed_without_receipt": False}
    with FailureHost(executable, evidence, ttl=5000, runtime=5000, max_requests=8) as host:
        # This is a real authenticated transaction through the production route.
        body = {"protocol_version": "0.1", "world_id": "workshop",
                "transaction_id": "018f7242-4387-7c98-a114-67787915a611",
                "idempotency": {"epoch": host.descriptor["epoch"], "sequence": 1},
                "expected_world_revision": 0,
                "apply_at": {"mode": "next_tick", "expires_after_ticks": 120},
                "budget": {"max_operations": 1, "max_blob_bytes": 0},
                "operations": [{"type": "entity.create", "temporary_id": "cube",
                                "prefab": "builtin.unit_cube"}]}
        payload = request_bytes(host.descriptor, "POST", "/v0/transactions", encoded(body))
        received = bytearray()
        closed = False
        with socket.create_connection(("127.0.0.1", host.descriptor["port"]), timeout=0.5) as connection:
            require(time.monotonic() - host.started < 2,
                    "request starts before normal runtime expiry can satisfy the oracle")
            deadline = time.monotonic() + COMPLETION_SECONDS
            connection.settimeout(COMPLETION_SECONDS)
            connection.sendall(payload)
            while time.monotonic() < deadline:
                connection.settimeout(max(0.001, deadline - time.monotonic()))
                try:
                    chunk = connection.recv(4096)
                except ConnectionResetError:
                    closed = True
                    break
                except socket.timeout:
                    break
                if not chunk:
                    closed = True
                    break
                received += chunk
                require(len(received) <= 65536, "failure response remains within its private byte cap")
            remaining = deadline - time.monotonic()
            if remaining > 0:
                try:
                    host.process.wait(timeout=remaining)
                except subprocess.TimeoutExpired:
                    pass
            result.update(marker_observed=host.marker.is_set(),
                          prompt_exit=host.process.poll() is not None and time.monotonic() <= deadline,
                          exit_code_4=host.process.poll() == 4,
                          closed_without_receipt=closed and not received)
    result["marker_observed"] = host.marker.is_set()
    evidence.retain_json("publication-failure.json", result)
    # Development runs expose only the same allowlisted boolean result, before
    # assertion, so an intentionally tested old source reports its actual failure.
    print(encoded(result).decode("ascii"))
    require(result["marker_observed"], "post-commit revision-one publication failure was actually injected")
    require(result["prompt_exit"], "claimed publication failure exits within two seconds of request send")
    require(result["exit_code_4"], "publication failure produces the native failure exit code")
    require(result["closed_without_receipt"], "failed publication closes the request without a successful receipt")


def copy_indexed(repository, destination, evidence):
    result = evidence.run(["git", "ls-files", "-z"], ["git", "ls-files", "-z"],
                          cwd=repository, timeout=10)
    require(result.returncode == 0, "enumerate the exact indexed candidate source files")
    tracked = [relative for relative in result.stdout.split("\0") if relative]
    require(TARGET in tracked, "injected source is present in the candidate index")
    hashes = {}
    for relative in tracked:
        path = PurePosixPath(relative)
        require(not path.is_absolute() and ".." not in path.parts and ".git" not in path.parts,
                "indexed source stays repository-relative")
        original = repository / relative
        copied = destination / relative
        require(original.is_file() and not original.is_symlink() and
                original.resolve().is_relative_to(repository) and
                copied.resolve().is_relative_to(destination), "source copy stays within its declared roots")
        raw = original.read_bytes()
        hashes[relative] = digest(raw)
        copied.parent.mkdir(parents=True, exist_ok=True)
        copied.write_bytes(raw)
    return hashes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path)
    parser.add_argument("--repository", type=Path, help="development only; prohibits retained evidence")
    parser.add_argument("--source-override", type=Path, help="development only; prohibits retained evidence")
    args = parser.parse_args()
    require(not (args.evidence_dir and (args.repository is not None or args.source_override is not None)),
            "development source overrides prohibit retained candidate evidence")
    repository = args.repository.resolve(strict=True) if args.repository is not None else ROOT
    build = args.build_dir.resolve(strict=True)
    name = "omniweft_agents.exe" if sys.platform == "win32" else "omniweft_agents"
    executable = build / name
    source_before = (repository / TARGET).read_bytes()
    executable_before = executable.read_bytes()
    evidence = Evidence(args.evidence_dir, executable, proof="post_commit_publication_failure")
    evidence.manifest["development_override"] = args.repository is not None or args.source_override is not None
    protected = {}
    try:
        cache = dict(re.findall(r"^([A-Za-z0-9_-]+):[^=\n]+=(.*)$",
                                (build / "CMakeCache.txt").read_text("utf-8"), re.MULTILINE))
        require(Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() == repository,
                "baseline build belongs to the exact tested source")
        require(cache.get("CMAKE_GENERATOR") == "Ninja" and
                cache.get("OW_ENABLE_VULKAN") in ("OFF", "FALSE", "0"),
                "publication failure uses the pinned headless Ninja baseline")
        original = args.source_override.read_bytes() if args.source_override is not None else source_before
        original_text = original.decode("utf-8-sig")
        require(original_text.count(PUBLICATION) == 1 and MARKER.decode("ascii") not in original_text,
                "failure injection matches exactly one completed-tick publication")
        injected_text = original_text.replace(PUBLICATION, INJECTION + PUBLICATION)
        with tempfile.TemporaryDirectory(prefix="ow-agents-publication-failure-") as temporary:
            source = Path(temporary) / "source"
            target_build = Path(temporary) / "build"
            source.mkdir()
            protected = copy_indexed(repository, source, evidence)
            (source / TARGET).write_text(injected_text, encoding="utf-8", newline="\n")
            evidence.manifest["failure_injection"] = {
                "original_source_sha256": digest(original),
                "injected_source_sha256": digest((source / TARGET).read_bytes()),
                "original_executable_sha256": digest(executable_before)}
            patch = "".join(difflib.unified_diff(original_text.splitlines(keepends=True),
                             injected_text.splitlines(keepends=True),
                             fromfile="original/" + TARGET, tofile="injected/" + TARGET))
            evidence.retain("publication-failure.patch", patch.encode("utf-8"))
            command = [cache["CMAKE_COMMAND"], "-S", str(source), "-B", str(target_build),
                       "-G", "Ninja", "-DBUILD_TESTING=OFF"]
            public = ["<cmake>", "-S", "<injected-source>", "-B", "<injected-build>",
                      "-G", "Ninja", "-DBUILD_TESTING=OFF"]
            for key in sorted(cache):
                if key.startswith("OW_") or key in {
                    "CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER", "CMAKE_MAKE_PROGRAM",
                    "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS"}:
                    value = cache[key]
                    if value.endswith("-NOTFOUND"):
                        continue
                    command.append("-D" + key + "=" + value)
                    shown = value if key in ("OW_ENABLE_VULKAN", "CMAKE_BUILD_TYPE") else "<same-baseline-setting>"
                    public.append("-D" + key + "=" + shown)
            configured = evidence.run(command, public, timeout=180)
            require(configured.returncode == 0, "isolated publication-failure source configures with pinned settings")
            compiled = evidence.run(
                [cache["CMAKE_COMMAND"], "--build", str(target_build), "--target", "omniweft_agents", "--parallel", "2"],
                ["<cmake>", "--build", "<injected-build>", "--target", "omniweft_agents", "--parallel", "2"], timeout=300)
            require(compiled.returncode == 0, "isolated real publication-failure host compiles")
            injected_executable = target_build / name
            evidence.manifest["failure_injection"]["injected_executable_sha256"] = digest(injected_executable.read_bytes())
            exercise(injected_executable, evidence)
        require(all(digest((repository / path).read_bytes()) == sha for path, sha in protected.items()),
                "indexed original candidate files remain byte-identical")
        require(executable.read_bytes() == executable_before, "original agents executable remains byte-identical")
        evidence.finish("passed")
        return 0
    except Failure as error:
        evidence.finish("failed", str(error))
        raise
    except Exception:
        evidence.finish("failed", "unexpected internal error; private details withheld")
        raise
    finally:
        if ((repository / TARGET).read_bytes() != source_before or executable.read_bytes() != executable_before or
                any(digest((repository / path).read_bytes()) != sha for path, sha in protected.items())):
            raise Failure("failure injection changed protected candidate source or executable")


if __name__ == "__main__":
    raise SystemExit(main_guard(main))
