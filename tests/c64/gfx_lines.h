/* gfx_lines.h -- the lines drawn by tests/c64/gfx_check.c (C64, gfx_c64)
 * and by tests/c64/gfx_ref.c (host renderer, gfx_host), so that
 * tools/check_bitmap.py can compare the two bitmaps. Endpoints are raw
 * (unclipped) int16 values: both sides clip with src/core/clip.c. */
#ifndef GFX_LINES_H
#define GFX_LINES_H
#include <stdint.h>

#define GL_MIN (-32767 - 1)             /* -32768 is a long constant in cc65 */

static const int16_t gfx_lines[][4] = {
    {   0,   0, 319, 199 },             /* the diagonal */
    {  10,  10, 309,  10 },             /* the box (10,10)-(309,189) */
    { 309,  10, 309, 189 },
    { 309, 189,  10, 189 },
    {  10, 189,  10,  10 },
    {   0,   0, 319,   0 },             /* horizontal and vertical extremes */
    { 319, 199,   0, 199 },
    {   0, 199,   0,   0 },
    { 319,   0, 319, 199 },
    { -50, -50, 400, 300 },             /* clipped on two sides */
    { -10, -10,  -1, 250 },             /* entirely off-screen (left) */
    { 330,  -5, 400, 300 },             /* entirely off-screen (right) */
    { GL_MIN, GL_MIN, 32767, 32767 },   /* far line, clipped to the diagonal 0..199 */
    { 319,   0,   0, 199 },             /* right to left (endpoints swapped) */
    { 100,   0, 110, 199 },             /* steep, down */
    { 200, 199, 190,   0 },             /* steep, up, right to left */
    {   0, 150, 319, 140 },             /* shallow, up */
    { 300,  20,  20,  40 },             /* shallow, down, right to left */
    { 160, 100, 160, 100 }              /* a single pixel */
};
#define GFX_NLINES (sizeof gfx_lines / sizeof gfx_lines[0])

#endif
