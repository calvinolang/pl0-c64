/* Host implementation of the I/O layer: stdout, or a capture buffer for tests. */
#include "platform/io.h"
#include "host/io_host.h"
#include <stdio.h>
#include <string.h>

static char cap[8192];
static size_t caplen;
static int capturing;
static int brk;

void io_capture_begin(void) { capturing = 1; caplen = 0; cap[0] = '\0'; }
const char *io_capture_get(void) { return cap; }
void io_capture_end(void) { capturing = 0; }
void io_host_set_break(int on) { brk = on; }

void io_put_str(const char *s)
{
    size_t n;
    if (!capturing) { fputs(s, stdout); return; }
    n = strlen(s);
    if (n > sizeof cap - 1 - caplen) n = sizeof cap - 1 - caplen;  /* truncate */
    memcpy(cap + caplen, s, n);
    caplen += n;
    cap[caplen] = '\0';
}

void io_put_nl(void) { io_put_str("\n"); }

int io_read_num(num_t *r) { (void)r; return 1; }   /* input arrives in Task 8 */

int io_break(void) { return brk; }
