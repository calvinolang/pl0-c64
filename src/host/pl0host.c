/* Host driver: pl0host [--lines <file>] [--pbm <file>] [--count-instr] <file.pl0>.
 * Output on stdout; errors as "ERROR line N: msg" on stderr with exit code 1.
 * --count-instr prints to stderr "instructions: N" (P-code instructions
 * executed), "functions: F" (how many were math function calls) and
 * "turtle-trig: T" (sin/cos evaluations by turn), the costly ROM calls on
 * the C64. It needs the PL0_TEST_HOOKS counters, which the Makefile's
 * host build enables (the C64 build never defines it, so it costs nothing
 * there). */
#include <stdio.h>
#include <string.h>
#include "host/gfx_host.h"
#include "core/compiler.h"
#include "core/vm.h"
#include "core/turtle.h"

static char source[MAX_SOURCE + 1];
static Program prog;

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    PL0Error e;
    int rc;

    const char *file = 0, *linesfile = 0, *pbmfile = 0;
    int i, count = 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lines") == 0 && i + 1 < argc) linesfile = argv[++i];
        else if (strcmp(argv[i], "--pbm") == 0 && i + 1 < argc) pbmfile = argv[++i];
        else if (strcmp(argv[i], "--count-instr") == 0) count = 1;
        else if (argv[i][0] != '-' && !file) file = argv[i];
        else { file = 0; break; }
    }
    if (!file) {
        fprintf(stderr, "usage: pl0host [--lines <file>] [--pbm <file>] [--count-instr] <file.pl0>\n");
        return 2;
    }
    f = fopen(file, "rb");
    if (!f) {
        perror(file);
        return 2;
    }
    n = fread(source, 1, sizeof source, f);
    fclose(f);
    if (n > MAX_SOURCE) {
        fprintf(stderr, "%s: file larger than %d bytes\n", file, MAX_SOURCE);
        return 2;
    }
    source[n] = '\0';
    rc = 0;
    if (pl0_compile(source, &prog, &e) != 0 || pl0_run(&prog, &e) != 0) {
        char msg[ERR_MESSAGE_SIZE];
        fflush(stdout);
        err_message(msg, e.code, e.line);
        fprintf(stderr, "%s\n", msg);
        rc = 1;
    }
#ifdef PL0_TEST_HOOKS
    if (count) {
        fprintf(stderr, "instructions: %lu\n", vm_instr_count());
        fprintf(stderr, "functions: %lu\n", vm_fn_count());
        fprintf(stderr, "turtle-trig: %u\n", turtle_trig_count());
    }
#else
    if (count) fprintf(stderr, "warning: --count-instr needs a PL0_TEST_HOOKS build\n");
#endif
    if (gfx_host_overflow() > 0)
        fprintf(stderr, "warning: %d line(s) not recorded (recorder full)\n", gfx_host_overflow());
    /* what was drawn before an error is still written */
    if (linesfile && gfx_host_write_lines(linesfile) != 0) {
        perror(linesfile);
        rc = rc ? rc : 2;
    }
    if (pbmfile && gfx_host_write_pbm(pbmfile) != 0) {
        perror(pbmfile);
        rc = rc ? rc : 2;
    }
    return rc;
}
