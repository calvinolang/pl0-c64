# C64 memory map of the core (Task 12b)

Measured from the ld65 map of `build/c64/core_check.prg` (`make c64-core`):
all of `src/core` (lexer, compiler, VM, turtle, clipper, errors) with the
C64 layers `num_c64`, `gfx_c64`, `io_c64`, linked with
`src/c64/pl0-c64.cfg` and `cc65 -O`, plus the test driver
`tests/c64/core_check.c` and `tests/c64/out.c`. The editor, picker, disk
code and main program (Tasks 13-16) are not in it yet.

## Per-module sizes (bytes)

| Module | Code | Rodata/data | BSS | Notes |
|---|---:|---:|---:|---|
| `lexer.c` | 2,731 | 206 | 0 | keyword table in rodata |
| `compiler.c` | 4,299 | 0 | 1,454 | `syms[64]` (22 B each) + lexer state |
| `vm.c` | 2,426 | 0 | 2,586 | `stack[512]` (5 B each) + display |
| `turtle.c` | 2,186 | 46 | 118 | |
| `errors.c` | 281 | 593 | 0 | message table + `err_message` |
| `clip.c` | 890 | 0 | 0 | 32-bit arithmetic (runtime `lmul`/`ldiv`) |
| `num_c64.c` | 735 | 0 | 6 | |
| `num_c64.s` | 1,661 | 9 | 80 | ROM shims, exact `num_parse`/`num_format` |
| `gfx_c64.c` | 169 | 0 | 6 | |
| `gfx_c64.s` | 464 | 8 | 11 | |
| `io_c64.c` | 367 | 3 | 23 | |
| `io_c64.s` | 390 | 10 | 5 | |
| **core + C64 layers** | **16,599** | **875** | **4,289** | |
| cc65 runtime + libc pulled in | 2,271 (incl. startup) | | 1 | `setjmp`, `memcpy`, 32-bit ops, `cgetc`, ... |
| `Program prog` (in the driver; the app needs it too) | | | 4,741 | `code[768]` 3,072 + `consts[128]` 640 + `strings` 1,024 + counters |
| test only: `core_check.c` + `out.c` | 917 | 964 | 80 | includes the embedded square source |

## Segments and limits

| Area | Range | Bytes | Content |
|---|---|---:|---|
| BASIC stub + startup | `$0801-$083F` | 63 | |
| CODE | `$0840-$54E0` | 19,617 | |
| RODATA + DATA + INIT | `$54E1-$5C54` | 1,908 | |
| BSS | `$5C55-$7FEB` | 9,111 | `prog` 4,741, VM stack 2,560, symbol table 1,408, rest |
| **free** | **`$7FEC-$97FF`** | **6,164** | for Tasks 13-16 (heap unused) |
| C stack | `$9800-$9FFF` | 2,048 | cc65 parameter stack |
| `BASICBSS` | `$A000-$BFFF` | 8,192 | the source buffer `source[MAX_SOURCE]` (only this) |
| `HIBSS` | `$C000-$CBFF` | 3,072 | **unused** |
| colour matrix | `$CC00-$CFE7` | 1,000 | bitmap colours (VIC bank 3) |
| bitmap | `$E000-$FF3F` | 8,000 | RAM under the KERNAL |

Total image (`core_check.prg`): `$0801-$5C54`, 21,588 bytes. An example
build (`build/c64/ex_NAME.prg`) embeds its source instead of the square, so
its BSS end moves by the difference (up to about 0.5 KB; see its map).

BSS ends at `$7FEB`, below the `$97FF` limit: 6,164 bytes free. Without
the test driver (about 1.9 KB of code and rodata) the final program has
about **8.0 KB** for the editor, picker, disk access, run flow, help texts
and `main` (the spike estimated 4.8-6.5 KB for these), plus the unused
3 KB of `HIBSS`.

## Placement decisions

- The source buffer is the only thing in `BASICBSS` (`$A000`): the
  driver declares `static char source[MAX_SOURCE]` under
  `#pragma bss-name (push, "BASICBSS")`, copies the program there and
  compiles from it. The lexer copies number text into `lx.text` (low RAM)
  before `num_parse`, so no pointer into `$A000-$BFFF` ever reaches a ROM
  shim. The buffer holds the terminating NUL, so a source has at most
  8,191 bytes.
- `Program prog` (4,741 bytes) is in the ordinary BSS. It does not fit
  `HIBSS` (3,072 bytes) as one struct; only its `code[]` array would. If
  Tasks 13-16 run short of memory, the first option is to make `code`
  a separate array placed in `HIBSS` (frees 3,072 bytes of low RAM;
  `pcode.h` would hold a pointer or the VM/compiler would address it
  directly). Other options, in order: trim the lexer (2.7 KB) and
  compiler, drop `clip.c`'s 32-bit runtime by a 16-bit clipper, shorten
  error texts.
- `HIBSS` and the colour matrix: `$C000-$CBFF` is free; `crt0` does not
  clear it.
