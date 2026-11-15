; fp.s -- SPIKE (throwaway): call C64 BASIC V2 ROM floating point from cc65.
;
; Pattern: every shim funnels through `callrom`, which
;   1. remembers the 6502 stack pointer,
;   2. points the BASIC error vector IERROR ($0300) at `trap`,
;   3. banks BASIC ROM in ($01 |= %011),
;   4. JSRs the ROM routine with A/X/Y passed through,
;   5. restores $01 and $0300 and returns A = 0 (ok) or the BASIC error
;      number (14 ILLEGAL QUANTITY, 15 OVERFLOW, 20 DIVISION BY ZERO, ...).
; If the ROM raises an error it does `LDX #err : JMP $A437`, and $A437 is
; `JMP ($0300)`, so `trap` gets control with X = error number; it resets
; the stack pointer to the saved value and leaves through the normal exit.
;
; Spike instrumentation: callrom also snapshots cc65's zero page
; ($02-$1B), fills it with $A5, and after the call records which bytes the
; ROM wrote (fp_zpclobber[]) before restoring them. This is how the
; clobber list in docs/architecture/spike-results.md was measured.

        .export _fp_load, _fp_store, _fp_add, _fp_sub, _fp_mul, _fp_div
        .export _fp_fn, _fp_fout, _fp_from_int, _fp_parse, _fp_cmp
        .export _fp_to_int, _fp_nop
        .export _fp_zpclobber, _fp_lasterr, _fp_endptr, _fp_instrument
        .export callrom, trap, finish, romjsr

; ---- BASIC V2 / KERNAL ROM entry points (verified in VICE, see run.sh) ----
MOVFM   = $BBA2         ; FAC  <- mem (A=lo, Y=hi)
CONUPK  = $BA8C         ; ARG  <- mem (A=lo, Y=hi)          (used inside FADD etc.)
MOVMF   = $BBD4         ; mem  <- FAC, rounded (X=lo, Y=hi)
FSUB    = $B850         ; FAC  <- mem - FAC (A/Y)
FADD    = $B867         ; FAC  <- mem + FAC (A/Y)
FMULT   = $BA28         ; FAC  <- mem * FAC (A/Y)
FDIV    = $BB0F         ; FAC  <- mem / FAC (A/Y)
FOUT    = $BDDD         ; FAC  -> ASCII at $0100, returns A=lo,Y=hi
GIVAYF  = $B391         ; FAC  <- signed int (A=hi, Y=lo)
FACINX  = $B1AA         ; A=hi,Y=lo <- FAC (-32768..32767 else ?ILLEGAL QUANTITY)
FIN     = $BCF3         ; FAC  <- ASCII at TXTPTR (via CHRGET)
FCOMP   = $BC5B         ; A = cmp(FAC, mem A/Y): 0 eq, 1 FAC>mem, $FF FAC<mem
SGN_ABS = $BC58         ; ABS: FAC <- |FAC|
INT     = $BCCC         ; FAC <- floor(FAC)
SQR     = $BF71
EXP     = $BFED
LOG     = $B9EA
NEGOP   = $BFB4         ; FAC <- -FAC
COS     = $E264
SIN     = $E26B
ATN     = $E30E
CHRGOT  = $0079         ; BASIC's CHRGET (copied to ZP at cold start)
TXTPTR  = $7A
IERROR  = $0300         ; BASIC error vector, default $E38B

ZPBASE  = $02           ; cc65 C64 zero page: $02..$1B
ZPLEN   = $1A

.bss
_fp_zpclobber:  .res ZPLEN      ; 1 = ROM wrote this cc65 ZP byte at least once
_fp_lasterr:    .res 1
_fp_endptr:     .res 2          ; fp_parse: TXTPTR after FIN
zpsave:         .res ZPLEN
rega:           .res 1
regx:           .res 1
regy:           .res 1
savesp:         .res 1
old01:          .res 1
olderr:         .res 2
savetxt:        .res 2

.data
_fp_instrument: .byte 1         ; 1 = do the ZP snapshot/fill/compare

.code

; ------------------------------------------------------------------------
; callrom: JSR to the address patched into `target`; A, X, Y passed in.
; Out: A = 0 or BASIC error number (also in _fp_lasterr); ROM's A/X/Y in
; rega/regx/regy.
callrom:
        sta     rega
        stx     regx
        sty     regy
        tsx
        stx     savesp
        lda     _fp_instrument
        beq     @noinst
        ldx     #ZPLEN-1
@sv:    lda     ZPBASE,x
        sta     zpsave,x
        lda     #$A5
        sta     ZPBASE,x
        dex
        bpl     @sv
@noinst:
        lda     IERROR
        sta     olderr
        lda     IERROR+1
        sta     olderr+1
        lda     #<trap
        sta     IERROR
        lda     #>trap
        sta     IERROR+1
        lda     $01
        sta     old01
        ora     #$03            ; LORAM|HIRAM -> BASIC + KERNAL ROM visible
        sta     $01
        lda     rega
        ldx     regx
        ldy     regy
romjsr: jsr     $FFFF           ; operand patched by the shims
target  = * - 2
        sta     rega
        stx     regx
        sty     regy
        lda     #0
finish:
        sta     _fp_lasterr
        lda     old01
        sta     $01
        lda     olderr
        sta     IERROR
        lda     olderr+1
        sta     IERROR+1
        lda     _fp_instrument
        beq     @done
        ldx     #ZPLEN-1
@rs:    lda     ZPBASE,x
        cmp     #$A5
        beq     @same
        lda     #1
        sta     _fp_zpclobber,x
@same:  lda     zpsave,x
        sta     ZPBASE,x
        dex
        bpl     @rs
@done:  lda     _fp_lasterr
        ldx     #0
        rts

; BASIC error: X = error number. Unwind to callrom's frame.
trap:
        txa
        ldx     savesp
        txs
        jmp     finish

.macro  SETTARGET addr
        pha
        lda     #<(addr)
        sta     target
        lda     #>(addr)
        sta     target+1
        pla
.endmacro

; A/X (cc65 fastcall pointer) -> A=lo, Y=hi
.macro  AX2AY
        pha
        txa
        tay
        pla
.endmacro

; void __fastcall__ fp_load(const num_t *x)       FAC = *x
_fp_load:
        AX2AY
        SETTARGET MOVFM
        jmp     callrom

; void __fastcall__ fp_store(num_t *r)            *r = FAC
_fp_store:
        AX2AY
        tax                     ; X=lo, Y=hi
        SETTARGET MOVMF
        jmp     callrom

; unsigned char __fastcall__ fp_add(const num_t *x)   FAC = *x + FAC
_fp_add:
        AX2AY
        SETTARGET FADD
        jmp     callrom
; FAC = *x - FAC
_fp_sub:
        AX2AY
        SETTARGET FSUB
        jmp     callrom
; FAC = *x * FAC
_fp_mul:
        AX2AY
        SETTARGET FMULT
        jmp     callrom
; FAC = *x / FAC
_fp_div:
        AX2AY
        SETTARGET FDIV
        jmp     callrom

; unsigned char __fastcall__ fp_fn(unsigned char fn)   FAC = fn(FAC)
; fn order = num.h: SIN COS ATN SQR ABS INT EXP LOG, then 8 = NEG
_fp_fn:
        asl     a
        tax
        lda     fntab,x
        sta     target
        lda     fntab+1,x
        sta     target+1
        jmp     callrom
fntab:  .word   SIN, COS, ATN, SQR, SGN_ABS, INT, EXP, LOG, NEGOP

; const char * fp_fout(void)       FAC -> string at $0100 (FAC destroyed)
_fp_fout:
        SETTARGET FOUT
        jsr     callrom
        lda     rega
        ldx     regy
        rts

; void __fastcall__ fp_from_int(int v)
_fp_from_int:
        tay                     ; Y = lo
        txa                     ; A = hi
        SETTARGET GIVAYF
        jmp     callrom

; int fp_to_int(void)   (check fp_lasterr afterwards)
_fp_to_int:
        SETTARGET FACINX
        jsr     callrom
        lda     regy
        ldx     rega
        rts

; signed char __fastcall__ fp_cmp(const num_t *x)
_fp_cmp:
        AX2AY
        SETTARGET FCOMP
        jsr     callrom
        lda     rega
        ldx     #0
        cmp     #$80
        bcc     @pos
        dex
@pos:   rts

; unsigned char __fastcall__ fp_parse(const char *s)
; FIN reads through CHRGET/TXTPTR. TXTPTR belongs to BASIC (it is where
; BASIC resumes after our SYS), so it is saved and restored.
_fp_parse:
        pha
        lda     TXTPTR
        sta     savetxt
        lda     TXTPTR+1
        sta     savetxt+1
        pla
        sta     TXTPTR
        stx     TXTPTR+1
        SETTARGET parse_entry
        jsr     callrom
        lda     TXTPTR
        sta     _fp_endptr
        lda     TXTPTR+1
        sta     _fp_endptr+1
        lda     savetxt
        sta     TXTPTR
        lda     savetxt+1
        sta     TXTPTR+1
        lda     _fp_lasterr
        ldx     #0
        rts
parse_entry:
        jsr     CHRGOT          ; A = first char, flags as FIN expects
        jmp     FIN

; void fp_nop(void)  -- callrom overhead only (target = an RTS)
_fp_nop:
        SETTARGET just_rts
        jmp     callrom
just_rts:
        rts
