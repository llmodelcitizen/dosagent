/*
 * protocol.c — Command dispatch and response helpers for DOSAGENT
 *
 * Parses incoming lines from the host-side Python client and dispatches
 * to the appropriate handler. Reuses escape/unescape pattern from
 * dos/client/src/protocol.c.
 *
 * Commands: PING, VER, EXEC, CWD, DOWNLOAD, UPLOAD, SCREEN, KEYTYPE
 * Responses: OK [json], ERR <msg>, STREAM <escaped>, END
 */

#include "agent.h"

/* ── Escape/unescape helpers ──────────────────────────────────── */

static char s_esc_buf[NET_BUF_SIZE];

/*
 * Escape text for sending: \n -> \\n, \\ -> \\\\, skip \r
 * Returns pointer to static buffer (not reentrant).
 */
const char *escape_text(const char *text)
{
    int i, j;
    int len = (int)strlen(text);

    for (i = 0, j = 0; i < len && j < NET_BUF_SIZE - 4; i++) {
        if (text[i] == '\\') {
            s_esc_buf[j++] = '\\';
            s_esc_buf[j++] = '\\';
        } else if (text[i] == '\n') {
            s_esc_buf[j++] = '\\';
            s_esc_buf[j++] = 'n';
        } else if (text[i] == '\r') {
            /* skip CR */
        } else {
            s_esc_buf[j++] = text[i];
        }
    }
    s_esc_buf[j] = '\0';
    return s_esc_buf;
}

/*
 * Unescape received text: \\n -> \n, \\\\ -> \\, skip \\r
 * Writes into dst buffer.
 */
void unescape_text(const char *src, char *dst, int dstsize)
{
    int i, j;

    for (i = 0, j = 0; src[i] && j < dstsize - 2; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            i++;
            switch (src[i]) {
            case 'n':  dst[j++] = '\n'; break;
            case 'r':  break;  /* skip \r */
            case 't':  dst[j++] = '\t'; break;
            case '\\': dst[j++] = '\\'; break;
            default:   dst[j++] = src[i]; break;
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* ── JSON helper ─────────────────────────────────────────────── */

/*
 * Escape a string for embedding in a JSON value (handles \ and ").
 * Writes into dst buffer.
 */
static void json_escape(const char *src, char *dst, int dstsize)
{
    int i, j;
    for (i = 0, j = 0; src[i] && j < dstsize - 2; i++) {
        if (src[i] == '\\' || src[i] == '"') {
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* ── Response helpers ─────────────────────────────────────────── */

void proto_send_ok(const char *json)
{
    char buf[NET_BUF_SIZE];

    if (json && json[0]) {
        sprintf(buf, "OK %s", json);
    } else {
        strcpy(buf, "OK");
    }
    net_send_line(buf);
}

void proto_send_error(const char *msg)
{
    char buf[NET_BUF_SIZE];
    sprintf(buf, "ERR %s", msg);
    net_send_line(buf);
}

void proto_send_stream(const char *text)
{
    char buf[NET_BUF_SIZE];
    const char *escaped = escape_text(text);
    sprintf(buf, "STREAM %s", escaped);
    net_send_line(buf);
}

void proto_send_end(void)
{
    net_send_line("END");
}

/* ── Command dispatch ─────────────────────────────────────────── */

void proto_handle_line(const char *line)
{
    char cmd[16];
    const char *rest;
    int i;

    /* Extract command word */
    for (i = 0; i < 15 && line[i] && line[i] != ' '; i++) {
        cmd[i] = line[i];
        /* uppercase */
        if (cmd[i] >= 'a' && cmd[i] <= 'z')
            cmd[i] -= 32;
    }
    cmd[i] = '\0';

    /* Skip whitespace after command */
    rest = line + i;
    while (*rest == ' ') rest++;

    /* ── PING ─────────────────────────────────────────────── */
    if (strcmp(cmd, "PING") == 0) {
        proto_send_ok("{\"status\":\"ok\"}");
        return;
    }

    /* ── VER ──────────────────────────────────────────────── */
    if (strcmp(cmd, "VER") == 0) {
        char json[128];
        sprintf(json, "{\"version\":\"%s\"}", AGENT_VERSION);
        proto_send_ok(json);
        return;
    }

    /* ── EXEC <command> ───────────────────────────────────── */
    if (strcmp(cmd, "EXEC") == 0) {
        if (*rest == '\0') {
            proto_send_error("EXEC requires a command");
        } else {
            exec_command(rest);
        }
        return;
    }

    /* ── CWD [path] ──────────────────────────────────────── */
    if (strcmp(cmd, "CWD") == 0) {
        if (*rest == '\0') {
            /* Return current directory */
            char json[256];
            char cwd[128];
            char esc[256];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                json_escape(cwd, esc, sizeof(esc));
                sprintf(json, "{\"cwd\":\"%s\"}", esc);
                proto_send_ok(json);
            } else {
                proto_send_error("Cannot get current directory");
            }
        } else {
            /* Change directory */
            if (chdir(rest) == 0) {
                char json[256];
                char cwd[128];
                char esc[256];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    json_escape(cwd, esc, sizeof(esc));
                    sprintf(json, "{\"cwd\":\"%s\"}", esc);
                    proto_send_ok(json);
                } else {
                    proto_send_ok(NULL);
                }
            } else {
                proto_send_error("Cannot change directory");
            }
        }
        return;
    }

    /* ── DOWNLOAD <path> ──────────────────────────────────── */
    if (strcmp(cmd, "DOWNLOAD") == 0) {
        if (*rest == '\0') {
            proto_send_error("DOWNLOAD requires a path");
        } else {
            file_download(rest);
        }
        return;
    }

    /* ── UPLOAD <path> ────────────────────────────────────── */
    if (strcmp(cmd, "UPLOAD") == 0) {
        if (*rest == '\0') {
            proto_send_error("UPLOAD requires path and data");
        } else {
            /* Format: UPLOAD <path>\n<escaped_data>
             * But since we're line-based, the data comes as escaped \n
             * within the same line: UPLOAD <path> <escaped_data>
             * Split on first space after path */
            const char *space;
            char path[128];
            int plen;

            space = strchr(rest, ' ');
            if (!space) {
                proto_send_error("UPLOAD requires path and data");
                return;
            }
            plen = (int)(space - rest);
            if (plen >= (int)sizeof(path))
                plen = sizeof(path) - 1;
            memcpy(path, rest, plen);
            path[plen] = '\0';

            space++;  /* skip the space */
            {
                char decoded[NET_LINE_SIZE];
                unescape_text(space, decoded, NET_LINE_SIZE);
                file_upload(path, decoded);
            }
        }
        return;
    }

    /* ── SCREEN (Phase 2) ─────────────────────────────────── */
    if (strcmp(cmd, "SCREEN") == 0) {
        screen_capture();
        return;
    }

    /* ── KEYTYPE (Phase 2) ────────────────────────────────── */
    if (strcmp(cmd, "KEYTYPE") == 0) {
        if (*rest == '\0') {
            proto_send_error("KEYTYPE requires text");
        } else {
            keys_type(rest);
        }
        return;
    }

    /* ── Unknown command ──────────────────────────────────── */
    {
        char errbuf[128];
        sprintf(errbuf, "Unknown command: %s", cmd);
        proto_send_error(errbuf);
    }
}
