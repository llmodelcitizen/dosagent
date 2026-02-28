/*
 * screen.c — VGA text buffer capture (Phase 2 stub)
 *
 * When implemented, reads the 80x25 VGA text buffer at segment 0xB800
 * and sends it as STREAM data. Each cell is 2 bytes: char + attribute.
 *
 * Pattern from dos/client/src/video.c: MK_FP(VGA_SEG, 0)
 */

#include "agent.h"

int screen_capture(void)
{
    /* Phase 2: not yet implemented.
     *
     * Will do:
     *   unsigned short far *vga = (unsigned short far *)MK_FP(VGA_SEG, 0);
     *   for each row 0..24:
     *     extract 80 chars (low byte of each word)
     *     send as STREAM line
     *   send END
     */
    proto_send_error("SCREEN not implemented (Phase 2)");
    return -1;
}
