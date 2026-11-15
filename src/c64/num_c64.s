; num_c64.s -- shims from cc65 C to the C64 BASIC V2 ROM floating point.
;
; Productionised from the Task 1 spike (spikes/rom-float/fp.s); see
; docs/architecture/spike-results.md section 4 for the proof.
; Build note: compile num_c64.c with cc65 into build/ (as the Makefile
; does), never with `cl65 -c` in place: cl65 would write its intermediate
; num_c64.s next to the source, over this file.
;
; Every ROM call goes through `_fp_call`, which
;   1. remembers the 6502 stack pointer,
;   2. points the BASIC error vector IERROR ($0300) at `trap`,
;   3. banks the BASIC ROM in ($01 |= %011) for the call only,
;   4. JSRs the ROM routine with A = X = pointer low byte, Y = high byte
;      (so it fits both the A/Y and the X/Y conventions of the ROM),
;   5. restores $01 and $0300 and returns A = 0, or the BASIC error number
;      (14 ILLEGAL QUANTITY, 15 OVERFLOW, 20 DIVISION BY ZERO).
; A ROM error does `LDX #err : JMP $A437`, and $A437 is `JMP ($0300)`, so
; `trap` runs with X = error number, resets the stack pointer and leaves
; through the normal exit: nothing ever reaches BASIC's error handler.
;
; Rules (callers in num_c64.c keep them): every pointer handed to the ROM
; points below $A000 (never into BASICBSS); interrupts never bank BASIC in.
; The ROM writes only the cc65 zero-page bytes $07 and $0D (caller-scratch),
; plus its own $22-$2A and $57-$70. Parsing and formatting are done in C
; (num_c64.c), not with ROM FIN/FOUT, which lose the last bits.

        .export _fp_call, _num_copy, _fp_a, _fp_y
        .import popa, popax
        .importzp ptr1, ptr2

; ---- BASIC V2 ROM entry points (verified in VICE, spike-results.md) ----
MOVFM   = $BBA2         ; FAC  <- mem (A/Y)
MOVMF   = $BBD4         ; mem  <- FAC, rounded (X/Y)
FSUB    = $B850         ; FAC  <- mem - FAC (A/Y)
FADD    = $B867         ; FAC  <- mem + FAC (A/Y)
FMULT   = $BA28         ; FAC  <- mem * FAC (A/Y)
FDIV    = $BB0F         ; FAC  <- mem / FAC (A/Y)
FCOMP   = $BC5B         ; A = sign(FAC - mem) as 0 / 1 / $FF (A/Y)
FACINX  = $B1AA         ; A = hi, Y = lo <- FAC (floor), -32768..32767
GIVAYF  = $B391         ; FAC  <- signed 16-bit (A = hi, Y = lo)
ABS     = $BC58
INT     = $BCCC         ; floor
SQR     = $BF71
EXP     = $BFED
LOG     = $B9EA
COS     = $E264
SIN     = $E26B
ATN     = $E30E
IERROR  = $0300         ; BASIC error vector

.bss
_fp_a:   .res 1         ; A after the ROM call (FCOMP result, FACINX high)
_fp_y:   .res 1         ; Y after the ROM call (FACINX low)
plo:     .res 1
phi:     .res 1
savesp:  .res 1
old01:   .res 1
olderr:  .res 2

.code

; unsigned char __fastcall__ fp_call(unsigned char op, const void *p);
; op: 0-7 = NUMFN_SIN..NUMFN_LOG on FAC, then the OP_* list of num_c64.c
; (keep both in the order of `optab`). Returns 0 or the BASIC error number.
_fp_call:
        sta     plo
        stx     phi
        jsr     popa            ; A = op
        asl     a
        tax
        lda     optab,x
        sta     target
        lda     optab+1,x
        sta     target+1
        tsx
        stx     savesp
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
        ora     #$03            ; BASIC + KERNAL ROM visible
        sta     $01
        lda     plo
        ldx     plo
        ldy     phi
        jsr     $FFFF           ; operand patched above
target  = * - 2
        sta     _fp_a
        sty     _fp_y
        lda     #0
finish: tax                     ; X = result while restoring
        lda     old01
        sta     $01
        lda     olderr
        sta     IERROR
        lda     olderr+1
        sta     IERROR+1
        txa
        ldx     #0
        rts

; BASIC error: X = error number. Unwind to _fp_call's frame and leave.
trap:   txa
        ldx     savesp
        txs
        jmp     finish

; GIVAYF wants A = high, Y = low; _fp_call passes A = low, Y = high.
fromint:
        sty     phi
        tay
        lda     phi
        jmp     GIVAYF

optab:  .word   SIN, COS, ATN, SQR, ABS, INT, EXP, LOG          ; 0-7
        .word   MOVFM, MOVMF, FADD, FSUB, FMULT, FDIV           ; 8-13
        .word   FCOMP, FACINX, fromint                          ; 14-16

; void __fastcall__ num_copy(num_t *dst, const num_t *src);  5-byte copy
_num_copy:
        sta     ptr1
        stx     ptr1+1
        jsr     popax
        sta     ptr2
        stx     ptr2+1
        ldy     #4
@loop:  lda     (ptr1),y
        sta     (ptr2),y
        dey
        bpl     @loop
        rts

; ---- bignum primitives for exact parsing and formatting (num_c64.c) -----
; `big` is a little-endian unsigned integer of `blen` bytes (no leading
; zero bytes after big_div / big_shr; the bytes from blen up are garbage).

        .export _big, _blen, _big_muladd, _big_div, _big_shl, _big_shr

.bss
_big:   .res 32
_blen:  .res 1
bm:     .res 1          ; multiplier / divisor / shift count
bc:     .res 1          ; carry byte / sticky
bt:     .res 1          ; scratch
bn:     .res 1          ; loop counter

.code

; void __fastcall__ big_muladd(uint16_t am);   big = big * m + a,
; m = low byte (A), a = high byte (X)
_big_muladd:
        sta     bm
        stx     bc              ; carry in = a
        ldx     #0
@byte:  cpx     _blen
        beq     @end
        lda     _big,x          ; 8x8 multiply: bt = multiplier,
        sta     bt              ; A:bt = big[x] * m
        lda     #0
        ldy     #8
        lsr     bt
@mul:   bcc     @noadd
        clc
        adc     bm
@noadd: ror     a
        ror     bt
        dey
        bne     @mul
        tay                     ; Y = high byte
        lda     bt
        clc
        adc     bc
        sta     _big,x
        tya
        adc     #0              ; new carry <= 255
        sta     bc
        inx
        bne     @byte
@end:   lda     bc
        beq     @done
        sta     _big,x
        inc     _blen
@done:  rts

; uint8_t __fastcall__ big_div(uint8_t d);   big = floor(big / d), d >= 1;
; returns the remainder.
_big_div:
        sta     bm
        lda     #0              ; remainder
        ldx     _blen
@byte:  dex
        bmi     @done
        ldy     #8
@bit:   asl     _big,x
        rol     a               ; remainder * 2 + next bit (9 bits)
        bcs     @sub            ; >= 256 > d
        cmp     bm
        bcc     @next
@sub:   sbc     bm              ; carry is set on both paths
        inc     _big,x          ; quotient bit
@next:  dey
        bne     @bit
        beq     @byte
@done:  sta     bc
        jsr     trim
        lda     bc
        ldx     #0
        rts

; void __fastcall__ big_shl(uint8_t t);   big <<= t
_big_shl:
        sta     bm
@loop:  lda     bm
        beq     @done
        dec     bm
        ldx     #0
        lda     _blen
        beq     @done           ; 0 stays 0
        sta     bn
        clc
@byte:  rol     _big,x
        inx
        dec     bn              ; keeps the carry
        bne     @byte
        bcc     @loop
        lda     #1
        sta     _big,x
        inc     _blen
        bne     @loop
@done:  rts

; uint8_t __fastcall__ big_shr(uint8_t t);   big = floor(big / 2^t);
; returns nonzero if a 1 bit was shifted out.
_big_shr:
        sta     bm
        lda     #0
        sta     bc
@loop:  lda     bm
        beq     @done
        dec     bm
        ldx     _blen
        beq     @done
        clc
@byte:  ror     _big-1,x
        dex                     ; keeps the carry
        bne     @byte
        bcc     @loop
        lda     #1              ; a 1 bit was lost
        sta     bc
        bne     @loop
@done:  jsr     trim
        lda     bc
        ldx     #0
        rts

; drop leading zero bytes
trim:   ldx     _blen
        beq     @done
        lda     _big-1,x
        bne     @done
        dec     _blen
        bne     trim
@done:  rts
