# dosagent

A **proof-of-concept** remote agent for controlling DOS systems over TCP/IP. A tiny C-based server
(`DOSAGENT.EXE`) runs inside DOS. A Python client (`da`) on the host sends commands, executes programs, and transfers files.

## Overview

**`DOSAGENT.EXE`** runs inside a physical DOS machine or a VM like [86Box](https://86box.net/) with SLiRP networking. The agent listens on TCP port 10000 and executes commands on behalf of the client.

The agent is built with the OpenWatcom compiler and runs in 32-bit protected mode via the DOS/32A extender. The agent code and the Watt-32 TCP/IP stack live in extended memory (above 1 MB). About 140 KB of conventional memory is used: the DOS/32A real-mode stub (~50 KB), dosagent's own static buffers and stack (~32 KB), and the rest are Watt-32's packet driver interface.

**`da`** is the Python CLI tool on the host that connects to the agent and provides interactive command and control. The client uses only Python standard library modules (socket, threading, configparser, argparse) — no external dependencies.

## Tested configuration

- **FreeDOS** running as a VM in [86Box](https://86box.net/)
- **SLiRP networking** with port forwarding for TCP port 10000
- **WSL2 Linux host** with port forwarding for TCP port 10000

### Your C2

<img width="803" height="1605" alt="dosagent1" src="https://github.com/user-attachments/assets/0c5568e2-7996-4c8e-b4b3-d0b73267b6d5" />

### `DOSAGENT.EXE` running in FreeDOS

<img width="1252" height="1033" alt="dosagent2" src="https://github.com/user-attachments/assets/918dd0d7-24f0-464d-b1a8-a1516e83a395" />

## Setup

Run the setup script from the project root:

```bash
./setup.sh
```

This performs 4 steps:

1. **Installs the OpenWatcom v2** compiler to `scripts/watcom/`
2. **Builds Watt-32** TCP/IP library (32-bit flat model) to `scripts/lib/watt32/`
3. **Compiles DOSAGENT.EXE** to `agent/bin/`
4. **Installs the `da` command** with interactive prompts for install directory (default `~/.local/bin`), host, and port

To uninstall everything:

```bash
./setup.sh --uninstall
```
This removes the `da` wrapper script, `~/.config/dosagent.conf`, the Watt-32 library, and the OpenWatcom compiler directory.

## Repository structure

```
dosagent/
├── setup.sh                    # Full setup: compiler, TCP lib, build, client install
├── agent/                      # DOS agent (C source + build)
│   ├── build.sh                # Compile DOSAGENT.EXE
│   ├── Makefile                # OpenWatcom wmake Makefile
│   └── src/                    # C source files
│       ├── agent.h             # Master header, constants, prototypes
│       ├── main.c              # Entry point, TCP listen loop
│       ├── net.c               # Watt-32 TCP server
│       ├── protocol.c          # Command dispatch, response helpers, escape/unescape
│       ├── exec.c              # DOS command execution via temp batch file
│       ├── file.c              # File upload/download
│       ├── screen.c            # VGA text capture (stub)
│       └── keys.c              # Keyboard injection (stub)
├── client/                     # Python client library + CLI
│   ├── __init__.py             # Python package marker
│   ├── agent_client.py         # DosAgent class / TCP client library
│   ├── daclient.py             # The primary interactive
│   └── test_agent.py           # Layer-by-layer test suite (9 tests)
└── scripts/                    # Build toolchain installers
    ├── install_compiler.sh     # Installs the OpenWatcom v2 portable
    ├── install_tcplib.sh       # Builds the Watt-32 TCP/IP library (32-bit)
    └── install_client.sh       # Installs the 'da' command and config file
```

## Configuration

### Host side

#### `~/.config/dosagent.conf`
This file is automatically created by `./setup.sh` in INI format:

```ini
[agent]
host = localhost
port = 10000
```

The `da` client loads defaults from this file. CLI arguments override it:

```
da [host] [port] [--timeout SECONDS]
```

### DOS side

#### `C:\TEMP` directory
Must exist inside the DOS VM

#### `WATTCP.CFG`
If needed, your 86Box / SLiRP config should exist in the same directory as `DOSAGENT.EXE` or in a location Watt-32 searches.

```
my_ip = 10.0.2.15
netmask = 255.255.255.0
gateway = 10.0.2.2
```

## Protocol

The agent speaks a line-based ASCII protocol over TCP. Lines are `\n`-delimited. Newlines and backslashes in payloads are escaped (`\n` → `\n` literal, `\\` → `\\`).

### Commands

| Command | Response |
|--------|----------|
| `PING` | `OK {"status":"ok"}` |
| `VER` | `OK {"version":"0.2.0"}` |
| `EXEC <command>` | `STREAM` lines → `END` → `OK {"rc":<n>}` |
| `CWD` or `CWD <path>` | `OK {"cwd":"<path>"}` |
| `DOWNLOAD <path>` | `STREAM` lines → `END` → `OK {"size":<n>}` |
| `UPLOAD <path> <data>` | `OK {"size":<n>}` |

Commands are case-insensitive. `SCREEN` and `KEYTYPE` are reserved for a future release.

### Response prefixes

- `OK` or `OK <json>` : success
- `ERR <message>` : error
- `STREAM <escaped_data>` : one chunk of streamed output
- `END` : end of stream (followed by a final `OK` line)

### EXEC details

EXEC writes the command to a temporary batch file, runs it via `COMMAND.COM`, and streams stdout back in 2048-byte chunks. Only stdout is captured (DOS `COMMAND.COM` does not support `2>&1`). TCP keepalives are not sent during execution, so the client should use a generous timeout (default: 60s).

## The `da` interactive

Once connected, `da` presents a REPL with the DOS working directory as the initial prompt. Any input that doesn't start with `.` is sent to the agent as a DOS command via `EXEC`. Dot-commands provide extra functionality:

| Command | Description |
|--------|-------------|
| `.help` | Show help |
| `.ping` | Ping the agent |
| `.ver` | Show agent version |
| `.cwd [path]` | Get or change working directory |
| `.download <path>` | Download and display a file |
| `.upload <path>` | Upload content (type lines, end with a blank line) |
| `.quit` / `.exit` | Disconnect |

## Testing

The test suite runs 9 layer-by-layer tests, ordered from lowest level (raw TCP) to highest (full commands). Failures pinpoint exactly which layer is broken.

```bash
cd client
./test_agent.py                         # localhost:10000
./test_agent.py 192.168.10.211          # custom host
./test_agent.py 192.168.10.211 10000    # custom host + port
./test_agent.py --test 3                # run only test 3
```

### Tests

| #  | Name              | What it tests                                    |
|----|-------------------|--------------------------------------------------|
| 1  | TCP connect       | Raw 3-way handshake completes                    |
| 2  | Raw PING          | Send PING over raw socket, expect OK response    |
| 3  | DosAgent.ping()   | Client library connect + ping                    |
| 4  | DosAgent.ver()    | VER returns a version string                     |
| 5  | DosAgent.cwd()    | CWD returns a directory path                     |
| 6  | DosAgent.exec()   | EXEC DIR with STREAM/END/OK flow                 |
| 7  | Upload + download | Round-trip file write/read to `C:\TEMP\TESTFILE.TXT`  |
| 8  | Reconnect         | Disconnect, reconnect, ping again                |
| 9  | Keepalive         | Hold connection 10s, then ping                   |

Tests stop on first failure. Output format: `ALL PASS — 9/9` or `FAILED — N/9`.

## Roadmap

- **SCREEN** — capture the VGA text buffer (80x25 at `0xB8000`) and return it to the client
- **KEYTYPE** — inject keystrokes into the BIOS keyboard buffer for automating interactive DOS programs
