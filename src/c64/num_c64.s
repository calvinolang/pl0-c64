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
; plus its own $22-$2A and $57-$70. num_parse and num_format (at the end of
; this file) do not use ROM FIN/FOUT, which lose the last bits.

        .include "ascii.inc"   ; ASCII character constants
        .assert 'E' = $45 && 'a' = $61, error, "text must be ASCII: include ascii.inc"
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

; ---- bignum primitives for exact parsing and formatting -----------------
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

; ---- exact conversions: num_parse and num_format ------------------------
; The methods are described at the end of num_c64.c (exact bignum
; arithmetic, round half to even); written in assembly because the cc65 C
; version was about 2 KB larger. Text is ASCII (ascii.inc): the exponent
; letter is read as 'e' or 'E' and written as 'E', as on the host.

        .export _num_parse, _num_format
        .importzp ptr3, ptr4


.bss
fa:     .res 5          ; working copy of the operand
dg:     .res 10         ; decimal digits
oi:     .res 1          ; output index
stk:    .res 1          ; sticky: nonzero if a remainder was lost
sexp:   .res 1          ; decimal scale s (signed)
xexp:   .res 1          ; binary / decimal exponent (signed)
gi:     .res 1          ; index of the first significant digit
last:   .res 1          ; index of the last nonzero digit
cnt:    .res 1
nd:     .res 1          ; significant digits read
dot:    .res 1
dig:    .res 1          ; any mantissa digit seen
neg:    .res 1          ; $80 if negative
eneg:   .res 1
ee:     .res 1          ; decimal exponent (signed)
ex:     .res 1          ; exponent digits (saturating)
bexp:   .res 2          ; binary exponent (16-bit)
bt2:    .res 1
bt3:    .res 1
bb:     .res 1
ysave:  .res 1

.rodata
tenm4:  .byte   $73, $51, $B7, $17, $59 ; C64 value nearest 1e-4 (above it)
two9:   .byte   $00, $94, $35, $77      ; 2e9, little-endian

.code

; A = bit length of big (at most 248)
bits:   ldx     _blen
        beq     @zero
        dex
        txa
        asl     a
        asl     a
        asl     a
        sta     bb
        lda     _big,x          ; top byte, nonzero
@loop:  inc     bb
        lsr     a
        bne     @loop
        lda     bb
@zero:  rts

; C = 0 and A = value if A held a decimal digit
isdig:  sec
        sbc     #'0'
        cmp     #10
        rts

; optional sign at (ptr4),Y: skips it; A = $80 for '-', else 0
psign:  lda     (ptr4),y
        cmp     #'-'
        bne     @plus
        iny
        lda     #$80
        rts
@plus:  cmp     #'+'
        bne     @none
        iny
@none:  lda     #0
        rts

; int __fastcall__ num_parse(const char *s, num_t *r);   0 ok, -1 bad
; Syntax as the host: [+-] digits [. digits] [(e|E) [+-] digits], at least
; one mantissa digit, at most 32 characters and 24 significant digits.
; value = D * 10^ee, D = the significant digits as an integer.
_num_parse:
        sta     ptr3            ; r
        stx     ptr3+1
        jsr     popax
        sta     ptr4            ; s
        stx     ptr4+1
        ldy     #0
@len:   lda     (ptr4),y        ; at most 32 characters
        beq     @lenok
        iny
        cpy     #33
        bne     @len
        jmp     perr
@lenok: lda     #0
        sta     _blen
        sta     nd
        sta     dot
        sta     stk
        sta     dig
        sta     ee
        sta     bexp
        sta     bexp+1
        tay
        jsr     psign
        sta     neg
@lp:    lda     (ptr4),y
        jsr     isdig
        bcs     @notd
        ldx     #1
        stx     dig
        ldx     _blen
        bne     @sig
        cmp     #0
        beq     @lead           ; leading zero
@sig:   ldx     nd
        cpx     #24
        bne     @add
        jmp     perr
@add:   inc     nd
        tax                     ; X = digit
        lda     #10
        sty     ysave
        jsr     _big_muladd     ; big = big * 10 + digit
        ldy     ysave
@lead:  lda     dot
        beq     @next
        dec     ee
@next:  iny
        bne     @lp
@notd:  lda     (ptr4),y
        cmp     #'.'
        bne     @mend
        lda     dot
        bne     @mend
        inc     dot
        bne     @next
@mend:  lda     dig
        bne     @hasd
        jmp     perr
@hasd:  lda     (ptr4),y
        cmp     #'E'
        beq     @exp
        cmp     #'e'
        bne     @noexp
@exp:   iny
        jsr     psign
        sta     eneg
        lda     (ptr4),y
        jsr     isdig
        bcc     @edig
        jmp     perr
@edig:  lda     #0
        sta     ex
@el:    lda     (ptr4),y
        jsr     isdig
        bcs     @eend
        tax
        lda     ex
        cmp     #25
        bcs     @esat
        asl     a               ; ex * 10 + digit (< 250)
        sta     bt2
        asl     a
        asl     a
        adc     bt2
        stx     bt2
        adc     bt2
        sta     ex
        bcc     @enext
@esat:  lda     #255            ; saturated: far out of range
        sta     ex
@enext: iny
        bne     @el
@eend:  lda     ex
        cmp     #81
        bcc     @einr
        lda     _blen           ; |exponent| > 80
        beq     @zero1
        lda     eneg
        bne     @zero1
        jmp     perr
@zero1: jmp     pzero
@einr:  lda     eneg
        beq     @eplus
        lda     ee
        sec
        sbc     ex
        sta     ee
        jmp     @noexp
@eplus: lda     ee
        clc
        adc     ex
        sta     ee
@noexp: lda     (ptr4),y        ; nothing may follow
        beq     @endok
        jmp     perr
@endok: lda     _blen
        beq     @zero1
        lda     ee              ; value < 10^(ee + nd)
        clc
        adc     nd
        bmi     @rneg
        cmp     #41
        bcc     @rok
        jmp     perr            ; >= 1e40
@rneg:  cmp     #<-39
        bcc     @zero1          ; < 1e-40
@rok:   lda     ee
        bne     @rnz
        jmp     @norm
@rnz:   bmi     @eneg
        sta     cnt             ; D * 10^ee
@m10:   lda     #10
        ldx     #0
        jsr     _big_muladd
        dec     cnt
        bne     @m10
        beq     @norm
@eneg:  eor     #$FF            ; i = -ee
        clc
        adc     #1
        sta     cnt
        sta     bt2             ; shift so >= 34 bits remain: 34 + 3i - bits
        asl     a               ; (5^i < 2^(3i))
        adc     bt2
        adc     #34
        sta     bt2
        jsr     bits
        sta     bt3
        lda     bt2
        sec
        sbc     bt3
        bcs     @k
        lda     #0
@k:     sta     bt2
        jsr     _big_shl
        lda     bt2             ; bexp = -(k + i)
        clc
        adc     cnt
        sta     bexp
        lda     #0
        rol     a
        sta     bexp+1
        lda     #0
        sec
        sbc     bexp
        sta     bexp
        lda     #0
        sbc     bexp+1
        sta     bexp+1
@d125:  lda     cnt             ; / 5^i: by 125 while i >= 3, then by 5
        cmp     #3
        bcc     @d5
        sbc     #3
        sta     cnt
        lda     #125
        jsr     div_sticky
        jmp     @d125
@d5:    lda     cnt
        beq     @norm
        lda     #5
        jsr     div_sticky
        dec     cnt
        jmp     @d5
@norm:  jsr     bits            ; normalise to 33 bits, bexp += bits
        sta     bt2
        clc
        adc     bexp
        sta     bexp
        bcc     @nc
        inc     bexp+1
@nc:    lda     bt2
        cmp     #34
        bcc     @le33
        sbc     #33
        jsr     _big_shr
        ora     stk
        sta     stk
        jmp     @rnd
@le33:  lda     #33
        sec
        sbc     bt2
        jsr     _big_shl
@rnd:   jsr     round           ; half bit: to even
        lda     _blen
        cmp     #5
        bcc     @pack
        lda     #1              ; carried to 2^32
        jsr     _big_shr
        inc     bexp
        bne     @pack
        inc     bexp+1
@pack:  lda     bexp            ; exponent byte = bexp + $80, 1..255
        clc
        adc     #$80
        tax
        lda     bexp+1
        adc     #0
        bmi     pzero
        bne     perr
        txa
        beq     pzero
        ldy     #0
        sta     (ptr3),y
        lda     _big+3
        and     #$7F
        ora     neg
        iny
        sta     (ptr3),y
        lda     _big+2
        iny
        sta     (ptr3),y
        lda     _big+1
        iny
        sta     (ptr3),y
        lda     _big
        iny
        sta     (ptr3),y
        lda     #0
        tax
        rts
pzero:  lda     #0
        ldy     #4
@z:     sta     (ptr3),y
        dey
        bpl     @z
        tax
        rts
perr:   lda     #$FF
        tax
        rts

; big /= A, stk |= remainder
div_sticky:
        jsr     _big_div
        ora     stk
        sta     stk
        rts

; drop the lowest bit of big; round up if it was 1 and (stk or the new
; lowest bit is 1): round half to even
round:  lda     #1
        jsr     _big_shr
        cmp     #0              ; (flags are from ldx #0)
        beq     @done
        lda     stk
        bne     @up
        lda     _big
        and     #1
        beq     @done
@up:    lda     #1
        ldx     #1
        jmp     _big_muladd
@done:  rts

; output one character to (ptr3),oi; keeps X
emit:   ldy     oi
        sta     (ptr3),y
        inc     oi
        rts

; emit dg[gi..last]
drest:  ldx     gi
@l:     lda     dg,x
        jsr     emit
        cpx     last
        beq     @done
        inx
        bne     @l
@done:  rts

; void __fastcall__ num_format(const num_t *a, char *buf);   buf >= 16
; The host rules (src/host/num_host.c), see num_c64.c for the method.
_num_format:
        sta     ptr3            ; buf
        stx     ptr3+1
        jsr     popax
        sta     ptr4            ; a
        stx     ptr4+1
        ldy     #4
@cp:    lda     (ptr4),y
        sta     fa,y
        dey
        bpl     @cp
        iny
        sty     oi
        sty     stk
        lda     fa
        bne     @nz
        lda     #'0'
        jsr     emit
        jmp     fend
@nz:    lda     fa+1
        bpl     @pos
        lda     #'-'
        jsr     emit
@pos:   lda     fa              ; s = 8 - fl, fl <= floor((K-1) log10 2)
        sec
        sbc     #$81
        bcc     @negk
        lsr     a               ; k >= 0: fl = k/4 + k/32 + k/64
        lsr     a
        sta     bt2
        lsr     a
        lsr     a
        lsr     a
        tax
        lsr     a
        clc
        adc     bt2
        sta     bt2
        txa
        adc     bt2
        eor     #$FF            ; s = 8 - fl
        sec
        adc     #8
        jmp     @have
@negk:  eor     #$FF            ; k < 0: n = -k (carry clear)
        adc     #1
        lsr     a               ; s = 10 + n/4 + n/16
        lsr     a
        sta     bt2
        lsr     a
        lsr     a
        clc
        adc     bt2
        adc     #10
@have:  sta     sexp
        lda     fa+1            ; big = 32-bit mantissa
        ora     #$80
        sta     _big+3
        lda     fa+2
        sta     _big+2
        lda     fa+3
        sta     _big+1
        lda     fa+4
        sta     _big
        lda     #4
        sta     _blen
        lda     fa              ; binary exponent of Q: K - 1 - 30 + s
        sec
        sbc     #$81+30
        clc
        adc     sexp
        sta     xexp
        lda     sexp            ; s > 0: * 5^s
        bmi     @nomul
        beq     @nomul
        sta     cnt
@mul:   lda     #5
        ldx     #0
        jsr     _big_muladd
        dec     cnt
        bne     @mul
@nomul: lda     xexp            ; * 2^x
        beq     @nosh
        bmi     @shr
        jsr     _big_shl
        jmp     @nosh
@shr:   eor     #$FF
        clc
        adc     #1
        jsr     _big_shr
        sta     stk
@nosh:  lda     sexp            ; s < 0: / 5^-s
        bpl     @ge
        eor     #$FF
        clc
        adc     #1
        sta     cnt
@div:   lda     #5
        jsr     div_sticky
        dec     cnt
        bne     @div
@ge:    lda     _blen           ; while Q >= 2e9: drop a digit
        cmp     #4
        bne     @gecmp
        ldx     #3
@gel:   lda     _big,x
        cmp     two9,x
        bne     @gecmp
        dex
        bpl     @gel
@gecmp: bcc     @lt
        lda     #10
        jsr     div_sticky
        dec     sexp
        jmp     @ge
@lt:    jsr     round
        lda     #9              ; 10 digits, dg[0] = '1' only for 10^9
        sta     cnt
@dgl:   lda     #10
        jsr     _big_div
        ora     #'0'
        ldx     cnt
        sta     dg,x
        dec     cnt
        bpl     @dgl
        lda     #9              ; x = 9 - s (decimal exponent)
        sec
        sbc     sexp
        sta     xexp
        ldx     #1
        lda     dg
        cmp     #'0'
        beq     @g1
        dex                     ; rounded up to 10^9
        inc     xexp
@g1:    stx     gi
        ldy     #9
@tz:    lda     dg,y            ; last nonzero digit (dg[gi] is nonzero)
        cmp     #'0'
        bne     @tzd
        dey
        bne     @tz
@tzd:   sty     last
        lda     xexp            ; E form if x >= 10 or x <= -4, or the
        bmi     @xneg           ; digits are exactly 1E-04 and |a| < 1e-4
        cmp     #10
        bcc     @plain
@toe:   jmp     @efmt
@xneg:  cmp     #<-3
        bcc     @toe
        bne     @plain
        ldx     gi
        cpx     last
        bne     @plain
        lda     dg,x
        cmp     #'1'
        bne     @plain
        lda     fa+1            ; |a| < C64(1e-4)?
        and     #$7F
        sta     fa+1
        ldx     #0
@bl:    lda     fa,x
        cmp     tenm4,x
        bne     @blne
        inx
        cpx     #5
        bne     @bl
@blne:  bcc     @efmt
@plain: lda     xexp
        beq     @frac
        bmi     @frac
        lda     #0              ; x > 0: digits, point after x of them,
        sta     cnt             ; zeros up to x
@ip:    lda     cnt
        cmp     xexp
        bne     @nodot
        lda     #'.'
        jsr     emit
@nodot: lda     cnt
        clc
        adc     gi
        tax
        lda     #'0'
        cpx     last
        beq     @dig
        bcs     @em
@dig:   lda     dg,x
@em:    jsr     emit
        inc     cnt
        lda     cnt
        cmp     xexp
        bcc     @ip
        clc
        adc     gi
        cmp     last
        bcc     @ip
        beq     @ip
        bcs     fend
@frac:  lda     #'0'            ; x <= 0: 0.000ddd
        jsr     emit
        lda     #'.'
        jsr     emit
        lda     xexp
        beq     @fd
@fz:    lda     #'0'
        jsr     emit
        inc     xexp
        bne     @fz
@fd:    jsr     drest
        jmp     fend
@efmt:  ldx     gi              ; d.dddE+xx
        lda     dg,x
        jsr     emit
        cpx     last
        beq     @noe
        lda     #'.'
        jsr     emit
        inc     gi
        jsr     drest
@noe:   lda     #'E'
        jsr     emit
        dec     xexp
        lda     xexp
        bpl     @eplus
        lda     #'-'
        jsr     emit
        lda     xexp
        eor     #$FF
        clc
        adc     #1
        jmp     @e2
@eplus: lda     #'+'
        jsr     emit
        lda     xexp
@e2:    ldx     #'0'-1          ; two digits
        sec
@ten:   inx
        sbc     #10
        bcs     @ten
        adc     #10+'0'
        pha
        txa
        jsr     emit
        pla
        jsr     emit
fend:   lda     #0
        ldy     oi
        sta     (ptr3),y
        rts
