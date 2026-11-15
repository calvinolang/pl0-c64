# Testing and toolchain

- `make test` builds each `tests/test_*.c` with all `src/core/*.c` and `src/host/*.c` (except `pl0host.c`) and runs it natively.
- `make host` builds `build/pl0host` (with the `PL0_TEST_HOOKS` counters, so
  `pl0host --count-instr` can report executed P-code instructions, math
  function calls and turtle sin/cos evaluations; the C64 build never defines
  `PL0_TEST_HOOKS`, so the counters cost nothing there).
- `make test` also runs `tests/run_examples.sh` (same as `make examples`):
  every `examples/*.pl0` against its golden (`.expected` text through
  `tools/compare_output.py`, or `.lines` exact line dump), the source and
  output rules in `tools/example_rules.py`, the 40,000-instruction budget,
  and that `docs/architecture/examples-budget.md` is current
  (`tests/run_examples.sh --write-budget` regenerates it).

## Toolchain status (checked 2026-11-15)

| Tool | Status |
|------|--------|
| gcc 13.3.0, make, python3 3.10.10 | present |
| VICE x64sc 3.7.1, c1541 | present |
| cc65 (cl65, cc65, ca65) | present (2.18, Ubuntu package 2.19-1; checked again for Task 11). CI installs cc65 itself. |

## C64 tests in headless VICE (Task 11)

Needs cc65 (`cc65`, `ca65`, `cl65`; 2.18/2.19 checked) and VICE `x64sc`
(3.7.1). None of this is part of `make test`, which stays host-only.

| Target | What it does |
|--------|--------------|
| `make c64-tests` | builds `build/c64/num_check.prg`, `gfx_check.prg` and `io_check.prg` from `tests/c64/*_check.c` and the modules in `src/c64/` with `src/c64/pl0-c64.cfg` (map and labels next to each: `*.map`, `*.lbl`). |
| `make c64-num` | number layer: `tests/vice_run.sh build/c64/num_check.prg PASS 100000000 FAIL`; about 15 s of wall time. |
| `make c64-gfx` | bitmap layer (`src/c64/gfx_c64.{c,s}` + `src/core/clip.c`): runs `gfx_check.prg` with `--snap` and checks the RAM snapshots with `tools/check_bitmap.py` against the host reference `build/gfx_ref` (same lines, `tests/c64/gfx_lines.h`, through `gfx_host` and the shared clipper). About 15 s. |
| `make c64-io` | text I/O layer (`src/c64/io_c64.{c,s}`): runs `io_check.prg`, types its input with `--on feedN 'keybuf ...'`, forces RUN/STOP in the keyboard matrix from the monitor, and checks the saved screens with `tools/check_io.py`. About 30 s. |
| `make c64-core` | the real core on the C64 (Task 12b): `build/c64/core_check.prg` links all of `src/core` with `num_c64`, `gfx_c64`, `io_c64` and the final link config, and runs the programs in `tests/c64/core_check.c` from the 8 KB source buffer at `$A000`: arithmetic, recursive factorial with a parameter (upper-case keywords), a compile error and a runtime error (`err_message`: `ERROR line N: text`), ROM sin/sqrt/atan, recursion depth 40 and a runaway recursion followed by another run, RUN/STOP (the harness types code 3 with `keybuf` after `g: stop me`), and the square (RAM snapshot at `--snap` step 1). `tools/check_c64_run.py --screen tests/c64/core_check.expected --pbm ...` then compares the whole screen line by line and the bitmap with `pl0host --pbm`. About 25 s. |
| `make c64-example EX=name [TSV=file]` | one example: `tools/pl0_to_c.py` embeds `examples/name.pl0` (ASCII bytes verbatim, NUL-terminated, as numbers) in `build/c64/ex_name.h`, and `core_check.c` in example mode (`CORE_EXAMPLE`) runs it on a cleared screen and prints `cycles C R` (CPU cycles of `pl0_compile` and `pl0_run`, CIA2 timers A+B cascaded, not TI) and `DONE`. Text: the screen rows above `cycles` must match `examples/name.expected` (`compare_output.py`); the example rules keep text output within 22 lines of 39 characters, so it never scrolls and the screen RAM read through the monitor is the whole output (more reliable than a RAM capture buffer, and the real io path). Drawing: the bitmap saved at `--snap` step 1 against `pl0host --pbm`: set pixels within 3%, bounding box within 2 pixels, and overlap: pixels with no pixel of the other drawing within 1 pixel (Chebyshev), counted both ways, each at most 3% of the host count (a centred figure mirrored or rotated would pass the first two checks; `tools/test_check_c64_run.py`, run by `make test`, proves this check rejects mirrored and rotated renders of real examples unless the figure is symmetric under the transformation, and accepts a 1-pixel shift). The `cycles` line is required. Cycle limit 200 M (about 200 s), so a slow example is measured, not killed. About 20-30 s each. |
| `make c64-examples` | `tests/c64_examples.sh`: all examples one after the other, a table (result, run, compile and total seconds, host P-code count) and a failure for any example whose total (compile + run, the time from F1 to the end) is over 30 s; `--record` writes the totals to `docs/architecture/c64-times.tsv`, the "C64 s" column of `examples-budget.md` (then `tests/run_examples.sh --write-budget`). About 6 minutes. |
| `make c64` | the program: `build/pl0.prg` (a copy of `build/c64/pl0.prg`; map and labels `build/c64/pl0.map`, `.lbl`): `src/c64/main.c`, the editor (`editor.c`, `edscreen.s`, `textbuf.c`), all of `src/core` and the C64 layers, linked with `src/c64/pl0-c64.cfg`. `main.c` gets the starter text (`examples/square.pl0`) from `build/c64/starter.h` (`tools/pl0_to_c.py`). |
| `make c64-editor` | the editor on the target (Task 14): `tests/vice_smoke.sh` runs 15 independent cases, each a fresh VICE: boot screen, typing, cursor keys and the sticky column, HOME/CLR/DEL/INST (joining lines), new lines and vertical scrolling both ways, a 60-character line scrolling sideways, compile errors on line 1 and 3 (message, cursor, the next key clears it), a successful compile, the help page, F2 with its question (N keeps, Y clears), a full buffer, keys handled together before one redraw (`batched_edits`), and ignored keys. Keys go in with `--on TRIGGER 'keybuf ...'` (PETSCII as `\xHH`); each trigger is a whole screen line (often the status bar), so a trigger that never shows fails the case too. After the final line, `tools/check_editor.py EXPECTFILE OUTDIR` checks more rows of the same screen dump (whole rows, prefixes, the only reverse-video cell of rows 1-23 = the cursor, reverse bars). `buffer_full` runs `build/c64/pl0_tiny.prg`, the same program built with `-DPL0_SOURCE_CAP=400`, so 21 typed characters fill the buffer (typing 7,800 would take minutes; poking the length from the monitor would bypass the gap layout). `tests/vice_smoke.sh NAME...` runs single cases. About 2.5 minutes. |

- C is compiled in two steps, `cc65 -t c64 -O -Isrc -W +error` to
  `build/c64/*.c.s` and `ca65` to `*.o`, then linked with
  `cl65 -t c64 -C src/c64/pl0-c64.cfg`. Every C source goes through a
  generated wrapper `build/c64/X.ascii.c` that includes
  `<ascii_charmap.h>` first, so all C64 text is ASCII (Task 12b; see
  "Text encoding" in spike-results.md); assembly includes
  `src/c64/ascii.inc`. C64 programs print with `io_put_str` (the check
  programs through `tests/c64/out.c`), never cc65 `printf`, whose format
  parsing is PETSCII. Never use `cl65 -c` on a source in
  place: cl65 writes its intermediate `.s` next to the source, which
  overwrote `src/c64/num_c64.s` once. Warnings are errors (`-W +error`).
- `tests/vice_run.sh [options] <prg> <expected-line> <cycle-limit> [fail-text]`
  starts `x64sc -console -default -warp -sounddev dummy -limitcycles N`
  with the text remote monitor (the options proven in the Task 1 spike),
  and `tools/vice_screen.py` breaks into the monitor about twice a second,
  saves the screen RAM `$0400-$07E7` with `bsave`, resumes, and decodes it
  with `tools/screen2txt.py` (lower/upper case charset; screen codes
  27-30 decode as `[ \ ] ^`, so the editor's `{` `}`, shown as brackets,
  read as `[` `]`). Matching is by
  whole screen lines, trailing spaces ignored: a line that starts with the
  fail text fails the run (checked first, so `FAIL` also catches `FAIL 3`);
  a line equal to the expected text passes it (`PASS` does not match
  `PASSED`). At the cycle limit, or after 360 s (`timeout`), it fails. The
  last screen is printed on failure; work files are in `build/vice/<prg>/`.
  Options:
  - `--on TRIGGER CMD`: once a screen line equals TRIGGER, send the monitor
    command CMD; repeatable, fired in order. Key injection uses the
    monitor's `keybuf` command, WITHOUT quotes (quotes are typed too):
    `--on feed1 'keybuf 12.5\n'`; `\n` is RETURN, `\xHH` any PETSCII code
    (`\x14` DEL, `\x03` RUN/STOP). The monitor starts in `bank ram`; to
    write I/O registers, send `bank io` first and `bank ram` after.
  - `--snap`: handshake for snapshots. The program writes a step number n
    to `$02A7` and waits until `$02A8` equals it; the harness saves all
    64 KB of RAM to `snapN.bin` and a VICE screenshot to `stepN.png`, then
    writes n to `$02A8`.
  - `--check CMD`: after the screen passed, run `CMD <workdir>`; its exit
    status decides.
- `tools/check_bitmap.py --ref PREFIX --lbl LABELS DIR` (c64-gfx): between
  the snapshots before and after `gfx_open` and after drawing, no byte
  outside the bitmap `$E000-$FF3F`, the matrix `$CC00-$CFE7`, the RAM NMI
  vector and volatile RAM (`$0000-$03FF`, BSS and C stack up to `$9FFF`)
  may change; matrix all `$01`, bitmap cleared; VIC registers as recorded
  by the program; the bitmap must equal a Python model of the assembly
  routine pixel for pixel, its pixel count must be within 1% of the host
  PBM, and every axis-parallel line must be complete. It writes
  `bitmap.png` and `host.png` next to the snapshots.
- `tools/check_io.py DIR` (c64-io): the scrolled screen (rows `line 36` ..
  `line 59`) and the wrapped 100-character string (40/40/20).
- `tests/c64/gfx_check.c` and `io_check.c` also check themselves (VIC
  registers and text screen restored after `gfx_close`, line timings with
  the CIA2 timers; screen codes, colour RAM scrolling, number input,
  `io_break` once per press and its cost) and print `PASS` or `FAIL n`.
  Like all C64 sources they are compiled with ASCII literals.
- `tests/c64/num_check.c` repeats the groups of `tests/test_num.c` (values
  outside the C64 range replaced and commented), adds aliasing, error-trap,
  boundary, compare and chain groups, and checks the generated vectors in
  `tests/c64/num_vectors.h`. Regenerate those with
  `python3 tools/gen_num_vectors.py` (expected strings come from the host
  `num_format`, compiled from `src/host/num_host.c`). It prints one line per
  group and `PASS` or `FAIL n`.
- `tools/vicemon.py` (breakpoint plus monitor commands, moved from the
  spike) remains for investigations.
