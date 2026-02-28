/*
 * file.c — File transfer operations for DOSAGENT
 *
 * DOWNLOAD: read file contents and send as STREAM chunks
 * UPLOAD: receive data and write to file
 */

#include "agent.h"

/* Chunk buffer for reading and streaming file contents */
static char s_chunk[EXEC_CHUNK_SIZE + 1];

int file_download(const char *path)
{
    FILE *f;
    long total = 0;
    int n;

    f = fopen(path, "rb");
    if (!f) {
        proto_send_error("Cannot open file");
        return -1;
    }

    /* Read and stream in chunks */
    while ((n = fread(s_chunk, 1, EXEC_CHUNK_SIZE, f)) > 0) {
        s_chunk[n] = '\0';
        proto_send_stream(s_chunk);
        total += n;
    }
    fclose(f);

    proto_send_end();
    {
        char json[128];
        sprintf(json, "{\"size\":%ld}", total);
        proto_send_ok(json);
    }

    return 0;
}

int file_upload(const char *path, const char *data)
{
    FILE *f;
    int len;

    f = fopen(path, "wb");
    if (!f) {
        proto_send_error("Cannot create file");
        return -1;
    }

    len = (int)strlen(data);
    fwrite(data, 1, len, f);
    fclose(f);

    {
        char json[128];
        sprintf(json, "{\"size\":%d}", len);
        proto_send_ok(json);
    }

    return 0;
}
