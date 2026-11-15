/* gfxtest.c -- SPIKE: hires bitmap in VIC bank 3 with the bitmap in RAM
 * under the KERNAL ROM ($E000-$FF3F) and the colour matrix at $CC00.
 * Nothing touches $0801-$9FFF (program) or $A000-$BFFF (BASIC ROM), and
 * the text screen at $0400 is left alone, so switching back is instant. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BITMAP ((unsigned char *)0xE000)   /* 8000 bytes, under KERNAL */
#define MATRIX ((unsigned char *)0xCC00)   /* 1000 bytes */
#define POKE(a, v) (*(volatile unsigned char *)(a) = (v))
#define PEEK(a)    (*(volatile unsigned char *)(a))

void __fastcall__ bm_or(unsigned char *addr);   /* gfx.s: RMW with KERNAL out */
extern unsigned char bm_mask;

static unsigned char save_dd00, save_d011, save_d018, save_d020, save_d021;

void gfx_open(void)
{
    save_dd00 = PEEK(0xDD00); save_d011 = PEEK(0xD011); save_d018 = PEEK(0xD018);
    save_d020 = PEEK(0xD020); save_d021 = PEEK(0xD021);
    memset(BITMAP, 0, 8000);              /* writes go to RAM under the ROM */
    memset(MATRIX, 0x01, 1000);           /* ink black (hi nibble), paper white (lo) */
    POKE(0xD020, 1);                      /* white border */
    POKE(0xDD00, (PEEK(0xDD00) & 0xFC) | 0x00);   /* VIC bank 3: $C000-$FFFF */
    POKE(0xD018, 0x38);                   /* matrix $CC00 (3<<4), bitmap +$2000 */
    POKE(0xD011, 0x3B);                   /* BMM on */
}

void gfx_close(void)
{
    POKE(0xD011, save_d011);              /* $1B */
    POKE(0xD018, save_d018);              /* $15 (uppercase) / $17 (lowercase) */
    POKE(0xDD00, (PEEK(0xDD00) & 0xFC) | (save_dd00 & 3));   /* bank 0 */
    POKE(0xD020, save_d020); POKE(0xD021, save_d021);
}

static void plot(int x, int y)
{
    if ((unsigned)x >= 320 || (unsigned)y >= 200) return;
    bm_mask = 0x80 >> (x & 7);
    bm_or(BITMAP + (unsigned)(y >> 3) * 320 + (x & ~7) + (y & 7));
}

static void line(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* run.sh puts breakpoints on these and takes screenshots */
void shown_bitmap(void) { }
void shown_text(void) { }
void spike_done(void) { for (;;) ; }

int main(void)
{
    int i;
    puts("text screen: this must survive the drawing");
    gfx_open();
    line(0, 0, 319, 0); line(319, 0, 319, 199); line(319, 199, 0, 199); line(0, 199, 0, 0);
    line(0, 0, 319, 199); line(0, 199, 319, 0);
    for (i = 0; i < 360; i += 30) line(160, 100, 160 + i / 4, 20 + i / 3);
    shown_bitmap();
    gfx_close();
    for (i = 0; i < 3; ++i) {             /* let VICE render a text frame */
        unsigned char j = PEEK(0xA2);     /* jiffy clock low byte */
        while (PEEK(0xA2) == j) ;
    }
    shown_text();
    spike_done();
    return 0;
}
