/*
 * exec.c — Execute DOS commands and capture output
 *
 * Strategy: write a batch file that redirects the command's output to a
 * temp file, then run the batch via system(). After completion, read the
 * temp file and send its contents as STREAM chunks, followed by END and
 * OK with the return code.
 *
 * This avoids dup2/freopen issues with WATTCP's internal state.
 */

#include "agent.h"

/* Chunk buffer for reading and streaming output */
static char s_chunk[EXEC_CHUNK_SIZE + 1];

int exec_command(const char *cmd)
{
    FILE *f;
    int rc;
    int n;

    /* Write batch file that runs the command with output redirection.
     * We redirect both stdout and stderr (2>&1 is not available in
     * COMMAND.COM, but > captures stdout which is the primary need). */
    f = fopen(TEMP_BATCH, "w");
    if (!f) {
        proto_send_error("Cannot create temp batch file");
        return -1;
    }
    fprintf(f, "@echo off\r\n");
    fprintf(f, "%s > %s\r\n", cmd, TEMP_STDOUT);
    fclose(f);

    /* Execute the batch file.
     * NOTE: tcp_tick() is NOT called during system(), so the TCP
     * connection may time out for long-running commands. The Python
     * client should use generous timeouts (60s+). */
    rc = system(TEMP_BATCH);

    /* Read and stream output in chunks — avoids needing a large buffer
     * that would eat conventional memory needed by system(). */
    f = fopen(TEMP_STDOUT, "rb");
    if (f) {
        while ((n = fread(s_chunk, 1, EXEC_CHUNK_SIZE, f)) > 0) {
            s_chunk[n] = '\0';
            proto_send_stream(s_chunk);
        }
        fclose(f);
    }

    /* Send END marker, then OK with return code */
    proto_send_end();

    {
        char json[64];
        sprintf(json, "{\"rc\":%d}", rc);
        proto_send_ok(json);
    }

    /* Clean up temp files */
    remove(TEMP_STDOUT);
    remove(TEMP_BATCH);

    return rc;
}
