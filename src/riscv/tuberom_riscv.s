
.equ     MEM_BOT, 0x00000000
.equ     MEM_TOP, 0x00F00000
.equ       STACK, 0x00F7FFFC
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

.equ  NUM_ECALLS, 16

.globl _start

.globl OSWRCH

.macro PUSH reg
    addi    sp, sp, -4
    sw      \reg, 0(sp)
.endm

.macro POP reg
    lw      \reg, 0(sp)
    addi    sp, sp, 4
.endm

.macro JMPI addr
    la      t0, \addr
    lw      t0, (t0)
    jalr    zero, t0
.endm

.macro ERROR address
    la      t0, \address
    la      t1, LAST_ERR
    sw      t0, (t1)
    j       ErrorHandler
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
TMP_DBG:    .word 0 # tmp store for debugging
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

    la      t0, InterruptHandler        # install the interrupt/exception handler
    csrw    mtvec, t0
    li      t0, 1 << 11                 # mie.MEIE=1 (enable external interrupts)
    csrrs   zero, mie, t0
    li      t0, 1 << 3                  # mstatus.MIE=1 (enable interrupts)
    csrrs   zero, mstatus, t0

    la      a0, BannerMessage           # send the reset message

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

    bltz    a2, CmdOSEscape

    la      a0, INPBUF
    jal     OS_CLI
    j       CmdOSLoop

CmdOSEscape:
    li      a0, 0x7e
    jal     OSBYTE
    ERROR   EscapeError

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

    li      sp, STACK                   # setup the stack
    jal     OSNEWL
    la      a0, LAST_ERR
    lw      a0, (a0)
    addi    a0, a0, 1
    jal     print_string
    jal     OSNEWL

    li      t0, 1 << 3                  # mstatus.MIE=1 (enable interrupts)
    csrrs   zero, mstatus, t0

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
# On entry, a0, a1, r2=OSBYTE parameters
# On exit, a0  preserved
#           If a0<$80, a1=returned value
#           If a0>$7F, a1, a2, Carry=returned values

osBYTE:

    PUSH    ra
    li      t0, 0x80                    # Jump for long OSBYTEs
    bge     a0, t0, ByteHigh

# Tube data  $04 X A    --  X

    PUSH    a0
    li      a0, 0x04
    jal     SendByteR2                  # Send command &04 - OSBYTELO
    mv      a0, a1
    jal     SendByteR2                  # Send single parameter
    POP     a0
    PUSH    a0
    jal     SendByteR2                  # Send function
    jal     WaitByteR2                  # Get return value
    mv      a1, a0
    POP     a0
    POP     ra
    ret

ByteHigh:
    li      t0, 0x82
    beq     a0, t0, Byte82
    li      t0, 0x83
    beq     a0, t0, Byte83
    li      t0, 0x84
    beq     a0, t0, Byte84

# Tube data  $06 X Y A  --  Cy Y X

    PUSH    a0
    li      a0, 0x06
    jal     SendByteR2                  # Send command &06 - OSBYTEHI
    mv      a0, a1
    jal     SendByteR2                  # Send parameter 1
    mv      a0, a2
    jal     SendByteR2                  # Send parameter 2
    POP     a0
    PUSH    a0
    jal     SendByteR2                  # Send function
    jal     WaitByteR2                  # Get carry - from bit 7
    mv      a3, a0
    jal     WaitByteR2                  # Get high byte
    mv      a2, a0
    jal     WaitByteR2                  # Get low byte
    mv      a1, a0
    POP     a0
    POP     ra
    ret

Byte84:                         # Read top of memory
    li      a1, MEM_TOP
    POP     ra
    ret

Byte83:                         # Read bottom of memory
    li      a1, MEM_BOT
    POP     ra
    ret

Byte82:                         # Return &0000 as memory high word
    mv      a1, zero
    POP     ra
    ret

# --------------------------------------------------------------

# OSCLI - Send command line to host
# =================================
# On entry, a0=>command string
#
# Tube data  &02 string &0D  --  &7F or &80
#

osCLI:
    PUSH    ra
    PUSH    a0                          # save the string pointer
    jal     cmdLocal                    # try to handle the command locally
    beqz    a0, dontEnterCode           # was command handled locally? (r1 = 0)

    li      a0, 0x02                    # Send command &02 - OSCLI
    jal     SendByteR2

    POP     a0                          # restore the string pointer
    PUSH    a0
    jal     SendStringR2

osCLI_Ack:
    jal     WaitByteR2

    li      t0, 0x80
    blt     a0, t0, dontEnterCode

#     JSR     (prep_env)

#     JSR     (enterCode)

dontEnterCode:
    POP     a0
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
# - a0 points to the user command
#
# On Exit:
# - a0 == 0 if command successfully processed locally
# - a0 != 0 if command should be
#
# Register usage:
# a0 points to start of user command
# a1 points within user command
# a2 points within command string table
# a3 points within command address
# t1 is current character in user command
# t2 is current character in command string table

cmdLocal:
    PUSH    a1
    PUSH    a2
    PUSH    a3
    PUSH    ra

    addi    a0, a0, -1
cmdLoop0:
    addi    a0, a0, 1
    jal     skip_spaces                 # skip leading spaces
    li      t0, '*'
    beq     a1, t0, cmdLoop0            # also skip leading *

    la      a2, cmdStrings-1            # initialize command string pointer (to char before)
    la      a3, cmdAddresses-4          # initialize command address pointer (to word before)

cmdLoop1:
    addi    a1, a0, -1                  # initialize user command pointer (to char before)
    addi    a3, a3, 4                   # move to the next command address

cmdLoop2:
    addi    a1, a1, 1                   # increment user command pointer
    addi    a2, a2, 1                   # increment command table pointer
    lb      t1, (a1)                    # read next character from user command
    ori     t1, t1, 0x20                # convert to lower case
    lb      t2, (a2)                    # read next character from command table
    beqz    t2, cmdCheck                # zero then we are done matching
    beq     t1, t2, cmdLoop2            # if a match, loop back for more

cmdLoop3:                               # skip to the end of the command in the table
    addi    a2, a2, 1
    lb      t2, (a2)
    bnez    t2, cmdLoop3

    li      t0, '.'                     # was the mis-match a '.'
    bne     t1, t0, cmdLoop1            # no, then start again with next command

    addi    a1, a1, 1                   # increment user command pointer past the '.'

cmdExec:
    mv      a0, a1                      # a0 = the command pointer to the params
    lw      a1, (a3)                    # a1 = the execution address
    jal     cmdExecA1

cmdExit:
    POP     ra
    POP     a3
    POP     a2
    POP     a1
    ret

# Additional code to make the match non-greedy
# e.g. *MEMORY should not match against the MEM command
#      also exclude *MEM.

cmdCheck:
    li      t0, '.'                     # check the first non-matching user char against '.'
    beq     t1, t0, cmdReject           # if == '.' then reject the command
    li      t0, 'a'                     # check the first non-matching user char against 'a'
    blt     t1, t0, cmdExec             # if < 'a' then execute the local command
    li      t0, 'z'                     # check the first non-matching user char against 'z'
    bgt     t1, t0, cmdExec             # if >= 'z'+1 then execute the local command
cmdReject:
    li      a0, 1                       # flag command as not handled here then return
    j       cmdExit                     # allowing the command to be handled elsewhere

# --------------------------------------------------------------

cmdGo:
    PUSH    ra
    jal     skip_spaces
    jal     read_hex_8
    jal     cmdExecA1
    mv      a0, zero
    POP     ra
    ret

# --------------------------------------------------------------

cmdHelp:
    PUSH     ra
    la      a0, HelpMessage
    jal     print_string
    li      a0, 1
    POP     ra
    ret

# --------------------------------------------------------------

cmdTest:
    PUSH    ra
    jal     skip_spaces
    jal     read_hex_8

    # Save param
    PUSH    a1

    # Set TIME to using OSWORD 2
    li      a0, 0x02
    la      a1, osword_pblock
    sw      zero, 0(a1)
    sw      zero, 4(a1)
    jal     OSWORD

    # Restore param
    POP     a0
    PUSH    a0

    # Print param (in a1)
    jal     print_hex_word_spc
    jal     print_dec_word
    jal     OSNEWL

    # Restore param
    pop     a0                          # ndigits
    li      a1, 0                       # some memory to use as workspace
    # Do some real work
    jal     pi_calc
    jal     OSNEWL

    # Read TIME using OSWORD 1
    li      a0, 0x01
    la      a1, osword_pblock
    jal     OSWORD

    # Print the LS word in decimal
    la      a1, osword_pblock
    lw      a0, (a1)
    jal     print_dec_word
    jal     OSNEWL

    mv      a0, zero
    POP     ra
    ret

# 8 bytes, to keep everything word aligned

osword_pblock:
    .word 0
    .word 0

# ---------------------------------------------------------

cmdEnd:
    li      a0, 1
    ret

# --------------------------------------------------------------

cmdExecA1:
    jalr   zero, a1

# --------------------------------------------------------------

osFILE:
    # TODO
    ret

# --------------------------------------------------------------

osFIND:
    # TODO
    ret

# --------------------------------------------------------------

osGBPB:
    # TODO
    ret

# --------------------------------------------------------------


# On entry:
#   a0 is the osword number
#   a1 points to the paramater block in memory

osWORD:
    beqz    a0, RDLINE

    PUSH    ra
    PUSH    a0                          # save calling param (osword number)
    PUSH    a1                          # save calling param (block)
    PUSH    a2                          # a2 used as a scratch register
    PUSH    a3                          # a3 used as a scratch register

    mv      a2, a0                      # save osword number
    mv      a3, a1                      # save parameter block
    li      a0, 8
    jal     SendByteR2                  # Send command &08 - OSWORD
    mv      a0, a2
    jal     SendByteR2                  # Send osword number
    li      t0, 0x15                    # Compute index into length table
    blt     a0, t0, osWORD_1
    mv      a2, zero                    # >= OSWORD 0x15, use slot 0
osWORD_1:
    la      a0, word_in_len
    add     a0, a0, a2
    lbu     a0, (a0)                    # a0 = block length
    mv      a1, a3                      # a1 = buffer address

    jal     SendByteR2                  # Send request block length
    jal     SendBlockR2                 # Send request block

    la      a0, word_out_len
    add     a0, a0, a2
    lbu     a0, (a0)                    # a0 = block length
    mv      a1, a3                      # a1 = buffer address

    jal     SendByteR2                  # Send response block length
    jal     ReceiveBlockR2              # Receive response block

    POP     a3
    POP     a2
    POP     a1
    POP     a0
    POP     ra
    ret

# Send a defined size block to tube FIFO R2
# a0 is the length
# a1 is the block address
# The complexity here is the block needs to be sent backwards!

SendBlockR2:
    PUSH    ra
    add     a1, a1, a0                  # calculate address of word containing last byte
    mv      t1, a0                      # use t1 as a loop counter
SendBlockR2lp:
    addi    a1, a1, -1
    beqz    t1, SendBlockDone
    lbu     a0, (a1)
    jal     SendByteR2
    addi    t1, t1, -1
    j       SendBlockR2lp
SendBlockDone:
    POP     ra
    ret

# Receive a defined size block from tube FIFO R2
# a0 is the length
# a1 is the block address
# The complexity here is the block is sent backwards!

ReceiveBlockR2:
    PUSH    ra
    add     a1, a1, a0                  # calculate address of word containing last byte
    mv      t1, a0                      # use t1 as a loop counter
ReceiveBlockR2lp:
    addi    a1, a1, -1
    beqz    t1, ReceiveBlockDone
    jal     WaitByteR2
    sb      a0, (a1)
    addi    t1, t1, -1
    j       ReceiveBlockR2lp
ReceiveBlockDone:
    POP     ra
    ret

word_in_len:
    .byte 16   # OSWORD default
    .byte 0    #  1  =TIME
    .byte 5    #  2  TIME=
    .byte 0    #  3  =IntTimer
    .byte 5    #  4  IntTimer=
    .byte 4    #  5  =IOMEM   JGH: must send full 4-byte address
    .byte 5    #  6  IOMEM=
    .byte 8    #  7  SOUND
    .byte 14   #  8  ENVELOPE
    .byte 4    #  9  =POINT()
    .byte 1    # 10  =CHR$()
    .byte 1    # 11  =Palette
    .byte 5    # 12  Pallette=
    .byte 0    # 13  =Coords
    .byte 8    # 14  =RTC
    .byte 25   # 15  RTC=
    .byte 16   # 16  NetTx
    .byte 13   # 17  NetRx
    .byte 0    # 18  NetArgs
    .byte 8    # 19  NetInfo
    .byte 128  # 20  NetFSOp
    .byte 0    # padding
    .byte 0    # padding
    .byte 0    # padding

word_out_len:
    .byte 16   # OSWORD default
    .byte 5    #  1  =TIME
    .byte 0    #  2  TIME=
    .byte 5    #  3  =IntTimer
    .byte 0    #  4  IntTimer=
    .byte 5    #  5  =IOMEM
    .byte 0    #  6  IOMEM=
    .byte 0    #  7  SOUND
    .byte 0    #  8  ENVELOPE
    .byte 5    #  9  =POINT()
    .byte 9    # 10  =CHR$()
    .byte 5    # 11  =Palette
    .byte 0    # 12  Palette=
    .byte 8    # 13  =Coords
    .byte 25   # 14  =RTC
    .byte 1    # 15  RTC=
    .byte 13   # 16  NetTx
    .byte 13   # 17  NetRx
    .byte 128  # 18  NetArgs
    .byte 8    # 19  NetInfo
    .byte 128  # 20  NetFSOp
    .byte 0    # padding
    .byte 0    # padding
    .byte 0    # padding

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

    j       RdLineExit

RdLineEscape:
    li      a2, -1                      # Set "carry" to indicate not-escape
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

osWRCH:
    lw      t0, R1STATUS(gp)
    andi    t0, t0, 0x40
    beq     t0, zero, osWRCH
    sw      a0, R1DATA(gp)
    ret

# --------------------------------------------------------------

osRDCH:
    PUSH    ra
    mv      a0, zero                    # Send command &00 - OSRDCH
    jal     SendByteR2
    jal     WaitByteR2                  # Receive carry
    mv      a1, a0
    jal     WaitByteR2                  # Receive A
    POP     ra
    ret

# -----------------------------------------------------------------------------
# Interrupts handlers
# -----------------------------------------------------------------------------

ECallHandler:

    # TODO: Check mcause = 11 (machine mode environment call)

    # A7 contains the system call number
    li      t0, NUM_ECALLS
    bgeu    a7, t0, BadECallError

    la      t0, ECallHandlerTable
    add     t0, t0, a7
    add     t0, t0, a7
    add     t0, t0, a7
    add     t0, t0, a7
    lw      t0, (t0)

    beqz    t0, BadECall

    PUSH    ra
    csrr    ra, mstatus                 # push critical machine state
    PUSH    ra
    csrr    ra, mepc
    PUSH    ra

    li      ra, 1 << 3                  # re-enable interrupts
    csrrs   zero, mstatus, ra

    jalr    ra, t0

    POP     ra
    addi    ra, ra, 4                   # return to instruction after the ecall
    csrw    mepc, ra
    POP     ra
    csrw    mstatus, ra
    POP     ra

    POP     t0
    mret

BadECall:
    ERROR   BadECallError

ECallHandlerTable:

#   .word EMT0      # ECALL  0 - QUIT
#   .word _CLI      # ECALL  1 - OSCLI
#   .word _BYTE     # ECALL  2 - OSBYTE
#   .word _WORD     # ECALL  3 - OSWORD
#   .word _WRCH     # ECALL  4 - OSWRCH
#   .word _NEWL     # ECALL  5 - OSNEWL
#   .word _RDCH     # ECALL  6 - OSRDCH
#   .word _FILE     # ECALL  7 - OSFILE
#   .word _ARGS     # ECALL  8 - OSARGS
#   .word _BGET     # ECALL  9 - OSBGET
#   .word _BPUT     # ECALL 10 - OSBPUT
#   .word _GBPB     # ECALL 11 - OSGBPB
#   .word _FIND     # ECALL 12 - OSFIND
#   .word EMT13     # ECALL 13 - System control
#   .word EMT14     # ECALL 14 - Set handlers
#   .word EMT15     # ECALL 15 - ERROR

    .word 0
    .word 0
    .word 0
    .word 0
    .word osWRCH
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0
    .word 0


InterruptHandler:
    PUSH    t0
    csrr    t0, mcause
    bgez    t0, ECallHandler

IRQ1Handler:

    # TODO: Check mcause = 11 (machine external interrupt) otherwise log

    lb      t0, R4STATUS(gp)            # sign extend to 32 bits
    bltz    t0, r4_irq                  # branch if data available in R4

    lb      t0, R1STATUS(gp)            # sign extend to 32 bits
    bltz    t0, r1_irq                  # branch if data available in R1

IRQ2:

    # TODO: Adopt PDP11 IRQV pattern

    j IRQ2
    JMPI    IRQ2V

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R1
# -----------------------------------------------------------------------------

r1_irq:
    lb      t0, R1DATA(gp)
    bltz    t0, r1_irq_escape
    PUSH    ra                          # Save registers
    PUSH    a0
    PUSH    a1
    PUSH    a2
    jal     WaitByteR1                  # Get Y parameter from Tube R1
    mv      a2, a0
    jal     WaitByteR1                  # Get X parameter from Tube R1
    mv      a1, a0
    jal     WaitByteR1                  # Get event number from Tube R1
    jal     LFD36                       # Dispatch event
    POP     a2                          # restore registers
    POP     a1
    POP     a0
    POP     ra
    POP     t0
    mret

LFD36:
    JMPI    EVNTV

r1_irq_escape:
    PUSH    t1
    add     t0, t0, t0
    la      t1, ESCAPE_FLAG
    sb      t0, (t1)
    POP     t1
    POP     t0
    mret

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R4
# -----------------------------------------------------------------------------

r4_irq:
    lb      t0, R4DATA(gp)
    bgez    t0, LFD65                   # b7=0, jump for data transfer

# Error    R4: &FF R2: &00 err string &00

    PUSH    ra
    PUSH    a0
    PUSH    t1
    jal     WaitByteR2                  # Skip data in Tube R2 - should be 0x00
    la      t1, ERRBUF
    jal     WaitByteR2                  # Get error number
    sb      a0, (t1)
err_loop:
    addi    t1, t1, 1
    jal     WaitByteR2                  # Get error message bytes
    sb      a0, (t1)
    bnez    a0, err_loop

    ERROR   ERRBUF

#
# Transfer R4: action ID block sync R3: data
#

LFD65:
    PUSH    ra
    PUSH    a0
    PUSH    t1           # working register for transfer type
    PUSH    t2           # working register for transfer address

    mv      t1, t0       # save transfer type

    jal     WaitByteR4
    li      t0, 0x05
    beq     t1, t0, Release
    jal     WaitByteR4   # block address MSB
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4   # block address ...
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4   # block address ...
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4   # block address LSB
    slli    t2, t2, 8
    or      t2, t2, a0
    lw      t0, R3DATA(gp)
    lw      t0, R3DATA(gp)
    jal     WaitByteR4   # sync

    la      t0, TransferHandlerTable
    slli    t1, t1, 2
    add     t0, t0, t1
    lw      t0, (t0)
    jalr    zero, t0

Release:
    POP     t2
    POP     t1
    POP     a0
    POP     ra
    POP     t0
    mret

TransferHandlerTable:
    .word    Type0
    .word    Type1
    .word    Type2
    .word    Type3
    .word    Type4
    .word    Release    # not actually used
    .word    Type6
    .word    Type7

# ============================================================
# Type 0 transfer: 1-byte parasite -> host (SAVE)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type0:
    lb      t0, R4STATUS(gp)        # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    andi    t0, t0, 0x40
    beqz    t0, Type0
    lb      t0, (t2)
    sw      t0, R3DATA(gp)
    addi    t2, t2, 1
    j       Type0


# ============================================================
# Type 1 transfer: 1-byte host -> parasite (LOAD)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type1:
    lb      t0, R4STATUS(gp)        # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    bgez    t0, Type1
    lw      t0, R3DATA(gp)
    sb      t0, (t2)
    addi    t2, t2, 1
    j       Type1

Type2:
    # TODO
    j       Release

Type3:
    # TODO
    j       Release

Type4:
    la      t0, ADDR
    sw      t2, (t0)
    j       Release

Type6:
    # TODO
    j       Release

Type7:
    # TODO
    j       Release


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


# --------------------------------------------------------------
#
# print_hex_word
#
# Prints a word-sized hex value
#
# Entry:
# - a0 is the value to be printed
#
# Exit:
# - all registers preserved

print_hex_word:

    PUSH    ra
    PUSH    a0                          # preserve working registers
    PUSH    a1
    PUSH    a2

    mv      a1, a0                      # a1 is now the value to be printed
    li      a2, 28                      # a2 is a loop counter for digits

print_hex_word_loop:
    srl     a0, a1, a2
    jal     print_hex_1
    addi    a2, a2, -4                  # decrement the loop counter and loop back for next digits
    bgez    a2, print_hex_word_loop

    POP     a2                          # restore working registers
    POP     a1
    POP     a0
    POP     ra
    ret

# --------------------------------------------------------------
#
# print_hex_1
#
# Prints a 1-digit hex value
#
# Entry:
# - a0 is the value to be printed
#
# Exit:
# - all registers preserved

print_hex_1:
    PUSH    ra
    andi    a0, a0, 0x0F                    # mask off everything but the bottom nibble
    li      t0, 0x0A
    blt     a0, t0, print_hex_0_9
    addi    a0, a0, 'A'-'9'-1
print_hex_0_9:
    add     a0, a0, '0'
    jal     OSWRCH
    POP     ra
    ret

# --------------------------------------------------------------
#
# print_hex_word_spc
#
# Prints a 4-digit hex value followed by a space
#
# Entry:
# - a0 is the value to be printed
#
# Exit:
# - all registers preserved

print_hex_word_spc:
    PUSH    ra
    jal     print_hex_word
    jal     print_spc
    POP     ra
    ret

# --------------------------------------------------------------
#
# print_spc
#
# Prints a space
#
# Entry:
# - a0 is the value to be printed
#
# Exit:
# - all registers preserved

print_spc:
    PUSH    ra
    PUSH    a0
    li      a0, 0x20
    jal     OSWRCH
    POP     a0
    POP     ra
    ret

# --------------------------------------------------------------
#
# print_dec_word
#
# Prints a word sized value as decimal
#
# Entry:
# - a0 is the value to be printed
#
# Exit:
# - all registers preserved


print_dec_word:
    PUSH    ra
    PUSH    a0
    PUSH    a1
    PUSH    a2
    PUSH    a3
    PUSH    a4

    mv      a4, zero                     # flag to manage supressing of leading zeros
    mv      a1, a0
    la      a2, DecTable - 4

print_dec_word_1:
    addi    a2, a2, 4
    lw      a3, (a2)
    beqz    a3, print_dec_word_5

    mv      a0, zero
print_dec_word_2:
    bltu    a1, a3, print_dec_word_3
    addi    a0, a0, 1
    sub     a1, a1, a3
    j       print_dec_word_2

print_dec_word_3:
    li      t0, 1                       # force printing of the last digit
    beq     a3, t0, print_dec_word_4
    add     a4, a4, a0                  # supress leading zeros
    beqz    a4, print_dec_word_1

print_dec_word_4:
    addi    a0, a0, 0x30
    jal     OSWRCH
    j       print_dec_word_1

print_dec_word_5:
    POP     a4
    POP     a3
    POP     a2
    POP     a1
    POP     a0
    POP     ra
    ret

DecTable:
    .word    1000000000
    .word     100000000
    .word      10000000
    .word       1000000
    .word        100000
    .word         10000
    .word          1000
    .word           100
    .word            10
    .word             1
    .word             0

# --------------------------------------------------------------
# skip_space
#
# Entry:
# - a0 is the address of the string
#
# Exit:
# - a0 is updated to skip and spaces
# - a1 is non-space character

skip_spaces:
    addi    a0, a0, -1
skip_spaces_loop:
    addi    a0, a0, 1
    lb      a1, (a0)
    li      t0, 0x20
    beq     a1, t0, skip_spaces_loop
    ret

# --------------------------------------------------------------
#
# read_hex_8
#
# Read a 8-digit hex value
#
# Entry:
# - a0 is the address of the hex string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - carry set if there was an error
# - all registers preserved

read_hex_8:
    PUSH    ra
    mv      a1, zero          # a1 is will contain the hex value
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    POP     ra
    ret

# --------------------------------------------------------------
#
# read_hex_4
#
# Read a 4-digit hex value
#
# Entry:
# - a0 is the address of the hex string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - carry set if there was an error
# - all registers preserved

read_hex_4:
    PUSH    ra
    mv      a1, zero          # a1 is will contain the hex value
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    jal     read_hex_1
    POP     ra
    ret

# --------------------------------------------------------------
#
# read_hex_2
#
# Read a 2-digit hex value
#
# Entry:
# - a0 is the address of the hex string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - carry set if there was an error
# - all registers preserved

read_hex_2:
    PUSH    ra
    mv      a1, zero                    # a1 will contain the hex value
    jal     read_hex_1
    jal     read_hex_1
    POP     ra
    ret

# --------------------------------------------------------------
#
# read_hex_1
#
# Read a 1-digit hex value
#
# Entry:
# - a0 is the address of the hex string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - carry set if there was an error
# - all registers preserved

read_hex_1:
    PUSH    a2
    lb      a2, (a0)
    li      t0, '0'
    blt     a2, t0, read_hex_1_invalid
    li      t0, '9'
    ble     a2, t0, read_hex_1_valid
    andi    a2, a2, 0xdf
    li      t0, 'A'
    blt     a2, t0, read_hex_1_invalid
    li      t0, 'F'
    bgt     a2, t0, read_hex_1_invalid
    addi    a2, a2, -('A'-'9'-1)

read_hex_1_valid:
    slli    a1, a1, 4
    andi    a2, a2, 0x0F
    add     a1, a1, a2

    addi    a0, a0, 1
#   CLC                                 # TODO
    POP     a2
    ret

read_hex_1_invalid:
#   SEC                                 # TODO
    POP     a2
    ret

# Wait for byte in Tube R1 while allowing requests via Tube R4
#
# Used during the event R1 IRQ to service R4 IRQs in a timely manner

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

WaitByteR1:
    lb      t0, R1STATUS(gp)
    bgez    t0, WaitByteR1
    lw      a0, R1DATA(gp)
    ret

# --------------------------------------------------------------

WaitByteR2:
    lb    t0, R2STATUS(gp)
    bgez  t0, WaitByteR2
    lw    a0, R2DATA(gp)
    ret

# --------------------------------------------------------------

WaitByteR4:
    lb    t0, R4STATUS(gp)
    bgez  t0, WaitByteR4
    lw    a0, R4DATA(gp)
    ret

# --------------------------------------------------------------

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
# - a0 points to the zero terminated string
#
# Exit:
# - all other registers preserved
#

print_string:
    PUSH    ra
    mv      t0, a0
print_string_loop:
    lb      a0, (t0)
    beqz    a0, print_string_exit
    PUSH    t0
    jal     OSWRCH
    POP     t0
    addi    t0, t0, 1
    j       print_string_loop
print_string_exit:
    POP     ra
    ret

# --------------------------------------------------------------

# Send 0D terminated string pointed to by a0 to Tube R2

SendStringR2:
    PUSH    ra
    PUSH    a1
    mv      a1, a0
SendStringR2Lp:
    lb      a0, (a1)
    jal     SendByteR2
    addi    a1, a1, 1
    li      t0, 0x0d
    bne     a0, t0, SendStringR2Lp
    POP     a1
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
# Command Table
# -----------------------------------------------------------------------------

cmdAddresses:
    .word    cmdEnd
    .word    cmdGo
    .word    cmdHelp
    .word    cmdTest
    .word    cmdEnd

cmdStrings:
    .string  "."
    .string  "go"
    .string  "help"
    .string  "test"
    .byte 0

# -----------------------------------------------------------------------------
# Messages
# -----------------------------------------------------------------------------

BannerMessage:
   .string "\nRISC-V Co Processor\n\n\r"


EscapeError:
    .byte 17
    .string "Escape"

BadECallError:
    .byte 255                           # re-use "Bad" error code
    .string "Bad ECall"

HelpMessage:
    .string "RISC-V 0.10\n\r"
