# SPDX-License-Identifier: Apache-2.0
"""An explicitly launched native host with an exclusively owned private channel."""
from __future__ import annotations

from pathlib import Path
import os
import queue
import subprocess
import threading

from .client import Client
from .models import ConnectionInfo, ProtocolError, integer


class NativeSession:
    def __init__(self, executable: str | Path, *, session_ttl_ms: int = 30000,
                 max_slots: int = 8, max_runtime_ms: int = 60000, max_requests: int = 1024) -> None:
        integer(session_ttl_ms, 50, 300000)
        integer(max_slots, 1, 1024)
        integer(max_runtime_ms, 1000, 600000)
        integer(max_requests, 1, 4096)
        self._executable = executable
        self._config = (session_ttl_ms, max_slots, max_runtime_ms, max_requests)
        self._process: subprocess.Popen | None = None
        self._info: ConnectionInfo | None = None
        self._lock = threading.RLock()
        self._renewals = 0
        self.returncode: int | None = None

    def __repr__(self) -> str:
        return "NativeSession(local=True, credentials=<private>)"

    @classmethod
    def launch(cls, executable: str | Path, **kwargs: int) -> NativeSession:
        session = cls(executable, **kwargs)
        session.start()
        return session

    def _descriptor(self) -> ConnectionInfo:
        if self._process is None or self._process.stdout is None:
            raise ProtocolError()
        stream = self._process.stdout
        result: queue.Queue[bytes | None] = queue.Queue(maxsize=1)

        def read() -> None:
            try:
                result.put_nowait(stream.readline(514))
            except (OSError, ValueError):
                result.put_nowait(None)

        reader = threading.Thread(target=read, name="omniweft-private-descriptor", daemon=True)
        reader.start()
        try:
            data = result.get(timeout=3)
            if data is None or not data.endswith(b"\n") or len(data) > 513:
                raise ProtocolError()
            info = ConnectionInfo.from_descriptor(data[:-1])
            if info.session_ttl_ms != self._config[0]:
                raise ProtocolError()
            return info
        except (queue.Empty, ProtocolError):
            self._terminate()
            raise ProtocolError() from None
        finally:
            reader.join(timeout=1)

    def start(self) -> NativeSession:
        with self._lock:
            if self._process is not None:
                return self
            try:
                executable = Path(self._executable).resolve(strict=True)
                if not executable.is_file():
                    raise ProtocolError()
                ttl, slots, runtime, requests = self._config
                args = [str(executable), "--world", "workshop", "--seed", "7",
                        "--max-slots", str(slots), "--session-ttl-ms", str(ttl),
                        "--max-runtime-ms", str(runtime), "--max-requests", str(requests)]
                self._process = subprocess.Popen(
                    args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                    bufsize=0, close_fds=True,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
                self._info = self._descriptor()
                self._renewals = 0
                return self
            except (OSError, ValueError, TypeError, ProtocolError):
                self._terminate()
                raise ProtocolError() from None

    def client(self) -> Client:
        with self._lock:
            if self._process is None or self._info is None or self._process.poll() is not None:
                raise ProtocolError()
            return Client(self._info)

    def renew(self) -> Client:
        with self._lock:
            if self._process is None or self._info is None or self._renewals >= 64:
                raise ProtocolError()
            old = self._info
            try:
                self._process.stdin.write(b"renew\n")
                self._process.stdin.flush()
                new = self._descriptor()
                if new.token == old.token or new.epoch == old.epoch or new.port != old.port:
                    raise ProtocolError()
                self._info = new
                self._renewals += 1
                return Client(new)
            except (OSError, ValueError, ProtocolError):
                self._terminate()
                raise ProtocolError() from None

    def _terminate(self) -> None:
        process, self._process = self._process, None
        self._info = None
        if process is None:
            return
        try:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=3)
            self.returncode = process.returncode
        finally:
            for stream in (process.stdin, process.stdout):
                if stream is not None:
                    stream.close()

    def close(self) -> None:
        with self._lock:
            process = self._process
            if process is None:
                return
            try:
                if process.poll() is None:
                    try:
                        process.stdin.write(b"stop\n")
                        process.stdin.flush()
                    except OSError:
                        # Natural completion can close the pipe after poll().
                        # Only the bounded wait and verified exit below decide
                        # whether this shutdown succeeded.
                        pass
                process.wait(timeout=3)
                self.returncode = process.returncode
                if process.returncode != 0:
                    raise ProtocolError()
            except (OSError, ValueError, subprocess.TimeoutExpired):
                self._terminate()
                raise ProtocolError() from None
            finally:
                self._terminate()

    def __enter__(self) -> NativeSession:
        return self.start()

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if exc_type is None:
            self.close()
        else:
            try:
                self.close()
            except (OSError, ProtocolError, subprocess.TimeoutExpired):
                pass
