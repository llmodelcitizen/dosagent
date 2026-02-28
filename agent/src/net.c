/*
 * net.c — WATTCP TCP server: listen, accept, poll, send/recv line-buffered
 *
 * Inverts the client pattern from dos/client/src/net.c:
 * instead of tcp_open() to connect, we tcp_listen() and wait for clients.
 */

#include "agent.h"
#include <tcp.h>

static tcp_Socket s_sock;
static int s_sock_open = 0;

/* Line receive buffer — accumulates bytes until \n */
static char s_line_buf[NET_LINE_SIZE];
static int  s_line_len = 0;

/* Send buffer */
static char s_send_buf[NET_BUF_SIZE];

int net_init(void)
{
    /* Initialize Watt-32 TCP/IP stack.
     * Reads WATTCP.CFG for IP configuration. */
    if (sock_init()) {
        return -1;
    }
    return 0;
}

void net_shutdown(void)
{
    if (s_sock_open) {
        sock_close(&s_sock);
        s_sock_open = 0;
    }
}

int net_listen(int port)
{
    /* Always close — in 32-bit protected mode the socket structure
     * must be fully cleaned up before tcp_listen() reuses it,
     * even if tcp_tick() already indicated the connection is gone. */
    sock_close(&s_sock);
    s_sock_open = 0;

    /* Listen for incoming TCP connections on the specified port.
     * tcp_listen(socket, local_port, remote_ip, remote_port, handler, buf_size)
     * remote_ip=0 means accept from any host. */
    if (!tcp_listen(&s_sock, (unsigned short)port, 0UL, 0, NULL, 0)) {
        return -1;
    }

    s_sock_open = 1;
    s_line_len = 0;
    g_agent.listening = 1;
    g_agent.connected = 0;

    return 0;
}

int net_wait_for_client(void)
{
    /* Block until a client connects (with tcp_tick polling).
     * sock_wait_established uses Watt-32's goto-label error mechanism. */
    sock_wait_established(&s_sock, 0, NULL, NULL);

    g_agent.listening = 0;
    g_agent.connected = 1;
    s_line_len = 0;

    return 0;

sock_err:
    s_sock_open = 0;
    g_agent.listening = 0;
    g_agent.connected = 0;
    return -1;
}

/*
 * Process received data byte-by-byte, assembling lines.
 * When a complete line (\n terminated) is found, dispatch it.
 */
static void process_recv(const char *data, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        char c = data[i];

        if (c == '\n') {
            /* Complete line received */
            if (s_line_len > 0 && s_line_buf[s_line_len - 1] == '\r')
                s_line_len--;
            s_line_buf[s_line_len] = '\0';
            if (s_line_len > 0) {
                proto_handle_line(s_line_buf);
            }
            s_line_len = 0;
        } else if (c != '\r') {
            /* Accumulate */
            if (s_line_len < NET_LINE_SIZE - 1) {
                s_line_buf[s_line_len++] = c;
            }
        }
    }
}

void net_poll(void)
{
    int len;
    char buf[512];

    if (!s_sock_open)
        return;

    /* Drive the TCP/IP stack; check if connection is still alive */
    if (!tcp_tick(&s_sock)) {
        /* Client disconnected or socket error */
        s_sock_open = 0;
        g_agent.connected = 0;
        g_agent.listening = 0;
        s_line_len = 0;
        return;
    }

    /* Check if a listening socket has become established */
    if (g_agent.listening && sock_established(&s_sock)) {
        g_agent.listening = 0;
        g_agent.connected = 1;
        s_line_len = 0;
    }

    /* Read available data */
    while (sock_dataready(&s_sock)) {
        len = sock_fastread(&s_sock, (unsigned char *)buf, sizeof(buf) - 1);
        if (len > 0) {
            process_recv(buf, len);
        } else if (len < 0) {
            s_sock_open = 0;
            g_agent.connected = 0;
            s_line_len = 0;
            return;
        } else {
            break;
        }
    }
}

void net_disconnect(void)
{
    sock_close(&s_sock);
    s_sock_open = 0;
    g_agent.connected = 0;
    g_agent.listening = 0;
    s_line_len = 0;
}

int net_send_line(const char *line)
{
    int len;

    if (!s_sock_open || !g_agent.connected)
        return -1;

    len = (int)strlen(line);
    if (len + 1 >= NET_BUF_SIZE)
        return -1;

    memcpy(s_send_buf, line, len);
    s_send_buf[len] = '\n';
    s_send_buf[len + 1] = '\0';

    sock_write(&s_sock, (unsigned char *)s_send_buf, len + 1);

    return 0;
}

int net_is_connected(void)
{
    return s_sock_open && g_agent.connected;
}
