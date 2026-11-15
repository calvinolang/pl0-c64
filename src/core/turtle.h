#ifndef PL0_TURTLE_H
#define PL0_TURTLE_H
#include "platform/num.h"

/* Turtle graphics. Position and heading are kept as numbers (never rounded
 * in the state); only the line endpoints are rounded to the nearest pixel.
 * Coordinates: origin at screen center, x right, y up (x -160..159,
 * y -100..99); screen x = 160 + x, screen y = 99 - y. Heading 0 = up,
 * positive turns are clockwise degrees.
 * The first turtle statement of a run calls gfx_open(); the front end
 * calls gfx_close() (never the VM).
 * Operations that compute return nonzero on numeric overflow. */
void turtle_reset(void);              /* start of every run: home, pen down, graphics unused */
int  turtle_used(void);               /* nonzero if this run executed a turtle statement */
int  turtle_forward(const num_t *d);
int  turtle_turn(const num_t *deg);
void turtle_penup(void);
void turtle_pendown(void);
void turtle_home(void);
int  turtle_moveto(const num_t *x, const num_t *y);

#ifdef PL0_TEST_HOOKS
/* Number of sin/cos evaluations since turtle_reset (the cache must keep
 * this at 1 pair for any number of forward calls without a turn). */
unsigned turtle_trig_count(void);
#endif

#endif
