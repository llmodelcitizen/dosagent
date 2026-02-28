/*
 * main.c — Entry point for DOSAGENT (DOS Remote Agent)
 *
 * Usage: DOSAGENT.EXE [port]
 *
 * Listens for TCP connections, accepts one client at a time,
 * processes commands in a polling loop, then re-listens on disconnect.
 * Press ESC to quit.
 */

#include "agent.h"

/* Global agent state */
AGENT_STATE g_agent;

static void print_banner(void)
{
    printf("DOSAGENT v%s - DOS Remote Agent\r\n", AGENT_VERSION);
    printf("Press ESC to quit.\r\n\r\n");
}

static void print_mem_info(void)
{
    printf("Running in 32-bit protected mode (DOS/32A)\r\n\r\n");
}

int main(int argc, char *argv[])
{
    int port = AGENT_PORT;

    /* Parse optional port argument */
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0)
            port = AGENT_PORT;
    }

    /* Initialize state */
    memset(&g_agent, 0, sizeof(g_agent));
    g_agent.port = port;
    getcwd(g_agent.cwd, sizeof(g_agent.cwd));

    print_banner();
    print_mem_info();

    /* Initialize WATTCP stack */
    printf("Initializing TCP/IP stack...\r\n");
    if (net_init() != 0) {
        printf("Error: Failed to initialize TCP/IP.\r\n");
        printf("Check WATTCP.CFG configuration.\r\n");
        return 1;
    }
    printf("TCP/IP initialized.\r\n");

    /* Main accept loop — listen, serve one client, repeat */
    while (!g_agent.quit) {
        printf("Listening on port %d...\r\n", port);

        if (net_listen(port) != 0) {
            printf("Error: Cannot listen on port %d.\r\n", port);
            break;
        }

        /* Wait for a client to connect (poll-based, check ESC) */
        while (g_agent.listening && !g_agent.quit) {
            net_poll();

            /* Check if connection was established during poll */
            if (g_agent.connected)
                break;

            /* Check for ESC key */
            if (kbhit()) {
                int key = getch();
                if (key == 27) {
                    g_agent.quit = 1;
                    break;
                }
            }
        }

        if (g_agent.quit)
            break;

        if (!g_agent.connected) {
            /* net_wait_for_client handles the established state */
            if (net_wait_for_client() != 0) {
                printf("Error: Connection failed.\r\n");
                net_disconnect();
                continue;
            }
        }

        printf("Client connected.\r\n");

        /* Command processing loop */
        while (g_agent.connected && !g_agent.quit) {
            net_poll();

            /* Check for ESC key */
            if (kbhit()) {
                int key = getch();
                if (key == 27) {
                    g_agent.quit = 1;
                    break;
                }
            }
        }

        printf("Client disconnected.\r\n");
        net_disconnect();
    }

    printf("Shutting down.\r\n");
    net_shutdown();

    return 0;
}
