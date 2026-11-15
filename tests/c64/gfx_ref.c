/* gfx_ref.c -- host reference for tests/c64/gfx_check.c: draws the same
 * lines (gfx_lines.h) with the host renderer (src/host/gfx_host.c, which
 * clips with the shared src/core/clip.c) and writes
 *   PREFIX.pbm    the host bitmap (Bresenham of gfx_host_write_pbm)
 *   PREFIX.lines  the clipped endpoints, one "x0 y0 x1 y1" per visible line
 * for tools/check_bitmap.py. Usage: gfx_ref PREFIX */
#include <stdio.h>
#include <string.h>
#include "platform/gfx.h"
#include "host/gfx_host.h"
#include "c64/gfx_lines.h"

int main(int argc, char **argv)
{
    char path[512];
    unsigned i;
    if (argc != 2 || strlen(argv[1]) > 500) {
        fprintf(stderr, "usage: gfx_ref PREFIX\n");
        return 2;
    }
    gfx_open();
    for (i = 0; i < GFX_NLINES; i++)
        gfx_line(gfx_lines[i][0], gfx_lines[i][1], gfx_lines[i][2], gfx_lines[i][3]);
    sprintf(path, "%s.pbm", argv[1]);
    if (gfx_host_write_pbm(path)) return 1;
    sprintf(path, "%s.lines", argv[1]);
    if (gfx_host_write_lines(path)) return 1;
    return 0;
}
