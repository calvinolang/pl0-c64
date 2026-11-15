#ifndef PL0_GFX_H
#define PL0_GFX_H
#include <stdint.h>

/* Platform graphics interface. Pixel space is 0..319 x 0..199, origin at the
 * top left, black pixels on a cleared bitmap. */
void gfx_open(void);     /* switch to the bitmap and clear it */
void gfx_close(void);    /* back to the text screen */
/* Draw a black line. Any int16 endpoints are allowed: the line is clipped to
 * the bitmap and nothing outside it is ever written. */
void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

#endif
