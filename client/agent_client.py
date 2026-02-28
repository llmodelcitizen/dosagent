"""
agent_client.py — TCP client library for DOSAGENT (DOS Remote Agent)

Connects to DOSAGENT.EXE running in a FreeDOS VM (via 86Box SLiRP
port forwarding) and provides a Python API for executing DOS commands,
transferring files, and querying agent state.

Protocol is line-based ASCII, same escape/unescape pattern as DOSCODE.
"""

import json
import socket
import threading
import time
from dataclasses import dataclass
from typing import Optional, Tuple


def _escape(text: str) -> str:
    """Escape text for sending: newlines and backslashes."""
    result = text.replace('\\', '\\\\')
    result = result.replace('\n', '\\n')
    result = result.replace('\r', '\\r')
    return result


def _unescape(text: str) -> str:
    """Unescape received text: \\n -> newline, etc."""
    result = text.replace('\\\\', '\x00')
    result = result.replace('\\n', '\n')
    result = result.replace('\\r', '\r')
    result = result.replace('\x00', '\\')
    return result


@dataclass
class ExecResult:
    """Result of executing a DOS command."""
    output: str
    rc: int


class DosAgent:
    """TCP client for DOSAGENT.EXE running in FreeDOS."""

    KEEPALIVE_INTERVAL = 3  # seconds between keepalive PINGs

    def __init__(self, host: str = "localhost", port: int = 10000,
                 timeout: float = 60.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._sock: Optional[socket.socket] = None
        self._buf = ""
        self._lock = threading.Lock()
        self._keepalive_stop = threading.Event()
        self._keepalive_thread: Optional[threading.Thread] = None

    def connect(self) -> None:
        """Connect to the DOS agent."""
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.settimeout(self.timeout)
        self._sock.connect((self.host, self.port))
        self._buf = ""
        self._keepalive_stop.clear()
        self._keepalive_thread = threading.Thread(
            target=self._keepalive_loop, daemon=True)
        self._keepalive_thread.start()

    def close(self) -> None:
        """Close the connection."""
        self._keepalive_stop.set()
        if self._keepalive_thread:
            self._keepalive_thread.join(timeout=2)
            self._keepalive_thread = None
        if self._sock:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None
        self._buf = ""

    def _keepalive_loop(self) -> None:
        """Background thread: send PING every few seconds to prevent
        the agent's idle timeout from firing during interactive use."""
        while not self._keepalive_stop.wait(self.KEEPALIVE_INTERVAL):
            with self._lock:
                if not self._sock:
                    break
                try:
                    self._sock.sendall(b"PING\n")
                except OSError:
                    break

    def _send_line(self, line: str) -> None:
        """Send a line to the agent."""
        if not self._sock:
            raise ConnectionError("Not connected")
        with self._lock:
            self._sock.sendall((line + "\n").encode("ascii", errors="replace"))

    def _recv_line(self, accept_ping_ok=False) -> str:
        """Receive a single line from the agent.

        Keepalive PINGs may produce unsolicited 'OK ...' responses.
        Skip them unless accept_ping_ok is True (used by ping()).
        """
        if not self._sock:
            raise ConnectionError("Not connected")

        while True:
            while "\n" not in self._buf:
                data = self._sock.recv(4096)
                if not data:
                    raise ConnectionError("Connection closed by agent")
                self._buf += data.decode("ascii", errors="replace")

            line, self._buf = self._buf.split("\n", 1)
            line = line.rstrip("\r")

            # Skip unsolicited PING responses from keepalive
            if not accept_ping_ok and line == 'OK {"status":"ok"}':
                continue
            return line

    def _collect_stream(self) -> Tuple[str, dict]:
        """Collect STREAM lines until END, then parse the final OK/ERR.

        Returns (collected_text, ok_json_dict).
        Raises RuntimeError on ERR response.
        """
        chunks = []

        while True:
            line = self._recv_line()

            if line.startswith("STREAM "):
                text = _unescape(line[7:])
                chunks.append(text)

            elif line == "END":
                # Next line should be OK with JSON
                continue

            elif line.startswith("OK"):
                data_str = line[2:].strip()
                data = json.loads(data_str) if data_str else {}
                return "".join(chunks), data

            elif line.startswith("ERR"):
                msg = line[4:].strip() if len(line) > 4 else "Unknown error"
                raise RuntimeError(f"Agent error: {msg}")

            else:
                # Unexpected line — ignore
                continue

    def _simple_command(self, cmd: str) -> dict:
        """Send a command and expect a single OK/ERR response."""
        self._send_line(cmd)
        line = self._recv_line(accept_ping_ok=(cmd == "PING"))

        if line.startswith("OK"):
            data_str = line[2:].strip()
            return json.loads(data_str) if data_str else {}

        if line.startswith("ERR"):
            msg = line[4:].strip() if len(line) > 4 else "Unknown error"
            raise RuntimeError(f"Agent error: {msg}")

        raise RuntimeError(f"Unexpected response: {line}")

    def ping(self) -> dict:
        """Health check. Returns status dict."""
        return self._simple_command("PING")

    def ver(self) -> str:
        """Get agent version string."""
        data = self._simple_command("VER")
        return data.get("version", "unknown")

    def exec(self, cmd: str) -> ExecResult:
        """Execute a DOS command and return output + return code."""
        self._send_line(f"EXEC {cmd}")
        output, data = self._collect_stream()
        return ExecResult(output=output, rc=data.get("rc", -1))

    def cwd(self, path: Optional[str] = None) -> str:
        """Get or set current working directory."""
        if path:
            data = self._simple_command(f"CWD {path}")
        else:
            data = self._simple_command("CWD")
        return data.get("cwd", "")

    def download(self, path: str) -> str:
        """Download file contents from DOS."""
        self._send_line(f"DOWNLOAD {path}")
        content, data = self._collect_stream()
        return content

    def upload(self, path: str, content: str) -> int:
        """Upload file to DOS. Returns bytes written."""
        escaped = _escape(content)
        data = self._simple_command(f"UPLOAD {path} {escaped}")
        return data.get("size", 0)

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *exc):
        self.close()
