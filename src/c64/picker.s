; picker.s -- reading the F3 list (src/c64/picker.c): the INDEX file and
; the directory, into entries kept as the 40-column screen rows they show.
; The same in cc65 C was about 750 bytes; low RAM is nearly full.
;
; Entry (PICK_ROW = 40 bytes, no NUL; ed_put stops after 40 characters):
; column 0 a space, the name from column 1 (up to 12 characters), spaces
; up to column 13, the description from column 14 (up to 24). Names
; compare as their space-padded columns 1-12, which sorts like the names.
;
; INDEX lines: `NAME description` + '\n': the examples, kept in file order.
; The directory ("$" on secondary address 0) is a BASIC listing: a load
; address, then lines of a link (2 bytes, 0 0 ends), a block count (2)
; and text up to a 0 byte: `"NAME"   SEQ` for a file. The first line is
; the disk title (skipped); `BLOCKS FREE.` has no quotes. The type is the
; first 3 characters after the closing quote that are not spaces (an
; unclosed file shows `*SEQ`, which does not match). Listed: SEQ files
; with a valid name (disk_name_ok; a space inside the quotes is stored as
; `*` so that `MY PROG` fails the check instead of listing as MY), not INDEX, not an example again,
; sorted by name after the examples; at most pk_max entries in all.
; Zero page: ptr3 (entry pointer), ptr4; disk.s uses ptr1 and ptr2.

        .include "ascii.inc"   ; ASCII character constants
        .assert 'E' = $45 && 'a' = $61, error, "text must be ASCII: include ascii.inc"
        .export _pk_read, _pk_at, _pk_list, _pk_max, _pk_n
        .import _dk_open, _dk_getc, _dk_close, _disk_name_ok
        .importzp ptr3, ptr4

ROW     = 40            ; PICK_ROW (picker.h)
DESC    = 14            ; column of the description
NL      = $0A

.segment "LOWBSS"       ; the cassette buffer (pl0-c64.cfg); main clears it
_pk_list: .res 2        ; the entries (memory lent by main.c)
_pk_max:  .res 1        ; room for this many
_pk_n:    .res 1        ; entries read
nex:    .res 1          ; of which examples (from INDEX)
nm:     .res 14         ; the name of rec, NUL-terminated
.segment "HIBSS"        ; the last 51 bytes there; main clears it
rec:    .res ROW        ; the entry being read
col:    .res 1          ; next column of rec
q:      .res 1          ; quotes seen on a directory line
lt:     .res 1          ; type characters seen
ty:     .res 3
first:  .res 1          ; on the directory's title line
sorted: .res 1          ; add: in name order (directory) or at the end
pos:    .res 1
idx:    .res 1
eofl:   .res 1

.rodata
s_index: .byte "INDEX,S,R", 0
s_iname: .byte "INDEX", 0
s_dir:   .byte "$", 0

.code
; char *__fastcall__ pk_at(uint8_t i): entry i, also left in ptr3
_pk_at: sta     ptr3
        lda     #0
        sta     ptr3+1
        lda     ptr3
        asl     a
        rol     ptr3+1
        asl     a
        rol     ptr3+1
        clc
        adc     ptr3            ; i * 5
        bcc     :+
        inc     ptr3+1
:       asl     a
        rol     ptr3+1
        asl     a
        rol     ptr3+1
        asl     a
        rol     ptr3+1          ; i * 40
        clc
        adc     _pk_list
        sta     ptr3
        lda     ptr3+1
        adc     _pk_list+1
        sta     ptr3+1
        tax
        lda     ptr3
        rts

; rec all spaces, col 1
rec_clear:
        lda     #' '
        ldy     #ROW-1
:       sta     rec,y
        dey
        bpl     :-
        lda     #1
        sta     col
        rts

; rec[col++] = A if col < X
putc:   cpx     col
        beq     :+
        bcc     :+
        ldy     col
        sta     rec,y
        inc     col
:       rts

; the name of rec (up to a space) into nm; A nonzero if it is valid
rec_name:
        ldy     #0
:       lda     rec+1,y
        cmp     #' '
        beq     :+
        sta     nm,y
        iny
        cpy     #13
        bne     :-
:       lda     #0
        sta     nm,y
        lda     #<nm
        ldx     #>nm
        jmp     _disk_name_ok

; Z set if the names of the entry at ptr3 and rec are equal; C clear if
; the entry's is smaller
cmp12:  ldy     #1
:       lda     (ptr3),y
        cmp     rec,y
        bne     :+
        iny
        cpy     #13
        bne     :-
:       rts

; add rec: at the end, or (sorted) in name order after the examples
add:    lda     _pk_n
        cmp     _pk_max
        bcs     @ret            ; full
        sta     pos
        lda     sorted
        beq     @put
        lda     #0
        sta     idx
@ex:    lda     idx             ; an example again: not listed twice
        cmp     nex
        bcs     @find
        jsr     _pk_at
        jsr     cmp12
        beq     @ret
        inc     idx
        bne     @ex
@find:  lda     idx             ; the first later entry not smaller
        cmp     _pk_n
        bcs     @found
        jsr     _pk_at
        jsr     cmp12
        bcs     @found
        inc     idx
        bne     @find
@found: lda     idx
        sta     pos
        lda     _pk_n
        sta     idx
@mv:    lda     idx             ; entries pos..n-1 one place on, last first
        cmp     pos
        beq     @put
        jsr     _pk_at
        lda     ptr3
        sec
        sbc     #ROW
        sta     ptr4
        lda     ptr3+1
        sbc     #0
        sta     ptr4+1
        ldy     #ROW-1
:       lda     (ptr4),y
        sta     (ptr3),y
        dey
        bpl     :-
        dec     idx
        jmp     @mv
@put:   lda     pos
        jsr     _pk_at
        ldy     #ROW-1
:       lda     rec,y
        sta     (ptr3),y
        dey
        bpl     :-
        inc     _pk_n
@ret:   rts

; uint8_t pk_read(void): read INDEX (if there is one) and the directory
; into pk_list; 0, or the disk result code
_pk_read:
        lda     #0
        sta     _pk_n
        sta     nex
        sta     sorted
        lda     #<s_index
        ldx     #>s_index
        jsr     _dk_open
        cmp     #0
        bne     @noidx
        jsr     rec_clear
@il:    jsr     _dk_getc
        stx     eofl            ; $FF at the end
        cpx     #$FF
        beq     @line
        cmp     #NL
        beq     @line
        ldx     col
        cmp     #' '
        bne     @ch
        cpx     #DESC
        bcs     @ch
        lda     #DESC           ; the space after the name
        sta     col
        bne     @il
@ch:    ldx     #DESC+24
        jsr     putc
        jmp     @il
@line:  lda     col
        cmp     #2
        bcc     @skip           ; an empty line
        jsr     rec_name
        tax
        beq     @skip
        jsr     add
@skip:  jsr     rec_clear
        lda     eofl
        beq     @il
        jsr     _dk_close
@noidx: cmp     #62             ; no INDEX: a disk of the user's own
        beq     @dir
        cmp     #0
        beq     @dir
        ldx     #0
        rts
@dir:   lda     _pk_n
        sta     nex
        inc     sorted
        lda     #<s_dir
        ldx     #>s_dir
        jsr     _dk_open
        cmp     #0
        beq     :+
        rts                     ; X = 0
@dend0: jmp     _dk_close       ; (in branch range of the loop below)
:       jsr     _dk_getc        ; load address
        jsr     _dk_getc
        lda     #1
        sta     first
@dl:    jsr     _dk_getc        ; link
        cpx     #$FF
        beq     @dend0
        jsr     _dk_getc
        jsr     _dk_getc        ; block count
        jsr     _dk_getc
        jsr     rec_clear
        lda     #0
        sta     q
        sta     lt
@dc:    jsr     _dk_getc
        cpx     #$FF
        beq     @dend0
        cmp     #0
        beq     @deol
        cmp     #'"'
        bne     @dq
        inc     q
        bne     @dc
@dq:    ldx     q
        cpx     #1
        bne     @dt
        cmp     #' '            ; a space inside a name: make the
        bne     :+              ; name invalid (MY PROG is not listed)
        lda     #'*'
:       ldx     #DESC           ; inside the quotes: the name
        jsr     putc
        jmp     @dc
@dt:    cpx     #2
        bne     @dc
        cmp     #' '
        beq     @dc
        ldx     lt              ; after them: the type
        cpx     #3
        bcs     @dc
        sta     ty,x
        inc     lt
        bne     @dc
@deol:  lda     first
        bne     @dnext
        lda     lt
        cmp     #3
        bne     @dnext
        lda     ty
        cmp     #'S'
        bne     @dnext
        lda     ty+1
        cmp     #'E'
        bne     @dnext
        lda     ty+2
        cmp     #'Q'
        bne     @dnext
        jsr     rec_name
        tax
        beq     @dnext
        ldy     #5              ; not INDEX itself
:       lda     nm,y
        cmp     s_iname,y
        bne     @add
        dey
        bpl     :-
        bmi     @dnext
@add:   jsr     add
@dnext: lda     #0
        sta     first
        jmp     @dl
        
