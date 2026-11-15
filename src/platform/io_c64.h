#ifndef PL0_IO_C64_H
#define PL0_IO_C64_H

/* C64-only extras of the I/O layer (src/c64/io_c64.c); not part of the
 * portable io.h. The run screen (Task 15) drives them. */
void io_text_open(void);    /* clear the 40x25 output screen, cursor home */
/* does nothing and has no caller: the label shares the RTS at the end
 * of io_text_open (costs no bytes), kept as the pair's closing call */
void io_text_close(void);
#include <stdint.h>
/* The output cursor's column: 0 when the next character starts a row
 * (after a new line); the run screen starts its own lines after it. */
extern uint8_t io_col;
/* Called by io_read_num before it prompts, if set (the program sets
 * gfx_hide: input is read on the text screen, never under a drawing). */
extern void (*io_before_input)(void);

#endif
