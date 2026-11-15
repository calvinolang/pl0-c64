# PL/0 for the Commodore 64

PL/0 interpreter with a portable C core, host tests and a cc65 C64 build. See docs/.

Run `make test` for host tests, `make check` for the gate (host tests and
the on-target suite in headless VICE) and `make dist` for `build/dist/pl0.prg`
and `build/dist/pl0.d64`. How to install the tools and what each target
covers: docs/architecture/testing.md.
