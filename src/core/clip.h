#ifndef PL0_CLIP_H
#define PL0_CLIP_H
#include <stdint.h>

/* Clip a line to the 320x200 bitmap (0..319, 0..199) for ANY int16
 * endpoints. Returns 1 and updates the endpoints if (part of) the line is
 * visible, 0 if nothing is visible (endpoints unspecified). Shared by every
 * gfx_line implementation (host recorder and C64 bitmap). Uses 32-bit
 * long for the intersections (cc65 has a 32-bit long). */
int clip_line(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);

#endif
