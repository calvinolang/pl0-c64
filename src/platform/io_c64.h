#ifndef PL0_IO_C64_H
#define PL0_IO_C64_H

/* C64-only extras of the I/O layer (src/c64/io_c64.c); not part of the
 * portable io.h. The run screen (Task 15) drives them. */
void io_text_open(void);    /* clear the 40x25 output screen, cursor home */
void io_text_close(void);   /* nothing yet */
#include <stdint.h>
/* The output cursor's column: 0 when the next character starts a row
 * (after a new line); the run screen starts its own lines after it. */
extern uint8_t io_col;

#endif
