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

## The program with the editor (Task 14)

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

## Task 15: step 0 and the final program

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
| CODE | `$0840-$73FF` | 27,584 |
| RODATA + DATA + INIT | `$7400-$7DC5` | |
| BSS | `$7DC6-$9718` | 6,483 |
| **free below the C stack** | **`$9719-$97FF`** | **231** |
| HIBSS | `$C000-$CBFE` | 3,071 of 3,072 (1 free) |
| LOWBSS (new) | `$033C-$03E6` | 171 of 192 (21 free) |

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
- Left for Tasks 16-18: 231 bytes of low RAM, 21 of LOWBSS, 1 of HIBSS.
  Further room, if needed: the 2 KB C stack was never measured (stack
  depth of the compiler's recursion), the help page (~690 bytes), and the
  rest of `$02A7-$02FF` (the test handshake uses `$02A7/$02A8` only in
  check programs).
