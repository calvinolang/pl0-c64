/* Host graphics: records the clipped lines instead of drawing them. */
#include <stdio.h>
#include <string.h>
#include "core/clip.h"
#include "platform/gfx.h"
#include "host/gfx_host.h"

#define MAX_LINES 4096

static int16_t lines[MAX_LINES][4];
static int nlines;
static int dropped;
static int opened;

void gfx_host_reset(void) { nlines = 0; dropped = 0; opened = 0; }
void gfx_open(void)  { nlines = 0; dropped = 0; opened = 1; }
void gfx_close(void) { opened = 0; }

void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (!clip_line(&x0, &y0, &x1, &y1)) return;
    if (nlines >= MAX_LINES) { dropped++; return; }
    lines[nlines][0] = x0; lines[nlines][1] = y0;
    lines[nlines][2] = x1; lines[nlines][3] = y1;
    nlines++;
}

int gfx_host_line_count(void) { return nlines; }
int gfx_host_overflow(void) { return dropped; }
int gfx_host_is_open(void) { return opened; }

void gfx_host_line(int i, int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1)
{
    *x0 = lines[i][0]; *y0 = lines[i][1];
    *x1 = lines[i][2]; *y1 = lines[i][3];
}

int gfx_host_write_lines(const char *path)
{
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return -1;
    for (i = 0; i < nlines; i++)
        fprintf(f, "%d %d %d %d\n", lines[i][0], lines[i][1], lines[i][2], lines[i][3]);
    return fclose(f) == 0 ? 0 : -1;
}

static unsigned char bitmap[200][320];

static void plot(int x, int y)
{
    if (x >= 0 && x < 320 && y >= 0 && y < 200) bitmap[y][x] = 1;
}

/* Bresenham on the clipped (hence in-range) endpoints. */
static void draw(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;      /* -|dy| */
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int gfx_host_write_pbm(const char *path)
{
    FILE *f;
    int i, x, y;
    memset(bitmap, 0, sizeof bitmap);
    for (i = 0; i < nlines; i++)
        draw(lines[i][0], lines[i][1], lines[i][2], lines[i][3]);
    f = fopen(path, "w");
    if (!f) return -1;
    fputs("P1\n320 200\n", f);
    for (y = 0; y < 200; y++) {
        for (x = 0; x < 320; x++) fputc(bitmap[y][x] ? '1' : '0', f);
        fputc('\n', f);
    }
    return fclose(f) == 0 ? 0 : -1;
}
