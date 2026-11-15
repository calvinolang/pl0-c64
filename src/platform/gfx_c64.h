#ifndef PL0_GFX_C64_H
#define PL0_GFX_C64_H

/* C64-only extra of the graphics layer (src/c64/gfx_c64.c); not part of
 * the portable gfx.h. Show the bitmap as it is (gfx_open shows it cleared):
 * after a run, SPACE toggles between the drawing (gfx_show) and the text
 * screen (gfx_close), which the bitmap never touches. */
void gfx_show(void);
/* Back to the text screen while a program reads input (io_c64.c calls it
 * through io_before_input); the next gfx_line shows the drawing again. */
void gfx_hide(void);
#include <stdint.h>
extern uint8_t gfx_shown;   /* nonzero while the bitmap is on screen */
/* gfx_c64.s: a lone RTI, the NMI (RESTORE) handler: gfx_open puts it in
 * the RAM vector $FFFA, main.c in the KERNAL's NMI vector $0318 */
void gfx_nmi_rti(void);

#endif
