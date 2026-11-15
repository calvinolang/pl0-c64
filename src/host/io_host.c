/* Host implementation of the I/O layer: stdout, or a capture buffer for tests. */
#include "platform/io.h"
#include "host/io_host.h"
#include <stdio.h>
#include <string.h>

static char cap[8192];
static size_t caplen;
static int capturing;
static int brk;
static int brk_after;   /* calls left before io_break() starts returning 1 */

void io_capture_begin(void) { capturing = 1; caplen = 0; cap[0] = '\0'; }
const char *io_capture_get(void) { return cap; }
void io_capture_end(void) { capturing = 0; }
void io_host_set_break(int on) { brk = on; }
void io_host_break_after(int n) { brk_after = n; brk = 0; }

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

static char inq[256];
static size_t inpos;

void io_host_set_input(const char *s)
{
    strncpy(inq, s, sizeof inq - 1);
    inq[sizeof inq - 1] = '\0';
    inpos = 0;
}

/* Next whitespace-separated word of the queue parsed as a number; an empty
 * queue or a malformed word is bad input. */
int io_read_num(num_t *r)
{
    char w[32];
    size_t n = 0;
    while (inq[inpos] == ' ' || inq[inpos] == '\t' || inq[inpos] == '\n' || inq[inpos] == '\r') inpos++;
    while (inq[inpos] && inq[inpos] != ' ' && inq[inpos] != '\t' &&
           inq[inpos] != '\n' && inq[inpos] != '\r') {
        if (n >= sizeof w - 1) return 1;
        w[n++] = inq[inpos++];
    }
    w[n] = '\0';
    return num_parse(w, r) != 0;
}

int io_break(void)
{
    if (brk_after > 0 && --brk_after == 0) brk = 1;
    return brk;
}
