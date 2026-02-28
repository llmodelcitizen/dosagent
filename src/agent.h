/*
 * agent.h — Master header for DOSAGENT (DOS Remote Agent)
 * OpenWatcom C, DOS 32-bit flat model (DOS/32A extender)
 *
 * TCP server that accepts commands from a host-side Python client,
 * executes DOS commands, and returns output over the network.
 * Uses Watt-32 (WATTCP) for TCP/IP over SLiRP.
 */

#ifndef AGENT_H
#define AGENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <process.h>

/* ── Network configuration ────────────────────────────────────── */

#define AGENT_PORT      10000
#define NET_BUF_SIZE    4096
#define NET_LINE_SIZE   4096

/* ── Command execution ────────────────────────────────────────── */

#define TEMP_STDOUT     "C:\\TEMP\\AOUT.TMP"
#define TEMP_BATCH      "C:\\TEMP\\ARUN.BAT"
#define EXEC_CHUNK_SIZE 2048        /* read/send in 2 KB chunks */

/* ── Screen capture (Phase 2) ─────────────────────────────────── */

#define VGA_ADDR        0xB8000     /* linear address in 32-bit mode */
#define SCREEN_COLS     80
#define SCREEN_ROWS     25

/* ── Version ──────────────────────────────────────────────────── */

#define AGENT_VERSION   "0.1.0"

/* ── Agent state ──────────────────────────────────────────────── */

typedef struct {
    int     listening;          /* 1 = waiting for client */
    int     connected;          /* 1 = client connected */
    int     quit;               /* 1 = exit requested */
    int     port;               /* listen port */
    char    cwd[128];           /* current working directory */
} AGENT_STATE;

extern AGENT_STATE g_agent;

/* ── net.c — WATTCP TCP server ────────────────────────────────── */

int  net_init(void);
void net_shutdown(void);
int  net_listen(int port);
int  net_wait_for_client(void);
void net_poll(void);
void net_disconnect(void);
int  net_send_line(const char *line);
int  net_is_connected(void);

/* ── protocol.c — Command dispatch + responses ────────────────── */

void proto_handle_line(const char *line);

/* Response helpers */
void proto_send_ok(const char *json);
void proto_send_error(const char *msg);
void proto_send_stream(const char *text);
void proto_send_end(void);

/* Escape/unescape helpers */
const char *escape_text(const char *text);
void unescape_text(const char *src, char *dst, int dstsize);

/* ── exec.c — Command execution ───────────────────────────────── */

int  exec_command(const char *cmd);

/* ── file.c — File operations ─────────────────────────────────── */

int  file_download(const char *path);
int  file_upload(const char *path, const char *data);

/* ── screen.c — VGA text capture (Phase 2 stub) ──────────────── */

int  screen_capture(void);

/* ── keys.c — Keyboard injection (Phase 2 stub) ──────────────── */

int  keys_type(const char *text);

#endif /* AGENT_H */
