/* C64 implementation of the graphics layer (src/platform/gfx.h).
 *
 * Memory map (docs/architecture/spike-results.md section 5, verified):
 * VIC bank 3, hi-res bitmap at $E000-$FF3F (RAM under the KERNAL ROM),
 * colour matrix at $CC00-$CFE7 filled with $01 (high nibble = colour of set
 * bits: black 0; low nibble = colour of clear bits: white 1). Nothing in
 * $0801-$9FFF or $A000-$BFFF is used, and the text screen at $0400 (bank 0)
 * is never touched, so gfx_close only restores the VIC registers and the
 * editor's text, colours and cursor reappear as they were.
 *
 * gfx_open saves what it changes ($DD00 bank bits, $D011, $D016, $D018,
 * border $D020) the first time it is called after a close, so opening twice
 * (a second drawing) keeps the text-mode values. The background $D021 is
 * not used in bitmap mode and is left alone.
 *
 * gfx_line is in gfx_c64.s: it clips with the shared src/core/clip.c
 * (identical to the host) and runs the Bresenham loop with the KERNAL
 * banked out. Not reentrant. */
#include <string.h>
#include "platform/gfx.h"
#include "platform/gfx_c64.h"

#define REG(a)  (*(volatile uint8_t *)(a))
#define BITMAP  ((void *)0xE000)
#define MATRIX  ((void *)0xCC00)
#define NMI_VEC (*(uint16_t *)0xFFFA)   /* RAM vector, read only while the KERNAL is out */

static uint8_t s_dd00, s_d011, s_d016, s_d018, s_d020;
uint8_t gfx_shown;      /* the bitmap is on screen (gfx_c64.h) */
uint8_t gfx_hidden;     /* a drawing was switched off for input */

/* switch the VIC to the bitmap without clearing it (gfx_c64.h): the run
 * screen toggles between a finished drawing and the text with it */
void gfx_show(void)
{
    if (!gfx_shown) {
        s_dd00 = REG(0xDD00) & 3;
        s_d011 = REG(0xD011) & 0x7F;    /* bit 7 is raster bit 8: write 0 */
        s_d016 = REG(0xD016);
        s_d018 = REG(0xD018);
        s_d020 = REG(0xD020);
        gfx_shown = 1;
    }
    gfx_hidden = 0;
    REG(0xD020) = 1;                    /* white border */
    REG(0xD018) = 0x38;                 /* matrix $CC00 (3 << 4), bitmap at +$2000 */
    REG(0xDD00) = REG(0xDD00) & 0xFC;   /* VIC bank 3: $C000-$FFFF */
    REG(0xD016) = s_d016 & 0xEF;        /* hi-res, not multicolour */
    REG(0xD011) = 0x3B;                 /* bitmap mode, screen on, 25 rows */
}

/* the text view while a program reads input; gfx_line shows the drawing
 * again (gfx_hidden) at its next line */
void gfx_hide(void)
{
    if (gfx_shown) {
        gfx_close();
        gfx_hidden = 1;
    }
}

void gfx_open(void)
{
    memset(BITMAP, 0, 8000);            /* CPU writes go to RAM under the ROM */
    memset(MATRIX, 0x01, 1000);         /* black ink, white paper */
    /* RESTORE (NMI) while gfx_line has the KERNAL out would fetch the
     * vector from RAM: make it a no-op. $FFFA lies past the bitmap. */
    NMI_VEC = (uint16_t)gfx_nmi_rti;
    gfx_show();
}

void gfx_close(void)
{
    if (!gfx_shown) return;
    REG(0xD011) = s_d011;
    REG(0xD016) = s_d016;
    REG(0xDD00) = (REG(0xDD00) & 0xFC) | s_dd00;
    REG(0xD018) = s_d018;
    REG(0xD020) = s_d020;
    gfx_shown = 0;
}
