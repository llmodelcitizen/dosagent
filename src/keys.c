/*
 * keys.c — Keyboard buffer injection (Phase 2 stub)
 *
 * When implemented, writes characters into the BIOS keyboard buffer
 * at 0x0040:001E, simulating keystrokes for the foreground program.
 */

#include "agent.h"

int keys_type(const char *text)
{
    (void)text;

    /* Phase 2: not yet implemented.
     *
     * Will stuff the BIOS keyboard buffer at 0040:001E
     * or use INT 16h AH=05h to insert keystrokes.
     */
    proto_send_error("KEYTYPE not implemented (Phase 2)");
    return -1;
}
