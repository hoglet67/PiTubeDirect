.equ     VERSION, 0x0020

.equ     MEM_BOT, 0x00000000
.equ     MEM_TOP, 0x00F80000
.equ       STACK, MEM_TOP - 16         # 16 byte aligned
.equ        TUBE, 0x00FFFFE0

.equ    R1STATUS, 0
.equ      R1DATA, 4
.equ    R2STATUS, 8
.equ      R2DATA, 12
.equ    R3STATUS, 16
.equ      R3DATA, 20
.equ    R4STATUS, 24
.equ      R4DATA, 28

.equ NUM_HANDLERS    , 12               # number of vectors in DefaultHandlers table

.equ NUM_ECALLS      , 16

.equ OS_QUIT         ,  0
.equ OS_CLI          ,  1
.equ OS_BYTE         ,  2
.equ OS_WORD         ,  3
.equ OS_WRCH         ,  4
.equ OS_NEWL         ,  5
.equ OS_RDCH         ,  6
.equ OS_FILE         ,  7
.equ OS_ARGS         ,  8
.equ OS_BGET         ,  9
.equ OS_BPUT         , 10
.equ OS_GBPB         , 11
.equ OS_FIND         , 12
.equ OS_SYS_CTRL     , 13
.equ OS_HANDLERS     , 14
.equ OS_ERROR        , 15

.equ BUFSIZE         , 0x80             # size of the Error buffer and Input Buffer

.globl _start

# Macros to maintain 16-byte stack alignment at all times

.macro PUSH1 reg1
    addi    sp, sp, -16
    sw      \reg1, 12(sp)
.endm

.macro PUSH2 reg1, reg2
    addi    sp, sp, -16
    sw      \reg1, 12(sp)
    sw      \reg2,  8(sp)
.endm

.macro PUSH3 reg1, reg2, reg3
    addi    sp, sp, -16
    sw      \reg1, 12(sp)
    sw      \reg2,  8(sp)
    sw      \reg3,  4(sp)
.endm

.macro PUSH4 reg1, reg2, reg3, reg4
    addi    sp, sp, -16
    sw      \reg1, 12(sp)
    sw      \reg2,  8(sp)
    sw      \reg3,  4(sp)
    sw      \reg4,  0(sp)
.endm

.macro POP1 reg1
    lw      \reg1, 12(sp)
    addi    sp, sp, 16
.endm

.macro POP2 reg1, reg2
    lw      \reg1, 12(sp)
    lw      \reg2,  8(sp)
    addi    sp, sp, 16
.endm

.macro POP3 reg1, reg2, reg3
    lw      \reg1, 12(sp)
    lw      \reg2,  8(sp)
    lw      \reg3,  4(sp)
    addi    sp, sp, 16
.endm

.macro POP4 reg1, reg2, reg3, reg4
    lw      \reg1, 12(sp)
    lw      \reg2,  8(sp)
    lw      \reg3,  4(sp)
    lw      \reg4,  0(sp)
    addi    sp, sp, 16
.endm

.macro SYS number
    li      a7, \number
    ecall
.endm

.section .text

_start:
    j       ResetHandler

ESCFLG:     .word 0                     # escape flag

ARGSBLK:    .word 0                     # block used for os args

#
# Handlers are patterned on JGH's pdp11 client ROM:
# https://mdfs.net/Software/Tube/PDP11/Tube11.src
#

Handlers:

EXITV:      .word 0                     # Address of exit handler
EXITADDR:   .word 0                     # unused
ESCV:       .word 0                     # Address of escape handler
ESCADDR:    .word 0                     # Address of escape flag
ERRV:       .word 0                     # Address of error handler
ERRADDR:    .word 0                     # Address of error buffer
EVENTV:     .word 0                     # Address of event handler
EVENTADDR:  .word 0                     # unused
IRQV:       .word 0                     # Address of unknown IRQ handler
IRQADDR:    .word 0                     # Tube Execution Address
ECALLV:     .word 0                     # Old SP within ECall handler
ECALLADDR:  .word 0                     # Address of ECall dispatch table

    .align  8,0

ERRBLK:
    .zero   BUFSIZE

CLIBUF:
    .zero   BUFSIZE

ResetHandler:
    li      sp, STACK                   # setup the stack

    la      t1, DefaultHandlers         # copy the handlers
    la      t2, Handlers
    li      t3, NUM_HANDLERS - 1
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
    SYS     OS_WRCH

    li      gp, TUBE
    jal     WaitByteR2                  # wait for the response and ignore
    li      gp, 0

DefaultExitHandler:
    li      sp, STACK                   # reset the stack - TODO: what else?
    li      t0, 1 << 3                  # enable interrupts
    csrrs   zero, mstatus, t0

CmdOSLoop:
    li      a0, 0x2a
    SYS     OS_WRCH

    mv      a0, zero
    la      a1, osword0_param_block
    SYS     OS_WORD

    bltz    a2, CmdOSEscape

    la      a0, CLIBUF
    SYS     OS_CLI

    j       CmdOSLoop

osword0_param_block:
     .word CLIBUF
     .word BUFSIZE
     .word 0x20
     .word 0xFF

CmdOSEscape:
    li      a0, 0x7e
    SYS     OS_BYTE
    SYS     OS_ERROR
    .byte   17
    .string "Escape"

BannerMessage:
    .string "\nRISC-V Co Processor\n\n\r"

    .align  2,0

# -----------------------------------------------------------------------------
# DEFAULT Handler TABLE
# -----------------------------------------------------------------------------

DefaultHandlers:
    .word   DefaultExitHandler
    .word   VERSION
    .word   DefaultEscapeHandler
    .word   ESCFLG
    .word   DefaultErrorHandler
    .word   ERRBLK
    .word   DefaultEventHandler
    .word   0
    .word   DefaultIRQHandler
    .word   0
    .word   DefaultECallHandler
    .word   ECallHandlerTable

# --------------------------------------------------------------
# Default Error Handler
#
# On Entry:
#   a0 points to the error block

DefaultErrorHandler:
    li      sp, STACK                   # setup the stack
    SYS     OS_NEWL
    addi    a0, a0, 1
    jal     print_string
    SYS     OS_NEWL
    j       DefaultExitHandler

# --------------------------------------------------------------
# Default Escape Handler
#
# On Entry:
#   a0 is the escape flag value byte as transferred over the tube (b7=1 b6=flag)

DefaultEscapeHandler:
    add     a0, a0, a0
    la      t0, ESCADDR                 # ESCADDR holds the address of the escape flag
    lw      t0, (t0)                    # so an extra indirection is required
    sb      a0, (t0)
    ret

# --------------------------------------------------------------
# Default Event Handler
#
# On Entry:
#   a0 is the event number    (A)
#   a1 is the first parameter (X)
#   a2 is the first parameter (Y)

DefaultEventHandler:
    ret

# --------------------------------------------------------------
# Default Interrupt Handler
#
# This is called when an unrecognised interrupt is detected

DefaultIRQHandler:
    ret

# --------------------------------------------------------------
# MOS interface
# --------------------------------------------------------------

ECallHandlerTable:
    .word   osQUIT                      # ECALL  0
    .word   osCLI                       # ECALL  1
    .word   osBYTE                      # ECALL  2
    .word   osWORD                      # ECALL  3
    .word   osWRCH                      # ECALL  4
    .word   osNEWL                      # ECALL  5
    .word   osRDCH                      # ECALL  6
    .word   osFILE                      # ECALL  7
    .word   osARGS                      # ECALL  8
    .word   osBGET                      # ECALL  9
    .word   osBPUT                      # ECALL 10
    .word   osGBPB                      # ECALL 11
    .word   osFIND                      # ECALL 12
    .word   osSYSCTRL                   # ECALL 13
    .word   osHANDLERS                  # ECALL 14
    .word   osERROR                     # ECALL 15

# --------------------------------------------------------------
# ECall  0 - OSQUIT - Exit current program
# On entry:
#     no parameters
# On exit:
#     this call does not return
# --------------------------------------------------------------

osQUIT:
    la      t0, EXITV
    lw      t0, (t0)
    jalr    zero, t0

# --------------------------------------------------------------
# ECall  1 - OSCLI - Send command line to host
# --------------------------------------------------------------
# On entry:
#     a0: pointer to command string, terminated by 0D
# On exit (for a command that runs on the host):
#     t0-t3: undefined, all other registers preserved
# On exit (for a program that runs on the parasite):
#     a0: program result
#     t0-t3: undefined, all other registers as set by the program
# --------------------------------------------------------------

osCLI:
    PUSH2   ra, a0

    jal     cmdLocal                    # try to handle the command locally
    beqz    a0, dontEnterCode           # was command handled locally? (r1 = 0)

# Tube data  &02 string &0D  --  &7F or &80

    li      a0, 0x02                    # Send command &02 - OSCLI
    jal     SendByteR2

    lw      a0, 8(sp)                   # restore the string pointer a0
    jal     SendStringR2

osCLI_Ack:
    jal     WaitByteR2

    li      t0, 0x80
    blt     a0, t0, dontEnterCode

    lw      a0, 8(sp)                   # restore the string pointer a0
    jal     prep_env

    la      t0, IRQADDR
    lw      t0, (t0)
    jalr    ra, t0

dontEnterCode:
    POP2    ra, a0
    ret

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

prep_env:
    PUSH1   ra
    addi    a0, a0, -1
prep_env_1:                                 # skip leading space or * characters
    addi    a0, a0, 1
    jal     skip_spaces
    li      t0, '*'
    beq     a1, t0, prep_env_1

    li      t0, '/'
    beq     a1, a0, prep_env_4

    la      a1, run_string - 1
    addi    a2, a0, -1
prep_env_2:                                 # skip a possibly abbreviated RUN
    addi    a1, a1, 1
    lbu     a3, (a1)                        # Read R U N <0>
    beqz    a3, prep_env_3
    addi    a2, a2, 1
    lbu     a4, (a2)
    li      t0, '.'
    beq     a4, t0, prep_env_3
    andi    a4, a4, 0xdf                    # force upper case
    bne     a3, a4, prep_env_5              # match against R U N
    j        prep_env_2                     # loop back for more characters

prep_env_3:
    mv      a0, a2

prep_env_4:
    addi    a0, a0, 1

prep_env_5:
    jal     skip_spaces
    POP1    ra
    ret

run_string:
    .string "RUN"

# --------------------------------------------------------------
# ECall  2 - OSBYTE - Call MOS OSBYTE function
# --------------------------------------------------------------
# On entry:
#     a0: OSBYTE A parameter (see AUG)
#     a1: OSBYTE X parameter (see AUG)
#     a2: OSBYTE Y parameter (see AUG)
# On exit:
#     a0: preserved
#     a1: OSBYTE X result
#     a2: OSBYTE Y result (if a0 >= 0x80, otherwise preserved)
#     a3: OSBYTE C result (if a0 >= 0x80, otherwise preserved)
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osBYTE:

    PUSH2   ra, a0
    li      t0, 0x80                    # Jump for long OSBYTEs
    bge     a0, t0, ByteHigh

# Tube data  $04 X A    --  X

    li      a0, 0x04
    jal     SendByteR2                  # Send command &04 - OSBYTELO
    mv      a0, a1
    jal     SendByteR2                  # Send single parameter

    lw      a0, 8(sp)                   # OSBYTE parameter a0
    jal     SendByteR2                  # Send function
    jal     WaitByteR2                  # Get return value
    mv      a1, a0

ByteReturn:
    POP2    ra, a0
    ret

ByteHigh:
    li      t0, 0x82
    beq     a0, t0, Byte82
    li      t0, 0x83
    beq     a0, t0, Byte83
    li      t0, 0x84
    beq     a0, t0, Byte84

# Tube data  $06 X Y A  --  Cy Y X

    li      a0, 0x06
    jal     SendByteR2                  # Send command &06 - OSBYTEHI
    mv      a0, a1
    jal     SendByteR2                  # Send parameter 1
    mv      a0, a2
    jal     SendByteR2                  # Send parameter 2
    lw      a0, 8(sp)                   # OSBYTE parameter a0
    jal     SendByteR2                  # Send function
    jal     WaitByteR2                  # Get carry - from bit 7
    mv      a3, a0
    jal     WaitByteR2                  # Get high byte
    mv      a2, a0
    jal     WaitByteR2                  # Get low byte
    mv      a1, a0
    j       ByteReturn

Byte84:                                 # Read top of memory
    li      a1, MEM_TOP
    j       ByteReturn

Byte83:                                 # Read bottom of memory
    li      a1, MEM_BOT
    j       ByteReturn

Byte82:                                 # Return &0000 as memory high word
    mv      a1, zero
    j       ByteReturn

# --------------------------------------------------------------
# ECall  3 - OSWORD - Call MOS OSWORD function
# --------------------------------------------------------------
# On entry:
#     a0: OSWORD number (see AUG)
#     a1: pointer to OSWORD block (see AUG)
# On exit:
#     a0: preserved
#     a1: preserved, OSWORD block updated with response data
#     t0-t3: undefined, all other registers preserved
#
# In addition, for OSWORD 0:
#     a2: response length, or -1 if input terminated by escape
# --------------------------------------------------------------

osWORD:
    beqz    a0, RDLINE

    PUSH4   ra, a0, a1, a2              # save calling params: a0->8(sp) and a1->4(sp)

    li      a0, 8
    jal     SendByteR2                  # Send command &08 - OSWORD
    lw      a0, 8(sp)
    jal     SendByteR2                  # Send osword number

    li      t0, 0x15                    # Compute index into length table
    mv      a2, a0                      # osword number
    blt     a2, t0, osWORD_1
    mv      a2, zero                    # >= OSWORD 0x15, use slot 0
osWORD_1:
    la      a0, word_in_len
    add     a0, a0, a2
    lbu     a0, (a0)                    # a0 = block length
    lw      a1, 4(sp)                   # a1 = buffer address

    jal     SendByteR2                  # Send request block length
    jal     SendBlockR2                 # Send request block

    la      a0, word_out_len
    add     a0, a0, a2
    lbu     a0, (a0)                    # a0 = block length
    lw      a1, 4(sp)                   # a1 = buffer address

    jal     SendByteR2                  # Send response block length
    jal     ReceiveBlockR2              # Receive response block

    POP4    ra, a0, a1, a2
    ret

# Send a defined size block to tube FIFO R2
# a0 is the length
# a1 is the block address
# The complexity here is the block needs to be sent backwards!

SendBlockR2:
    PUSH1   ra
    add     a1, a1, a0                  # a1 = block + length
    mv      t1, a0                      # use t1 as a loop counter
SendBlockR2lp:
    addi    a1, a1, -1
    beqz    t1, SendBlockDone
    lbu     a0, (a1)
    jal     SendByteR2
    addi    t1, t1, -1
    j       SendBlockR2lp
SendBlockDone:
    POP1    ra
    ret

# Receive a defined size block from tube FIFO R2
# a0 is the length
# a1 is the block address
# The complexity here is the block is sent backwards!

ReceiveBlockR2:
    PUSH1   ra
    add     a1, a1, a0                  # a1 = block + length
    mv      t1, a0                      # use t1 as a loop counter
ReceiveBlockR2lp:
    addi    a1, a1, -1
    beqz    t1, ReceiveBlockDone
    jal     WaitByteR2
    sb      a0, (a1)
    addi    t1, t1, -1
    j       ReceiveBlockR2lp
ReceiveBlockDone:
    POP1    ra
    ret

word_in_len:
    .byte 16                            # OSWORD default
    .byte 0                             #  1  =TIME
    .byte 5                             #  2  TIME=
    .byte 0                             #  3  =IntTimer
    .byte 5                             #  4  IntTimer=
    .byte 4                             #  5  =IOMEM   JGH: must send full 4-byte address
    .byte 5                             #  6  IOMEM=
    .byte 8                             #  7  SOUND
    .byte 14                            #  8  ENVELOPE
    .byte 4                             #  9  =POINT()
    .byte 1                             # 10  =CHR$()
    .byte 1                             # 11  =Palette
    .byte 5                             # 12  Pallette=
    .byte 0                             # 13  =Coords
    .byte 8                             # 14  =RTC
    .byte 25                            # 15  RTC=
    .byte 16                            # 16  NetTx
    .byte 13                            # 17  NetRx
    .byte 0                             # 18  NetArgs
    .byte 8                             # 19  NetInfo
    .byte 128                           # 20  NetFSOp
    .byte 0                             # padding
    .byte 0                             # padding
    .byte 0                             # padding

word_out_len:
    .byte 16                            # OSWORD default
    .byte 5                             #  1  =TIME
    .byte 0                             #  2  TIME=
    .byte 5                             #  3  =IntTimer
    .byte 0                             #  4  IntTimer=
    .byte 5                             #  5  =IOMEM
    .byte 0                             #  6  IOMEM=
    .byte 0                             #  7  SOUND
    .byte 0                             #  8  ENVELOPE
    .byte 5                             #  9  =POINT()
    .byte 9                             # 10  =CHR$()
    .byte 5                             # 11  =Palette
    .byte 0                             # 12  Palette=
    .byte 8                             # 13  =Coords
    .byte 25                            # 14  =RTC
    .byte 1                             # 15  RTC=
    .byte 13                            # 16  NetTx
    .byte 13                            # 17  NetRx
    .byte 128                           # 18  NetArgs
    .byte 8                             # 19  NetInfo
    .byte 128                           # 20  NetFSOp
    .byte 0                             # padding
    .byte 0                             # padding
    .byte 0                             # padding

# --------------------------------------------------------------
# ECall  3 continued - OSWORD0 - Read a line of text
# --------------------------------------------------------------
# On entry, a0 = 0
#           a1 = block
#
# On exit,  a0 = 0
#           a1 = block
#           a2 = length of returned string, or -1 to indicate escape
#
# Tube data  &0A block  --  &FF or &7F string &0D
# --------------------------------------------------------------
RDLINE:
    PUSH2   ra, a1
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
    POP2    ra, a1
    mv      a0, zero                    # Clear r0 to be tidy
    ret

# --------------------------------------------------------------
# ECall  4 - OSWRCH - Write character to output stream
# --------------------------------------------------------------
# On entry:
#     a0: character to output
# On exit:
#     a0: preserved
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osWRCH:
    lb      t0, R1STATUS(gp)
    andi    t0, t0, 0x40
    beq     t0, zero, osWRCH
    sb      a0, R1DATA(gp)
    ret

# --------------------------------------------------------------
# ECall  5 - OSNEWL - Write <NL><CR> to output stream
# --------------------------------------------------------------
# On entry:
#     no parameters
# On exit:
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osNEWL:
    PUSH2   ra, a0
    li      a0, 0x0a
    jal     osWRCH
    li      a0, 0x0d
    jal     osWRCH
    POP2    ra, a0
    ret

# --------------------------------------------------------------
# ECall  6 - OSRDCH - Wait for character from input stream
# --------------------------------------------------------------
# On entry:
#     no parameters
# On exit:
#     a0: character read from input, or negative if escape pressed
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osRDCH:
    PUSH1   ra
    #  Tube data: &00  --  Carry Char
    mv      a0, zero                    # Send command &00 - OSRDCH
    jal     SendByteR2
    jal     WaitByteR2                  # Receive carry
    mv      t1, a0
    jal     WaitByteR2                  # Receive A
    andi    t1, t1, 0x80
    beqz    t1, rdch_done
    ori     a0, a0, 0xffffff00          # negative indicates escape (or other error)
rdch_done:
    POP1    ra
    ret

# --------------------------------------------------------------
# ECall  7 - OSFILE - Read/write a whole files or its attributes
# --------------------------------------------------------------
# On entry:
#     a0: function (see AUG)
#     a1: pointer to filename, terminated by zero
#     a2: pointer to 16-byte block containing:
#            load  address (4 bytes)
#            exec  address (4 bytes)
#            start address (4 bytes)
#            end   address (4 bytes)
# On exit:
#     a0: result
#     a1: preserved
#     a2: preserved, block updated with response data
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osFILE:
    # Tube data: &14 block string &0D A            A block
    PUSH4   ra, a0, a1, a2
    li      a0, 0x14
    jal     SendByteR2                  # Send command &14 - OSFILE
    li      a0, 16
    mv      a1, a2
    jal     SendBlockR2
    lw      a0, 4(sp)                   # filename
    jal     SendStringR2
    lw      a0, 8(sp)                   # reason
    jal     SendByteR2
    jal     WaitByteR2
    sw      a0, 8(sp)                   # result
    li      a0, 16
    mv      a1, a2
    jal     ReceiveBlockR2
    POP4    ra, a0, a1, a2
    ret

# --------------------------------------------------------------
# ECall  8 - OSARGS - Read/write an open files's arguments
# --------------------------------------------------------------
# On entry:
#     a0: function (see AUG)
#     a1: file handle
#     a2: 32-bit value to read/write (Note: NOT a block pointer)
# On exit:
#     a0: preserved (except for a0=0 a1=0, where its the FS number)
#     a1: preserved
#     a2: 32-bit value to read/write (updated for a read operation)
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osARGS:
    # Tube data: &0C Y block A                     A block
    PUSH3   ra, a0, a1
    li      a0, 0x0C
    jal     SendByteR2                  # 0x0c
    mv      a0, a1
    jal     SendByteR2                  # handle
    li      a0, 4
    la      a1, ARGSBLK
    sw      a2, (a1)
    jal     SendBlockR2                 # control block
    lw      a0, 8(sp)
    jal     SendByteR2                  # function
    jal     WaitByteR2                  # result
    sw      a0, 8(sp)
    li      a0, 4
    la      a1, ARGSBLK
    jal     ReceiveBlockR2              # block
    la      a1, ARGSBLK
    lw      a2, (a1)
    POP3    ra, a0, a1
    ret

# --------------------------------------------------------------
# ECall  9 - OSBGET - Get a byte from open file
# --------------------------------------------------------------
# On entry:
#     a1: file handle
# On exit:
#     a0: byte read (0..255), or -1 if EOF reached
#     a1: preserved
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osBGET:
    # Tube data: &0E Y                             Cy A
    PUSH1   ra
    li      a0, 0x0e
    jal     SendByteR2                  # 0x0e
    mv      a0, a1
    jal     SendByteR2                  # Y

    jal     WaitByteR2                  # Cy
    mv      t1, a0
    jal     WaitByteR2                  # A
    andi    t1, t1, 0x80
    beqz    t1, bget_done
    li      a0, -1                      # -1 indicates EOF reached
bget_done:
    POP1    ra
    ret

# --------------------------------------------------------------
# ECall 10 - OSBPUT - Put a byte to an open file
# --------------------------------------------------------------
# On entry:
#     a0: byte to write
#     a1: file handle
# On exit:
#     a0: preserved
#     a1: preserved
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osBPUT:
    # Tube data: &10 Y A                           &7F
    PUSH2   ra, a0
    li      a0, 0x10
    jal     SendByteR2                  # 0x10
    mv      a0, a1
    jal     SendByteR2                  # Y
    lw      a0, 8(sp)
    jal     SendByteR2                  # A
    jal     WaitByteR2
    POP2    ra, a0
    ret

# --------------------------------------------------------------
# ECall 11 - OSGBPB - Read or write multiple bytes
# --------------------------------------------------------------
# On entry:
#     a0: function (see AUG)
#     a1: pointer to 13-byte block containing:
#            file handle                 (1 byte)
#            start address of data       (4 bytes)
#            number of bytes to transfer (4 bytes)
#            sequential file pointer     (4 bytes)
# On exit:
#     a0: preserved
#     a1: preserved, block updated with response data
#     a2: result (0 = ok; 1 = fail, same a C on 6502 API)
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

# TODO: Review this API
#   use a1 for file handle?
#   use a0 for result?

osGBPB:
    # Tube data: &16 block A                       block Cy A
    PUSH3   ra, a0, a1
    li      a0, 0x16
    jal     SendByteR2                  # 0x16
    li      a0, 13
    jal     SendBlockR2                 # control block
    lw      a0, 8(sp)
    jal     SendByteR2                  # function

    li      a0, 13
    lw      a1, 4(sp)
    jal     ReceiveBlockR2              # control block
    jal     WaitByteR2                  # cy
    srli    a0, a0, 7
    mv      a2, a0
    jal     WaitByteR2                  # result in a0
    sw      a0, 8(sp)                   # store result in stack frame so it's returned in a0
    POP3    ra, a0, a1
    ret

# --------------------------------------------------------------
# ECall 12 - OSFIND - Open or Close a file
# --------------------------------------------------------------
# On entry:
#     a0: function (see AUG)
#     a1: file handle                          (if close file), or
#         pointer to filename terminated by 0D (if open file)
# On exit:
#     a0=zero or handle
#     t0-t3: undefined, all other registers preserved
# --------------------------------------------------------------

osFIND:
    # Tube data: &12 A string &0D                  A
    PUSH2   ra, a0
    li      a0, 0x12
    jal     SendByteR2                      # 0x12
    lw      a0, 8(sp)
    jal     SendByteR2                      # function
    beqz    a0, osfind_close
    mv      a0, a1
    jal     SendStringR2                    # filename
    jal     WaitByteR2                      # A
    j       osfind_exit

osfind_close:
    # Tube data: &12 &00 Y                         &7F
    mv      a0, a1
    jal     SendByteR2                      # handle
    jal     WaitByteR2
    li      a0, 0

osfind_exit:
    sw      a0, 8(sp)                   # store result in stack frame so it's returned in a0
    POP2    ra, a0
    ret

# --------------------------------------------------------------
# ECall 13 - OS SYSTEM CONTROL - Miscellaneous
# --------------------------------------------------------------
#
# TODO: this call is not implemented yet
#
# On entry:
#     a0: function
# On exit:
#     a0: result
#     t0-t3: undefined, all other registers preserved
# Only one function is currently defined:
#     a0 = &01: setup new program enviroment. This must be called by
#     code that wants to become the current program instead of being
#     transient code. The current program is re-entered at Soft Break.
# --------------------------------------------------------------

osSYSCTRL:
    # TODO
    ret

# --------------------------------------------------------------
# ECall 14 - OS HANDLERS - Reads/writes environment handlers
#                          or EMT dispatch table entries.
# --------------------------------------------------------------
#
# TODO: this call is only partly implemented
#
# a0 >= 0: Reads or writes EMT dispatch address
#
# On entry:
#     a0: EMT number
#     a1: address of EMT routine, or zero to read
# On exit:
#     a0: preserved
#     a1: previous EMT dispatch address
#
# a0 < 0: Reads or writes environment handler:
#
# On entry:
#     a0: Environment handler number
#     a1: address of environment hander or zero to read
#     a2: address of environment data block or zero to read
# On exit:
#     a0: preserved
#     a1: previous environment handler address
#     a2: previous environment data address
#
# Environment handler numbers are:
#     a0     a1                   a2
#     &FFFF  Exit handler         version
#     &FFFE  Escape handler       Escape flag (one byte)
#     &FFFD  Error handler        Error buffer (256 bytes)
#     &FFFC  Event handler        unused
#     &FFFB  Unknown IRQ handler  (used during data transfer)
#     &FFFA  (used during EMT)    EMT dispatch table (512 bytes)
#
# Internal handlers:
#     &FFF9  LPTR                 ADDRHI
#     &FFF8  MEMBOT               MEMTOP
#     &FFFA  ADDR                 TRANS
#     &FFFB  PROG                 MISC+ESCFLG
#
# The Exit handler is entered with a0=return value.
#
# The Escape handler is entered with a0=new escape state in
# b6, must preserve all registers other than a0 and return
# with RTS PC.
#
# The Error handler is entered with a0=>error block. Note that
# this may not be the address of the error buffer, the error
# buffer is used for dynamically generated error messages.
#
# The Event handler is entered with a0,a1,a2 holding the event
# parameters, must preserve all registers, and return with RTS PC.
#
# The Unknown IRQ handler must preserve all registers, and
# return with RTI.
# --------------------------------------------------------------

osHANDLERS:
    li      t0, 0xfffa
    bltu    a0, t0, oshdone
    li      t0, 0xffff
    bgtu    a0, t0, oshdone

    sub     a0, t0, a0                  # a0: 0,1,2,3,4,5
    slli    a0, a0, 3                   # a0: 0,8,16,24,32,40
    la      t0, Handlers
    add     a0, a0, t0                  # a0: entry in handlers table

    lw      t0, 0(a0)
    beqz    a1, skip_write_a1
    sw      a1, 0(a0)
skip_write_a1:
    mv      a1, t0

    lw      t0, 4(a0)
    beqz    a2, skip_write_a2
    sw      a2, 4(a0)
skip_write_a2:
    mv      a2, t0

oshdone:
    ret

# --------------------------------------------------------------
# ECall 15 - OS ERROR - invoke the current error handler
# --------------------------------------------------------------
# On entry:
#     the error block should follow the ecall instruction
# On exit:
#     this call does not return
# --------------------------------------------------------------

osERROR:
    lw     a0, 12(sp)                   # 12(sp) holds the stacked mepc which is the ecall address
    addi   a0, a0, 4                    # step past the ecall instruction
    la     t0, ERRV
    lw     t0, (t0)
    addi   t0, t0, -4
    sw     t0, 12(sp)                   # force ecall handler to return to the error handler
    ret                                 # rather than the original user program

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
    PUSH4   ra, a1, a2, a3

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
    jalr    ra, a1                      # execute the command

cmdExit:
    POP4    ra, a1, a2, a3
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

# -----------------------------------------------------------------------------
# Command Table
# -----------------------------------------------------------------------------

cmdAddresses:
    .word   cmdEnd
    .word   cmdGo
    .word   cmdHelp
    .word   cmdTest
    .word   cmdPi
    .word   cmdEnd

cmdStrings:
    .string "."
    .string "go"
    .string "help"
    .string "test"
    .string "pi"
    .byte   0
    .align  2,0

# ---------------------------------------------------------

cmdEnd:
    li      a0, 1
    ret

# --------------------------------------------------------------

cmdGo:
    PUSH1   ra
    jal     read_hex
    beqz    a2, BadAddress
    jalr    ra, a1
    mv      a0, zero
    POP1    ra
    ret

BadAddress:
    SYS     OS_ERROR
    .byte   252
    .string "Bad address"
    .align  2,0

# --------------------------------------------------------------

cmdHelp:
    PUSH1   ra
    la      a0, HelpMessage
    jal     print_string
    li      a0, 1
    POP1    ra
    ret

# TODO: Version should come from VERSION definition

HelpMessage:
    .string "RISC-V 0.20\n\r"
    .align  2,0

# --------------------------------------------------------------

cmdTest:
    PUSH1   ra
    jal     read_hex
    beqz    a2, BadAddress
    mv      a0, a1
    jal     print_hex_word
    li      a0, ' '
    SYS     OS_WRCH
    mv      a0, a1
    jal     print_dec_word
    SYS     OS_NEWL
    mv      a0, zero
    POP1    ra
    ret

# --------------------------------------------------------------

cmdPi:
    PUSH2   ra, a1

    jal     read_dec
    beqz    a2, BadNumber
    sw      a1, 8(sp)

    # Set TIME to using OSWORD 2
    li      a0, 0x02
    la      a1, osword_pblock
    sw      zero, 0(a1)
    sw      zero, 4(a1)
    SYS     OS_WORD

    # Restore param
    lw      a0, 8(sp)

    # Print param (in a1)
    jal     print_dec_word
    SYS     OS_NEWL

    # Restore param
    lw      a0, 8(sp)

    li      a1, 0                       # some memory to use as workspace
    # Do some real work
    jal     pi_calc
    SYS     OS_NEWL

    # Read TIME using OSWORD 1
    li      a0, 0x01
    la      a1, osword_pblock
    SYS     OS_WORD

    # Print the LS word in decimal
    la      a1, osword_pblock
    lw      a0, (a1)
    jal     print_dec_word
    SYS     OS_NEWL

    mv      a0, zero
    POP2    ra, a1
    ret

BadNumber:
    SYS     OS_ERROR
    .byte   252
    .string "Bad number"
    .align  2,0

osword_pblock:
    .word 0
    .word 0

# -----------------------------------------------------------------------------
# Exception handler
# -----------------------------------------------------------------------------

DefaultECallHandler:

    # TODO: Check mcause = 11 (machine mode environment call)

    # A7 contains the system call number
    li      t0, NUM_ECALLS
    bgeu    a7, t0, BadECall

    la      t0, ECallHandlerTable
    add     t0, t0, a7
    add     t0, t0, a7
    add     t0, t0, a7
    add     t0, t0, a7
    lw      t0, (t0)

    beqz    t0, BadECall

    csrr    t1, mepc                    # push critical machine state
    csrr    t2, mstatus
    PUSH2   t1, t2

    li      ra, 1 << 3                  # re-enable interrupts
    csrrs   zero, mstatus, ra

    jalr    ra, t0

    POP2    t1, t2
    addi    t1, t1, 4                   # return to instruction after the ecall
    csrw    mepc, t1
    csrw    mstatus, t2

    sw      a0, 8(sp)                   # store the result on the a0 slot on the stack
    J       InterruptHandlerExit

BadECall:
    csrr    a0, mepc
    jal     print_hex_word
    li      a0, ':'
    SYS     OS_WRCH
    mv      a0, a7
    jal     print_hex_word
    li      a0, ':'
    SYS     OS_WRCH
    SYS     OS_ERROR
    .byte   255                         # re-use "Bad" error code
    .string "Bad ECall"
    .align  2,0

# -----------------------------------------------------------------------------
# Uncaught Exception Handler
# -----------------------------------------------------------------------------

UncaughtExceptionHandler:
    csrr    a0, mepc
    mv      a1, t0
    jal     print_hex_word
    li      a0, ':'
    SYS     OS_WRCH
    mv      a0, a1
    jal     print_hex_word
    li      a0, ':'
    SYS     OS_WRCH
    SYS     OS_ERROR
    .byte   255                         # re-use "Bad" error code
    .string "Uncaught Exception"
    .align  2,0

# -----------------------------------------------------------------------------
# Interrupt handler
# -----------------------------------------------------------------------------

InterruptHandler:
    PUSH4   ra, a0, t0, gp

    li      gp, TUBE                    # setup a register that points to the tube
    csrr    t0, mcause
    addi    t0, t0, -11
    beqz    t0, DefaultECallHandler     # TODO indirect through ecall vector??
    csrr    t0, mcause
    bgez    t0, UncaughtExceptionHandler

    # TODO: Check mcause = 11 (machine external interrupt) otherwise log

    lb      t0, R4STATUS(gp)            # sign extend to 32 bits
    bltz    t0, r4_irq                  # branch if data available in R4

    lb      t0, R1STATUS(gp)            # sign extend to 32 bits
    bltz    t0, r1_irq                  # branch if data available in R1

    la      t0, IRQV
    lw      t0, (t0)
    jalr    ra, t0

InterruptHandlerExit:
    POP4    ra, a0, t0, gp
    mret

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R1
# -----------------------------------------------------------------------------

r1_irq:
    lb      t0, R1DATA(gp)
    bltz    t0, r1_irq_escape

    PUSH2   a1, a2                      # save registers
    jal     WaitByteR1                  # Get Y parameter from Tube R1
    mv      a2, a0
    jal     WaitByteR1                  # Get X parameter from Tube R1
    mv      a1, a0
    jal     WaitByteR1                  # Get event number from Tube R1
    la      t0, EVENTV
    lw      t0, (t0)
    jalr    ra, t0                      # indirect through the event vector
    POP2    a1, a2                      # restore registers

    j       InterruptHandlerExit

r1_irq_escape:
    mv      a0, t0                      # copy escape flag into a0
    la      t0, ESCV
    lw      t0, (t0)
    jalr    ra, t0                      # indirect call to the escape vector

    j       InterruptHandlerExit

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R4
# -----------------------------------------------------------------------------

r4_irq:
    lb      t0, R4DATA(gp)
    bgez    t0, r4_datatransfer         # b7=0, jump for data transfer

# Error    R4: &FF R2: &00 err string &00

    PUSH1   t1                          # save registers
    jal     WaitByteR2                  # Skip data in Tube R2 - should be 0x00
    la      t1, ERRADDR                 # ERRADDR is the address of the error buffer
    lw      t1, (t1)                    # so an extra level of indirection is required
    jal     WaitByteR2                  # Get error number
    sb      a0, (t1)
err_loop:
    addi    t1, t1, 1
    jal     WaitByteR2                  # Get error message bytes
    sb      a0, (t1)
    bnez    a0, err_loop
    POP1    t1                          # restore registers

    la      a0, ERRADDR                 # ERRADDR is the address of the error buffer
    lw      a0, (a0)                    # so an extra level of indirection is required
    sw      a0, 8(sp)                   # and store it on the stack to be restored as a0 by InterruptHandlerExit

    la      t0, ERRV
    lw      t0, (t0)
    csrw    mepc, t0                    # replace interrupt return address with that of error handler

    j       InterruptHandlerExit

#
# Transfer R4: action ID block sync R3: data
#

r4_datatransfer:
    PUSH2   t1, t2                      # save registers

    mv      t1, t0                      # save transfer type

    jal     WaitByteR4
    li      t0, 0x05
    beq     t1, t0, Release
    jal     WaitByteR4                  # block address MSB
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4                  # block address ...
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4                  # block address ...
    slli    t2, t2, 8
    or      t2, t2, a0
    jal     WaitByteR4                  # block address LSB
    slli    t2, t2, 8
    or      t2, t2, a0
    lb      t0, R3DATA(gp)
    lb      t0, R3DATA(gp)
    jal     WaitByteR4                  # sync

    la      t0, TransferHandlerTable
    slli    t1, t1, 2
    add     t0, t0, t1
    lw      t0, (t0)
    jalr    zero, t0

Release:
    POP2    t1, t2                      # restore registers
    j       InterruptHandlerExit

TransferHandlerTable:
    .word   Type0
    .word   Type1
    .word   Type2
    .word   Type3
    .word   Type4
    .word   Release                     # not actually used
    .word   Type6
    .word   Type7

# ============================================================
# Type 0 transfer: 1-byte parasite -> host (SAVE)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type0:
    lb      t0, R4STATUS(gp)            # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    andi    t0, t0, 0x40
    beqz    t0, Type0
    lb      t0, (t2)
    sb      t0, R3DATA(gp)
    addi    t2, t2, 1
    j       Type0

# ============================================================
# Type 1 transfer: 1-byte host -> parasite (LOAD)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type1:
    lb      t0, R4STATUS(gp)            # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    bgez    t0, Type1
    lb      t0, R3DATA(gp)
    sb      t0, (t2)
    addi    t2, t2, 1
    j       Type1

# ============================================================
# Type 2 transfer: 2-byte parasite -> host (SAVE)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type2:
    lb      t0, R4STATUS(gp)            # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    andi    t0, t0, 0x40
    beqz    t0, Type2
    lh      t0, (t2)                    # load half word from memory
    sb      t0, R3DATA(gp)              # store lo byte to tube
    srli    t0, t0, 8
    sb      t0, R3DATA(gp)              # store hi byte to tube
    addi    t2, t2, 2
    j       Type2

# ============================================================
# Type 3 transfer: 2-byte host -> parasite (LOAD)
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type3:
    lb      t0, R4STATUS(gp)            # Test for an pending interrupt signalling end of transfer
    bltz    t0, Release
    lb      t0, R3STATUS(gp)
    bgez    t0, Type3
    lb      t0, R3DATA(gp)
    sb      t0, (t2)                    # store lo byte to memory
    lb      t1, R3DATA(gp)
    sb      t0, 1(t2)                   # store hi byte to memory
    addi    t2, t2, 2
    j       Type3

# ============================================================
# Type 4 transfer: Execute Code
#
# t0 - scratch register
# t2 - address register (memory address)
# ============================================================

Type4:
    la      t0, IRQADDR
    sw      t2, (t0)
    j       Release

# ============================================================
# Type 6 transfer: 256-byte parasite -> host
#
# t0 - scratch register
# t1 - loop counter
# t2 - address register (memory address)
# ============================================================

Type6:
    li      t1, 0x100
Type6lp:
    lb      t0, R3STATUS(gp)
    andi    t0, t0, 0x40
    beqz    t0, Type6lp
    lb      t0, (t2)
    sb      t0, R3DATA(gp)
    addi    t2, t2, 1
    addi    t1, t1, -1
    bnez    t1, Type6lp
Type6sync:
    lb      t0, R3STATUS(gp)
    andi    t0, t0, 0x40
    beqz    t0, Type6sync
    sb      zero, R3DATA(gp)
    j       Release

# ============================================================
# Type 7 transfer: 256-byte host -> parasite
#
# t0 - scratch register
# t1 - loop counter
# t2 - address register (memory address)
# ============================================================

Type7:
    li      t1, 0x100
Type7lp:
    lb      t0, R3STATUS(gp)
    bgez    t0, Type7lp
    lb      t0, R3DATA(gp)
    sb      t0, (t2)
    addi    t2, t2, 1
    addi    t1, t1, -1
    bnez    t1, Type7lp
    j       Release


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
    PUSH4   ra, a0, a1, a2              # save working registers

    mv      a1, a0                      # a1 is now the value to be printed
    li      a2, 28                      # a2 is a loop counter for digits

print_hex_word_loop:
    srl     a0, a1, a2
    jal     print_hex_1
    addi    a2, a2, -4                  # decrement the loop counter and loop back for next digits
    bgez    a2, print_hex_word_loop

    POP4    ra, a0, a1, a2              # restore working registers
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
    PUSH1   ra
    andi    a0, a0, 0x0F                # mask off everything but the bottom nibble
    li      t0, 0x0A
    blt     a0, t0, print_hex_0_9
    addi    a0, a0, 'A'-'9'-1
print_hex_0_9:
    add     a0, a0, '0'
    SYS     OS_WRCH
    POP1    ra
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
    PUSH4   ra, a0, a1, a2
    PUSH2   a3, a4

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
    SYS     OS_WRCH
    j       print_dec_word_1

print_dec_word_5:
    POP2    a3, a4
    POP4    ra, a0, a1, a2
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
# read_hex
#
# Read a hex value
#
# Entry:
# - a0 is the address of the hex string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - a2 contains the number of digits processed
# - all registers preserved

read_hex:
    PUSH1   ra
    jal     skip_spaces
    mv      a1, zero                    # a1 will contain the hex value
    mv      a2, zero                    # a2 will contain the number of digtis read
read_hex_lp:
    lb      t1, (a0)
    li      t0, '0'
    blt     t1, t0, read_hex_done
    li      t0, '9'
    ble     t1, t0, read_hex_valid
    andi    t1, t1, 0xdf
    li      t0, 'A'
    blt     t1, t0, read_hex_done
    li      t0, 'F'
    bgt     t1, t0, read_hex_done
    addi    t1, t1, -('A'-'9'-1)

read_hex_valid:
    slli    a1, a1, 4
    andi    t1, t1, 0x0F
    add     a1, a1, t1
    addi    a0, a0, 1
    addi    a2, a2, 1
    j       read_hex_lp

read_hex_done:
    POP1    ra
    ret

# --------------------------------------------------------------
#
# read_dec
#
# Read a decimal value
#
# Entry:
# - a0 is the address of the decimal string
#
# Exit:
# - a0 is updated after processing the string
# - a1 contains the hex value
# - a2 contains the number of digits processed
# - all registers preserved

read_dec:
    PUSH1   ra
    jal     skip_spaces
    mv      a1, zero                    # a1 will contain the dec value
    mv      a2, zero                    # a2 will contain the number of digtis read
read_dec_lp:
    lb      t1, (a0)
    li      t0, '0'
    blt     t1, t0, read_dec_done
    li      t0, '9'
    bgt     t1, t0, read_dec_done

read_dec_valid:
    li      t0, 10
    mul     a1, a1, t0
    andi    t1, t1, 0x0F
    add     a1, a1, t1
    addi    a0, a0, 1
    addi    a2, a2, 1
    j       read_dec_lp

read_dec_done:
    POP1    ra
    ret

# --------------------------------------------------------------

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
    lbu     a0, R1DATA(gp)
    ret

# --------------------------------------------------------------

WaitByteR2:
    lb    t0, R2STATUS(gp)
    bgez  t0, WaitByteR2
    lbu   a0, R2DATA(gp)
    ret

# --------------------------------------------------------------

WaitByteR4:
    lb    t0, R4STATUS(gp)
    bgez  t0, WaitByteR4
    lbu   a0, R4DATA(gp)
    ret

# --------------------------------------------------------------

SendByteR2:
    lw    t0, R2STATUS(gp)
    andi  t0, t0, 0x40
    beqz  t0, SendByteR2
    sb    a0, R2DATA(gp)
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
    PUSH2   ra, a1
    mv      a1, a0
print_string_loop:
    lb      a0, (a1)
    beqz    a0, print_string_exit
    SYS     OS_WRCH
    addi    a1, a1, 1
    j       print_string_loop
print_string_exit:
    POP2    ra, a1
    ret

# --------------------------------------------------------------

# Send 0D terminated string pointed to by a0 to Tube R2

SendStringR2:
    PUSH2   ra, a1
    mv      a1, a0
SendStringR2Lp:
    lb      a0, (a1)
    jal     SendByteR2
    addi    a1, a1, 1
    li      t0, 0x0d
    bne     a0, t0, SendStringR2Lp
    POP2    ra, a1
    ret
