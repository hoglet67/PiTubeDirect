bytevec        = &020A
wordvec        = &020C
wrcvec         = &020E
rdcvec         = &0210

pblock         = &F0
bufptr         = &E8

oswrch         = &FFEE
osrdch         = &FFE0
osnewl         = &FFE7
osword         = &FFF1
osbyte         = &FFF4

page           = &09

char_cursor_x  = &FEF1
char_cursor_y  = &FEF2
char_at_cursor = &FEF3
vdu_var_addr   = &FEF4
vdu_var_data   = &FEF4

cpu 1

osword6_param   = &80
osword6_addr_lo = osword6_param + 0
osword6_addr_hi = osword6_param + 1
osword6_data    = osword6_param + 4

org &0280
guard &300

.start

.init

;; Copy minimal OSWRCH to host at 0900
    LDA #page
    STA osword6_addr_hi
    LDY #host_oswrch_end - host_oswrch_start - 1
.oswloop1
    STY osword6_addr_lo
    LDA host_oswrch_start,Y
    PHY
    JSR osword6
    PLY
    DEY
    BPL oswloop1
    INY

;; *FX 4,1 to disable cursor editing
    LDA #4
    LDX #1
    JSR osbyte

;; Revector Parasite OSWRCH
    LDA #LO(parasite_oswrch)
    STA wrcvec
    LDA #HI(parasite_oswrch)
    STA wrcvec+1

;; Revector Parasite OSBYTE to intercept OSBYTE0
    LDA bytevec
    STA oldosbyte
    LDA bytevec+1
    STA oldosbyte+1
    LDA #LO(newosbyte)
    STA bytevec
    LDA #HI(newosbyte)
    STA bytevec+1

;; Revector Parasite OSWORD to intercept OSWORD0
    LDA wordvec
    STA oldosword
    LDA wordvec+1
    STA oldosword+1
    LDA #LO(newosword)
    STA wordvec
    LDA #HI(newosword)
    STA wordvec+1

;; Revector Host OSWRCH to &0900
    LDA #HI(wrcvec)
    STA osword6_addr_hi
    LDA #LO(wrcvec)
    STA osword6_addr_lo
    LDA #&00
    JSR osword6
    LDA #page
    JSR osword6

;; force the beeb text window left/right window limits to 0/79
;; to work around the ADFS formatting bug (#130)

    LDA #&03
    STA osword6_addr_hi
    LDA #&08
    STA osword6_addr_lo
    LDA #0      ;; 0308 (window left) = 0
    JSR osword6
    INC osword6_addr_lo
    LDA #79     ;; 030a (window right) = 79
    BNE osword6

.pcopy
    EQUB &00
    EQUB &00
    EQUB &00
    EQUB &00
    EQUB &00

;; Entry point is at a nice address (&300)
;;
;; Only init code is before this, so it matters less if it gets
;; trashed once it has run.

org &300
clear &300, &3FF

    JMP init

.osword6
    STA osword6_data
    LDA #6
    LDX #LO(osword6_param)
    LDY #HI(osword6_param)
    JSR osword
    INC osword6_addr_lo
    RTS

.newosbyte
    CMP #&86
    BEQ osbyte86
    CMP #&87
    BEQ osbyte87
    CMP #&A0
    BEQ osbyteA0

    EQUB &4C ; JMP

.oldosbyte
    NOP
    NOP

.osbyte86
    LDX char_cursor_x
    LDY char_cursor_y
    RTS

.osbyte87
    LDX #&55-1
    JSR osbyteA0
    LDX char_at_cursor
    RTS

.osbyteA0
    INX
    STX vdu_var_addr
    LDY vdu_var_data
    DEX
    STX vdu_var_addr
    LDX vdu_var_data
    RTS

;; Host-side OSWRCH Redirector
;;
;; This redirects OSWRCH calls on the host back over to the Pi VDU driver
;;
;; It's used for the output of MOS command (like *CAT, *HELP, etc)

.host_oswrch_start
    ;; Stack the character to be printed
    PHA
    ;; Emulate the POS VDU variable at &0318
    ;; This is used directly by *HELP MOS for padding and also by *CAT in ADFS
    CMP #12         ; Clear screen returns cursor to POS=0
    BEQ zero_pos
    CMP #13         ; So does carriage return
    BEQ zero_pos
    CMP #30         ; So does home cursor
    BEQ zero_pos
    ;; There are other cases we ignore for now (VDU 8; VDU 9; VDU 31,x,y; VDU 127)
    ;; as these are very unlikely to be emitted by the POS commands that read back POS
    CMP #32         ; Set C=1 if printable char (i.e. space or greater)
    BCC write_fifo
    INC &0318       ; printable char, so increment Xcursor (which is absolute)
    LDA &030A       ; load window right
    CMP &0318       ; compare with Xcussor
    BCS write_fifo  ; Still within window? (i.e. window right >= Xcursor)
.zero_pos
    ;; Zero pos (put X cursor in left most column)
    LDA &0308       ; load window left
    STA &0318       ; store in Xcursor
.write_fifo
    ;; Select the VDU FIFO at &FEE4
    LDA #&03
    STA &FEE2
    ;; Restore the character to be printed
    PLA
    ;; Write character to the VDU FIFO
    STA &FEE4
    RTS
.host_oswrch_end

.parasite_oswrch
    STA &FEF8
    RTS

.newosword
    CMP #0
    BEQ osword0

    EQUB &4C ; JMP

.oldosword
    NOP
    NOP

;; X/Y Param Block points to
;;
;; Buffer LSB
;; Buffer MSB
;; Max Line Length
;; Min ASCII
;; Max ASCII
;;
;; On Exit:
;; C = 0 if CR terminated input
;; C = 1 if ESC terminated input
;; Y contains line length, including CR if used

.osword0

    STX pblock
    STY pblock + 1

    LDY #&04       ; Y=4
.ploop
    LDA (pblock),Y ; transfer bytes 4,3,2 to 2B3-2B5
    STA pcopy,Y    ;
    DEY            ; decrement Y
    CPY #&02       ; until Y=1
    BCS ploop

    LDA (pblock),Y ; get address of input buffer
    STA bufptr+1   ; store it in &E9 as temporary buffer
    DEY            ; decrement Y
    LDA (pblock),Y ; get lo byte of address
    STA bufptr     ; and store in &E8
    BCC cloop2     ; Jump to cloop

.bell
    LDA #&07       ; A=7
.y0
    DEY            ; decrement Y

.y1
    INY            ; increment Y

.cloop1
    JSR oswrch     ; and call oswrch

.cloop2
    JSR osrdch     ; else read character  from input stream
    BCS exit_err   ; if carry set then illegal character or other error and exit
    CMP #&7F       ; if character is not delete
    BNE not_del    ; process it
    CPY #&00       ; else is Y=0
    BEQ cloop2     ; and goto cloop2
    DEY            ; decrement Y
    BCS cloop1     ; and if carry set cloop1 to output it

.not_del
    CMP #&15       ; is it delete line &21
    BNE not_eol    ; if not store
    TYA            ; else Y=A, if its 0 we are still reading first character
    BEQ cloop2     ; so cloop2
    LDA #&7F       ; else output DELETES

.dloop
    JSR oswrch     ; until Y=0
    DEY            ;
    BNE dloop      ;
    BEQ cloop2     ; then read character again

.not_eol
    CMP #&87
    BEQ copy
    BCC store
    CMP #&8C
    BCS store

;; Handle cursor editing by sending &88-&8B to VDU
    PHA
    LDA #&1B
    JSR oswrch     ; edit controls prefixed by escape
    PLA
    BNE cloop1

;; Handle Copy Key (&87)
.copy
    LDA char_at_cursor ; read character at edit cursor from frame buffer
    BEQ cloop2     ; invalid char, loop back
    PHA
    LDA #&1B
    JSR oswrch     ; edit controls prefixed by escape
    LDA #&89
    JSR oswrch     ; move the eding cursor to the right
    PLA

.store
    STA (bufptr),Y ; store character in designated buffer
    CMP #&0D       ; is it CR?
    BEQ exit_ok    ; if so E96C
    CPY pcopy+2    ; else check the line length
    BCS bell       ; if = or greater loop to ring bell
    CMP pcopy+3    ; check minimum character
    BCC y0         ; if less than minimum backspace
    CMP pcopy+4    ; check maximum character
    BEQ y1         ; if equal y1
    BCC y1         ; or less y1
    BCS y0         ; then y0

.exit_ok
    JSR osnewl     ; output CR/LF

.exit_err
    LDA &FF        ; A=ESCAPE FLAG
    ROL A          ; put bit 7 into carry
    LDA #&00       ; Preserve A
    RTS            ; and exit routine

.end

SAVE "osword", start, end
