; vmasm.s -- SPIKE: minimal assembly dispatch loop (LOD, STO, HLT only) on
; the same 4-byte Instr layout as vm2.c (op, lev, a = byte offset), to
; measure what an assembly VM would cost per simple instruction.
        .export _vmasm_run
        .import _code, _stack
        .importzp ptr1, ptr3, ptr4

ip = ptr3
tp = ptr4

.bss
disp:   .res 2 * 5

.code
_vmasm_run:
        lda     #<_code
        sta     ip
        lda     #>_code
        sta     ip+1
        lda     #<_stack
        sta     tp
        sta     disp
        lda     #>_stack
        sta     tp+1
        sta     disp+1
loop:   ldy     #0
        lda     (ip),y          ; opcode
        asl     a
        tax
        lda     jt+1,x
        pha
        lda     jt,x
        pha
        rts                     ; jump to handler
next:   clc
        lda     ip
        adc     #4
        sta     ip
        bcc     loop
        inc     ip+1
        jmp     loop

; ptr1 = disp[lev] + a
addr:   ldy     #1
        lda     (ip),y
        asl     a
        tax
        iny
        clc
        lda     (ip),y
        adc     disp,x
        sta     ptr1
        iny
        lda     (ip),y
        adc     disp+1,x
        sta     ptr1+1
        rts

op_lod: jsr     addr
        clc
        lda     tp
        adc     #5
        sta     tp
        bcc     :+
        inc     tp+1
:       ldy     #4
@c:     lda     (ptr1),y
        sta     (tp),y
        dey
        bpl     @c
        jmp     next

op_sto: jsr     addr
        ldy     #4
@c:     lda     (tp),y
        sta     (ptr1),y
        dey
        bpl     @c
        sec
        lda     tp
        sbc     #5
        sta     tp
        bcs     :+
        dec     tp+1
:       jmp     next

op_int: ldy     #2              ; tp += a * 5 (a < 51 here)
        lda     (ip),y
        sta     ptr1
        asl     a
        asl     a
        adc     ptr1
        clc
        adc     tp
        sta     tp
        bcc     :+
        inc     tp+1
:       jmp     next

op_hlt:
op_bad: rts

jt:     .word   op_bad-1, op_lod-1, op_sto-1, op_bad-1, op_int-1
        .repeat 26
        .word   op_bad-1
        .endrepeat
        .word   op_hlt-1        ; OP_HLT = 31
