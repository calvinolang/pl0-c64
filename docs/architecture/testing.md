# Testing and toolchain

This page is the one place for the tests: the layers, how to run each,
what the gates cover, how long they take, and how to install the tools.

## The gates

| Command | What it runs | Time (measured) |
|---|---|---|
| `make test` | host only: unit tests, example goldens, tool self-tests | about 15 s after `make clean`, 4 s with the build done |
| `make smoke` | the on-target suite in headless VICE (table below), one target after the other with a time limit each, then a summary table (case, result, seconds); `tests/smoke.sh` | about 4.5 minutes |
| `make check` | the standard gate: `make test`, then `make smoke` | about 4.5 minutes (273 s after `make clean`, 250 s with the build done; 2026-11-15) |
| `make check-full` | `make check`, then `c64-examples` and `c64-app-examples` (every example on the C64 twice) | about 8 minutes (465 s: check 250 s, c64-examples 81 s, c64-app-examples 134 s) |

`make smoke` first checks VICE (`x64sc`, `c1541`), the five ROM images it
needs and cc65 (`tests/need_vice.sh`). If VICE or its ROMs are missing it
prints how to install them and fails; with `ALLOW_NO_VICE=1` it prints
`SKIPPED: VICE not installed` and succeeds instead (for a machine that can
only run the host tests); `make check` and `make check-full` then end with
`PASSED (on-target SKIPPED)`. Logs: `build/smoke/TARGET.log`.

`make dist` copies the products to `build/dist/` (`pl0.prg`, `pl0.d64`),
verifies the image (`tools/mkdisk.py --verify examples IMAGE PRG`: the
directory is exactly `PL0` PRG, `INDEX` and the 15 examples as SEQ, and
every file read back with c1541 equals `build/pl0.prg`, the INDEX text and
`examples/*.pl0` byte for byte), lists the directory and prints the sizes.

`make media` (slow, not part of any gate; the results are committed)
regenerates the VICE captures in `docs/img/c64/` and the demo GIF;
`make web` builds the showcase site in `build/site/` and checks its links.
`make test` checks that the site's generated gallery is current
(`tools/mksite.py --check`) and the links of the committed `web/` sources
(`tools/check_site.py --source`). Both are described in `media.md`.

## Test layers

| Layer | Target | Covers |
|---|---|---|
| host unit tests | `make test` | `tests/test_*.c`: lexer, numbers, compiler and VM, errors, turtle, text buffer |
| host example goldens | `make test` (`make examples`) | every example against `.expected` / `.lines`, the example rules, the instruction budget, `examples-budget.md` current |
| numbers on the C64 | `make c64-num` | the BASIC ROM float layer |
| bitmap | `make c64-gfx` | line drawing against the host reference |
| text I/O | `make c64-io` | screen output, number input, RUN/STOP |
| core | `make c64-core` | the compiler and VM on the C64 |
| editor | `make c64-editor` | 15 editor cases |
| disk | `make c64-disk` | the KERNAL disk primitives on a 1541 |
| memory gate | `make c64-mem` | BSS below `$97FF`, HIBSS, LOWBSS; the tables in `memory-map.md` equal the map |
| program | `make c64-app` | run flow, picker, save and load on a 1541 (12 cases) |
| autostart | `make c64-autostart` | `x64sc -autostart` of the `.d64` and of the `.prg` with VICE's defaults: boot, then F1 draws the square |
| examples (core) | `make c64-examples` | every example through `core_check`, run times (full gate only) |
| examples (program) | `make c64-app-examples` | every example through the program: F3, the example, F1, output or drawing, time from F1 at most 30 s (full gate only) |

The first eight on-target rows and autostart are `make smoke`; the last
two are only in `make check-full`.

## Installing the tools (Ubuntu 24.04)

    sudo apt-get install gcc make python3 cc65 vice
    sh tools/install_vice_roms.sh

- cc65: the Ubuntu package `cc65` (2.19-1; `cc65 --version` says 2.18).
  The memory tables in `memory-map.md` are generated for this version; a
  different cc65 gives different sizes (`make c64-mem-doc` regenerates).
- VICE: the package `vice` (3.7.1, universe). It ships without the
  Commodore ROMs (Debian DFSG). `tools/install_vice_roms.sh` downloads the
  VICE 3.7.1 source release from SourceForge (or takes a tarball given as
  argument), checks the SHA-256 of the five images the tests use (C64
  KERNAL 901227-03, BASIC 901226-01, character ROM 901225-01, 1541 and
  1541-II DOS) and copies them to `~/.local/share/vice/C64` and
  `~/.local/share/vice/DRIVES`, where VICE looks for them.
- No display is needed: the harness runs `x64sc -console` and talks to
  the remote monitor (checked with `DISPLAY` and `WAYLAND_DISPLAY` unset).

Checked here (2026-11-15): gcc 13.3.0, GNU make, python3 3.10, cc65 2.19-1,
VICE 3.7.1+dfsg1-2build3, Ubuntu 24.04.

## Clean clone

Checked 2026-11-15 for Task 16: `git clone -b impl` of the repository
(commit `7f9229b`) into a new directory, then `make check` there with
nothing else prepared: **PASSED in 261 s** (host tests, then 9/9
on-target targets; c64-app 131 s, c64-editor 46 s, c64-autostart 38 s).
The clone needed no file outside the repository besides the installed
tools above, and left only `build/` (ignored) behind. In the working
tree, `make clean && make check`: PASSED in 273 s.

## VICE keyboard (for trying the program by hand)

VICE maps the PC keyboard by position: Esc is RUN/STOP, Page Up is
RESTORE, Home is CLR/HOME, Backspace is INST/DEL, F1-F8 are the C64
function keys. In the harness keys are typed with the monitor's `keybuf`
as PETSCII codes instead (see `tests/vice_app.sh` for the codes).

## CI

`.github/workflows/ci.yml`: job `host` runs `make test`; job `target`
installs cc65 and VICE, the ROMs with `tools/install_vice_roms.sh`, and
runs `make smoke` with no display. Nothing is cached; the workflow has
read-only `contents` permission and time limits (host 15 minutes, target
60).

## Host tests

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

## C64 test targets

Need cc65 (`cc65`, `ca65`, `cl65`) and VICE `x64sc` (3.7.1) with its
ROMs. None of this is part of `make test`, which stays host-only. Times
are wall time on the development machine after the Task 16 harness fix
(below).

| Target | What it does |
|--------|--------------|
| `make c64-tests` | builds `build/c64/num_check.prg`, `gfx_check.prg` and `io_check.prg` from `tests/c64/*_check.c` and the modules in `src/c64/` with `src/c64/pl0-c64.cfg` (map and labels next to each: `*.map`, `*.lbl`). |
| `make c64-num` | number layer: `tests/vice_run.sh build/c64/num_check.prg PASS 100000000 FAIL`; about 5 s. |
| `make c64-gfx` | bitmap layer (`src/c64/gfx_c64.{c,s}` + `src/core/clip.c`): runs `gfx_check.prg` with `--snap` and checks the RAM snapshots with `tools/check_bitmap.py` against the host reference `build/gfx_ref` (same lines, `tests/c64/gfx_lines.h`, through `gfx_host` and the shared clipper). About 5 s. |
| `make c64-io` | text I/O layer (`src/c64/io_c64.{c,s}`): runs `io_check.prg`, types its input with `--on feedN 'keybuf ...'`, forces RUN/STOP in the keyboard matrix from the monitor, and checks the saved screens with `tools/check_io.py`. About 10 s. |
| `make c64-core` | the real core on the C64 (Task 12b): `build/c64/core_check.prg` links all of `src/core` with `num_c64`, `gfx_c64`, `io_c64` and the final link config, and runs the programs in `tests/c64/core_check.c` from the 8 KB source buffer at `$A000`: arithmetic, recursive factorial with a parameter (upper-case keywords), a compile error and a runtime error (`err_message`: `ERROR line N: text`), ROM sin/sqrt/atan, recursion depth 40 and a runaway recursion followed by another run, RUN/STOP (the harness types code 3 with `keybuf` after `g: stop me`), and the square (RAM snapshot at `--snap` step 1). `tools/check_c64_run.py --screen tests/c64/core_check.expected --pbm ...` then compares the whole screen line by line and the bitmap with `pl0host --pbm`. About 5 s. |
| `make c64-example EX=name [TSV=file]` | one example: `tools/pl0_to_c.py` embeds `examples/name.pl0` (ASCII bytes verbatim, NUL-terminated, as numbers) in `build/c64/ex_name.h`, and `core_check.c` in example mode (`CORE_EXAMPLE`) runs it on a cleared screen and prints `cycles C R` (CPU cycles of `pl0_compile` and `pl0_run`, CIA2 timers A+B cascaded, not TI) and `DONE`. Text: the screen rows above `cycles` must match `examples/name.expected` (`compare_output.py`); the example rules keep text output within 22 lines of 39 characters, so it never scrolls and the screen RAM read through the monitor is the whole output (more reliable than a RAM capture buffer, and the real io path). Drawing: the bitmap saved at `--snap` step 1 against `pl0host --pbm`: set pixels within 3%, bounding box within 2 pixels, and overlap: pixels with no pixel of the other drawing within 1 pixel (Chebyshev), counted both ways, each at most 3% of the host count (a centred figure mirrored or rotated would pass the first two checks; `tools/test_check_c64_run.py`, run by `make test`, proves this check rejects mirrored and rotated renders of real examples unless the figure is symmetric under the transformation, and accepts a 1-pixel shift). The `cycles` line is required. Cycle limit 200 M (about 200 s), so a slow example is measured, not killed. About 20-30 s each. |
| `make c64-examples` | `tests/c64_examples.sh`: all examples one after the other, a table (result, run, compile and total seconds, host P-code count) and a failure for any example whose total (compile + run, the time from F1 to the end) is over 30 s; `--record` writes the totals to `docs/architecture/c64-times.tsv`, the "C64 s" column of `examples-budget.md` (then `tests/run_examples.sh --write-budget`). About 1.5 minutes. |
| `make c64` | the program: `build/pl0.prg` (a copy of `build/c64/pl0.prg`; map and labels `build/c64/pl0.map`, `.lbl`): `src/c64/main.c`, the editor (`editor.c`, `edscreen.s`, `textbuf.c`), all of `src/core` and the C64 layers, linked with `src/c64/pl0-c64.cfg`. `main.c` gets the starter text (`examples/square.pl0`) from `build/c64/starter.h` (`tools/pl0_to_c.py`). |
| `make c64-editor` | the editor on the target (Task 14): `tests/vice_smoke.sh` runs 15 independent cases, each a fresh VICE: boot screen, typing, cursor keys and the sticky column, HOME/CLR/DEL/INST (joining lines), new lines and vertical scrolling both ways, a 60-character line scrolling sideways, compile errors on line 1 and 3 (message, cursor, the next key clears it), F1 running the starter and a key back (`compile_ok`), the help page, F2 with its question (N keeps, Y clears), a full buffer, keys handled together before one redraw (`batched_edits`), and ignored keys. Keys go in with `--on TRIGGER 'keybuf ...'` (PETSCII as `\xHH`); each trigger is a whole screen line (often the status bar), so a trigger that never shows fails the case too. After the final line, `tools/check_editor.py EXPECTFILE OUTDIR` checks more rows of the same screen dump (whole rows, prefixes, the only reverse-video cell of rows 1-23 = the cursor, reverse bars). `buffer_full` runs `build/c64/pl0_tiny.prg`, the same program built with `-DPL0_SOURCE_CAP=400`, so 21 typed characters fill the buffer (typing 7,800 would take minutes; poking the length from the monitor would bypass the gap layout). `tests/vice_smoke.sh NAME...` runs single cases. About 50 s. |
| `make disk` | the disk image `build/pl0.d64` (Task 15): `tools/mkdisk.py build/pl0.prg examples build/pl0.d64` formats a D64 titled `PL0 TURTLE` (id `25`) with `c1541` and writes `PL0` (the program, PRG), `INDEX` (SEQ: one line `NAME description` per example, in the demo order SQUARE POLYGON STAR SPIRAL ROSE SINE LISSAJOUS FACTORIAL GCD PRIMES NEWTON PI KOCH SIERPINSKI TREE; the description is the first `{ }` comment, at most 24 characters) and the 15 examples as SEQ files named in upper case without `.pl0`, bytes unchanged (ASCII, `\n` line ends). It then verifies the image: directory names and types, and every file read back with `c1541 -read` and compared. c1541 turns lower case ASCII names into the PETSCII the C64 sends for A-Z, so the tool passes names in lower case (`factorial,s`). `tools/mkdisk.py --index examples` prints INDEX; `--verify examples IMAGE [PRG]` checks an image (and `PL0` against PRG). |
| `make c64-disk` | the KERNAL disk primitives (`src/c64/disk.{c,s}`): `tests/vice_disk.sh` runs `build/c64/disk_check.prg` (`tests/c64/disk_check.c`) on a copy of `build/pl0.d64` in a 1541 with true drive emulation: directory lines, the first INDEX line, save (new file 0, again without replace 63, with replace 0), load (the same text back), load of a missing file (62), a file too big for a 100-byte buffer (253, buffer unchanged); then reads `DISKTEST` back from the image with c1541. About 7 s. |
| `make c64-app` | the whole program with the disk (Task 15): `tests/vice_app.sh` (after `make c64-mem`), 12 cases, 21 VICE runs, each with a fresh copy of the disk image (or one the case builds with c1541) in a 1541 with true drive emulation: `run_factorial` (F3, the 8th entry, F1, output and `-- PRESS ANY KEY --`, back with the text identical to the example), `run_error` (a compile error stays in the editor; `begin !7; !1/0 end.` shows `7`, the error and the key prompt on the text screen, then the editor with the error on line 1), `drawing_runs` (SQUARE from the picker; the bitmap against the host rendering; SPACE twice toggles text/drawing, checked on `$D011` bit 5; a key returns, bitmap off), `input_after_drawing` (`?a` after a drawing prompts on the text view, the next line brings the drawing back; a runtime error after a drawing goes to the text view), `stop_with_runstop`, `save_roundtrip` (save TEST, read it back with c1541, F2, open TEST from the list: name set, not modified), `save_bad_name` (empty, `*`, `/`, 13 characters: `BAD NAME`, directory unchanged; replace: N keeps the old bytes, Y replaces them), `picker` (26 entries: order, scrolling to the last one and stopping there, RUN/STOP, the discard question, FILE TOO BIG for a 9,100-byte file, CR LF/CR/tab conversion, a name with a space not listed), `missing_file` (an INDEX entry scratched from the image: `DISK ERROR: FILE NOT FOUND`, text and name unchanged), `read_error` (the image detached by the monitor during a load: an error, nothing loaded), `no_disk` (a drive without a disk, and no drive at all: `NO DISK`, `DISK ERROR: NO DISK`, the editor still works), `source_at_a000` (a 7,995-byte file loaded into the buffer under the BASIC ROM and saved again as COPY, compared on the image). `tools/check_app.py` checks the final screen, screen dumps, the editor's text in a 64 KB RAM dump (`ed_tb` from `build/c64/pl0.lbl`, read around the gap), a VIC register and the bitmap. `tests/vice_app.sh NAME...` runs single cases; `APP_PRG=...` runs another build (that is how the cases were seen to fail against the Task 14 program). About 2.5 minutes. |
| `make c64-mem` | `tools/c64_mem.py build/c64/pl0.map --doc docs/architecture/memory-map.md`: BSS must end at `$97FF` at the latest (the C stack is `$9800-$9FFF`), HIBSS within `$C000-$CBFF`, LOWBSS within `$033C-$03FB`; prints the bytes left in each. It also builds the per-module and area tables from the map's module and segment lists (checking that the modules add up to every segment) and fails if the generated section of `memory-map.md` differs. `make c64-mem-doc` rewrites that section (`--write-doc`). `make c64-mem-limits` checks only the limits; `c64-app` depends on that one, so a stale document fails `c64-mem` (and smoke) without stopping the app cases. |
| `make c64-autostart` | `tests/vice_autostart.sh`, cases `d64` and `prg`: `x64sc -autostart build/pl0.d64` (a copy) and `x64sc -autostart build/pl0.prg` exactly as a user starts them: `-default` (stock ROMs, drive 8 a 1541-II), true drive emulation on, VICE's own autostart (`--as-user`: LOAD"*",8,1 and RUN from the disk; the `.prg` goes through a temporary disk image, AutostartPrgMode 2); only warp is added. A breakpoint on `main()`, set by a `-moncommands` file before anything runs, gives the exact boot cycle (measured 78.6-78.7 M cycles = about 80 s of emulated time from power-on, both cases: the 28 KB load at 1541 speed), which must be under 120 M. The boot screen must show the status bar `PL/0   SQUARE ... L 1  C 1` and the key bar exactly, both reverse, the starter on row 1 and the cursor at line 1 column 1; then F1 must show `-- SPACE: DRAWING  ANY KEY: EDITOR --` and the bitmap must match `pl0host --pbm examples/square.pl0` (count, box, overlap). About 40 s. |
| `make c64-app-examples` | `tests/c64_app_examples.sh [--record] [name...]`: the final acceptance. For each example in INDEX order, a fresh VICE with `build/pl0.prg` and a copy of the disk in a 1541 (TDE): F3, CRSR down to the example's INDEX entry, RETURN, F1, then at the end-of-run prompt: text examples, the rows above `-- PRESS ANY KEY --` against `examples/NAME.expected` (`check_app.py output`: `compare_output.py`, numbers within 1e-6; if the output filled the screen only its visible tail would be compared, and the check says so; the example rules keep every example within 22 rows, so none does); drawings, the bitmap against the host rendering with the overlap check; then a key: back in the editor with the example's name and its text byte for byte. Time: at F1 the harness resets VICE's cycle stopwatch and sets a breakpoint on `turtle_used()`, which `main.c` calls once, right after `pl0_run` returns; the stopwatch where the CPU stopped is the time from F1 to the end of the program (compile included). Fails if an example needs more than 30 s. Prints a table with the `core_check` time next to it; `--record` writes `docs/architecture/c64-app-times.tsv`, the "C64 app run (s)" column of `examples-budget.md`. About 2.5 minutes. |

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
    The command `pause` sends nothing and lets the emulation run until
    the next screen dump before later commands fire (so the program can
    react to keys just typed, e.g. SPACE on a drawing, which changes
    nothing in screen RAM). The expected line counts only after every
    `--on` has fired, so a final line that also showed earlier does not
    end the run early; the fail text counts at any time.
  - `--vice ARG`: one more x64sc argument (repeatable), e.g.
    `--vice -8 --vice IMAGE.d64`. With `-8` the harness sends `detach 8`
    before `quit`, which writes the drive's last track to the image file.
    (Task 15 saw a file saved just before the end stay an unclosed `*SEQ`
    with 0 blocks and blamed a plain `quit`. That is unconfirmed: Task 16
    found that VICE was then not quitting at all but was killed 20 s
    later, which also loses the track. `detach 8` stays because it is
    correct either way.)
  - `--snap`: handshake for snapshots. The program writes a step number n
    to `$02A7` and waits until `$02A8` equals it; the harness saves all
    64 KB of RAM to `snapN.bin` and a VICE screenshot to `stepN.png`, then
    writes n to `$02A8`.
  - `--check CMD`: after the screen passed, run `CMD <workdir>`; its exit
    status decides.
  - `--as-user`: VICE's default autostart (a `.prg` through a disk image
    and the drive) instead of `-autostartprgmode 1` (inject into RAM).
  - `--work NAME`: work directory `build/vice/NAME` instead of the prg's
    name.
  - Checkpoints: when the CPU stops at a `break`/`watch` (sent with `--on`
    or from `--vice -moncommands FILE`), the harness reads the stopwatch
    (`sw`) while the CPU is still stopped and appends the cycle count to
    `stops.txt` in the work directory, then resumes.
  - Shutdown: with a disk attached the harness sends `detach 8`, resumes
    (`x`) and then `quit`, repeated each second until VICE exits (kill
    after 10 s). VICE 3.7.1 ignored a `quit` read in the monitor right
    after `detach 8`: before Task 16 every run with a disk waited 20 s for
    the kill (`make c64-app` took about 9 minutes, now 2.5).
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
