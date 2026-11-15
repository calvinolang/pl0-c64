; gfx_c64.s -- gfx_line (src/platform/gfx.h): clip, then Bresenham into the
; hi-res bitmap at $E000 (VIC bank 3) that gfx_open (gfx_c64.c) set up.
;
; gfx_line keeps the endpoints in x0,y0,x1,y1 below. If one lies outside
; 0..319 x 0..199 it calls the shared clip_line (src/core/clip.c, so the
; C64 clips exactly like the host) on them; clip_line costs about 3,000
; cycles (32-bit arithmetic) and returns a visible line unchanged, so it is
; skipped when all four values are inside. After clipping the range is
; checked again, as a guard: whatever the clipper returns, nothing outside
; the 8000-byte bitmap is ever written. Then every pixel lies in
; $E000-$FF3F:
;   pixel (x,y) = byte $E000 + (y>>3)*320 + (x>>3)*8 + (y&7), bit 7-(x&7).
; The pointer ptr1 holds the address of the current 8x8 cell row base
; ($E000 + (y>>3)*320 + (x>>3)*8), Y holds y&7 and `mask` the pixel bit, so
; plotting is LDA (ptr1),Y / ORA / STA and every step is an increment:
;   x+1: mask >>= 1; when the bit falls out, mask = $80 and ptr1 += 8
;   y+1: Y += 1; at 8, Y = 0 and ptr1 += 320    (y-1 the same way down)
; The endpoints are ordered so that x always grows. With dx = x1-x0 and
; dy = |y1-y0|, the loop runs along the longer axis (dx >= dy: x-major)
; for max(dx,dy)+1 pixels, err starts at major/2, minus minor per pixel,
; and when it goes negative it gets +major and the minor axis steps.
; tools/check_bitmap.py has a Python model of exactly this loop.
;
; Reading the bitmap needs the KERNAL ROM out: the whole line runs with
; interrupts off and $01 = %xx101 (RAM at $A000-$BFFF and $E000-$FFFF, I/O
; in), restored afterwards. A 320-pixel line keeps interrupts off for about
; 31 ms (measured). The CIA latches only one pending timer interrupt, so the
; jiffy IRQs falling due in that window collapse into one, served late: the
; jiffy clock TI falls behind by 1-2 ticks per long line and the matching
; KERNAL key scans are skipped. Later code must not rely on TI (or on key
; scans) across gfx_line calls. An NMI (RESTORE) in that
; window vectors through RAM $FFFA, which gfx_open points at gfx_nmi_rti.

        .include "ascii.inc"   ; ASCII character constants
        .export _gfx_line, _gfx_nmi_rti
        .import _clip_line, popax, pushax
        .importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3

err     = ptr2          ; 16-bit Bresenham error (x-major); low byte only (y-major)
cnt     = ptr3          ; pixels left after the current one
mask    = tmp1          ; pixel bit in the current byte
dy      = tmp2          ; |y1 - y0|, 0..199
ydir    = tmp3          ; bit 7 set: y decreases

.bss
; int16 endpoints; keep the order: `inside` and the swap index them as
; 4-byte records x0,y0 and x1,y1. After the checks only y's low byte is used.
x0:     .res 2
y0:     .res 2
x1:     .res 2
y1:     .res 2
dx:      .res 2         ; x1 - x0, 0..319
save01:  .res 1

.rodata
bits:   .byte $80, $40, $20, $10, $08, $04, $02, $01

.code
_gfx_nmi_rti:
        rti

; void __fastcall__ gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
_gfx_line:
        sta     y1
        stx     y1+1
        jsr     popax
        sta     x1
        stx     x1+1
        jsr     popax
        sta     y0
        stx     y0+1
        jsr     popax
        sta     x0
        stx     x0+1
        jsr     inside
        bcc     draw            ; all inside: no clipping needed
        lda     #<x0            ; clip_line(&x0, &y0, &x1, &y1)
        ldx     #>x0
        jsr     pushax
        lda     #<y0
        ldx     #>y0
        jsr     pushax
        lda     #<x1
        ldx     #>x1
        jsr     pushax
        lda     #<y1
        ldx     #>y1
        jsr     _clip_line
        tax                     ; 0: nothing visible
        beq     ret
        jsr     inside
        bcc     draw
ret:    rts

; C = 0 if 0 <= x0,x1 <= 319 and 0 <= y0,y1 <= 199 (int16, so negative
; values have a high byte of $FF and fail)
inside: ldx     #4              ; x1,y1 then x0,y0
@lp:    lda     x0+1,x
        beq     @y              ; 0..255
        cmp     #>320
        bne     @out
        lda     x0,x
        cmp     #<320
        bcs     @out
@y:     lda     y0+1,x
        bne     @out
        lda     y0,x
        cmp     #200
        bcs     @out
        dex
        dex
        dex
        dex
        bpl     @lp
        clc
        rts
@out:   sec
        rts

draw:
; order the endpoints so that x0 <= x1
        lda     x1
        cmp     x0
        lda     x1+1
        sbc     x0+1
        bcs     ordered
        ldx     #3
swap:   lda     x0,x
        ldy     x1,x
        sta     x1,x
        tya
        sta     x0,x
        dex
        bpl     swap
ordered:
; dx = x1 - x0
        lda     x1
        sec
        sbc     x0
        sta     dx
        lda     x1+1
        sbc     x0+1
        sta     dx+1
; dy = |y1 - y0|, ydir
        ldx     #0
        lda     y1
        sec
        sbc     y0
        bcs     :+
        eor     #$FF
        adc     #1              ; C = 0: A = -A
        dex                     ; ydir = $FF: up
:       sta     dy
        stx     ydir
; ptr1 = $E000 + row*320 + (x0 & $FFF8), row = y0>>3; row*320 = row*256 + row*64
        lda     y0
        lsr
        lsr
        lsr
        sta     ptr1+1          ; row (temporarily)
        ldx     #0
        stx     ptr1
        lsr
        ror     ptr1
        lsr
        ror     ptr1            ; A = row>>2, ptr1 = (row&3)<<6: row*64; C = 0
        adc     ptr1+1
        adc     #$E0            ; high byte: $E0 + row + row*64/256 (no carry, max $FE)
        sta     ptr1+1
        lda     x0
        and     #$F8
        adc     ptr1            ; C = 0 from above
        sta     ptr1
        lda     x0+1
        adc     ptr1+1
        sta     ptr1+1
        lda     x0
        and     #7
        tax
        lda     bits,x
        sta     mask
        lda     y0
        and     #7
        tay
; KERNAL out for the whole line
        php
        sei
        lda     $01
        sta     save01
        and     #$F8
        ora     #$05
        sta     $01
; choose the major axis
        lda     dx+1
        bne     xmajor
        lda     dx
        cmp     dy
        bcc     ymajor

; ---- x-major: dx >= dy, dx+1 pixels
xmajor: lda     dx+1
        sta     cnt+1
        lsr
        sta     err+1
        lda     dx
        sta     cnt
        ror
        sta     err             ; err = dx/2
xloop:  lda     (ptr1),y
        ora     mask
        sta     (ptr1),y
        lda     cnt             ; cnt-- or done
        bne     :+
        lda     cnt+1
        beq     done
        dec     cnt+1
:       dec     cnt
        lsr     mask            ; x+1
        bcc     :+
        ror     mask            ; mask = $80, C = 0
        lda     ptr1
        adc     #8
        sta     ptr1
        bcc     :+
        inc     ptr1+1
:       lda     err             ; err -= dy
        sec
        sbc     dy
        sta     err
        bcs     xloop           ; no borrow: err >= 0 still
        lda     err+1
        sbc     #0
        sta     err+1
        bpl     xloop
        lda     err             ; err < 0: err += dx, y step
        clc
        adc     dx
        sta     err
        lda     err+1
        adc     dx+1
        sta     err+1
        jsr     ystep
        jmp     xloop

; ---- y-major: dy > dx (so dx < 199 fits a byte), dy+1 pixels.
; err stays in 0..dy-1 < 256, so 8 bits with the borrow as the sign suffice.
ymajor: lda     dy
        sta     cnt
        lsr
        sta     err
yloop:  lda     (ptr1),y
        ora     mask
        sta     (ptr1),y
        lda     cnt
        beq     done
        dec     cnt
        jsr     ystep
        lda     err             ; err -= dx
        sec
        sbc     dx
        bcs     ynox            ; no borrow: no x step
        adc     dy              ; C = 0: err += dy (back in 0..dy-1)
        sta     err
        lsr     mask            ; x+1
        bcc     yloop
        ror     mask            ; mask = $80, C = 0
        lda     ptr1
        adc     #8
        sta     ptr1
        bcc     yloop
        inc     ptr1+1
        bne     yloop           ; always (ptr1+1 is $E0-$FF)
ynox:   sta     err
        bcs     yloop           ; always

done:   lda     save01
        sta     $01
        plp
        rts

; one pixel row down (ydir clear) or up; changes A and Y
ystep:  bit     ydir
        bmi     up
        iny
        cpy     #8
        bne     yret
        ldy     #0
        lda     ptr1
        clc
        adc     #<320
        sta     ptr1
        lda     ptr1+1
        adc     #>320
        sta     ptr1+1
yret:   rts
up:     dey
        bpl     yret
        ldy     #7
        lda     ptr1
        sec
        sbc     #<320
        sta     ptr1
        lda     ptr1+1
        sbc     #>320
        sta     ptr1+1
        rts
