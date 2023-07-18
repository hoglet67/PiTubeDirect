
.equ       STACK, 0x000EFFFC
.equ        TUBE, 0x00FFFFE0

.equ    R1STATUS, 0
.equ      R1DATA, 4
.equ    R2STATUS, 8
.equ      R2DATA, 12
.equ    R3STATUS, 16
.equ      R3DATA, 20
.equ    R4STATUS, 24
.equ      R4DATA, 28

.equ NUM_VECTORS, 27           # number of vectors in DefaultVectors table

.globl _start

.macro PUSH reg
    sw      \reg, 0(sp)
    addi    sp, sp, -4
.endm

.macro POP reg
    addi    sp, sp, 4
    lw      \reg, 0(sp)
.endm

.macro JMPI addr
    la      t0, \addr
    lw      t0, (t0)
    jalr    zero, (t0)
.endm

.macro CLC
# TODO
.endm

.macro SEC
# TODO
.endm

.section .text

_start:
    j       ResetHandler

USERV:      .word  0
BRKV:       .word  0
IRQ1V:      .word  0
IRQ2V:      .word  0
CLIV:       .word  0
BYTEV:      .word  0
WORDV:      .word  0
WRCHV:      .word  0
RDCHV:      .word  0
FILEV:      .word  0
ARGSV:      .word  0
BGETV:      .word  0
BPUTV:      .word  0
GBPBV:      .word  0
FINDV:      .word  0
FSCV:       .word  0
EVNTV:      .word  0
UPTV:       .word  0
NETV:       .word  0
VDUV:       .word  0
KEYV:       .word  0
INSV:       .word  0
REMV:       .word  0
CNPV:       .word  0
IND1V:      .word  0
IND2V:      .word  0
IND3V:      .word  0

.align 8

ERRBUF:
INPBUF:
            .zero 0x80
INPEND:

ADDR:       .word 0 # tube execution address
TMP_R1:     .word 0 # tmp store for R1 during IRQ
LAST_ERR:   .word 0 # last error
ESCAPE_FLAG:.word 0 # escape flag

ResetHandler:
    li      gp, TUBE                    # setup the register that points to the tube (TODO: Probably a bad idea to use a register like this!)
    li      sp, STACK                   # setup the stack

    la      t1, DefaultVectors          # copy the vectors
    la      t2, USERV
    li      t3, NUM_VECTORS - 1
InitVecLoop:
    lw      t0, (t1)
    sw      t0, (t2)
    addi    t1, t1, 4
    addi    t2, t2, 4
    addi    t3, t3, -1
    bnez    t3, InitVecLoop

#     EI      ()                        # enable interrupts

    la      a1, BannerMessage           # send the reset message
    jal     print_string

    mv      a0, zero                    # send the terminator
    jal     OSWRCH

    jal     WaitByteR2                  # wait for the response and ignore

CmdPrompt:

CmdOSLoop:
    li      a0, 0x2a
    jal     OSWRCH

    mv      a0, zero
    la      a1, osword0_param_block

    jal     OSWORD

#     c.mov   pc, r0, CmdOSEscape

    la      a0, INPBUF
    jal     OS_CLI
    j       CmdOSLoop

# CmdOSEscape:
#     mov     r1, r0, 0x7e
#     JSR     (OSBYTE)
#     ERROR   (EscapeError)

# --------------------------------------------------------------
# MOS interface
# --------------------------------------------------------------

NullReturn:
    ret

# --------------------------------------------------------------

Unsupported:
    ret

# --------------------------------------------------------------

ErrorHandler:

#     mov     r14, r0, STACK              # Clear the stack

#     JSR     (OSNEWL)
#     ld       r1, r0, LAST_ERR           # Address of the last error: <error num> <err string> <00>
#     add      r1, r0, 1                  # Skip over error num
#     JSR     (print_string)              # Print error string
#     JSR     (OSNEWL)

#     mov     pc, r0, CmdPrompt           # Jump to command prompt
    j       CmdPrompt

# --------------------------------------------------------------

osARGS:
#     # TODO
    ret

# --------------------------------------------------------------

osBGET:
#     # TODO
    ret

# --------------------------------------------------------------

osBPUT:
#     # TODO
    ret

# --------------------------------------------------------------

# OSBYTE - Byte MOS functions
# ===========================
# On entry, r1, r2, r3=OSBYTE parameters
# On exit,  r1  preserved
#           If r1<$80, r2=returned value
#           If r1>$7F, r2, r3, Carry=returned values
#
osBYTE:
#     PUSH    (r13)
#     cmp     r1, r0, 0x80        # Jump for long OSBYTEs
#     c.mov   pc, r0, ByteHigh
#
# Tube data  $04 X A    --  X
#
#     PUSH    (r1)
#     mov     r1, r0, 0x04        # Send command &04 - OSBYTELO
#     JSR     (SendByteR2)
#     mov     r1, r2
#     JSR     (SendByteR2)        # Send single parameter
#     POP     (r1)
#     PUSH    (r1)
#     JSR     (SendByteR2)        # Send function
#     JSR     (WaitByteR2)        # Get return value
#     mov     r2, r1
#     POP     (r1)
#     POP     (r13)
    ret

# ByteHigh:
#     cmp     r1, r0, 0x82        # Read memory high word
#     z.mov   pc, r0, Byte82
#     cmp     r1, r0, 0x83        # Read bottom of memory
#     z.mov   pc, r0, Byte83
#     cmp     r1, r0, 0x84        # Read top of memory
#     z.mov   pc, r0, Byte84
#
# Tube data  $06 X Y A  --  Cy Y X
#

#     PUSH    (r1)
#     mov     r1, r0, 0x06
#     JSR     (SendByteR2)        # Send command &06 - OSBYTEHI
#     mov     r2, r1
#     JSR     (SendByteR2)        # Send parameter 1
#     mov     r3, r1
#     JSR     (SendByteR2)        # Send parameter 2
#     POP     (r1)
#     PUSH    (r1)
#     JSR     (SendByteR2)        # Send function
#   cmp     r1, r0, 0x8e        # If select language, check to enter code
#   z.mov   pc, r0, CheckAck
#     cmp     r1, r0, 0x9d        # Fast return with Fast BPUT
#     z.mov   pc, r0, FastReturn
#     JSR     (WaitByteR2)        # Get carry - from bit 7
#     add     r1, r0, -0x80
#     JSR     (WaitByteR2)        # Get high byte
#     mov     r3, r1
#     JSR     (WaitByteR2)        # Get low byte
#     mov     r2, r1
# FastReturn:
#     POP     (r1)                # restore original r1
#     POP     (r13)
#     RTS     ()

# Byte84:                         # Read top of memory
#     mov      r1, r0, MEM_TOP
#     POP     (r13)
#     RTS     ()
# Byte83:                         # Read bottom of memory
#     mov     r1, r0, MEM_BOT
#     POP     (r13)
#     RTS     ()

# Byte82:                         # Return &0000 as memory high word
#     mov     r1, r0
#     POP     (r13)
#     RTS     ()

# --------------------------------------------------------------

# OSCLI - Send command line to host
# =================================
# On entry, a0=>command string
#
# Tube data  &02 string &0D  --  &7F or &80
#

osCLI:
#     PUSH    (r13)
#     PUSH    (r2)

    mv        a1, a0
    PUSH      ra

#     PUSH    (r1)                    # save the string pointer
#     JSR     (cmdLocal)              # try to handle the command locally
#     cmp     r1, r0                  # was command handled locally? (r1 = 0)
#     z.mov   pc, r0, dontEnterCode   # yes, then nothing more to do
#     POP     (r1)                    # restore the string pointer

    PUSH    a1
    li      a0, 0x02                    # Send command &02 - OSCLI
    jal     SendByteR2
    jal     SendStringR2

osCLI_Ack:
    jal     WaitByteR2

    li      t0, 0x80
    blt     a0, t0, dontEnterCode

    POP     a1
    PUSH    a1
#     JSR     (prep_env)

#     JSR     (enterCode)

dontEnterCode:
    POP     a1
    POP     ra
    ret

# enterCode:
#     ld      pc, r0, ADDR

# Find the start of the command string
#
# Lots of ways a file can be run:
# *    filename params
# * /  filename params
# *R.  filename params
# *RU. filename params
# *RUN filename params
#
#
# In general you want:
# - skip leading space or * characters
# - skip any form of *RUN, followed by trailing spaces
# - leave the environment point at the first character of filename

# prep_env:
#     PUSH    (r13)
#     DEC     (r1, 1)
# prep_env_1:                         # skip leading space or * characters
#     INC     (r1, 1)
#     JSR     (skip_spaces)
#     cmp     r2, r0, 0x2A            # *
#     z.mov   pc, r0, prep_env_1

#     cmp     r2, r0, 0x2F            # /
#     z.mov   pc, r0, prep_env_4

#     mov     r2, r0, run_string - 1
#     mov     r3, r1, -1
# prep_env_2:                         # skip a possibly abbreviated RUN
#     INC     (r2, 1)
#     ld      r4, r2                  # Read R U N <0>
#     z.mov   pc, r0, prep_env_3
#     INC     (r3, 1)
#     ld      r5, r3
#     cmp     r5, r0, 0x2E            # .
#     z.mov   pc, r0, prep_env_3
#     and     r5, r0, 0xDF            # force upper case
#     cmp     r4, r5                  # match against R U N
#     nz.mov  pc, r0, prep_env_5
#     mov     pc, r0, prep_env_2      # loop back for more characters

# prep_env_3:
#     mov     r1, r3

# prep_env_4:
#     INC     (r1, 1)

# prep_env_5:
#     JSR     (skip_spaces)
#     POP     (r13)
#     RTS     ()

# run_string:
#     STRING  "RUN"
#     WORD    0

# --------------------------------------------------------------
# Local Command Processor
#
# On Entry:
# - r1 points to the user command
#
# On Exit:
# - r1 == 0 if command successfully processed locally
# - r1 != 0 if command should be
#
# Register usage:
# r1 points to start of user command
# r2 points within command table
# r3 points within user command
# r4 is current character in command table
# r5 is current character in user command

# cmdLocal:
#     PUSH    (r2)
#     PUSH    (r3)
#     PUSH    (r4)
#     PUSH    (r5)
#     PUSH    (r13)

#     sub     r1, r0, 1
# cmdLoop0:
#     add     r1, r0, 1
#     JSR     (skip_spaces)           # skip leading spaces
#     cmp     r2, r0, ord('*')        # also skip leading *
#     z.mov   pc, r0, cmdLoop0

#     mov     r2, r0, cmdTable-1      # initialize command table pointer (to char before)

# cmdLoop1:
#     mov     r3, r1, -1              # initialize user command pointer (to char before)

# cmdLoop2:
#     add     r2, r0, 1               # increment command table pointer
#     add     r3, r0, 1               # increment user command pointer
#     ld      r5, r3                  # read next character from user command
#     or      r5, r0, 0x20            # convert to lower case
#     ld      r4, r2                  # read next character from command table
#     mi.mov  pc, r0, cmdCheck        # if an address, then we are done matching
#     cmp     r5, r4                  # compare the characters
#     z.mov   pc, r0, cmdLoop2        # if a match, loop back for more

#     sub     r2, r0, 1
# cmdLoop3:                           # skip to the end of the command in the table
#     add     r2, r0, 1
#     ld      r4, r2
#     pl.mov  pc, r0, cmdLoop3

#     cmp     r5, r0, ord('.')        # was the mis-match a '.'
#     nz.mov  pc, r0, cmdLoop1        # no, then start again with next command

#     add     r3, r0, 1               # increment user command pointer past the '.'

# cmdExec:

#     mov     r1, r3                  # r1 = the command pointer to the params
#     mov     r2, r4                  # r2 = the execution address

#     JSR     (cmdExecR2)

# cmdExit:
#     POP     (r13)
#     POP     (r5)
#     POP     (r4)
#     POP     (r3)
#     POP     (r2)
#     RTS     ()
#                                     # Additional code to make the match non-greedy
#                                     # e.g. *MEMORY should not match against the MEM command
#                                     #      also exclude *MEM.
# cmdCheck:
#     cmp     r5, r0, ord('.')        # check the first non-matching user char against '.'
#     z.mov   pc, r0, cmdReject       # if == '.' then reject the command
#     cmp     r5, r0, ord('a')        # check the first non-matching user char against 'a'
#     nc.mov  pc, r0, cmdExec         # if < 'a' then execute the local command
#     cmp     r5, r0, ord('z')+1      # check the first non-matching user char against 'z'
#     c.mov   pc, r0, cmdExec         # if >= 'z'+1 then execute the local command
# cmdReject:
#     mov     r1, r0, 1               # flag command as not handled here then return
#     mov     pc, r0, cmdExit         # allowing the command to be handled elsewhere

# --------------------------------------------------------------

# cmdGo:
#     PUSH    (r13)
#     JSR     (read_hex)
#     JSR     (cmdExecR2)
#     mov     r1, r0
#     POP     (r13)
#     RTS     ()

# --------------------------------------------------------------

# cmdMem:
#     PUSH    (r13)
#     JSR     (read_hex)
#     mov     r1, r2
#     JSR     (dump_mem)
#     mov     r1, r0
#     POP     (r13)
#     RTS     ()

# --------------------------------------------------------------

# cmdDis:
#     PUSH    (r13)
#     JSR     (read_hex)

#     mov     r3, r0, 16
# dis_loop:
#     mov     r1, r2
#     JSR     (disassemble)
#     mov     r2, r1
#     JSR     (OSNEWL)
#     DEC     (r3, 1)
#     nz.mov  pc, r0, dis_loop
#     mov     r1, r0
#     POP     (r13)
#     RTS     ()

# --------------------------------------------------------------

# cmdHelp:
#     PUSH   (r13)
#     mov    r1, r0, msgHelp
#     JSR    (print_string)
#     mov    r1, r0, 1
#     POP    (r13)
#     RTS    ()

# msgHelp:
#     CPU_STRING()
#     STRING   " 0.55"
#     WORD     10, 13, 0

# --------------------------------------------------------------

# cmdTest:
#     PUSH   (r13)
#     JSR    (read_hex)

#     # Save param
#     PUSH   (r2)

#     # Set TIME to using OSWORD 2
#     mov    r1, r0, 0x02
#     mov    r2, r0, osword_pblock
#     sto    r0, r2
#     sto    r0, r2, 1
#     sto    r0, r2, 2     # only necessary when word size is 16 bits
#     JSR    (OSWORD)

#     # Restore param
#     POP    (r1)
#     PUSH    (r1)

#     # Print param
#     JSR    (print_hex_word_spc)
#     JSR    (print_dec_word)
#     JSR    (OSNEWL)


#     # Restore param
#     POP    (r1)

#     # Do some real work
#     JSR    (pi_calc)
#     JSR    (OSNEWL)

#     # Read TIME using OSWORD 1
#     mov    r1, r0, 0x01
#     mov    r2, r0, osword_pblock
#     JSR    (OSWORD)

#     # Print the LS word in decomal
#     ld     r1, r0, osword_pblock
#     JSR    (print_dec_word)
#     JSR    (OSNEWL)

#     mov    r1, r0
#     POP    (r13)
#     RTS    ()

# osword_pblock:
#     WORD   0
#     WORD   0
#     WORD   0

# -------------------------------------------------------
# Pi Test Program
#
# On entry, r1 is the number of digits
# -------------------------------------------------------
#     EQU       p, 0x1000

# pi_calc:
#     PUSH   (r13)

#     # r1 = ndigits
#     # psize calculated dynamicaly as = 1 + ndigits * 10 / 3

#     PUSH   (r1)                # push ndigits
#     mov    r11, r0, 10
#     JSR    (mul)

#     mov    r11, r1
#     mov    r1, r0, 3
#     JSR    (div)               # r11 / r1 => quotient in r11, remainder in r1
#     mov    r2, r11, 1
#     PUSH   (r2)                # push psize

#     JSR    (init)              # ndigits in r2

#     POP    (r3)                # pop psize
#     POP    (r2)                # pop ndigits

#     #mov    r2, r0, ndigits    # ldx #359
#     #mov    r3, r0, psize      # ldy #1193

#     mov    r7, r2, -1          # r7 = ndigits - 1
# l1:
#     mov    r6, r3              # phy
#     mov    r4, r1              # pha
#     mov    r5, r2              # phx
#     mov    r11, r0             # stz q
#     mov    r1, r3              # tya
#     mov    r2, r1              # tax

# l2:
#     mov    r1, r2              # txa
#     JSR    (mul)
#     mov    r9, r1              # sta s
#     mov    r1, r0, 10          # lda #10
#     mov    r11, r1             # sta q
#     ld     r1, r2, p-1         # lda p-1,x
#     JSR    (mul)
#                                # clc
#     add    r1, r9              # adc s
#     mov    r11, r1             # sta q
#     mov    r1, r2              # txa
#     add    r1, r1              # asl
#     mov    r1, r1, -1          # dec
#     JSR    (div)
#     sto    r1, r2, p-1         # sta p-1,x
#     mov    r2, r2, -1          # dex
#     nz.mov pc, r0, l2          # bne l2

#     mov    r1, r0, 10          # lda #10
#     JSR    (div)
#     sto    r1, r0, p           # sta p
#     mov    r2, r5              # plx
#     mov    r1, r4              # pla
#     mov    r3, r11             # ldy q
#     cmp    r3, r0, 10          # cpy #10
#     nc.mov pc, r0, l3          # bcc l3
#     mov    r3, r0              # ldy #0
#     mov    r1, r1, 1           # inc
# l3:
#     cmp    r2, r7              # cpx #358
#     nc.mov pc, r0, l4          # bcc l4
#     nz.mov pc, r0, l5          # bne l5
#     JSR    (OSWRCH)
#     mov    r1, r0, 46          # lda #46
# l4:
#     JSR    (OSWRCH)
# l5:
#     mov    r1, r3              # tya
#     xor    r1, r0, 48          # eor #48
#     mov    r3, r6              # ply
#     cmp    r2, r7              # cpx #358
#     c.mov  pc, r0, l6          # bcs l6
#                                # dey
#                                # dey
#     mov    r3, r3, -3          # dey by 3
# l6:
#     mov    r2, r2, -1          # dex
#     nz.mov pc, r0, l1          # bne l1
#     JSR    (OSWRCH)
#     mov    r0, r0, 3142        # RTS()
#     POP    (r13)
#     RTS    ()

# init:
#     mov    r1, r0, 2           # lda #2
#     #mov    r2, r0, psize       # was ldx #1192
# i1:
#     sto    r1, r2, p-1         # was sta p,x
#     mov    r2, r2, -1          # dex
#     nz.mov pc, r0, i1          # bne instead of bpl i1
#     RTS    ()

# mul:                           # uses y as loop counter
#     mov    r10, r1             # sta r
#     mov    r3, r0, WORD_SIZE   # ldy #16
# m1:
#     add    r1, r1              # asl
#     add    r11, r11            # asl q
#     nc.mov pc, r0, m2          # bcc m2
#                                # clc
#     add    r1, r10             # adc r
# m2:
#     mov    r3, r3, -1          # dey
#     nz.mov pc, r0, m1          # bne m1
#     RTS()

# div:                           # uses y as loop counter
#     mov    r10, r1             # sta r
#     mov    r3, r0, WORD_SIZE   # ldy #16
#     mov    r1, r0, 0           # lda #0
#     add    r11, r11            # asl q
# d1:
#     ROL    (r1, r1)            # rol
#     cmp    r1, r10             # cmp r
#     nc.mov pc, r0, d2          # bcc d2
#     sub    r1, r10             # sbc r
# d2:
#     ROL    (r11, r11)          # rol q
#     mov    r3, r3, -1          # dey
#     nz.mov pc, r0, d1          # bne d1
#     RTS    ()


# ---------------------------------------------------------

# cmdSrec:
#     PUSH   (r13)
#     JSR    (srec_load)
#     cmp    r1, r0, 1
#     z.mov  pc, r0, CmdOSEscape
#     cmp    r1, r0, 2
#     z.mov  pc, r0, cmdSrecChecksumError
#     cmp    r1, r0
#     nz.mov pc, r0, cmdSrecBadFormatError
#     POP    (r13)
#     RTS    ()

# cmdSrecChecksumError:
#     ERROR   (checksumError)

# checksumError:
#     WORD    17                    # TODO assign proper error code
#     STRING "Checksum Mismatch"
#     WORD    0x00

# cmdSrecBadFormatError:
#     ERROR   (badFormatError)

# badFormatError:
#     WORD    17                    # TODO assign proper error code
#     STRING "Bad Format"
#     WORD    0x00

# ---------------------------------------------------------

# cmdEnd:
#     mov    r1, r0, 1
#     RTS    ()

# --------------------------------------------------------------

# cmdExecR2:
#     mov    pc, r2

# --------------------------------------------------------------

# cmdTable:
#     STRING  "."
#     WORD    cmdEnd  | END_MARKER
#     STRING  "go"
#     WORD    cmdGo   | END_MARKER
#     STRING  "mem"
#     WORD    cmdMem  | END_MARKER
#     STRING  "dis"
#     WORD    cmdDis  | END_MARKER
#     STRING  "help"
#     WORD    cmdHelp | END_MARKER
#     STRING  "test"
#     WORD    cmdTest | END_MARKER
#     STRING  "srec"
#     WORD    cmdSrec | END_MARKER
#     WORD    cmdEnd  | END_MARKER

# --------------------------------------------------------------

osFILE:
#     # TODO
    ret

# --------------------------------------------------------------

osFIND:
    #     # TODO
    ret

# --------------------------------------------------------------

osGBPB:
#     # TODO
    ret

# --------------------------------------------------------------


# On entry:
#   a0 is the osword number
#   a1 points to the paramater block in memory

osWORD:
    beqz    a0, RDLINE

#     PUSH    (r13)
#     PUSH    (r1)              # save calling param (osword number)
#     PUSH    (r2)              # save calling param (block)
#     PUSH    (r3)              # r3 used as a scratch register
#     PUSH    (r4)              # r4 used as a scratch register

#     mov     r3, r1
#     mov     r4, r2
#     mov     r1, r0, 8
#     JSR     (SendByteR2)      # Send command &08 - OSWORD
#     mov     r1, r3
#     JSR     (SendByteR2)      # Send osword number
#     cmp     r3, r0, 0x15      # Compute index into length table
#     c.mov   r3, r0            # >= OSWORD 0x15, use slot 0

#     ld      r1, r3, word_in_len
#     JSR     (SendByteR2)      # Send request block length

#     ld      r1, r3, word_in_len
#     mov     r2, r4
#     JSR     (SendBlockR2)     # Send request block

#     ld      r1, r3, word_out_len
#     JSR     (SendByteR2)      # Send response block length

#     ld      r1, r3, word_out_len
#     mov     r2, r4
#     JSR     (ReceiveBlockR2)  # Receive response block

#     POP     (r4)
#     POP     (r3)
#     POP     (r2)
#     POP     (r1)
#     POP     (r13)
    ret

# Send a defined size block to tube FIFO R2
# r1 is the length
# r2 is the block address
# The complexity here is the block needs to be send backwards!

# SendBlockR2:
#     PUSH    (r13)
#     PUSH    (r3)
#     PUSH    (r4)
#     mov     r4, r1, -1         # r4 = block length - 1
#     mi.mov  pc, r0, SendBlockDone

##ifdef CPU_OPC7

#     mov     r1, r4            # calculate address of word containing last byte
#     lsr     r1, r1
#     lsr     r1, r1
#     add     r2, r1
#     ld      r3, r2            # load the first word from memory
#     mov     r1, r4
#     and     r1, r0, 3         # word out the byte alignment of the last byte in the block (first byte to send)
#     z.mov   pc, r0, SendBlockB0
#     cmp     r1, r0, 1
#     z.mov   pc, r0, SendBlockB1
#     cmp     r1, r0, 2
#     z.mov   pc, r0, SendBlockB2
# SendBlockB3:
#     bperm   r1, r3, 0x4443
#     JSR     (SendByteR2)      # send byte 3
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
# SendBlockB2:
#     bperm   r1, r3, 0x4442
#     JSR     (SendByteR2)      # send byte 2
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
# SendBlockB1:
#     bperm   r1, r3, 0x4441
#     JSR     (SendByteR2)      # send byte 1
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
# SendBlockB0:
#     bperm   r1, r3, 0x4440
#     JSR     (SendByteR2)      # send byte 0
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
#     DEC     (r2, 1)
#     ld      r3, r2            # load the next word from memory
#     mov     pc, r0, SendBlockB3

##else

#     mov     r1, r4            # calculate address of word containing last byte
#     LSR     (r1, r1)
#     add     r2, r1
#     ld      r3, r2            # load the first word from memory
#     mov     r1, r4
#     and     r1, r0, 1         # word out the byte alignment of the last byte in the block (first byte to send)
#     z.mov   pc, r0, SendBlockB0
# SendBlockB1:
#     bswp    r1, r3
#     JSR     (SendByteR2)      # send byte 1
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
# SendBlockB0:
#     JSR     (SendByteR2)      # send byte 0
#     DEC     (r4, 1)
#     mi.mov  pc, r0, SendBlockDone
#     DEC     (r2, 1)
#     ld      r3, r2            # load the next word from memory
#     mov     pc, r0, SendBlockB1

##endif

# SendBlockDone:
#     POP     (r4)
#     POP     (r3)
#     POP     (r13)
#     RTS     ()


# Receive a defined size block from tube FIFO R2
# r1 is the length
# r2 is the block address

# ReceiveBlockR2:
#     PUSH    (r13)
#     PUSH    (r3)
#     PUSH    (r4)
#     mov     r4, r1, -1         # r4 = block length - 1
#     mi.mov  pc, r0, ReceiveBlockDone

##ifdef CPU_OPC7

#     mov     r1, r4            # calculate address of word containing last byte
#     lsr     r1, r1
#     lsr     r1, r1
#     add     r2, r1
#     mov     r3, r0            # clear the receive word
#     mov     r1, r4
#     and     r1, r0, 3         # word out the byte alignment of the last byte in the block (first byte to send)
#     z.mov   pc, r0, ReceiveBlockB0
#     cmp     r1, r0, 1
#     z.mov   pc, r0, ReceiveBlockB1
#     cmp     r1, r0, 2
#     z.mov   pc, r0, ReceiveBlockB2
# ReceiveBlockB3:
#     JSR     (WaitByteR2)      # receive byte 3
#     bperm   r1, r1, 0x0444
#     or      r3, r1
#     DEC     (r4, 1)
#     mi.mov  pc, r0, ReceiveBlockWrite
# ReceiveBlockB2:
#     JSR     (WaitByteR2)      # receive byte 2
#     bperm   r1, r1, 0x4044
#     or      r3, r1
#     DEC     (r4, 1)
# ReceiveBlockB1:
#     mi.mov  pc, r0, ReceiveBlockWrite
#     JSR     (WaitByteR2)      # receive byte 1
#     bperm   r1, r1, 0x4404
#     or      r3, r1
#     DEC     (r4, 1)
# ReceiveBlockB0:
#     mi.mov  pc, r0, ReceiveBlockWrite
#     JSR     (WaitByteR2)      # receive byte 0
#     bperm   r1, r1, 0x4440    # this instruction could probably be skipped
#     or      r3, r1
# ReceiveBlockWrite:
#     sto     r3, r2
#     mov     r3, r0
#     DEC     (r2, 1)
#     DEC     (r4, 1)
#     pl.mov  pc, r0, ReceiveBlockB3

##else

#     mov     r1, r4            # calculate address of word containing last byte
#     LSR     (r1, r1)
#     add     r2, r1
#     mov     r3, r0            # clear the receive word
#     mov     r1, r4
#     and     r1, r0, 1         # word out the byte alignment of the last byte in the block (first byte to send)
#     z.mov   pc, r0, ReceiveBlockB0
# ReceiveBlockB1:
#     mi.mov  pc, r0, ReceiveBlockWrite
#     JSR     (WaitByteR2)      # receive byte 1
#     bswp    r1, r1
#     or      r3, r1
#     DEC     (r4, 1)
# ReceiveBlockB0:
#     mi.mov  pc, r0, ReceiveBlockWrite
#     JSR     (WaitByteR2)      # receive byte 0
#     or      r3, r1
# ReceiveBlockWrite:
#     sto     r3, r2
#     mov     r3, r0
#     DEC     (r2, 1)
#     DEC     (r4, 1)
#     pl.mov  pc, r0, ReceiveBlockB1

##endif

# ReceiveBlockDone:
#     POP     (r4)
#     POP     (r3)
#     POP     (r13)
#     RTS     ()

# word_in_len:
#     WORD 16   # OSWORD default
#     WORD 0    #  1  =TIME
#     WORD 5    #  2  TIME=
#     WORD 0    #  3  =IntTimer
#     WORD 5    #  4  IntTimer=
#     WORD 4    #  5  =IOMEM   JGH: must send full 4-byte address
#     WORD 5    #  6  IOMEM=
#     WORD 8    #  7  SOUND
#     WORD 14   #  8  ENVELOPE
#     WORD 4    #  9  =POINT()
#     WORD 1    # 10  =CHR$()
#     WORD 1    # 11  =Palette
#     WORD 5    # 12  Pallette=
#     WORD 0    # 13  =Coords
#     WORD 8    # 14  =RTC
#     WORD 25   # 15  RTC=
#     WORD 16   # 16  NetTx
#     WORD 13   # 17  NetRx
#     WORD 0    # 18  NetArgs
#     WORD 8    # 19  NetInfo
#     WORD 128  # 20  NetFSOp

# word_out_len:
#     WORD 16   # OSWORD default
#     WORD 5    #  1  =TIME
#     WORD 0    #  2  TIME=
#     WORD 5    #  3  =IntTimer
#     WORD 0    #  4  IntTimer=
#     WORD 5    #  5  =IOMEM
#     WORD 0    #  6  IOMEM=
#     WORD 0    #  7  SOUND
#     WORD 0    #  8  ENVELOPE
#     WORD 5    #  9  =POINT()
#     WORD 9    # 10  =CHR$()
#     WORD 5    # 11  =Palette
#     WORD 0    # 12  Palette=
#     WORD 8    # 13  =Coords
#     WORD 25   # 14  =RTC
#     WORD 1    # 15  RTC=
#     WORD 13   # 16  NetTx
#     WORD 13   # 17  NetRx
#     WORD 128  # 18  NetArgs
#     WORD 8    # 19  NetInfo
#     WORD 128  # 20  NetFSOp

# --------------------------------------------------------------

#
# RDLINE - Read a line of text
# ============================
# On entry, a0 = 0
#           a1 = control block
#
# On exit,  a0 = 0
#           a1 = control block
#           a2 = length of returned string
#           Cy=0 ok, Cy=1 Escape
#
# Tube data  &0A block  --  &FF or &7F string &0D
#
RDLINE:
    PUSH    ra
    PUSH    a1
    li      a0, 0x0a
    jal     SendByteR2                  # Send command &0A - RDLINE

    lw      a0, 12(a1)                  # Send <char max>
    jal     SendByteR2
    lw      a0, 8(a1)                   # Send <char min>
    jal     SendByteR2
    lw      a0, 4(a1)                   # Send <buffer len>
    jal     SendByteR2
    li      a0, 0x07                    # Send <buffer addr MSB>
    jal     SendByteR2
    li      a0, 0x00                    # Send <buffer addr LSB>
    jal     SendByteR2
    jal     WaitByteR2                  # Wait for response &FF [escape] or &7F

    mv      a2, zero                    # initialize response length to 0
    li      t0, 0x80                    # test for escape
    bge     a0, t0, RdLineEscape

    lw      a1, (a1)                    # Load the local input buffer from the control block
RdLineLp:
    jal     WaitByteR2                  # Receive a response byte
    sb      a0, (a1)
    addi    a1, a1, 1                   # Increment buffer pointer
    addi    a2, a2, 1                   # Increment count
    li      t0, 0x0d                    # Compare against terminator and loop back
    bne     a0, t0, RdLineLp

    CLC                                 # Clear "carry" to indicate not-escape
    j       RdLineExit

RdLineEscape:
    SEC                                 # Set "carry" to indicate not-escape
RdLineExit:
    POP     a1
    POP     ra
    mv      a0, zero                    # Clear r0 to be tidy
    ret

# -------------------------------------------------------------
# Control block for command prompt input
# --------------------------------------------------------------

osword0_param_block:
     .word INPBUF
     .word INPEND - INPBUF
     .word 0x20
     .word 0xFF

# --------------------------------------------------------------

# osWRCH:
#     PUSH    (r12)
# osWRCH1:
#     IN      (r12, r1status)
#     and     r12, r0, 0x40
#     z.mov   pc, r0, osWRCH1
#     OUT     (r1, r1data)
#     POP     (r12)
#     RTS     ()

osWRCH:
    lw      t0, R1STATUS(gp)
    andi    t0, t0, 0x40
    beq     t0, zero, osWRCH
    sw      a0, R1DATA(gp)
    ret

# --------------------------------------------------------------

# osRDCH:
#     PUSH    (r13)
#     mov     r1, r0        # Send command &00 - OSRDCH
#     JSR     (SendByteR2)
#     JSR     (WaitByteR2)  # Receive carry
#     add     r1, r0, -0x80
#     JSR     (WaitByteR2)  # Receive A
#     POP     (r13)
#     RTS     ()

osRDCH:
    push    ra
    mv      a0, zero                    # Send command &00 - OSRDCH
    jal     SendByteR2
    jal     WaitByteR2                  # Receive carry
                                        # What to do with carry? (escape)
    jal     WaitByteR2                  # Receive A
    pop     ra
    ret

# --------------------------------------------------------------

# -----------------------------------------------------------------------------
# Interrupts handlers
# -----------------------------------------------------------------------------

IRQ1Handler:
    ret
#     IN      (r1, r4status)
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, r4_irq
#     IN      (r1, r1status)
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, r1_irq
#     ld      pc, r0, IRQ2V


# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R1
# -----------------------------------------------------------------------------

# r1_irq:
#     IN      (r1, r1data)
#     cmp     r1, r0, 0x80
#     c.mov   pc, r0, r1_irq_escape

#     PUSH   (r13)          # Save registers
#     PUSH   (r2)
#     PUSH   (r3)
#     JSR    (WaitByteR1)   # Get Y parameter from Tube R1
#     mov    r3, r1
#     JSR    (WaitByteR1)   # Get X parameter from Tube R1
#     mov    r2, r1
#     JSR    (WaitByteR1)   # Get event number from Tube R1
#     JSR    (LFD36)        # Dispatch event
#     POP    (r3)           # restore registers
#     POP    (r2)
#     POP    (r13)

#     ld     r1, r0, TMP_R1 # restore R1 from tmp location
#     rti    pc, pc         # rti

# LFD36:
#     ld     pc, r0, EVNTV

# r1_irq_escape:
#     add    r1, r1
#     sto    r1, r0, ESCAPE_FLAG

#     ld     r1, r0, TMP_R1 # restore R1 from tmp location
#     rti    pc, pc         # rti

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R4
# -----------------------------------------------------------------------------

# r4_irq:

#     IN      (r1, r4data)
#     cmp     r1, r0, 0x80
#     nc.mov  pc, r0, LFD65  # b7=0, jump for data transfer

#
# Error    R4: &FF R2: &00 err string &00
#

#     PUSH    (r2)
#     PUSH    (r13)

#     JSR     (WaitByteR2)   # Skip data in Tube R2 - should be 0x00

#     mov    r2, r0, ERRBUF

#     JSR     (WaitByteR2)   # Get error number
#     sto     r1, r2
#     mov     r2, r2, 1

# err_loop:
#     JSR     (WaitByteR2)   # Get error message bytes
#     sto     r1, r2
#     mov     r2, r2, 1
#     cmp     r1, r0
#     nz.mov  pc, r0, err_loop

#     ERROR   (ERRBUF)

#
# Transfer R4: action ID block sync R3: data
#

# LFD65:
#     PUSH    (r13)
#     PUSH    (r2)           # working register for transfer type
#     PUSH    (r3)           # working register for transfer address
#     mov     r2, r1
#     JSR     (WaitByteR4)
#     cmp     r2, r0, 0x05
#     z.mov   pc, r0, Release
##ifdef CPU_OPC7
#     JSR     (WaitByteR4)   # block address MSB
#     bperm   r3, r1, 0x4404
#     JSR     (WaitByteR4)   # block address ...
#     or      r3, r1
#     bperm   r3, r3, 0x2104
#     JSR     (WaitByteR4)   # block address ...
#     or      r3, r1
#     bperm   r3, r3, 0x2104
#     JSR     (WaitByteR4)   # block address LSB
#     or      r3, r1
##else
#     JSR     (WaitByteR4)   # block address MSB - ignored
#     JSR     (WaitByteR4)   # block address ... - ignored
#     JSR     (WaitByteR4)   # block address ...
#     bswp    r1, r1
#     mov     r3, r1
#     JSR     (WaitByteR4)   # block address LSB
#     or      r3, r1
##endif
#     IN      (r1, r3data)
#     IN      (r1, r3data)

#     JSR     (WaitByteR4)   # sync

#     add     r2, r0, TransferHandlerTable
#     ld      pc, r2

# Release:
#     POP     (r3)
#     POP     (r2)
#     POP     (r13)

#     ld      r1, r0, TMP_R1 # restore R1 from tmp location
#     rti     pc, pc         # rti


# TransferHandlerTable:
#     WORD    Type0
#     WORD    Type1
#     WORD    Type2
#     WORD    Type3
#     WORD    Type4
#     WORD    Release # not actually used
#     WORD    Type6
#     WORD    Type7

# ============================================================
# Type 0 transfer: 1-byte parasite -> host (SAVE)
#
# r1 - scratch register
# r2 - data register (16-bit data value read from memory)
# r3 - address register (16-bit memory address)
# ============================================================

# TODO: we should be able to have one common implementation
# i.e. the 16-bit should be a degenerate case of 32-bit

# Type0:
##ifdef CPU_OPC7

# r2 also tracks when we need load a new word, because
# we cycle a ff through from the MSB. So the continuation
# test sees one of the following:
#    ff 33 22 11     (3 bytes remaining)
#    00 ff 33 22     (2 bytes remaining)
#    00 00 ff 33     (1 bytes remaining)
#    00 00 00 ff     (0 bytes remaining, need to reload)

#     mov     r2, r0                # clear the continuation flag

# Type0_loop:
#     IN      (r1, r4status)        # Test for an pending interrupt signalling end of transfer
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, Release

#     IN      (r1, r3status)        # Wait for Tube R3 free
#     and     r1, r0, 0x40
#     z.mov   pc, r0, Type0_loop

#     bperm   r1, r2, 0x3214        # test continuation flag, more efficient than mov r1, r2 + and r1, r0, 0xffffff00
#     nz.mov  pc, r0, Type0_continuation

#     ld      r2, r3                # Read word from memory, increment memory pointer
#     mov     r3, r3, 1
#     OUT     (r2, r3data)          # Send LSB to Tube R3
#     or      r2, r0, 0xff          # set the continuation flag
#     bperm   r2, r2, 0x0321        # cyclic rotate, r2 = ff 33 22 11
#     mov     pc, r0, Type0_loop

# Type0_continuation:

#     OUT     (r2, r3data)         # Send odd byte to Tube R3
#     bperm   r2, r2, 0x4321       # shift right one byte
#     mov     pc, r0, Type0_loop   # loop back, clearing odd byte flag

##else

#     mov     r2, r0                # clean the odd byte flag (start with an even byte)

# Type0_loop:
#     IN      (r1, r4status)        # Test for an pending interrupt signalling end of transfer
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, Release

#     IN      (r1, r3status)        # Wait for Tube R3 free
#     and     r1, r0, 0x40
#     z.mov   pc, r0, Type0_loop

#     and     r2, r2                # test odd byte flag
#     mi.mov  pc, r0, Type0_odd_byte

#     ld      r2, r3                # Read word from memory, increment memory pointer
#     mov     r3, r3, 1
#     OUT     (r2, r3data)          # Send even byte to Tube R3
#     bswp    r2, r2
#     or      r2, r0, 0x8000        # set the odd byte flag
#     mov     pc, r0, Type0_loop

# Type0_odd_byte:
#     OUT     (r2, r3data)         # Send odd byte to Tube R3
#     mov     pc, r0, Type0        # loop back, clearing odd byte flag

##endif

# ============================================================
# Type 1 transfer: 1-byte host -> parasite (LOAD)
#
# r1 - scratch register
# r2 - data register (16-bit data value read from memory)
# r3 - address register (16-bit memory address)
# ============================================================

# TODO: we should be able to have one common implementation
# i.e. the 16-bit should be a degenerate case of 32-bit

# Type1:
##ifdef CPU_OPC7

#     mov     r2, r0, 0xff          # set the last byte flag in byte 0

# Type1_loop:
#     IN      (r1, r4status)        # Test for an pending interrupt signalling end of transfer
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, Release

#     IN      (r1, r3status)        # Wait for Tube R3 free
#     and     r1, r0, 0x80
#     z.mov   pc, r0, Type1_loop

#     IN      (r1, r3data)          # Read the last byte from Tube T3

#     and     r2, r2                # test last byte flag
#     mi.mov  pc, r0, Type1_last_byte

#     bperm   r2, r2, 0x2104
#     or      r2, r1
#     mov     pc, r0, Type1_loop

# Type1_last_byte:

#     bperm   r2, r2, 0x2104
#     or      r2, r1
#     bperm   r2, r2, 0x0123        # byte swap to make it little endian

#     sto     r2, r3                # Write word to memory, increment memory pointer
#     mov     r3, r3, 1
#     mov     pc, r0, Type1         # loop back, clearing last byte flag

##else

#     mov     r2, r0                # clean the odd byte flag (start with an even byte)

# Type1_loop:
#     IN      (r1, r4status)        # Test for an pending interrupt signalling end of transfer
#     and     r1, r0, 0x80
#     nz.mov  pc, r0, Release

#     IN      (r1, r3status)        # Wait for Tube R3 free
#     and     r1, r0, 0x80
#     z.mov   pc, r0, Type1_loop

#     and     r2, r2                # test odd byte flag
#     mi.mov  pc, r0, Type1_odd_byte

#     IN      (r2, r3data)          # Read the even byte from Tube T3
#     or      r2, r0, 0x8000        # set the odd byte flag
#     mov     pc, r0, Type1_loop

# Type1_odd_byte:

#     IN      (r1, r3data)          # Read the odd byte from Tube T3
#     bswp    r1, r1                # Shift it to the upper byte
#     and     r2, r0, 0x00FF        # Clear the odd byte flag
#     or      r2, r1                # Or the odd byte in the MSB

#     sto     r2, r3                # Write word to memory, increment memory pointer
#     mov     r3, r3, 1
#     mov     pc, r0, Type1         # loop back, clearing odd byte flag

##endif

# Type2:
#     mov     pc, r0, Release

# Type3:
#     mov     pc, r0, Release

# Type4:
#     sto     r3, r0, ADDR
#     mov     pc, r0, Release

# Type6:
#     mov     pc, r0, Release

# Type7:
#     mov     pc, r0, Release

# -----------------------------------------------------------------------------
# Initial interrupt handler, called from 0x0002 (or 0xfffe in PTD)
# -----------------------------------------------------------------------------

# InterruptHandler:
#     sto     r1, r0, TMP_R1
#     GETPSR  (r1)
#     and     r1, r0, SWI_MASK
#     nz.mov  pc, r0, SWIHandler
#     ld      pc, r0, IRQ1V        # invoke the IRQ handler

# SWIHandler:
#     GETPSR  (r1)
#     and     r1, r0, ~SWI_MASK
#     PUTPSR  (r1)
#     ld      r1, r0, TMP_R1       # restore R1 from tmp location
#     sto     r1, r0, LAST_ERR     # save the address of the last error
#     EI      ()                   # re-enable interrupts
#     ld      pc, r0, BRKV         # invoke the BRK handler

##ifdef CPU_OPC5LS

# Limit check to precent code running into next block...

# Limit1:
#     EQU dummy, 0 if (Limit1 < 0xFEF8) else limit1_error

# -----------------------------------------------------------------------------
# TUBE ULA registers
# -----------------------------------------------------------------------------

# ORG TUBE
#     WORD 0x0000     # 0xFEF8
#     WORD 0x0000     # 0xFEF9
#     WORD 0x0000     # 0xFEFA
#     WORD 0x0000     # 0xFEFB
#     WORD 0x0000     # 0xFEFC
#     WORD 0x0000     # 0xFEFD
#     WORD 0x0000     # 0xFEFE
#     WORD 0x0000     # 0xFEFF

##endif

# -----------------------------------------------------------------------------
# DEFAULT VECTOR TABLE
# -----------------------------------------------------------------------------

DefaultVectors:
     .word Unsupported    # &200 - USERV
     .word ErrorHandler   # &202 - BRKV
     .word IRQ1Handler    # &204 - IRQ1V
     .word Unsupported    # &206 - IRQ2V
     .word osCLI          # &208 - CLIV
     .word osBYTE         # &20A - BYTEV
     .word osWORD         # &20C - WORDV
     .word osWRCH         # &20E - WRCHV
     .word osRDCH         # &210 - RDCHV
     .word osFILE         # &212 - FILEV
     .word osARGS         # &214 - ARGSV
     .word osBGET         # &216 - BGetV
     .word osBPUT         # &218 - BPutV
     .word osGBPB         # &21A - GBPBV
     .word osFIND         # &21C - FINDV
     .word Unsupported    # &21E - FSCV
     .word NullReturn     # &220 - EVNTV
     .word Unsupported    # &222 - UPTV
     .word Unsupported    # &224 - NETV
     .word Unsupported    # &226 - VduV
     .word Unsupported    # &228 - KEYV
     .word Unsupported    # &22A - INSV
     .word Unsupported    # &22C - RemV
     .word Unsupported    # &22E - CNPV
     .word NullReturn     # &230 - IND1V
     .word NullReturn     # &232 - IND2V
     .word NullReturn     # &234 - IND3V

# -----------------------------------------------------------------------------
# Helper methods
# -----------------------------------------------------------------------------

# Wait for byte in Tube R1 while allowing requests via Tube R4
# WaitByteR1:
#     IN      (r12, r1status)
#     and     r12, r0, 0x80
#     nz.mov  pc, r0, GotByteR1

#     IN      (r12, r4status)
#     and     r12, r0, 0x80
#     z.mov   pc, r0, WaitByteR1

# TODO
#
# 6502 code uses re-entrant interrups at this point
#
# we'll need to think carefully about this case
#
#LDA $FC             # Save IRQ's A store in A register
#PHP                 # Allow an IRQ through to process R4 request
#CLI
#PLP
#STA $FC             # Restore IRQ's A store and jump back to check R1
#JMP WaitByteR1

# GotByteR1:
#     IN     (r1, r1data)    # Fetch byte from Tube R1 and return
#     RTS    ()

# --------------------------------------------------------------

# WaitByteR2:
#     IN      (r1, r2status)
#     and     r1, r0, 0x80
#     z.mov   pc, r0, WaitByteR2
#     IN      (r1, r2data)
#     RTS     ()

WaitByteR2:
    lw    t0, R2STATUS(gp)
    andi  t0, t0, 0x80
    beqz  t0, WaitByteR2
    lw    a0, R2DATA(gp)
    ret

# --------------------------------------------------------------

# WaitByteR4:
#     IN      (r1, r4status)
#     and     r1, r0, 0x80
#     z.mov   pc, r0, WaitByteR4
#     IN      (r1, r4data)
#     RTS     ()

# --------------------------------------------------------------

# SendByteR2:
#     IN      (r12, r2status)       # Wait for Tube R2 free
#     and     r12, r0, 0x40
#     z.mov   pc, r0, SendByteR2
#     OUT     (r1, r2data)          # Send byte to Tube R2
#     RTS()

SendByteR2:
    lw    t0, R2STATUS(gp)
    andi  t0, t0, 0x40
    beqz  t0, SendByteR2
    sw    a0, R2DATA(gp)
    ret


# --------------------------------------------------------------
#
# print_string
#
# Prints the zero terminated ASCII string
#
# Entry:
# - a1 points to the zero terminated string
#
# Exit:
# - all other registers preserved
#

print_string:
    PUSH    ra
print_string_loop:
    lb      a0, (a1)
    beqz    a0, print_string_exit
    jal     OSWRCH
    addi    a1, a1, 1
    j       print_string_loop
print_string_exit:
    POP     ra
    ret

# --------------------------------------------------------------

# SendStringR2:
#     PUSH    (r13)
#     PUSH    (r2)

# SendStringR2Lp:
#     ld      r1, r2
#     JSR     (SendByteR2)
#     mov     r2, r2, 1
#     cmp     r1, r0, 0x0d
#     nz.mov  pc, r0, SendStringR2Lp
#     POP     (r2)
#     POP     (r13)
#     RTS     ()

# Send 0D terminated string pointed to by a1 to Tube R2

SendStringR2:
    PUSH    ra
SendStringR2Lp:
    lb      a0, (a1)
    jal     SendByteR2
    addi    a1, a1, 1
    li      t0, 0x0d
    bne     a0, t0, SendStringR2Lp
    POP     ra
    ret

# --------------------------------------------------------------

osnewl_code:
    PUSH    ra
    li      a0, 0x0a
    jal     OSWRCH
    li      a0, 0x0d
    jal     OSWRCH
    POP     ra
    ret

# -----------------------------------------------------------------------------
# Messages
# -----------------------------------------------------------------------------

BannerMessage:
   .string "\nRISC-V Co Processor\n\n\r"


# EscapeError:
#     WORD    17
#     STRING "Escape"
#     WORD    0x00

# Currently about 10 words free

# Limit check to precent code running into next block...

# Limit2:
#     EQU dummy, 0 if (Limit2 < 0xFFC8) else limit2_error

# -----------------------------------------------------------------------------
# MOS interface
# -----------------------------------------------------------------------------

# ORG MOS

NVRDCH:                      # &FFC8
     j     osRDCH

# ORG MOS + (0xCB-0xC8)

NVWRCH:                      # &FFCB
     j     osWRCH

# ORG MOS + (0xCE-0xC8)

OSFIND:                      # &FFCE
    JMPI FINDV

# ORG MOS + (0xD1-0xC8)

OSGBPB:                      # &FFD1
    JMPI GBPBV

# ORG MOS + (0xD4-0xC8)

OSBPUT:                      # &FFD4
    JMPI BPUTV

# ORG MOS + (0xD7-0xC8)

OSBGET:                      # &FFD7
    JMPI BGETV

# ORG MOS + (0xDA-0xC8)

OSARGS:                      # &FFDA
    JMPI ARGSV

# ORG MOS + (0xDD-0xC8)

OSFILE:                      # &FFDD
    JMPI FILEV

# ORG MOS + (0xE0-0xC8)

OSRDCH:                      # &FFE0
    JMPI RDCHV

# ORG MOS + (0xE3-0xC8)

OSASCI:                      # &FFE3
    li     t0, 0x0d
    bne    a0, t0, OSWRCH

# ORG MOS + (0xE7-0xC8)

OSNEWL:                      # &FFE7
    j       osnewl_code

# ORG MOS + (0xEC-0xC8)

OSWRCR:                      # &FFEC
    li      a0, 0x0D

# ORG MOS + (0xEE-0xC8)

OSWRCH:                      # &FFEE
    JMPI WRCHV

# ORG MOS + (0xF1-0xC8)

OSWORD:                      # &FFF1
    JMPI WORDV

# ORG MOS + (0xF4-0xC8)

OSBYTE:                      # &FFF4
    JMPI BYTEV

# ORG MOS + (0xF7-0xC8)

OS_CLI:                      # &FFF7
    JMPI CLIV


# -----------------------------------------------------------------------------
# Reset vectors, used by PiTubeDirect
# -----------------------------------------------------------------------------

# ORG MOS + (0xFA-0xC8)

# NMI_ENTRY:                   # &FFFA

# ORG MOS + (0xFC-0xC8)

# RST_ENTRY:                   # &FFFC
#     mov     pc, r0, ResetHandler

# ORG MOS + (0xFE-0xC8)

# IRQ_ENTRY:                   # &FFFE
#     mov     pc, r0, InterruptHandler
