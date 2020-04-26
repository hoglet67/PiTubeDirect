.equ        WORK 0x0600      ; system jump blocks (512 locations) below ROM
.equ        CODE 0x0800      ; This is the location of main F100 ROM
.equ        TUBE 0x7EF8      ; Hardware mapped in here - tube registers
.equ         MOS 0x7FC8      ; This is a small additional ROM at top of memory
.equ         APP 0x1000      ; Application RAM starts here
.equ  END_MARKER 0x8000      ; makes the end of each command -ve

.equ    r1status TUBE
.equ      r1data TUBE + 1
.equ    r2status TUBE + 2
.equ      r2data TUBE + 3
.equ    r3status TUBE + 4
.equ      r3data TUBE + 5
.equ    r4status TUBE + 6
.equ      r4data TUBE + 7

.equ    CARRY           0x04    ; carry is bit 4 of the CR
.equ    MULTI           0x05    ; multi-word flag is bit 5 of CR
.equ    ZERO            0x01    ; zero bit in CR
.equ    SIGN            0x03    ; sign bit in CR
.equ    LSP             0       ; link stack pointer location
.equ    USP             1       ; user stack pointer location
.equ    LINK_STACK      0x7C01  ; initial value for link stack (must be ODD)
.equ    USER_STACK      0x7800  ; initial value for user stack
.equ    R0              2
.equ    R1              R0+1
.equ    R2              R0+2
.equ    R3              R0+3
.equ    R4              R0+4
.equ    R5              R0+5
.equ    R6              R0+6
.equ    R7              R0+7
.equ    R8              R0+8
.equ    R9              R0+9
.equ    R10             R0+10
.equ    R11             R0+11
.equ    R12             R0+12
.equ    R13             R0+13
.equ    R14             R0+14
.equ    R15             R0+15
.equ    R16             R0+16
.equ    R17             R0+17
.equ    R18             R0+18
.equ    R19             R0+19


.equ    MEM_BOT         APP
.equ    MEM_TOP         0x77FF


; -----------------------------------------------------------------------------
; Memory from 0x0000 to 0x00FF is reserved for system use
; -----------------------------------------------------------------------------
.equ      NUM_VECTORS     27            ; number of vectors in DefaultVectors table

.equ       USERV  WORK + 0x0010 + 0x0010
.equ        BRKV  WORK + 0x0011 + 0x0011
.equ       IRQ1V  WORK + 0x0012 + 0x0012
.equ       IRQ2V  WORK + 0x0013 + 0x0013
.equ        CLIV  WORK + 0x0014 + 0x0014
.equ       BYTEV  WORK + 0x0015 + 0x0015
.equ       WORDV  WORK + 0x0016 + 0x0016
.equ       WRCHV  WORK + 0x0017 + 0x0017
.equ       RDCHV  WORK + 0x0018 + 0x0018
.equ       FILEV  WORK + 0x0019 + 0x0019
.equ       ARGSV  WORK + 0x001A + 0x001A
.equ       BGETV  WORK + 0x001B + 0x001B
.equ       BPUTV  WORK + 0x001C + 0x001C
.equ       GBPBV  WORK + 0x001D + 0x001D
.equ       FINDV  WORK + 0x001E + 0x001E
.equ        FSCV  WORK + 0x001F + 0x001F
.equ       EVNTV  WORK + 0x0020 + 0x0020
.equ        UPTV  WORK + 0x0021 + 0x0021       ; not implemented
.equ        NETV  WORK + 0x0022 + 0x0022       ; not implemented
.equ        VDUV  WORK + 0x0023 + 0x0023       ; not implemented
.equ        KEYV  WORK + 0x0024 + 0x0024       ; not implemented
.equ        INSV  WORK + 0x0025 + 0x0025       ; not implemented
.equ        REMV  WORK + 0x0026 + 0x0026       ; not implemented
.equ        CNPV  WORK + 0x0027 + 0x0027       ; not implemented
.equ       IND1V  WORK + 0x0028 + 0x0028       ; not implemented
.equ       IND2V  WORK + 0x0029 + 0x0029       ; not implemented
.equ       IND3V  WORK + 0x002A + 0x002A       ; not implemented

.equ      ERRBUF WORK + 0x0030 + 0x0030
.equ      INPBUF WORK + 0x0030 + 0x0030
.equ      INPEND WORK + 0x00F6 + 0x00F6

.equ        ADDR WORK + 0x00F6 + 0x00F6 ; tube execution address
.equ    LAST_ERR WORK + 0x00FD + 0x00FD ; last error
.equ ESCAPE_FLAG WORK + 0x00FF + 0x00FF ; escape flag

; -----------------------------------------------------------------------------
; 2K Rom Start
; -----------------------------------------------------------------------------

.org CODE
; ;;include "lib_printstring.s"
; ;;include "lib_printhex.s"
; ;;include "lib_printdec.s"
; ;;include "lib_readhex.s"
; ;;include "lib_srec.s"
; ;;include "lib_dumpmem.s"
; ;;include "lib_disassemble.s"

ResetHandler:
        LDA ,LINK_STACK         ; initialize link stack pointer
        STO .LSP
        LDA ,USER_STACK         ; initialize user stack pointer
        STO .USP


.equ    START   0-(2*NUM_VECTORS-1) ; need to write JMP <vector> to each addr = 2 words
        LDA     ,START
        STO     R0
        LDA     ,DefaultVectors-1
        STO     R1
        LDA     ,USERV-1
        STO     R2
InitVecLoop:
        LDA     /R1+
        STO     /R2+
        ICZ     R0 InitVecLoop

        LDA     ,BannerMessage
        CAL     .print_string
        ; CAL     .PI
        ; CAL     .newline
        LDA     ,0            ; send the terminator
        CAL     .OSWRCH

        CAL     .waitByteR2

CmdPrompt:
CmdOsLoop:
        LDA     ,0x2A
        CAL     .OSWRCH
        LDA     ,0
        STO     R1
        LDA     ,osword0_param_block
        STO     R2
        CAL     .OSWORD
        JBS     CARRY CR CmdOSEscape
        LDA     ,INPBUF
        STO     R1
        CAL     .OS_CLI
        JMP     .CmdOsLoop

CmdOsEscape:
        LDA     ,0x7E
        STO     R1
        CAL     .OSBYTE
        ;; Handle Escape Error by just printing a message
        CAL     .newline
        LDA     ,EscapeError+1
        CAL     .print_string
        CAL     .newline
        JMP     .CmdOsLoop


Idle:
        HALT   ,123

; --------------------------------------------------------------
; MOS interface
; --------------------------------------------------------------

NullReturn:
    RTN

; --------------------------------------------------------------

Unsupported:
    RTN

; --------------------------------------------------------------

ErrorHandler:

    LDA     ,LINK_STACK                 ; Clear the stack
    STO     .LSP
    CAL     .newline
    LDA     ,LAST_ERR
    STO     R0
    LDA     /R0                         ; Address of the last error: <error num> <err string> <00>
    ADD     ,1                          ; Skip over error num
    CAL     .print_string               ; Print error string
    CAL     .newline
    JMP     .CmdPrompt                  ; Jump to command prompt

; --------------------------------------------------------------
mosARGS:
    ; TODO
    RTN

; --------------------------------------------------------------

mosBGET:
    ; TODO
    RTN

; --------------------------------------------------------------

mosBPUT:
    ; TODO
    RTN

; --------------------------------------------------------------
; OSBYTE - Byte MOS functions
;
; On entry, r1, r2, r3=OSBYTE parameters
; On exit,  r1  preserved
;           If r1<$80, r2=returned value
;           If r1>$7F, r2, r3, r0 =Carry byte =returned values

mosBYTE:
        LDA     R1
        AND     ,0x80
        JBC     ZERO CR ByteHigh
;
; Tube data  $04 X A    --  X
;
        LDA     R1
        STO     /USP+
        LDA     ,0x04           ; Send command &04 - OSBYTELO
        CAL     .sendByteR2
        LDA     R2
        CAL     .sendByteR2     ; Send single parameter
        LDA     /USP-
        STO     R1
        LDA     /USP+
        CAL     .sendByteR2      ; Send function
        CAL     .waitByteR2     ; Get return value
        STO     R2
        LDA     /USP-
        STO     R1
        RTN

ByteHigh:
        LDA     ,0
        STO     R0
        LDA     R1
        CMP     ,0x82
        JBS     ZERO CR Byte82 ; Read memory high word
        CMP     ,0x83
        JBS     ZERO CR Byte83 ; Read bottom of memory
        CMP     ,0x84
        JBS     ZERO CR Byte84 ; Read top of memory
;
; Tube data  $06 X Y A  --  Cy Y X
;
        LDA     ,0x06
        CAL     .sendByteR2     ; Send command &06 - OSBYTEHI
        LDA     R2
        CAL     .sendByteR2     ; Send parameter 1
        LDA     R3
        CAL     .sendByteR2     ; Send parameter 2
        LDA     R1
        CAL     .sendByteR2     ; send function
;   cmp     r1, r0, 0x8e        ; If select language, check to enter code
;   z.mov   pc, r0, CheckAck
        LDA     R1
        CMP     ,0x9D           ; Fast return with Fast BPUT
        JBS     ZERO CR FastReturn
        CAL     .waitByteR2     ; Get carry byte
        STO     /USP+           ; save it on the user stack
        CAL     .waitByteR2     ; Get high byte
        STO     R3
        CAL     .waitByteR2     ; Get low byte
        STO     R2
        LDA     /USP-
        STO     R0
FastReturn:
        LDA     /USP-           ; restore original r1
        STO     R1
        RTN
Byte84:                         ; Read top of memory
        LDA     ,MEM_TOP
        STO     R1
        RTN
Byte83:                         ; Read bottom of memory
        LDA     ,MEM_BOT
        STO     R1
        RTN
Byte82:                         ; Return &0000 as memory high word
        LDA     ,0x0
        STO     R1
        RTN

; --------------------------------------------------------------
; OSCLI - Send command line to host
;
; Entry
; r1 = command string pointer
;
; Tube data  &02 string &0D  --  &7F or &80
;
mosCLI:
        LDA    R2
        STO    /USP+
        LDA    R1                       ; Save the string pointer
        STO    /USP+
        CAL    .cmdLocal                ; try to handle the command locally (return C=1 if done)
        LDA    R0                       ; sets zero flag
        JBS    ZERO CR dontEnterCode    ; Yes nothing more to do
        LDA    ,0x2                     ; send command &02  - OSCLI
        CAL    .sendByteR2
        LDA    /USP-                    ; restore string pointer into Acc
        CAL    .sendStringR2            ; send string pointed to by r2
mosCLI_Ack:
        CAL    .waitByteR2
        AND    ,0x80
        JBS    ZERO CR dontEnterCode
        LDA    /USP-
        STO    R1
        STO    /USP+
        CAL    .prep_env
        CAL    .enterCode
dontEnterCode:
        LDA    /USP-
        STO    R1
        LDA    /USP-
        STO    R2
        RTN

enterCode:
        JMP     .ADDR

; Find the start of the command string
;
; Lots of ways a file can be run:
; *    filename params
; * /  filename params
; *R.  filename params
; *RU. filename params
; *RUN filename params
;
;
; In general you want:
; - skip leading space or * characters
; - skip any form of *RUN, followed by trailing spaces
; - leave the environment point at the first character of filename


prep_env:
        LDA     ,1
        SBS     R1

prep_env_1:                         ; skip leading space or * characters
        LDA     ,1
        ADS     R1
        CAL     .skip_spaces
        STO     R2
        CMP     ,0x2A
        JBS     ZERO CR prep_env_1
        CMP     ,0x2F
        JBS     ZERO CR prep_env_4
        LDA     ,run_string-1
        STO     R2
        LDA     ,1
        SUB     R1
        STO     R3
prep_env_2:                             ; skip a possibly abbreviated RUN
        LDA     ,1
        ADS     R2
        LDA     /R2
        STO     R4                      ; Read R U N <0>
        JBS     ZERO CR prep_env_3
        LDA     ,1
        ADS     R3
        LDA     /R3
        STO     R5
        CMP     ,0x2E
        JBS     ZERO CR prep_env_3
        LDA     ,0xDF                   ; force upper case
        AND     R5
        STO     R5
        CMP     R4                      ; match against R U N
        JBC     ZERO CR  prep_env_5
        JMP     .prep_env_2      ; loop back for more characters

prep_env_3:
        LDA     R3
        STO     R1

prep_env_4:
        LDA     ,1
        ADS     R1

prep_env_5:
        CAL     .skip_spaces
        STO     R2
        RTN

run_string:
        .WORD   0x52,0x55,0x4E,0x00 ; STRING  "RUN\0"

; --------------------------------------------------------------
; Local Command Processor
;
; On Entry:
; - r1 points to the user command
;
; On Exit:
; - R0 == 0 if command successfully processed locally
; - R0 != 0 if command should be handled by host
;
; Register usage:
; r1 points to start of user command
; r2 points within command table
; r3 points within user command
; r4 is current character in command table
; r5 is current character in user command

cmdLocal:
;; -- CODEME --    PUSH    (r2)
;; -- CODEME --    PUSH    (r3)
;; -- CODEME --    PUSH    (r4)
;; -- CODEME --    PUSH    (r5)
;; -- CODEME --    PUSH    (r13)
;; -- CODEME --
;; -- CODEME --    sub     r1, r0, 1
;; -- CODEME --cmdLoop0:
;; -- CODEME --    add     r1, r0, 1
;; -- CODEME --    JSR     (skip_spaces)           ; skip leading spaces
;; -- CODEME --    cmp     r2, r0, ord('*')        ; also skip leading *
;; -- CODEME --    z.mov   pc, r0, cmdLoop0
;; -- CODEME --
;; -- CODEME --    mov     r2, r0, cmdTable-1      ; initialize command table pointer (to char before)
;; -- CODEME --
;; -- CODEME --cmdLoop1:
;; -- CODEME --    mov     r3, r1, -1              ; initialize user command pointer (to char before)
;; -- CODEME --
;; -- CODEME --cmdLoop2:
;; -- CODEME --    add     r2, r0, 1               ; increment command table pointer
;; -- CODEME --    add     r3, r0, 1               ; increment user command pointer
;; -- CODEME --    ld      r4, r2                  ; read next character from command table
;; -- CODEME --    mi.mov  pc, r0, cmdExec         ; if an address, then we are done
;; -- CODEME --    ld      r5, r3                  ; read next character from user command
;; -- CODEME --    or      r5, r0, 0x20            ; convert to lower case
;; -- CODEME --    cmp     r5, r4                  ; compare the characters
;; -- CODEME --    z.mov   pc, r0, cmdLoop2        ; if a match, loop back for more
;; -- CODEME --
;; -- CODEME --    sub     r2, r0, 1
;; -- CODEME --cmdLoop3:                           ; skip to the end of the command in the table
;; -- CODEME --    add     r2, r0, 1
;; -- CODEME --    ld      r4, r2
;; -- CODEME --    pl.mov  pc, r0, cmdLoop3
;; -- CODEME --
;; -- CODEME --    cmp     r5, r0, 0x2e            ; was the mis-match a '.'
;; -- CODEME --    nz.mov  pc, r0, cmdLoop1        ; no, then start again with next command
;; -- CODEME --
;; -- CODEME --cmdExec:
;; -- CODEME --
;; -- CODEME --    mov     r1, r3                  ; r1 = the command pointer to the params
;; -- CODEME --    mov     r2, r4                  ; r2 = the execution address
;; -- CODEME --
;; -- CODEME --    JSR     (cmdExecR2)
;; -- CODEME --
;; -- CODEME --    POP     (r13)
;; -- CODEME --    POP     (r5)
;; -- CODEME --    POP     (r4)
;; -- CODEME --    POP     (r3)
;; -- CODEME --    POP     (r2)
;; -- CODEME --

        LDA     ,0xFF           ; non-zero value in R0 to show command wasn't handled
        STO     R0
        RTN





; --------------------------------------------------------------
mosFILE:
    ; TODO
    RTN

; --------------------------------------------------------------

mosFIND:
    ; TODO
    RTN

; --------------------------------------------------------------

mosGBPB:
    ; TODO
    RTN


; --------------------------------------------------------------
; mosWORD
;
; Entry:
; - R0 osword number
; - R1 points to parameter block in memory
; Exit:
; - R0 trashed

mosWORD:
        LDA     r1
        JBS     ZERO CR RDLINE
        ;;  save calling params + 2 scratch registers on the stack
        STO     /USP+
        LDA     R2
        STO     /USP+
        LDA     R3
        STO     /USP+
        LDA     R4
        STO     /USP+

        LDA     R1
        STO     R3
        LDA     R2
        STO     R4
        LDA     ,0x8
        CAL     .sendByteR2   ; Send command &08 - OSWORD
        LDA     R3
        CAL     .sendByteR2   ; Send OSWORD number

        ; compute index into length table
        LDA       R3
        CMP       ,0x15
        JBC       CARRY CR mosWORDL1
        LDA       ,0            ; >= OSWORD 0x15, use slot 0
        STO       R3
mosWORDL1:
        LDA     R3
        ADD     ,word_in_len
        STO     R0
        LDA     /R0
        CAL     .sendByteR2     ; Send request block length
        LDA     R3
        ADD     ,word_in_len
        STO     R0
        LDA     /R0
        STO     R1
        LDA     R4
        STO     R2
        CAL     .sendBlockR2     ; Send request block
        LDA     R3
        ADD     ,word_out_len
        STO     R0
        LDA     /R0
        CAL     .sendByteR2      ; Send response block length
        LDA     R3
        ADD     ,word_out_len
        STO     R0
        LDA     /R0
        STO     R1
        LDA     R4
        STO     R2
        CAL     .receiveBlockR2  ; Receive response block
        LDA     /USP-
        STO     R4
        LDA     /USP-
        STO     R3
        LDA     /USP-
        STO     R2
        LDA     /USP-
        STO     R1
        RTN

; --------------------------------------------------------------
; skip_space
;
; Entry:
; - r1 is the address of the string
;
; Exit:
; - r1 is updated to skip and spaces
; - ACC is non-space character
; - all other registers preserved

skip_spaces:
        LDA     ,1
        SBS     R1
skip_spaces_loop:
        LDA     /R1+
        CMP     ,0x20
        JBS     ZERO CR skip_spaces_loop
        RTN


; --------------------------------------------------------------
; print_string
;
; Prints the zero terminated ASCII string
;
; Entry:
; - ACC points to the location of the zero terminated string
;
; Exit:
; - r0,r1 trashed

print_string:
        STO     R0
        LDA     ,1
        SUB     R0 ; Need addr-1 in R1 because we will use pre-incr pointer to address the string
        STO     R1
print_string_loop:
        LDA     /R1+
        AND     ,0xFF
        JBS     ZERO CR print_string_exit
        CAL     .OSWRCH
        JMP     .print_string_loop
print_string_exit:
        RTN

; --------------------------------------------------------------
; NEWLINE
;
; emit a newline LF+CR pair
;
; Entry:
; - None
;
; Exit:
; - R0 trashed
newline:
        LDA     ,0x0A
        CAL     .OSWRCH
        LDA     ,0x0D
        CAL     .OSWRCH
        RTN

; --------------------------------------------------------------
; sendBlockR2 - Send a defined size block to tube FIFO R2
;
; NB. The complexity here is the block needs to be send backwards!
;
; Entry:
; - R1 = length
; - R2 = block address
; Exit:
; - R0-2 trashed


sendBlockR2:
        LDA     R3
        STO     /USP+
        LDA     R4
        STO     /USP+
        LDA     ,-1
        ADD     R1
        STO     R4      ;  r4 = block length - 1
        JBS     SIGN CR sendBlockDone

        SRL     1 R4      ; calculate address of word containing last byte
        ADD     R2
        STO     R2
        LDA     /R2     ; load the first word from memory
        STO     R3
        LDA     R4
        AND     ,1      ; work out the byte alignment of the last byte in the block (first byte to send)
        STO     R1
        JBS     CARRY CR sendBlockB0
sendBlockB1:
        ; byte swap R3 into R1
        SLE     8  R3
        CAL     .sendByteR2     ; send byte 1
        LDA     ,1              ; Decrement R4
        SBS     R4
        JBS     SIGN CR sendBlockDone
sendBlockB0:
        CAL     .sendByteR2     ; send byte 0
        LDA     ,1
        SBS     R4
        JBS     SIGN CR sendBlockDone
        LDA     ,1
        SBS     R2
        LDA     /R2
        STO     R3               ; load the next word from memory
        JMP     .sendBlockB1
sendBlockDone:
        LDA     /USP-
        STO     R4
        LDA     /USP-
        STO     R3
        RTN

; --------------------------------------------------------------
; receiveBlockR2 - Receive a defined size block from tube FIFO R2
;
; Entry:
; - R1 = length
; - R2 = block address
; Exit:
; - R0-2 trashed

receiveBlockR2:
        LDA     R3
        STO     /USP+
        LDA     R4
        STO     /USP+
        LDA     ,1
        SBS     R4      ; r4 = block length - 1
        JBS     SIGN CR receiveBlockDone
        SRL     1 R4 ; calculate address of word containing last byte
        ADS     R2
        LDA     ,0
        STO     R3      ;  clear the receive word
        LDA     ,1
        AND     R4      ; work out the byte alignment of the last byte in the block (first byte to send)
        JBS     ZERO CR receiveBlockB0
receiveBlockB1:
        ; JBS     SIGN CR receiveBlockWrite
        CAL     .waitByteR2     ; receive byte 1 in Acc
        SLE     8 A             ; byte swap
        AND     ,0xFF00         ; mask off top byte
        ADS     R3              ; merge it into R3
        LDA     ,1              ; decrement counter
        SBS     R4
receiveBlockB0:
        JBS     SIGN CR receiveBlockWrite
        CAL     .waitByteR2     ; receive byte 0 in Acc
        AND     ,0x0FF          ; mask off bottom byte
        ADS     R3              ; merge into R3
receiveBlockWrite:
        LDA     R3
        STO     /R2-            ; write word and post decrement R2
        LDA     ,0              ; clear the receive work for next bytes
        STO     R3
        LDA     ,1
        SBS     R4
        JBC     SIGN CR receiveBlockB1
receiveBlockDone:
        LDA     /USP-
        STO     R4
        LDA     /USP-
        STO     R3
        RTN

word_in_len:
        .WORD 16   ; OSWORD default
        .WORD 0    ;  1  =TIME
        .WORD 5    ;  2  TIME=
        .WORD 0    ;  3  =IntTimer
        .WORD 5    ;  4  IntTimer=
        .WORD 4    ;  5  =IOMEM   JGH: must send full 4-byte address
        .WORD 5    ;  6  IOMEM=
        .WORD 8    ;  7  SOUND
        .WORD 14   ;  8  ENVELOPE
        .WORD 4    ;  9  =POINT()
        .WORD 1    ; 10  =CHR$()
        .WORD 1    ; 11  =Palette
        .WORD 5    ; 12  Pallette=
        .WORD 0    ; 13  =Coords
        .WORD 8    ; 14  =RTC
        .WORD 25   ; 15  RTC=
        .WORD 16   ; 16  NetTx
        .WORD 13   ; 17  NetRx
        .WORD 0    ; 18  NetArgs
        .WORD 8    ; 19  NetInfo
        .WORD 128  ; 20  NetFSOp

word_out_len:
        .WORD 16   ; OSWORD default
        .WORD 5    ;  1  =TIME
        .WORD 0    ;  2  TIME=
        .WORD 5    ;  3  =IntTimer
        .WORD 0    ;  4  IntTimer=
        .WORD 5    ;  5  =IOMEM
        .WORD 0    ;  6  IOMEM=
        .WORD 0    ;  7  SOUND
        .WORD 0    ;  8  ENVELOPE
        .WORD 5    ;  9  =POINT()
        .WORD 9    ; 10  =CHR$()
        .WORD 5    ; 11  =Palette
        .WORD 0    ; 12  Palette=
        .WORD 8    ; 13  =Coords
        .WORD 25   ; 14  =RTC
        .WORD 1    ; 15  RTC=
        .WORD 13   ; 16  NetTx
        .WORD 13   ; 17  NetRx
        .WORD 128  ; 18  NetArgs
        .WORD 8    ; 19  NetInfo
        .WORD 128  ; 20  NetFSOp


; --------------------------------------------------------------
; RDLINE - Read a line of text
;
; Entry:
; - R1 = 0
; - R2 = control block
; Exit:
; - R1 = 0
; - R2 = control block
; - R3 = length of returned string
; - C=0 ok OR C=1 Escape
;
; Tube data  &0A block  --  &FF or &7F string &0D

RDLINE:
        LDA     R2
        STO     /USP+
        LDA     ,0x0A
        CAL     .sendByteR2  ; Send command &0A - RDLINE
        LDA     R2
        ADD     ,0x03
        STO     R0
        LDA     /R0
        CAL     .sendByteR2  ; Send <char max>
        LDA     R2
        ADD     ,0x02
        STO     R0
        LDA     /R0
        CAL     .sendByteR2  ; Send <char min>
        LDA     R2
        ADD     ,0x01
        STO     R0
        LDA     /R0
        CAL     .sendByteR2  ; Send <buffer len>
        LDA     ,0x07
        CAL     .sendByteR2  ; Send <buffer addr MSB>
        LDA     ,0
        CAL     .sendByteR2   ; Send <buffer addr LSB>
        CAL     .waitByteR2   ; Wait for response &FF [escape] or &7F
        AND     ,0x80         ; test for escape
        JBS     ZERO CR RdLineCont
        LDA     ,0x10        ; set bit 4
        STO     R1           ; R1 will be returned as Carry by RTN
        JMP     .RdLineEscape
RdLineCont:
        LDA     ,0
        STO     R3           ; initialize response length to 0
        LDA     /R2
        STO     R2           ; Load the local input buffer from the control block

RdLineLp:
        CAL     .waitByteR2  ; Receive a response byte
        STO     R0           ; save response for later check
        STO     /R2
        LDA     ,1
        ADS     R2           ; Increment buffer pointer
        LDA     ,1           ; increment count
        ADS     R3
        LDA     R0           ; reload response byte and
        CMP     ,0x0D        ; compare against terminator and loop back
        JBC     ZERO CR RdLineLp
        LDA     ,0           ; reset bit 4
        STO     R1           ; R1 will be returned as Carry by RTN
RdLineEscape:
        LDA     .LSP
        STO     R0
        LDA     R1
        STO     /R0         ; overwrite CR bits on the stack for return
        LDA     /USP-
        STO     R2
        LDA     ,0
        STO     R1           ; Clear r1 to be tidy
        RTN
; -------------------------------------------------------------
; Control block for command prompt input
; --------------------------------------------------------------

osword0_param_block:
        .word INPBUF
        .word INPEND - INPBUF
        .word 0x20
        .word 0xFF

; --------------------------------------------------------------
; mosWRCH (and OSWRDIG)
;
; Prints single character
;
; Entry:
; - Acc holds byte to be written
;
; Exit:
; - R0 trashed
OSWRDIG:                        ; Alternative entry with digit in Acc
        ADD     ,48             ; make ASCII
mosWRCH:
        STO     R0              ; save in R0
mosWRCH1:
        LDA     .r1status
        AND     ,0x40
        JBS     ZERO CR mosWRCH1
        LDA     R0
        STO     .r1data
        RTN

; --------------------------------------------------------------
; mosRDCH
;
; Read a single character
;
; Entry:
; - None
;
; Exit:
; - Acc holds word read

mosRDCH:
        LDA     ,0x00           ; Send command &00 - OSRDCH
        CAL     .sendByteR2
        CAL     .waitByteR2     ; Receive carry
        ; need to convert data into a carry from previous call and place in CR on stack
        CAL     .waitByteR2     ; Receive byte
        RTN

; -----------------------------------------------------------------------------
; Interrupts handlers
; -----------------------------------------------------------------------------

InterruptHandler:
        STO     /USP+
        LDA     R1
        STO     /USP+
        JMP     .IRQ1V

IRQ1Handler:
        LDA     .r4status
        AND     ,0x080
        JBC     ZERO CR r4_irq
        LDA     .r1status
        AND     ,0x80
        JBC     ZERO CR r1_irq
        JMP     .IRQ2V

; -----------------------------------------------------------------------------
; Interrupt generated by data in Tube R1
; -----------------------------------------------------------------------------

r1_irq:
        LDA     .r1data
        STO     R1
        CMP     ,0x80
        JBC     CARRY CR r1_irq_escape
        LDA     R2
        STO     /USP+
        LDA     R3
        STO     /USP+
        CAL     .waitByteR1 ; Get Y parameter from Tube R1
        STO     R3
        CAL     .waitByteR1 ; Get X parameter from Tube R1
        STO     R2
        CAL     .waitByteR1 ; Get event number from Tube R1
        STO     R1
        CAL     .LFD36      ; Dispatch event
        LDA     /USP-       ; restore registers
        STO     R3
        LDA     /USP-
        STO     R2
        LDA     /USP-
        STO     R1
        LDA     /USP-
        RTN

LFD36:
        JMP     .EVNTV

r1_irq_escape:
        LDA     R1
        ADS     R1
        STO     .ESCAPE_FLAG
        LDA     /USP-
        STO     R1
        LDA     /USP-
        RTN

; -----------------------------------------------------------------------------
; Interrupt generated by data in Tube R4
; -----------------------------------------------------------------------------
r4_irq:
        LDA     .r4data
        STO     R1
        AND     ,0x80
        JBS     ZERO CR LFD65 ; b7=0, jump for data transfer
;
; Error    R4: &FF R2: &00 err string &00
;
        LDA     R2
        STO     /USP+
        CAL     .waitByteR2     ; Skip data in Tube R2 - should be 0x00
        LDA     ,ERRBUF
        STO     R2
        CAL     .waitByteR2     ; Get error number
        STO     /R2
        LDA     ,1
        ADS     R2

err_loop:
        CAL     .waitByteR2     ; Get error message bytes
        STO     R1
        STO     /R2
        LDA     ,1
        ADS     R2
        LDA     R1
        JBC     ZERO CR err_loop

        ; FIXME - just jumping to Error handler here
        ;    ERROR   (ERRBUF)
        LDA     ,ERRBUF
        STO     .LAST_ERR
        JMP     .ErrorHandler

;
; Transfer R4: action ID block sync R3: data
;

LFD65:
        LDA     R2          ; working register for transfer type
        STO     /USP+
        LDA     R3          ; working register for transfer address
        STO     /USP+
        LDA     R1
        STO     R2
        CAL     .waitByteR4
        STO     R2
        CMP     ,0x05
        JBS     ZERO CR Release
        CAL     .waitByteR4 ; block address MSB - ignored
        CAL     .waitByteR4 ; block address ... - ignored
        CAL     .waitByteR4 ; block address ...
        SLE     8 A         ; byte swap into upper byte
        AND     ,0xFF00
        STO     R3
        CAL     .waitByteR4 ; block address LSB
        ADS     R3          ; merge new byte into lower byte
        LDA     .r3data
        LDA     .r3data
        CAL     .waitByteR4 ; sync

        LDA     ,TransferHandlerTable
        ADS     R2
        JMP     R2

Release:
        LDA     /USP-
        STO     R3
        LDA     /USP-
        STO     R2
        LDA     /USP-
        STO     R1
        LDA     /USP-
        RTN

TransferHandlerTable:
        .word    Type0
    	.word    Type1
    	.word    Type2
    	.word    Type3
    	.word    Type4
    	.word    Release ; not actually used
    	.word    Type6
    	.word    Type7

; ============================================================
; Type 0 transfer: 1-byte parasite -> host (SAVE)
;
; r1 - scratch register
; r2 - data register (16-bit data value read from memory)
; r3 - address register (16-bit memory address)
; ============================================================

Type0:
        LDA     ,0
        STO     R2              ; clean the odd byte flag (start with an even byte)

Type0_loop:
        LDA     .r4status       ; Test for an pending interrupt signalling end of transfer
        STO     R1
        AND     ,0x80
        JBC     ZERO CR Release

        LDA     .r3status       ; Wait for Tube R3 free
        STO     R1
        AND     ,0x40
        JBS     ZERO CR Type0_loop

        LDA     ,0              ; test odd byte flag
        SUB     R2
        JBS     SIGN CR Type0_odd_byte

        LDA     /R3             ; Read word from memory
        STO     R2
        STO     .r3data         ; Send even byte to Tube R3
        LDA     ,1              ; Increment memory pointer
        ADS     R3
        SLE     8 R2            ; Byte swap
        AND     ,0x00FF         ; mask off low byte
        ADD     ,0x8000         ; set top bit (odd byte flag)
        STO     R2
        JMP     .Type0_loop

Type0_odd_byte:
        STO     .r3data         ; Send odd byte to Tube R3
        JMP     .Type0          ; loop back, clearing odd byte flag


; ============================================================
; Type 1 transfer: 1-byte host -> parasite (LOAD)
;
; r1 - scratch register
; r2 - data register (16-bit data value read from memory)
; r3 - address register (16-bit memory address)
; ============================================================

Type1:
        LDA     ,0
        STO     R2              ; clean the odd byte flag (start with an even byte)

Type1_loop:
        LDA     .r4status       ; Test for an pending interrupt signalling end of transfer
        STO     R1
        AND     ,0x80
        JBC     ZERO CR Release

        LDA     .r3status       ; Wait for Tube R3 free
        STO     R1
        AND     ,0x80
        JBS     ZERO CR Type1_loop

        LDA     ,0              ; test odd byte flag
        SUB     R2
        JBS     SIGN CR Type1_odd_byte

        LDA     .r3data         ; Read the even byte from Tube T3
        ADD     ,0x8000         ; set the odd byte flag
        STO     R2
        JMP     .Type1_loop
Type1_odd_byte:
        LDA     R2              ; Clear upper byte of R2 ready for merging
        AND     0x00FF
        STO     R2
        LDA     .r3data         ; Read the odd byte from Tube T3
        SLL     8 A             ; Shift it to the upper byte
        ADS     R2              ; Merge into R2

        STO     /R3             ; Write word to memory, increment memory pointer
        LDA     ,1
        ADS     R3
        JMP     .Type1          ; loop back, clearing odd byte flag

Type4:
        LDA     R3
        STO     ,ADDR
Type2:
Type3:
Type6:
Type7:
        JMP     .Release


; --------------------------------------------------------------
; waitByteR1
;
; Wait for byte in Tube R1 while allowing requests via Tube R4
;
; Entry:
; - None
; Exit:
; - Acc holds byte, all other registers preserved

WaitByteR1:
        LDA     .r1status
        AND     ,0x80
        JBC     ZERO CR GotByteR1
        LDA     .r4status
        AND     0x80
        JBS     ZERO CR WaitByteR1

GotByteR1:
        LDA     .r1data
        RTN

; --------------------------------------------------------------
; waitByteR2
;
; Wait for a byte from Tube Reg R2 and return in ACC
;
; Entry:
; - None
; Exit:
; - Acc holds byte, all other registers preserved
waitByteR2:
        LDA     .r2status
        AND     ,0x80
        JBS     ZERO CR waitByteR2
        LDA     .r2data
        RTN

; --------------------------------------------------------------
; sendByteR2
;
; Wait for Tube reg R2 to be free and then send a byte
;
; Entry:
; - Acc hold byte to send
; Exit:
; - Acc preserved, R0 trashed
sendByteR2:
        STO     R0
sendByteR2_L1:
        LDA     .r2status
        AND     ,0x40
        JBS     ZERO CR sendByteR2_L1
        LDA     R0
        STO     .r2data
        RTN

; --------------------------------------------------------------
; sendStringR2
;
; Send a 0x0D terminated ASCII string to Tube Reg R2 (including
; the 0x0D byte itself)
; Entry:
; - ACC points to the location of the zero terminated string
;
; Exit:
; - r0,r1 trashed

sendStringR2:
        STO     R0
        LDA     ,1
        SUB     R0 ; Need addr-1 in R1 because we will use pre-incr pointer to address the string
        STO     R1
sendStringR2_loop:
        LDA     /R1+
        CAL     .sendByteR2
        CMP     ,0x0D
        JBC     ZERO CR sendStringR2_loop
sendStringR2_exit:
        RTN

; --------------------------------------------------------------
; waitByteR4
;
; Wait for a byte from Tube Reg R4 and return in ACC
;
; Entry:
; - None
; Exit:
; - Acc holds byte, all other registers preserved
waitByteR4:
        LDA     .r4status
        AND     ,0x80
        JBS     ZERO CR waitByteR4
        LDA     .r4data
        RTN

; --------------------------------------------------------------
; sendByteR4
;
; Wait for Tube reg R4 to be free and then send a byte
;
; Entry:
; - Acc hold byte to send
; Exit:
; - R0 trashed
sendByteR4:
        STO     R0
sendByteR4_L1:
        LDA     .r4status
        AND     ,0x40
        JBS     ZERO CR sendByteR4_L1
        LDA     R0
        STO     .r4data
        RTN

; -----------------------------------------------------------------------------
; Messages
; -----------------------------------------------------------------------------

BannerMessage:
        .WORD   0x0a
        ; .STRING "Ferranti F100-L (32K words)"
        .WORD   0x46,0x65,0x72,0x72,0x61,0x6e,0x74,0x69,0x20,0x46,0x31,0x30,0x30,0x2d,0x4c,0x20
        .WORD   0x28,0x33,0x32,0x4b,0x20,0x77,0x6f,0x72,0x64,0x73,0x29
        .WORD   0x0a,0x0a,0x0d,0x00
EscapeError:
        .WORD    17
        ; .STRING "Escape\0"
        .word   0x45,0x73,0x63,0x61,0x70,0x65,0x00

DefaultVectors:

	.WORD 0xF800,Unsupported    ; &200 - USERV (word addresses)
	.WORD 0xF800,ErrorHandler   ; &202 - BRKV
	.WORD 0xF800,IRQ1Handler    ; &204 - IRQ1V
	.WORD 0xF800,Unsupported    ; &206 - IRQ2V
	.WORD 0xF800,mosCLI         ; &208 - CLIV
	.WORD 0xF800,mosBYTE        ; &20A - BYTEV
	.WORD 0xF800,mosWORD        ; &20C - WORDV
	.WORD 0xF800,mosWRCH        ; &20E - WRCHV
	.WORD 0xF800,mosRDCH        ; &210 - RDCHV
	.WORD 0xF800,mosFILE        ; &212 - FILEV
	.WORD 0xF800,mosARGS        ; &214 - ARGSV
	.WORD 0xF800,mosBGET        ; &216 - BGetV
	.WORD 0xF800,mosBPUT        ; &218 - BPutV
	.WORD 0xF800,mosGBPB        ; &21A - GBPBV
	.WORD 0xF800,mosFIND        ; &21C - FINDV
	.WORD 0xF800,Unsupported    ; &21E - FSCV
	.WORD 0xF800,NullReturn     ; &220 - EVNTV
	.WORD 0xF800,Unsupported    ; &222 - UPTV
	.WORD 0xF800,Unsupported    ; &224 - NETV
	.WORD 0xF800,Unsupported    ; &226 - VduV
	.WORD 0xF800,Unsupported    ; &228 - KEYV
	.WORD 0xF800,Unsupported    ; &22A - INSV
	.WORD 0xF800,Unsupported    ; &22C - RemV
	.WORD 0xF800,Unsupported    ; &22E - CNPV
	.WORD 0xF800,NullReturn     ; &230 - IND1V
	.WORD 0xF800,NullReturn     ; &232 - IND2V
	.WORD 0xF800,NullReturn     ; &234 - IND3V

; -----------------------------------------------------------------------------
; Built-in application test: pi-spigot
; -----------------------------------------------------------------------------
.equ  DIGITS  120
.equ  COLS    1+(DIGITS*10//3)

PI:
        ; Initialise remainder array to 2
        LDA     ,REMAINDER-1
        STO     R1
        LDA     ,-COLS          ;  -cols negative counter for use with ICZ
        STO     R0
LOOP:
        LDA     ,2
        STO     /R1+            ; init remainder values to 2
        ICZ     R0 LOOP

        LDA     ,-DIGITS        ; Digit counter from -Digits to 0
        STO     R8
        LDA     ,0             ; C = 0
        STO     R3
        STO     R9              ; init outer loop counter
        STO     R2              ; nines=0

L3:     LDA     ,COLS-1         ; i = COLS-1
        STO     R4
        ADD     ,REMAINDER      ; setup remainder pointer to end of data
        STO     R7
        LDA     ,0            ; Q = 0
        STO     R5

L4:     LDA     /R7             ; Q += R[i]*10
        SLA     3 A
        ADD     /R7
        ADD     /R7
        ADS     R5
        SLA     1 R4            ; denom=(2*i)-1
        STO     R15
        LDA     ,1
        SBS     R15
        LDA     R5              ; r[i] = q % denom ; q = q//denom
        STO     R14
        CAL     .UDIV16         ; returns R14=Quotient, R15=Remainder
        LDA     R14
        STO     R5              ; Q = quotient
        LDA     R15
        STO     /R7             ; r[i] = remainder
        LDA     ,1
        SBS     R4              ; R4--
        JBS     ZERO CR L5
        LDA     /R7-            ; decrement pointer R7
        LDA     R5              ; Q *= i
        STO     R14
        LDA     R4
        STO     R15
        CAL     .MUL16
        LDA     R15
        STO     R5
        JMP     .L4

L5:     LDA     R5              ; result = C+Q//10
        STO     R14
        LDA     ,10
        STO     R15
        CAL     .UDIV16
        LDA     R14
        ADD     R3
        STO     R6
        LDA     R15             ; C=Q%10
        STO     R3

        LDA     R6              ; check if result=9
        CMP     ,9
        JBC     ZERO CR L5A
        LDA     ,1              ; decrement 9s if yes (-ve counting)
        SBS     R2
        ICZ     R8 L3           ; loop again without printing digits

L5A:    LDA     R6              ; Check if result=10 (ie overflow)
        CMP     ,10
        JBC     ZERO CR L6
        LDA     ,0            ; if yes, zero result
        STO     R6
        LDA     ,1              ; and inc predigit
        ADS     R9
        CAL     .OSWRDIG        ; print predigit and all nines will be zeros now
        LDA     R2
        JBS     ZERO CR L7
L5B:    LDA     ,0
        CAL     .OSWRDIG
        ICZ     R2 L5B

L6:     LDA     R8              ; If this isnt the first digit then print the existing pre-digit
        CMP     ,-DIGITS
        JBS     ZERO CR L7
        LDA     R9
        CAL     .OSWRDIG        ; print predigit and all nines
        LDA     R2
        JBS     ZERO CR L7
L6B:    LDA     ,9
        CAL     .OSWRDIG
        ICZ     R2 L6B

L7:     LDA     R6              ; predigit <- result
        STO     R9
L8:     ICZ     R8 L3
        ;; Print last digit
        LDA     R9
        CAL     .OSWRDIG
        RTN

; ------------------------------------
; UDIV16
;
; Unsigned integer 16 bit division of two numbers
; returning quotient and remainder.
; ------------------------------------
; Entry: R14 -> Numerator
;        R15 -> Denominator
;
; Exit:  R14 -> Quotient
;        R15 -> Remainder
;        R11-13 -> trashed
; ------------------------------------
; Algorithm
; ------------------------------------
;   def div16(N,D):
;       if N<D :
;           return(0,N)
;       Q = 0
;       R = 0
;       for i in range (-15, 1 ):
;           Q <<= 1
;           R <<= 1
;           if bit (N,15) :
;               R = bitset(R,0)
;           if R >= D:
;               R = R - D
;               Q = bitset(Q,0)
;           N <<= 1
;       return (Q,R)
; ------------------------------------

;; Local storage
.equ UD16_N    R14
.equ UD16_D    R15
.equ UD16_Q    R13
.equ UD16_R    R12
.equ UD16_LCTR R11

UDIV16:
         LDA ,0
         STO UD16_Q
         STO UD16_R
UD16_L1:
         LDA ,-16
         STO UD16_LCTR
UD16_LOOP:
         SLA 1 UD16_Q
         STO UD16_Q
         SLA 1 UD16_R
         STO UD16_R
         JBC 15 UD16_N UD16_SKIP
         SET 0 UD16_R
UD16_SKIP:
         LDA UD16_D
         CMP UD16_R
         JBS SIGN CR UD16_SKIP2        ; JUMP if R < D
         LDA UD16_D
         SBS UD16_R
         SET 0 UD16_Q
UD16_SKIP2:
         SLA 1 UD16_N
         STO UD16_N
         ICZ UD16_LCTR UD16_LOOP
UD16_EXIT:
         LDA UD16_R
         STO R15
         LDA UD16_Q
         STO R14
         RTN

; ------------------------------------
; MUL16
;
; Multiply 2 16 bit numbers to yield a 16b result
; ------------------------------------
; Entry: R14 -> A
;        R15 -> B
;
; Exit:  R15    -> A*B
;        R11-14 -> trashed
; --------------------------------------------------------------
; Register Map
.equ    M16L_count      R11
.equ    M16L_aa         R13
.equ    M16L_bb         R14
.equ    M16L_res        R15

MUL16:
        LDA ,-15
        STO M16L_count
        LDA R15
        STO M16L_aa
        LDA ,0
        STO M16L_res
M16L_LOOP:
        JBC 0 M16L_aa M16L_SKIPADD
        LDA M16L_bb
        ADS M16L_res
M16L_SKIPADD:
        SRA 1 M16L_aa
        STO M16L_aa
        SLL 1 M16L_bb
        STO M16L_bb
        ICZ M16L_count M16L_LOOP
        RTN


; -----------------------------------------------------------------------------
; E N D  O F  R O M
; -----------------------------------------------------------------------------

        .ORG APP                ; running pi-spigot uses all of apps area
REMAINDER:                      ; array space for remainder (RAM)




; -----------------------------------------------------------------------------
; MOS interface - normal for Acorn to be at the top of the memory map
;               - see http://mdfs.net/Software/Tube/Z80/ClientZ80.src for Z80 impl.
; -----------------------------------------------------------------------------

.org MOS

NVRDCH:                      ; &7FC8
    JMP	.mosRDCH

.org MOS + (0xCB-0xC8)

NVWRCH:                      ; &7FCB
    JMP	.mosWRCH

.org MOS + (0xCE-0xC8)

OSFIND:                      ; &7FCE
    JMP	.FINDV

.org MOS + (0xD1-0xC8)

OSGBPB:                      ; &7FD1
    JMP	.GBPBV

.org MOS + (0xD4-0xC8)

OSBPUT:                      ; &7FD4
    JMP	.BPUTV

.org MOS + (0xD7-0xC8)

OSBGET:                      ; &7FD7
    JMP	.BGETV

.org MOS + (0xDA-0xC8)

OSARGS:                      ; &7FDA
    JMP	.ARGSV

.org MOS + (0xDD-0xC8)

OSFILE:                      ; &7FDD
    JMP	.FILEV

.org MOS + (0xE0-0xC8)

OSRDCH:                      ; &7FE0
    JMP	.RDCHV

.org MOS + (0xE3-0xC8)

OSASCI:                      ; &7FE3
    CMP     0x0d
    JBC     ZERO CR OSWRCH

.org MOS + (0xE7-0xC8)

OSNEWL:                      ; &7FE7
    JMP     .newline

.org MOS + (0xEC-0xC8)

OSWRCR:                      ; &7FEC
    LDA     0x0D

.org MOS + (0xEE-0xC8)

OSWRCH:                      ; &7FEE
    JMP	.WRCHV

.org MOS + (0xF1-0xC8)

OSWORD:                      ; &7FF1
    JMP	.WORDV

.org MOS + (0xF4-0xC8)

OSBYTE:                      ; &7FF4
    JMP	.BYTEV

.org MOS + (0xF7-0xC8)

OS_CLI:                      ; &7FF7
    JMP	.CLIV


.org MOS + (0xFE-0xC8)

IRQ_ENTRY:                   ; &7FFE
    JMP     .InterruptHandler
