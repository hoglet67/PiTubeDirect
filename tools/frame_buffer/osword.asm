wordvec        = &020C
wrcvec         = &020E
rdcvec         = &0210

pblock         = &F0
bufptr         = &E8
pcopy          = &2B1

oswrch         = &FFEE
osrdch         = &FFE0
osnewl         = &FFE7
osword         = &FFF1
osbyte         = &FFF4

page           = &09

char_at_cursor = &FEF3

cpu 1

org &F700

.start

.init

;; Copy minimal OSWRCH to host at 0900
;; 0900 STA &FEE2
;; 0903 RTS
    LDA #page
    STA osword6_addr_hi
    LDY #host_oswrch_end - host_oswrch_start - 1
.oswloop1
    STY osword6_addr_lo
    LDA host_oswrch_start,Y
    JSR osword6
    DEY
    BPL oswloop1

;; Revector Host OSWRCH to &0900
    LDA #HI(wrcvec)
    STA osword6_addr_hi
    LDA #LO(wrcvec)
    STA osword6_addr_lo
    LDA #&00
    JSR osword6
    LDA #page
    JSR osword6

;; Revector Parasite OSWORD to intercept OSWORD0
    LDA wordvec
    STA oldosword
    LDA wordvec+1
    STA oldosword+1
    LDA #LO(newosword)
    STA wordvec
    LDA #HI(newosword)
    STA wordvec+1

;; *FX 4,1 to disable cursor editing
    LDA #4
    LDX #1
    LDY #0
    JMP osbyte

.osword6
    STA osword6_data
    PHX
    PHY
    LDA #6
    LDX #LO(osword6_param)
    LDY #HI(osword6_param)
    JSR osword
    INC osword6_addr_lo
    PLY
    PLX
    RTS

.host_oswrch_start
    STA &FEE2
    RTS
.host_oswrch_end

.oldosword
    NOP
    NOP

.newosword
    CMP #0
    BEQ osword0
    JMP (oldosword)

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
    BCC cloop1

;; Handle Copy Key (&87)
.copy
    LDA char_at_cursor ; read character at edit cursor from frame buffer
    BEQ cloop2     ; invalid char, loop back
    PHA
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

.osword6_param
.osword6_addr_lo
   EQUB &00
.osword6_addr_hi
   EQUB &00
   EQUB &FF        ; ignored
   EQUB &FF        ; ignored
.osword6_data
   EQUB 00

.end

SAVE "osword", start, end
