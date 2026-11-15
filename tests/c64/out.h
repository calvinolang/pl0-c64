#ifndef C64_OUT_H
#define C64_OUT_H
#include <stdint.h>

/* Text output for the C64 check programs, through the io layer
 * (io_put_str: ASCII, scrolls). cc65's printf cannot be used: its format
 * letters are parsed in PETSCII, and these programs are compiled with the
 * ASCII charmap like everything else (Makefile, src/core/ascii.h). */
void out_str(const char *s);
void out_nl(void);
void out_line(const char *s);        /* s and a new line */
void out_ul(unsigned long v);        /* decimal */
void out_l(long v);                  /* decimal, signed */
void out_hex(uint8_t b);             /* two lower-case hex digits */

#endif
