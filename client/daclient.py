#!/usr/bin/env python3
"""
da — Interactive shell for DOSAGENT (DOS Remote Agent)

Usage: da [host] [port]

Connects to DOSAGENT.EXE running in a FreeDOS VM and provides an
interactive REPL for executing DOS commands.

Defaults are read from ~/.config/dosagent.conf if it exists,
otherwise falls back to localhost:10000. CLI args override both.

Special commands:
    .quit / .exit   - Disconnect and exit
    .ping           - Health check
    .ver            - Agent version
    .cwd [path]     - Get/set working directory
    .download path  - Download and display file
    .upload path    - Upload file (reads from stdin until blank line)
    .help           - Show available commands
"""

import argparse
import configparser
import os
import sys

from .agent_client import DosAgent

CONFIG_PATH = os.path.expanduser("~/.config/dosagent.conf")

DEFAULTS = {
    "host": "localhost",
    "port": 10000,
}


def load_config():
    """Load host/port defaults from ~/.config/dosagent.conf if it exists."""
    config = dict(DEFAULTS)
    if not os.path.isfile(CONFIG_PATH):
        return config
    cp = configparser.ConfigParser()
    cp.read(CONFIG_PATH)
    if cp.has_option("agent", "host"):
        config["host"] = cp.get("agent", "host")
    if cp.has_option("agent", "port"):
        try:
            config["port"] = cp.getint("agent", "port")
        except ValueError:
            pass
    return config


def print_help():
    print("DOSAGENT Interactive Shell")
    print()
    print("Type any DOS command to execute it (e.g., DIR, TYPE, COPY).")
    print()
    print("Special commands:")
    print("  .quit / .exit   Disconnect and exit")
    print("  .ping           Health check")
    print("  .ver            Agent version")
    print("  .cwd [path]     Get/set working directory")
    print("  .download path  Download and display file contents")
    print("  .upload path    Upload file (type content, blank line to end)")
    print("  .help           Show this help")
    print()


def main():
    cfg = load_config()
    parser = argparse.ArgumentParser(description="DOSAGENT interactive shell")
    parser.add_argument("host", nargs="?", default=cfg["host"],
                        help=f"Agent host (default: {cfg['host']})")
    parser.add_argument("port", nargs="?", type=int, default=cfg["port"],
                        help=f"Agent port (default: {cfg['port']})")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="Command timeout in seconds (default: 60)")
    args = parser.parse_args()

    agent = DosAgent(host=args.host, port=args.port, timeout=args.timeout)

    print(f"Connecting to {args.host}:{args.port}...")
    try:
        agent.connect()
    except Exception as e:
        print(f"Connection failed: {e}")
        sys.exit(1)

    # Verify connection
    try:
        data = agent.ping()
        ver = agent.ver()
        print(f"Connected to DOSAGENT v{ver}")
    except Exception as e:
        print(f"Agent handshake failed: {e}")
        agent.close()
        sys.exit(1)

    cwd = agent.cwd()
    print(f"Working directory: {cwd}")
    print("Type .help for commands, .quit to exit.")
    print()

    # REPL
    while True:
        try:
            prompt = f"{cwd}> "
            line = input(prompt)
        except (EOFError, KeyboardInterrupt):
            print()
            break

        line = line.strip()
        if not line:
            continue

        # Special commands
        if line.lower() in (".quit", ".exit"):
            break

        if line.lower() == ".help":
            print_help()
            continue

        if line.lower() == ".ping":
            try:
                data = agent.ping()
                print(f"PONG: {data}")
            except Exception as e:
                print(f"Error: {e}")
            continue

        if line.lower() == ".ver":
            try:
                ver = agent.ver()
                print(f"Agent version: {ver}")
            except Exception as e:
                print(f"Error: {e}")
            continue

        if line.lower().startswith(".cwd"):
            path = line[4:].strip()
            try:
                cwd = agent.cwd(path if path else None)
                print(f"Directory: {cwd}")
            except Exception as e:
                print(f"Error: {e}")
            continue

        if line.lower().startswith(".download "):
            path = line[10:].strip()
            if not path:
                print("Usage: .download <path>")
                continue
            try:
                content = agent.download(path)
                print(content)
            except Exception as e:
                print(f"Error: {e}")
            continue

        if line.lower().startswith(".upload "):
            path = line[8:].strip()
            if not path:
                print("Usage: .upload <path>")
                continue
            print("Enter file content (blank line to end):")
            lines = []
            while True:
                try:
                    l = input()
                except (EOFError, KeyboardInterrupt):
                    break
                if l == "":
                    break
                lines.append(l)
            content = "\n".join(lines) + "\n"
            try:
                size = agent.upload(path, content)
                print(f"Uploaded {size} bytes to {path}")
            except Exception as e:
                print(f"Error: {e}")
            continue

        # Regular DOS command — execute via EXEC
        try:
            result = agent.exec(line)
            if result.output:
                print(result.output, end="")
                if not result.output.endswith("\n"):
                    print()
            if result.rc != 0:
                print(f"[Exit code: {result.rc}]")
        except Exception as e:
            print(f"Error: {e}")

    agent.close()
    print("Disconnected.")


if __name__ == "__main__":
    main()
