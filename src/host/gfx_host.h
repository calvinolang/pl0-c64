#ifndef PL0_GFX_HOST_H
#define PL0_GFX_HOST_H
#include <stdint.h>

/* Host-only: the host gfx layer records the CLIPPED lines instead of drawing. */
int  gfx_host_line_count(void);
void gfx_host_line(int i, int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);
int  gfx_host_overflow(void);                  /* lines dropped (array full) */
int  gfx_host_is_open(void);                   /* gfx_open called, not yet closed */
void gfx_host_reset(void);                     /* forget lines and open state */
int  gfx_host_write_pbm(const char *path);     /* 0 ok, -1 file error */
int  gfx_host_write_lines(const char *path);   /* "x0 y0 x1 y1" per line; 0 ok */

#endif
