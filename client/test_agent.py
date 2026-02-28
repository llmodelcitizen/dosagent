#!/usr/bin/env python3
"""
test_agent.py — Layer-by-layer test suite for DOSAGENT

Tests are ordered from lowest-level (raw TCP) to highest (full commands),
so failures pinpoint exactly which layer is broken.

Usage: ./test_agent.py [host] [port]
       ./test_agent.py 192.168.10.211
       ./test_agent.py 192.168.10.211 10000
       ./test_agent.py --test 2 192.168.10.211
"""

import argparse
import socket
import sys
import time

from agent_client import DosAgent


# -- Helpers ------------------------------------------------------------------

def raw_connect(host, port, timeout=5.0):
    """Open a plain TCP socket."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((host, port))
    return s


def raw_send_recv(sock, line, timeout=5.0):
    """Send a line and receive until \\n."""
    sock.settimeout(timeout)
    sock.sendall((line + "\n").encode("ascii"))
    buf = ""
    while "\n" not in buf:
        data = sock.recv(4096)
        if not data:
            raise ConnectionError("Connection closed")
        buf += data.decode("ascii", errors="replace")
    return buf.split("\n", 1)[0].rstrip("\r")


# -- Tests --------------------------------------------------------------------

def test_tcp_connect(host, port):
    """TCP connect — 3-way handshake completes"""
    s = raw_connect(host, port, timeout=5.0)
    s.close()
    # Brief pause so agent can cycle back to listen state
    time.sleep(1)


def test_raw_ping(host, port):
    """Raw PING — send PING, recv OK over raw socket"""
    s = raw_connect(host, port, timeout=5.0)
    try:
        resp = raw_send_recv(s, "PING", timeout=5.0)
        assert resp == 'OK {"status":"ok"}', f"Expected OK ping response, got: {resp!r}"
    finally:
        s.close()
    time.sleep(1)


def test_client_ping(host, port):
    """DosAgent.ping() — client library connect + ping"""
    agent = DosAgent(host=host, port=port, timeout=10.0)
    try:
        agent.connect()
        data = agent.ping()
        assert data.get("status") == "ok", f"Unexpected ping data: {data}"
    finally:
        agent.close()
    time.sleep(1)


def test_ver(host, port):
    """DosAgent.ver() — VER returns version string"""
    agent = DosAgent(host=host, port=port, timeout=5.0)
    try:
        agent.connect()
        ver = agent.ver()
        assert isinstance(ver, str) and len(ver) > 0, f"Bad version: {ver!r}"
    finally:
        agent.close()
    time.sleep(1)


def test_cwd(host, port):
    """DosAgent.cwd() — CWD returns a directory path"""
    agent = DosAgent(host=host, port=port, timeout=5.0)
    try:
        agent.connect()
        cwd = agent.cwd()
        assert isinstance(cwd, str) and len(cwd) > 0, f"Bad cwd: {cwd!r}"
    finally:
        agent.close()
    time.sleep(1)


def test_exec_dir(host, port):
    """DosAgent.exec("DIR") — EXEC with STREAM/END/OK flow"""
    agent = DosAgent(host=host, port=port, timeout=15.0)
    try:
        agent.connect()
        result = agent.exec("DIR")
        assert len(result.output) > 0, "DIR produced no output"
        assert result.rc == 0, f"DIR returned rc={result.rc}"
    finally:
        agent.close()
    time.sleep(1)


def test_upload_download(host, port):
    """Upload + download — round-trip file write/read"""
    test_content = "Hello from test_agent.py\nLine 2\n"
    test_path = "C:\\TESTFILE.TXT"

    agent = DosAgent(host=host, port=port, timeout=10.0)
    try:
        agent.connect()
        size = agent.upload(test_path, test_content)
        assert size > 0, f"Upload returned size={size}"

        content = agent.download(test_path)
        # Normalize line endings — DOS writes \r\n, protocol may strip \r
        norm = lambda s: s.replace('\r\n', '\n').strip()
        assert norm(content) == norm(test_content), \
            f"Download mismatch: {content!r}"
    finally:
        agent.close()
    time.sleep(1)


def test_reconnect(host, port):
    """Reconnect — disconnect, reconnect, ping again"""
    agent = DosAgent(host=host, port=port, timeout=10.0)
    try:
        agent.connect()
        data = agent.ping()
        assert data.get("status") == "ok"
    finally:
        agent.close()

    time.sleep(2)

    agent2 = DosAgent(host=host, port=port, timeout=10.0)
    try:
        agent2.connect()
        data = agent2.ping()
        assert data.get("status") == "ok", f"Reconnect ping failed: {data}"
    finally:
        agent2.close()
    time.sleep(1)


def test_keepalive(host, port):
    """Keepalive — hold connection 10s, then ping"""
    agent = DosAgent(host=host, port=port, timeout=15.0)
    try:
        agent.connect()
        # Hold the connection open; keepalive thread sends PINGs
        time.sleep(10)
        # Should still be alive
        data = agent.ping()
        assert data.get("status") == "ok", f"Post-keepalive ping failed: {data}"
    finally:
        agent.close()
    time.sleep(1)


# -- Runner -------------------------------------------------------------------

TESTS = [
    ("TCP connect",       test_tcp_connect),
    ("Raw PING",          test_raw_ping),
    ("DosAgent.ping()",   test_client_ping),
    ("DosAgent.ver()",    test_ver),
    ("DosAgent.cwd()",    test_cwd),
    ("DosAgent.exec()",   test_exec_dir),
    ("Upload + download", test_upload_download),
    ("Reconnect",         test_reconnect),
    ("Keepalive",         test_keepalive),
]


def run_test(num, name, fn, host, port):
    """Run a single test, print result, return True on pass."""
    sys.stdout.write(f"  [{num}] {name} ... ")
    sys.stdout.flush()

    t0 = time.time()
    try:
        fn(host, port)
        elapsed = time.time() - t0
        print(f"PASS ({elapsed:.1f}s)")
        return True
    except Exception as e:
        elapsed = time.time() - t0
        print(f"FAIL ({elapsed:.1f}s)")
        print(f"       {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Layer-by-layer test suite for DOSAGENT")
    parser.add_argument("host", nargs="?", default="localhost",
                        help="Agent host (default: localhost)")
    parser.add_argument("port", nargs="?", type=int, default=10000,
                        help="Agent port (default: 10000)")
    parser.add_argument("--test", type=int, metavar="N",
                        help="Run only test N (1-9)")
    args = parser.parse_args()

    print(f"DOSAGENT test suite — {args.host}:{args.port}")
    print()

    if args.test:
        idx = args.test - 1
        if idx < 0 or idx >= len(TESTS):
            print(f"Error: test number must be 1-{len(TESTS)}")
            sys.exit(1)
        name, fn = TESTS[idx]
        ok = run_test(args.test, name, fn, args.host, args.port)
        sys.exit(0 if ok else 1)

    passed = 0
    failed = 0

    for i, (name, fn) in enumerate(TESTS, 1):
        if run_test(i, name, fn, args.host, args.port):
            passed += 1
        else:
            failed += 1
            break  # stop on first failure

    print()
    total = passed + failed
    if failed:
        print(f"FAILED — {passed}/{total} passed")
        sys.exit(1)
    else:
        print(f"ALL PASS — {passed}/{total}")
        sys.exit(0)


if __name__ == "__main__":
    main()
