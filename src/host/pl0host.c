/* Host driver: pl0host <file.pl0>. Output on stdout; errors as
 * "ERROR line N: msg" on stderr with exit code 1. */
#include <stdio.h>
#include "core/compiler.h"
#include "core/vm.h"

static char source[MAX_SOURCE + 1];
static Program prog;

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    PL0Error e;

    if (argc != 2) {
        fprintf(stderr, "usage: pl0host <file.pl0>\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    n = fread(source, 1, sizeof source, f);
    fclose(f);
    if (n > MAX_SOURCE) {
        fprintf(stderr, "%s: file larger than %d bytes\n", argv[1], MAX_SOURCE);
        return 2;
    }
    source[n] = '\0';
    if (pl0_compile(source, &prog, &e) != 0 || pl0_run(&prog, &e) != 0) {
        fflush(stdout);
        fprintf(stderr, "ERROR line %u: %s\n", (unsigned)e.line, err_text(e.code));
        return 1;
    }
    return 0;
}
