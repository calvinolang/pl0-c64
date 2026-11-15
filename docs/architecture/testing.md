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
| cc65 (cl65) | MISSING: not installed, and sudo needs a password. Install with `apt install cc65`. Until then the `c64` target is a placeholder and CI installs cc65 itself. |
