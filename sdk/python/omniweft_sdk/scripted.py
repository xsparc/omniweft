# SPDX-License-Identifier: Apache-2.0
"""A bounded, explicitly launched, repository-authored scripted builder."""
from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
from .models import ConnectionInfo, OutcomeUnknown, ProtocolError, Receipt, Snapshot, decode, fields


@dataclass(frozen=True)
class BuilderProgress:
    event: str
    receipts: tuple[Receipt, ...]
    snapshots: tuple[Snapshot, ...]


class ScriptedBuilder:
    """Own a separate provider process with explicit first/finish barriers.

    Only the fixed bundled worker is launched. Its credentials travel on a
    private pipe; the parent receives bounded typed domain results.
    """
    def __init__(self, session) -> None:
        self._session = session
        self._process = None
        self._state = "new"
        self._secrets: tuple[str, ...] = ()
        self.initial: BuilderProgress | None = None

    def __repr__(self) -> str:
        return "ScriptedBuilder(local=True, credentials=<private>)"

    def _read(self, event: str, count: int) -> BuilderProgress:
        process = self._process
        if process is None or process.stdout is None:
            raise ProtocolError()
        result = queue.Queue(maxsize=1)

        def read() -> None:
            try:
                result.put_nowait(process.stdout.readline(65537))
            except (OSError, ValueError):
                result.put_nowait(None)

        reader = threading.Thread(target=read, name="omniweft-builder-result", daemon=True)
        reader.start()
        try:
            data = result.get(timeout=3)
            if data is None or len(data) > 65536 or not data.endswith(b"\n"):
                raise ProtocolError()
            decoded = decode(data, 65536)
            pending = [decoded]
            while pending:
                item = pending.pop()
                if isinstance(item, dict):
                    pending.extend(item.keys())
                    pending.extend(item.values())
                elif isinstance(item, list):
                    pending.extend(item)
                elif isinstance(item, str) and any(secret in item for secret in self._secrets):
                    raise ProtocolError()
            value = fields(decoded, {"event", "receipts", "snapshots"})
            if (value["event"] != event or not isinstance(value["receipts"], list)
                    or len(value["receipts"]) != count
                    or not isinstance(value["snapshots"], list)
                    or len(value["snapshots"]) != max(1, count)):
                raise ProtocolError()
            receipts = tuple(Receipt.from_dict(x) for x in value["receipts"])
            snapshots = tuple(Snapshot.from_dict(x) for x in value["snapshots"])
            if any(x.status != "committed" for x in receipts):
                raise ProtocolError()
            return BuilderProgress(event, receipts, snapshots)
        except (queue.Empty, OSError, ValueError, TypeError):
            self._terminate()
            raise ProtocolError() from None
        finally:
            if reader.is_alive():
                self._terminate()
            reader.join(timeout=1)

    def start(self) -> ScriptedBuilder:
        if self._state != "new":
            raise ProtocolError()
        try:
            descriptor = self._session._provider_descriptor()
            info = ConnectionInfo.from_descriptor(descriptor)
            self._secrets = (info.token, info.epoch)
            self._process = subprocess.Popen(
                [sys.executable, str(Path(__file__).with_name("_scripted_worker.py"))],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                bufsize=0, close_fds=True,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            self._process.stdin.write(descriptor + b"\n")
            self._process.stdin.flush()
            self.initial = self._read("ready", 0)
            self._state = "ready"
            return self
        except Exception:
            self._terminate()
            raise ProtocolError() from None

    def _command(self, command: bytes, event: str, count: int) -> BuilderProgress:
        try:
            self._process.stdin.write(command + b"\n")
            self._process.stdin.flush()
            return self._read(event, count)
        except Exception:
            # The worker may have committed before its domain result was lost.
            # Poison this builder; the caller must observe/resync explicitly.
            self._state = "failed"
            self._terminate()
            raise OutcomeUnknown() from None

    def create_first(self) -> BuilderProgress:
        if self._state != "ready":
            raise ProtocolError()
        result = self._command(b"build", "first", 1)
        self._state = "first"
        return result

    def finish(self) -> BuilderProgress:
        if self._state != "first":
            raise ProtocolError()
        result = self._command(b"finish", "done", 2)
        self._state = "done"
        return result

    def _terminate(self) -> None:
        process, self._process = self._process, None
        self._secrets = ()
        if process is None:
            return
        try:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=3)
        finally:
            for stream in (process.stdin, process.stdout):
                if stream is not None:
                    stream.close()

    def close(self) -> None:
        process = self._process
        if process is None:
            return
        try:
            if process.poll() is None and self._state != "done":
                try:
                    process.stdin.write(b"stop\n")
                    process.stdin.flush()
                except OSError:
                    pass
            process.wait(timeout=3)
            if process.returncode != 0:
                raise ProtocolError()
        except (OSError, ValueError, subprocess.TimeoutExpired):
            raise ProtocolError() from None
        finally:
            self._terminate()
            self._state = "closed"

    def __enter__(self) -> ScriptedBuilder:
        return self.start()

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        if exc_type is None:
            self.close()
        else:
            try:
                self.close()
            except (OSError, ProtocolError, subprocess.TimeoutExpired):
                pass
