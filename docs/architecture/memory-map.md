# C64 memory map

## The program now (generated)

`build/pl0.prg` as linked (`src/c64/pl0-c64.cfg`, cc65 2.19 from the
Ubuntu package, `-O`). The tables between the markers are written by
`make c64-mem-doc` (`tools/c64_mem.py build/c64/pl0.map --write-doc`)
from the ld65 map, and `make c64-mem` fails when they differ from the
map, so they cannot drift from the build (a different cc65 version gives
different sizes: regenerate). Per module, in bytes: Code is CODE (plus the
startup segments for the runtime row), Rodata + data is RODATA, DATA and
INIT.

<!-- begin c64_mem.py: generated from build/c64/pl0.map, checked by make c64-mem -->
| Module | Code | Rodata + data | BSS | HIBSS | LOWBSS | BASICBSS |
|---|---:|---:|---:|---:|---:|---:|
| `main.o` | 1,054 | 545 | 4,753 |  | 78 | 8,192 |
| `editor.o` | 3,590 | 837 |  | 181 |  |  |
| `edscreen_s.o` | 792 |  |  | 279 |  |  |
| `textbuf.o` | 1,120 |  |  |  |  |  |
| `disk.o` | 612 | 111 |  |  | 55 |  |
| `disk_s.o` | 588 |  |  |  | 14 |  |
| `picker.o` | 570 | 69 |  |  | 8 |  |
| `picker_s.o` | 552 | 18 |  | 51 | 19 |  |
| `lexer.o` | 2,731 | 206 |  |  |  |  |
| `compiler.o` | 4,299 |  | 1,454 |  |  |  |
| `vm.o` | 2,426 |  | 26 | 2,560 |  |  |
| `turtle.o` | 2,186 | 46 | 118 |  |  |  |
| `errors.o` | 281 | 593 |  |  |  |  |
| `clip.o` | 890 |  |  |  |  |  |
| `num_c64.o` | 735 |  | 6 |  |  |  |
| `num_c64_s.o` | 1,661 | 9 | 80 |  |  |  |
| `gfx_c64.o` | 191 |  | 7 |  |  |  |
| `gfx_c64_s.o` | 472 | 8 | 11 |  |  |  |
| `io_c64.o` | 384 | 3 | 25 |  |  |  |
| `io_c64_s.o` | 390 | 10 | 5 |  |  |  |
| cc65 runtime + libc (95 modules) | 2,306 | 69 | 1 |  |  |  |
| **total** | 27,830 | 2,524 | 6,486 | 3,071 | 174 | 8,192 |

| Area | Range | Bytes | Notes |
|---|---|---:|---|
| BASIC stub + startup | `$0801-$083F` | 63 |  |
| CODE | `$0840-$7490` | 27,729 |  |
| RODATA + DATA + INIT | `$7491-$7E6C` | 2,524 |  |
| BSS | `$7E6D-$97C2` | 6,486 | ONCE (run-once startup code) lies under its start |
| **free below the C stack** | `$97C3-$97FF` | 61 | BSS may grow up to $97FF |
| C stack | `$9800-$9FFF` | 2,048 | cc65 parameter stack |
| BASICBSS | `$A000-$BFFF` | 8,192 | the source buffer, under the BASIC ROM |
| HIBSS | `$C000-$CBFE` | 3,071 | 3,071 of 3,072 used, 1 free |
| LOWBSS | `$033C-$03E9` | 174 | 174 of 192 used, 18 free (the cassette buffer) |

Image `$0801-$7E92` (to the end of ONCE): 30,356 bytes in `pl0.prg` with the 2-byte load address.
<!-- end c64_mem.py -->

## The stacks (measured)

The compiler is recursive (expressions, statements, procedure blocks);
its depth is bounded by `MAX_EXPR_NEST` (16), `MAX_STMT_NEST` (16) and
`MAX_NEST` (4) in `src/core/limits.h`, which exist to protect the two
stacks: the 6502 hardware stack (`$0100-$01FF`, 256 bytes, return
addresses and the ROM's own use) and the cc65 C stack (`$9800-$9FFF`,
2,048 bytes, parameters and locals).

Measured on the C64 (VICE) by case (i) of `tests/c64/core_check.c`,
part of `make c64-core`: a program with every limit at its maximum (4
nested procedures, 16 nested `begin`/`if`/`while`, 16 parentheses around
a number literal in the innermost one; one more of any gives `nested too
deeply`), compiled and run from the 8 KB source buffer like any program.
Method: before the case, `stack_fill` writes `$AA` to the unused part of
both stacks (the hardware stack from `$0100` up to 32 bytes below the
stack pointer, the C stack from `$9800` up to 64 bytes below cc65's
`sp`); after it, `stack_scan` finds the lowest byte that is no longer
`$AA` in each. The results stay in two globals, and `make c64-core`
prints them from the RAM snapshot (`build/vice/core_check/snap1.bin`)
through the labels in `build/c64/core_check.lbl`:

    c64-core: case i (the nesting limits): 75 of 256 bytes of the 6502 stack free, 460 of 2048 bytes of the C stack used

(2026-11-15, cc65 2.19; 75 to 77 from run to run, depending on where the
jiffy interrupt falls). So the binding constraint is the 256-byte
hardware stack, with 75 bytes to spare; the C stack uses under a quarter
of its 2 KB. The program itself (`main` -> `run_program` ->
`pl0_compile`) sits a few return addresses deeper than `core_check`'s
`main` -> `run` -> `pl0_compile`, which the margin covers. The case
fails `make c64-core` (a `FAIL stack` line) if fewer than 32 bytes of
the hardware stack stay free or more than 1,024 bytes of the C stack are
used. A stored `$AA` at the deepest point would hide a byte or two.

## History (superseded numbers)

The sections below are the history: how the map got here, measured at
each task. Their numbers are superseded by the generated tables above;
do not quote them for the current program.

### The core (Task 12b)

Measured from the ld65 map of `build/c64/core_check.prg` (`make c64-core`):
all of `src/core` (lexer, compiler, VM, turtle, clipper, errors) with the
C64 layers `num_c64`, `gfx_c64`, `io_c64`, linked with
`src/c64/pl0-c64.cfg` and `cc65 -O`, plus the test driver
`tests/c64/core_check.c` and `tests/c64/out.c`. The editor, picker, disk
code and main program (Tasks 13-16) are not in it yet.

#### Per-module sizes (bytes)

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

#### Segments and limits

| Area | Range | Bytes | Content |
|---|---|---:|---|
| BASIC stub + startup | `$0801-$083F` | 63 | |
| CODE | `$0840-$54E0` | 19,617 | |
| RODATA + DATA + INIT | `$54E1-$5C54` | 1,908 | |
| BSS | `$5C55-$7FEB` | 9,111 | `prog` 4,741, VM stack 2,560, symbol table 1,408, rest |
| **free** | **`$7FEC-$97FF`** | **6,164** | for Tasks 13-16 (heap unused) |
| C stack | `$9800-$9FFF` | 2,048 | cc65 parameter stack |
| `BASICBSS` | `$A000-$BFFF` | 8,192 | the source buffer `source[MAX_SOURCE]` (only this) |
| `HIBSS` | `$C000-$CBFF` | 3,072 | **unused** here; the editor uses it from Task 14 (below) |
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

#### Placement decisions

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

### The program with the editor (Task 14)

Measured from `build/c64/pl0.map` (`make c64`): `main.c`, the editor
(`editor.c`, `edscreen.s`, `textbuf.c`), all of `src/core` and the C64
layers. `vm.o` and `turtle.o` are linked although `main` does not run
programs yet (Task 15), so the numbers include them.

| Module | Code | Rodata/data | BSS | HIBSS | Notes |
|---|---:|---:|---:|---:|---|
| `main.c` | 205 | 452 | 4,756 | | `Program prog` 4,741; starter text 380; source buffer 8,192 in `BASICBSS` |
| `editor.c` | 3,540 | 765 | | 181 | help page about 650 of the rodata |
| `edscreen.s` | 792 | | | 279 | 256-byte ASCII-to-screen-code table |
| `textbuf.c` | 2,072 | | | | 952 of it unused here (line queries, up/down/home/end: the editor scans in assembly) |
| core + C64 layers | 16,599 | 875 | 4,289 | | as above |
| cc65 runtime + libc | 2,142 (+ startup 51) | 107 | 1 | | |

| Area | Range | Bytes |
|---|---|---:|
| CODE | `$0840-$6B45` | 25,350 |
| RODATA + DATA + INIT | `$6B46-$73B6` | 2,161 |
| BSS | `$73B7-$970C` | 9,046 |
| **free below the C stack** | **`$970D-$97FF`** | **243** |
| HIBSS used | `$C000-$C1CB` | 460 (2,612 free) |

Total image `$0801-$73B6` (27,574 bytes). The editor's variables moved to
`HIBSS` because low RAM was full: with them in the ordinary BSS the image
overflowed `$97FF` by 80 bytes. crt0 does not clear `HIBSS`, so `main`
clears it (`memset(_HIBSS_RUN__, 0, _HIBSS_SIZE__)`) before anything else.

**Task 15 needs room** (run screen, picker, disk access), so it must free low
RAM first. The options, measured or sized here:

- Move a big core array to `HIBSS` (2,612 bytes free there): the VM stack
  (`stack[512]`, 2,560 bytes) fits; `Program.code` (3,072) no longer fits
  next to the editor's 460 bytes.
- Leave out the textbuf functions the program does not call (952 bytes of
  code; for example behind a C64-only define, the host tests keep them).
- Put Task 15's own variables in `HIBSS` too, and keep long texts (help
  page, example descriptions) on the disk.

### Task 15: step 0 and the final program

**Step 0** (commit `874bd94`, before any Task 15 feature code):

- The VM stack (`stack[512]`, 2,560 bytes) moved to `HIBSS` on cc65
  (`src/core/vm.c`; it needs no clearing).
- The C64 build leaves out the textbuf functions it does not call (line
  queries, vertical moves, home/end, goto, `tb_len`, `tb_cursor`,
  `tb_set_cursor`): `TB_NO_LINE_SCANS` is defined for cc65 in
  `textbuf.h` unless `TB_LINE_SCANS` is; the host tests keep them all.
- Result: BSS end `$970C` -> `$89BD`, **3,650 bytes free** below `$97FF`
  (was 243); HIBSS 3,020 of 3,072 bytes used.

**Then** (all of Task 15 linked; `make c64-mem`):

| Module | Code | Rodata | BSS | HIBSS | LOWBSS | Notes |
|---|---:|---:|---:|---:|---:|---|
| `main.c` | 989 | 535 | 4,753 | | 78 | run flow, save prompt, open; `big` = Program or picker list |
| `editor.c` | 3,596 | 837 | | 181 | | help page ~690 of the rodata |
| `edscreen.s` | 792 | | | 279 | | |
| `textbuf.c` | 1,120 | | | | | |
| `disk.c` | 599 | 99 | | | 52 | load, save, messages |
| `disk.s` | 575 | | | | 14 | KERNAL calls (C version: ~1,100 bytes) |
| `picker.c` | 567 | 69 | | | 8 | screen and keys |
| `picker.s` | 546 | 18 | | 51 | 19 | INDEX and directory parsing, sorted insert (C version: ~750) |
| `vm.c` | 2,426 | | 26 | 2,560 | | the stack in HIBSS |

| Area | Range | Bytes |
|---|---|---:|
| CODE | `$0840-$7464` | 27,685 |
| RODATA + DATA + INIT | `$7465-$7E36` | 2,514 |
| BSS | `$7E37-$978C` | 6,486 |
| **free below the C stack** | **`$978D-$97FF`** | **115** |
| HIBSS | `$C000-$CBFE` | 3,071 of 3,072 (1 free) |
| LOWBSS (new) | `$033C-$03E9` | 174 of 192 (18 free) |

- **LOWBSS** is a new BSS-only area in `pl0-c64.cfg`: the KERNAL's
  cassette buffer `$033C-$03FB`, unused because the program never touches
  tape (disk I/O does not use it; on real hardware a fast-load cartridge
  or loader might use the tape buffer area, which would clash). crt0
  does not clear it; `main` clears
  it with HIBSS. It holds the small variables of `main.c`, `disk.{c,s}`
  and `picker.{c,s}`.
- The picker list reuses the idle `Program` (a union in `main.c`, 118
  entries of 40 bytes); see editor.md for the ownership rule.
- Without the C-to-assembly moves (disk, picker parsing) and LOWBSS the
  program overflowed `$97FF` by 988 bytes.
- Left for Tasks 16-18 (after the review fixes: message buffer, read
  errors, input after a drawing): 115 bytes of low RAM, 18 of LOWBSS, 1
  of HIBSS. The module table above is from before those fixes.
  Further room, if needed: the C stack (2 KB, of which the deepest
  compile uses 460 bytes: [measured](#the-stacks-measured); shrinking it
  is possible but leaves less margin for future recursion), the help
  page (~690 bytes), and the rest of `$02A7-$02FF` (the test handshake
  uses `$02A7/$02A8` only in check programs).

### The final review's fixes

The reserved save names (`PL0`, `INDEX`) and the disabled RESTORE cost
60 bytes of code in `main.c`; removing the unused `editor_modified()`
gave 6 back. BSS end `$978C` -> `$97C2`: 61 bytes free below `$97FF`
(was 115); LOWBSS and HIBSS unchanged. The generated tables above are
current.
