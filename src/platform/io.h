#ifndef PL0_IO_H
#define PL0_IO_H
#include "platform/num.h"

/* Platform text I/O used by the VM. */
void io_put_str(const char *s);
void io_put_nl(void);
int  io_read_num(num_t *r);      /* 0 ok, nonzero bad input */
int  io_break(void);             /* nonzero if the user asked to stop */

#endif
