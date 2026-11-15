#ifndef PL0_GFX_C64_H
#define PL0_GFX_C64_H

/* C64-only extra of the graphics layer (src/c64/gfx_c64.c); not part of
 * the portable gfx.h. Show the bitmap as it is (gfx_open shows it cleared):
 * after a run, SPACE toggles between the drawing (gfx_show) and the text
 * screen (gfx_close), which the bitmap never touches. */
void gfx_show(void);

#endif
