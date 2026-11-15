; disk.s -- the KERNAL side of the C64 disk layer (src/c64/disk.h; the
; file operations built on it are in disk.c). The same code in cc65 C was
; about 1,100 bytes; low RAM is nearly full (memory-map.md).
;
; Logical file 15 is the drive's command/error channel, opened without a
; name first (OPEN then sends nothing over the bus); file 2 is the data
; file. The status after an OPEN, after a command and after closing a
; written file comes from channel 15: `NN, TEXT,TT,SS` + CR, of which only
; NN is kept; 0-19 are fine (01 FILES SCRATCHED), and so is 73 (the
; drive's power-on message, when nothing ran before). A missing drive shows
; as a failed OPEN, CHKIN or CHKOUT (KERNAL error 5, device not present)
; or as a status line without digits: DK_NODEV (254). Every operation
; starts by closing 2 and 15, so a failed earlier one never leaves a
; "file open" behind; closing 15 closes every channel of the drive, so 2
; is always closed first.
;
; Text bytes pass to and from the KERNAL one at a time in A. The text
; buffer lies under the BASIC ROM ($A000), which cc65 keeps banked out, so
; (ptr),y reaches its RAM; no pointer into it reaches a ROM routine.
; Zero page: ptr1, ptr2 (cc65's, caller-saved; the KERNAL does not use
; $02-$1B). Every function returns with X = 0 (or A/X = -1, a count).

        .include "ascii.inc"   ; ASCII character constants
        .assert 'E' = $45 && 'a' = $61, error, "text must be ASCII: include ascii.inc"
        .export _disk_dev, _disk_init, _disk_name_ok
        .export _dk_open, _dk_getc, _dk_close, _dk_text, _dk_write, _dk_cmd, _dk_lim
        .importzp ptr1, ptr2

READST  = $FFB7
SETLFS  = $FFBA
SETNAM  = $FFBD
OPEN    = $FFC0
CLOSE   = $FFC3
CHKIN   = $FFC6
CHKOUT  = $FFC9
CLRCHN  = $FFCC
CHRIN   = $FFCF
CHROUT  = $FFD2
FA_LAST = $BA           ; KERNAL: the device last used (the program's own)

LF_DATA = 2
LF_CMD  = 15
NODEV   = 254           ; DK_NODEV in disk.h
DK_READ = 252           ; DK_READ in disk.h
CR      = $0D
NL      = $0A

.segment "LOWBSS"       ; the cassette buffer (pl0-c64.cfg); main clears it
_disk_dev: .res 1       ; device number
_dk_lim:   .res 2       ; dk_text stops after this many text bytes
code:   .res 1          ; status: the DOS error number
cnt:    .res 1          ; status: characters read; dk_open: name length
ch:     .res 1          ; the byte just read
st:     .res 1          ; READST after it
eof:    .res 1          ; nonzero: dk_getc has nothing more
outm:   .res 1          ; dk_open: nonzero for a file opened to write
sa:     .res 1          ; dk_open: secondary address
prevcr: .res 1          ; dk_text: the last byte was CR
store:  .res 1          ; dk_text: nonzero when storing
count:  .res 2          ; dk_text: text bytes so far

.code
; void disk_init(void): the device the program was loaded from, else 8
_disk_init:
        lda     FA_LAST
        cmp     #8
        bcc     @d
        cmp     #31
        bcc     @ok
@d:     lda     #8
@ok:    sta     _disk_dev
        rts

; back to the keyboard and screen; close 2, then 15
close_all:
        jsr     CLRCHN
        lda     #LF_DATA
        jsr     CLOSE
        lda     #LF_CMD
        jmp     CLOSE

; close all, then open 15 without a name
begin:  jsr     close_all
        lda     #LF_CMD
        ldx     _disk_dev
        ldy     #15
        jsr     SETLFS
        lda     #0
        jsr     SETNAM
        jmp     OPEN

; A = the drive status (0 when fine), X = 0; reads channel 15
status: ldx     #LF_CMD
        jsr     CHKIN
        bcs     nodev
        lda     #0
        sta     code
        sta     cnt
@l:     jsr     CHRIN
        sta     ch
        jsr     READST
        sta     st
        lda     cnt
        cmp     #2
        bcs     @nd
        lda     ch              ; the first two characters: digits
        sec
        sbc     #'0'
        cmp     #10
        bcs     nodev           ; no number: nobody answered
        pha
        lda     code            ; code = code * 10 + digit
        asl     a
        asl     a
        clc
        adc     code
        asl     a
        sta     code
        pla
        clc
        adc     code
        sta     code
@nd:    inc     cnt
        lda     ch
        cmp     #CR
        beq     @done
        lda     st
        bne     @done
        lda     cnt
        cmp     #40
        bne     @l
@done:  jsr     CLRCHN
        lda     code
        cmp     #20
        bcc     ret0
        cmp     #73
        beq     ret0
        ldx     #0
        rts
nodev:  jsr     CLRCHN
        lda     #NODEV
        ldx     #0
        rts
ret0:   lda     #0
        tax
        rts

; uint8_t __fastcall__ dk_open(const char *fname)
; Open fname as file 2: secondary address 0 for "$" (the directory),
; else 2; a name ending in W (NAME,S,W) is opened for output (CHKOUT),
; anything else for input (CHKIN). 0, or the result code (all closed).
_dk_open:
        sta     ptr1
        stx     ptr1+1
        jsr     begin
        ldy     #0
@len:   lda     (ptr1),y
        beq     @e
        iny
        bne     @len
@e:     sty     cnt
        dey
        lda     (ptr1),y        ; the last character
        ldx     #0
        cmp     #'W'
        bne     :+
        inx
:       stx     outm
        ldy     #0
        lda     (ptr1),y
        ldx     #2
        cmp     #'$'
        bne     :+
        ldx     #0
:       stx     sa
        lda     cnt
        ldx     ptr1
        ldy     ptr1+1
        jsr     SETNAM
        lda     #LF_DATA
        ldx     _disk_dev
        ldy     sa
        jsr     SETLFS
        jsr     OPEN
        bcs     @nodev
        jsr     status
        cmp     #0
        bne     @fail
        sta     eof
        ldx     #LF_DATA
        lda     outm
        bne     @w
        jsr     CHKIN
        jmp     @c
@w:     jsr     CHKOUT
@c:     bcs     @nodev
        jmp     ret0
@nodev: lda     #NODEV
@fail:  pha
        jsr     close_all
        pla
        ldx     #0
        rts

; int dk_getc(void): the next byte of file 2, or -1 at the end or after
; an error (READST bits other than $40 EOF: the byte is not data)
_dk_getc:
        lda     eof
        bne     @end
        jsr     CHRIN
        sta     ch
        jsr     READST
        sta     st
        and     #$BF
        bne     @last
        lda     st
        beq     @ok
        sta     eof             ; $40: this was the last byte
@ok:    lda     ch
        ldx     #0
        rts
@last:  sta     eof
@end:   lda     #$FF
        tax
        rts

; uint8_t dk_close(void): close file 2; the drive status, or DK_READ if
; it is fine but dk_getc stopped on a READST error bit; all closed
_dk_close:
        jsr     CLRCHN
        lda     #LF_DATA
        jsr     CLOSE
        jsr     status
        cmp     #0
        bne     @st             ; the drive's error first
        lda     eof             ; a KERNAL read error (not just EOF $40)
        and     #$BF
        beq     @st
        lda     #DK_READ
@st:    pha
        jsr     close_all
        pla
        ldx     #0
        rts

; uint16_t __fastcall__ dk_text(char *dst)
; Read the rest of file 2 as text: CR, LF and CR LF become one '\n', a tab
; a space, other bytes outside $20-$7E are dropped. Stores at dst unless
; dst is 0 (count only); stops after dk_lim text bytes. Returns the count.
_dk_text:
        sta     ptr2
        stx     ptr2+1
        ora     ptr2+1
        sta     store
        lda     #0
        sta     prevcr
        sta     count
        sta     count+1
@loop:  lda     count           ; count >= dk_lim: stop
        cmp     _dk_lim
        lda     count+1
        sbc     _dk_lim+1
        bcs     @done
@next:  jsr     _dk_getc
        cpx     #$FF
        beq     @done
        cmp     #NL
        bne     @notlf
        ldx     prevcr
        beq     @nl             ; a lone LF
        ldx     #0              ; the LF of CR LF: dropped
        stx     prevcr
        beq     @next
@notlf: ldx     #0
        stx     prevcr
        cmp     #CR
        bne     @notcr
        inc     prevcr
@nl:    lda     #NL
        bne     @keep
@notcr: cmp     #9              ; tab
        bne     :+
        lda     #' '
:       cmp     #$20
        bcc     @next
        cmp     #$7F
        bcs     @next
@keep:  ldy     store
        beq     @cnt
        ldy     #0
        sta     (ptr2),y
        inc     ptr2
        bne     @cnt
        inc     ptr2+1
@cnt:   inc     count
        bne     @loop
        inc     count+1
        bne     @loop
@done:  lda     count
        ldx     count+1
        rts

; void __fastcall__ dk_write(const char *s): s to the output channel
; (file 2 after dk_open of NAME,S,W)
_dk_write:
        jsr     put
        jmp     CLRCHN
put:    sta     ptr1
        stx     ptr1+1
@l:     ldy     #0
        lda     (ptr1),y
        beq     @e
        jsr     CHROUT
        inc     ptr1
        bne     @l
        inc     ptr1+1
        bne     @l
@e:     rts

; uint8_t __fastcall__ dk_cmd(const char *s): send the DOS command s
; (S0:NAME) on channel 15; the drive status. Leaves 15 open (the next
; operation closes it).
_dk_cmd:
        pha
        txa
        pha
        jsr     begin
        ldx     #LF_CMD
        jsr     CHKOUT
        bcs     @nodev
        pla
        tax
        pla
        jsr     put
        lda     #CR
        jsr     CHROUT
        jsr     CLRCHN
        jmp     status
@nodev: pla
        pla
        jmp     nodev

; uint8_t __fastcall__ disk_name_ok(const char *s): nonzero if s has
; 1..12 characters, all of A-Z 0-9 - _ .
_disk_name_ok:
        sta     ptr1
        stx     ptr1+1
        ldy     #0
@l:     lda     (ptr1),y
        beq     @end
        cmp     #'-'
        beq     @ok
        cmp     #'_'
        beq     @ok
        cmp     #'.'
        beq     @ok
        cmp     #'0'
        bcc     @bad
        cmp     #'9'+1
        bcc     @ok
        cmp     #'A'
        bcc     @bad
        cmp     #'Z'+1
        bcs     @bad
@ok:    iny
        cpy     #13
        bne     @l
@bad:   lda     #0
        tax
        rts
@end:   tya                     ; 0 for "", else the length
        ldx     #0
        rts
