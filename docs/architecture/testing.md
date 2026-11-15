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
| `make c64-tests` | builds `build/c64/num_check.prg` from `tests/c64/num_check.c` and `src/c64/num_c64.{c,s}` with `src/c64/pl0-c64.cfg` (map and labels next to it: `num_check.map`, `num_check.lbl`). |
| `make c64-num` | runs it in headless VICE: `tests/vice_run.sh build/c64/num_check.prg PASS 100000000 FAIL`; about 15 s of wall time. |
| `make c64` | still a placeholder (the full program comes with Task 14). |

- C is compiled in two steps, `cc65 -t c64 -O -Isrc -W +error` to
  `build/c64/*.c.s` and `ca65` to `*.o`, then linked with
  `cl65 -t c64 -C src/c64/pl0-c64.cfg`. Never use `cl65 -c` on a source in
  place: cl65 writes its intermediate `.s` next to the source, which
  overwrote `src/c64/num_c64.s` once. Warnings are errors (`-W +error`).
- `tests/vice_run.sh <prg> <expected-text> <cycle-limit> [fail-text]` starts
  `x64sc -console -default -warp -sounddev dummy -limitcycles N` with the
  text remote monitor (the options proven in the Task 1 spike), and
  `tools/vice_screen.py` breaks into the monitor about twice a second,
  saves the screen RAM `$0400-$07E7` with `bsave`, resumes, and decodes it
  with `tools/screen2txt.py` (lower/upper case charset). It stops when the
  expected text (exit 0) or the fail text (exit 1) appears; at the cycle
  limit, or after 360 s (`timeout`), it fails. The last screen is printed
  on failure; work files are in `build/vice/<prg>/`.
- `tests/c64/num_check.c` repeats the groups of `tests/test_num.c` (values
  outside the C64 range replaced and commented), adds aliasing, error-trap,
  boundary, compare and chain groups, and checks the generated vectors in
  `tests/c64/num_vectors.h`. Regenerate those with
  `python3 tools/gen_num_vectors.py` (expected strings come from the host
  `num_format`, compiled from `src/host/num_host.c`). It prints one line per
  group and `PASS` or `FAIL n`.
- `tools/vicemon.py` (breakpoint plus monitor commands, moved from the
  spike) remains for investigations.
