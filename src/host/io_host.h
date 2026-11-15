#ifndef PL0_IO_HOST_H
#define PL0_IO_HOST_H

/* Host-only test hooks for io_host.c. */
void io_capture_begin(void);         /* start capturing output (clears it) */
const char *io_capture_get(void);    /* captured output so far */
void io_capture_end(void);           /* back to stdout */
void io_host_set_break(int on);      /* value io_break() returns */

#endif
