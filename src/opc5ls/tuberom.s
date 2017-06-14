EQU NUM_VECTORS, 27

EQU TMP_R1, 0xfc
EQU ESCAPE_FLAG, 0xff

EQU MEM_TOP, 0xf2

EQU USERV, 0x200
EQU  BRKV, 0x202
EQU IRQ1V, 0x204
EQU IRQ2V, 0x206
EQU  CLIV, 0x208
EQU BYTEV, 0x20A
EQU WORDV, 0x20C
EQU WRCHV, 0x20E
EQU RDCHV, 0x210
EQU FILEV, 0x212
EQU ARGSV, 0x214
EQU BGETV, 0x216
EQU BPUTV, 0x218
EQU GBPBV, 0x21A
EQU FINDV, 0x21C
EQU  FSCV, 0x21E
EQU EVNTV, 0x220

EQU ERRBUF, 0x236
EQU INPBUF, 0x236
EQU INPEND, 0x300

EQU STACK, 0xF7FF

# -----------------------------------------------------------------------------
# TUBE ULA registers
# -----------------------------------------------------------------------------

EQU r1status, 0xFEF8
EQU r1data  , 0xFEF9
EQU r2status, 0xFEFA
EQU r2data  , 0xFEFB
EQU r3status, 0xFEFC
EQU r3data  , 0xFEFD
EQU r4status, 0xFEFE
EQU r4data  , 0xFEFF

# -----------------------------------------------------------------------------
# Macros
# -----------------------------------------------------------------------------

MACRO   CLC()
    add r0,r0
ENDMACRO

MACRO   SEC()
    ror r0,r0,1
ENDMACRO

MACRO   EI()
    psr     r12, psr
    or      r12, r12, 0x0008
    psr     psr, r12
ENDMACRO

MACRO   DI()
    psr     r12, psr
    and      r12, r12, 0xfff7
    psr     psr, r12
ENDMACRO

MACRO JSR( _address_)
   mov      r13, pc, 0x0002
   mov      pc,  r0, _address_
ENDMACRO

MACRO RTS()
    mov     pc, r13
ENDMACRO

MACRO   PUSH( _data_)
    mov     r14, r14, -1
    sto     _data_, r14, 1
ENDMACRO

MACRO   POP( _data_ )
    ld      _data_, r14, 1
    mov     r14, r14, 1
ENDMACRO

# -----------------------------------------------------------------------------
# Code
# -----------------------------------------------------------------------------

ORG 0xF800

Reset:

    mov     r14, r0, STACK              # setup the stack

    mov     r2, r0, NUM_VECTORS         # copy the vectors
    mov     r3, r0, NUM_VECTORS * 2
InitVecLoop:
    ld      r1,  r2, LFF80
    sto     r1,  r3, USERV
    sub     r3,  r0, 2
    sub     r2,  r0, 1
    pl.mov  pc,  r0, InitVecLoop

    EI      ()                          # enable interrupts

    mov     r1, r0, BannerMessage       # send the reset message
    JSR     (print_string)

    mov     r1, r0                      # send the terminator
    JSR     (OSWRCH)

    JSR     (WaitByteR2)                # wait for the response and ignore

CmdPrompt:

CmdOSLoop:
    mov     r1, r0, 0x2a
    JSR     (OSWRCH)

    mov     r1, r0
    mov     r2, r0, osword0_param_block
    JSR     (OSWORD)
    c.mov   pc, r0, CmdOSEscape

    mov     r1, r0, INPBUF
    JSR     (OS_CLI)
    mov     pc, r0, CmdOSLoop

CmdOSEscape:
    mov     r1, r0, 0x7e
    JSR     (OSBYTE)
    mov     r1, r0, EscapeMessage
    JSR     (print_string)
    mov     pc, r0, CmdOSLoop

EscapeMessage:
    STRING "Escape"
    WORD    0x0a, 0x0d, 0x00
    WORD    0x00

BannerMessage:
    WORD    0x0a
    STRING "OPC5LS Co Processor"
    WORD    0x0a, 0x0a, 0x0d, 0x00

# --------------------------------------------------------------
# MOS interface
# --------------------------------------------------------------

NullReturn:
    RTS     ()

# --------------------------------------------------------------

Unsupported:
    RTS     ()

# --------------------------------------------------------------

ErrorHandler:

    mov     r14, r0, STACK              # Clear the stack

    JSR     (OSNEWL)
    mov     r1, r0, ERRBUF + 1          # Print error string
    JSR     (print_string)
    JSR     (OSNEWL)

    mov     pc, r0, CmdPrompt           # Jump to command prompt

# --------------------------------------------------------------

osARGS:
    # TODO
    RTS     ()

# --------------------------------------------------------------

osBGET:
    # TODO
    RTS     ()

# --------------------------------------------------------------

osBPUT:
    # TODO
    RTS     ()

# --------------------------------------------------------------

# OSBYTE - Byte MOS functions
# ===========================
# On entry, r1, r2, r3=OSBYTE parameters
# On exit,  r1  preserved
#           If r1<$80, r2=returned value
#           If r1>$7F, r2, r3, Carry=returned values
#
osBYTE:
    PUSH    (r13)
    cmp     r1, r0, 0x80        # Jump for long OSBYTEs
    c.mov   pc, r0, ByteHigh
#
# Tube data  $04 X A    --  X
#
    PUSH    (r1)
    mov     r1, r0, 0x04        # Send command &04 - OSBYTELO
    JSR     (SendByteR2)
    mov     r1, r2
    JSR     (SendByteR2)        # Send single parameter
    POP     (r1)
    PUSH    (r1)
    JSR     (SendByteR2)        # Send function
    JSR     (WaitByteR2)        # Get return value
    mov     r1, r2
    POP     (r1)
    POP     (r13)
    RTS     ()

ByteHigh:
    cmp     r1, r0, 0x82        # Read memory high word
    z.mov   pc, r0, Byte82
    cmp     r1, r0, 0x83        # Read bottom of memory
    z.mov   pc, r0, Byte83
    cmp     r1, r0, 0x84        # Read top of memory
    z.mov   pc, r0, Byte84
#
# Tube data  $06 X Y A  --  Cy Y X
#

    PUSH    (r1)
    mov     r1, r0, 0x06
    JSR     (SendByteR2)        # Send command &06 - OSBYTEHI
    mov     r2, r1
    JSR     (SendByteR2)        # Send parameter 1
    mov     r3, r1
    JSR     (SendByteR2)        # Send parameter 2
    POP     (r1)
    PUSH    (r1)
    JSR     (SendByteR2)        # Send function
#   cmp     r1, r0, 0x8e        # If select language, check to enter code
#   z.mov   pc, r0, CheckAck
    cmp     r1, r0, 0x9d        # Fast return with Fast BPUT
    z.mov   pc, r0, FastReturn
    JSR     (WaitByteR2)        # Get carry - from bit 7
    add     r1, r0, 0xff80
    JSR     (WaitByteR2)        # Get high byte
    mov     r1, r3
    JSR     (WaitByteR2)        # Get low byte
    mov     r1, r2
FastReturn:
    POP     (r1)                # restore original r1
    POP     (r13)
    RTS     ()

Byte84:                         # Read top of memory from &F2/3
    # TODO - MEM_TOP not populated yet
    ld      r1, r0, MEM_TOP
    POP     (r13)
    RTS     ()
Byte83:                         # Read bottom of memory
    mov     r1, r0, 0x0800
    POP     (r13)
    RTS     ()

Byte82:                         # Return &0000 as memory high word
    mov     r1, r0
    POP     (r13)
    RTS     ()

# --------------------------------------------------------------

# OSCLI - Send command line to host
# =================================
# On entry, r1=>command string
#
# Tube data  &02 string &0D  --  &7F or &80
#

osCLI:
    PUSH    (r13)
    mov     r2, r1
    mov     r1, r0, 0x02            # Send command &02 - OSCLI
    JSR     (SendByteR2)
    JSR     (SendStringR2)          # Send string pointed to by r2

osCLI_Ack:
    JSR     (WaitByteR2)
    cmp     r1, r0, 0x80
    c.mov   pc, r0, enterCode

    POP     (r13)
    RTS     ()

enterCode:

# --------------------------------------------------------------


osFILE:
    # TODO
    RTS     ()

# --------------------------------------------------------------

osFIND:
    # TODO
    RTS     ()

# --------------------------------------------------------------

osGBPB:
    # TODO
    RTS     ()

# --------------------------------------------------------------

osWORD:
    cmp     r1, r0
    z.mov   pc, r0, RDLINE

    # TODO
    RTS     ()

# --------------------------------------------------------------


#
# RDLINE - Read a line of text
# ============================
# On entry, r1 = 0
#           r2 = control block
#
# On exit,  r1 = 0
#           r2 = control block
#           r3 = length of returned string
#           Cy=0 ok, Cy=1 Escape
#
# Tube data  &0A block  --  &FF or &7F string &0D
#

RDLINE:
    PUSH    (r2)
    PUSH    (r13)
    mov     r1, r0, 0x0a
    JSR     (SendByteR2)  # Send command &0A - RDLINE

    ld      r1, r2, 3     # Send <char max>
    JSR     (SendByteR2)
    ld      r1, r2, 2     # Send <char min>
    JSR     (SendByteR2)
    ld      r1, r2, 1     # Send <buffer len>
    JSR     (SendByteR2)
    mov     r1, r0, 0x07  # Send <buffer addr MSB>
    JSR     (SendByteR2)
    mov     r1, r0        # Send <buffer addr LSB>
    JSR     (SendByteR2)
    JSR     (WaitByteR2)  # Wait for response &FF [escape] or &7F
    ld      r3, r0        # initialize response length to 0
    cmp     r1, r0, 0x80  # test for escape
    c.mov   pc, r0, RdLineEscape

    ld      r2, r2        # Load the local input buffer from the control block
RdLineLp:
    JSR     (WaitByteR2)  # Receive a response byte
    sto     r1, r2
    mov     r2, r2, 1     # Increment buffer pointer
    mov     r3, r3, 1     # Increment count
    cmp     r1, r0, 0x0d  # Compare against terminator and loop back
    nz.mov  pc, r0, RdLineLp

    CLC     ()            # Clear carry to indicate not-escape

RdLineEscape:
    POP     (r13)
    POP     (r2)
    mov     r1, r0        # Clear r0 to be tidy
    RTS     ()

-------------------------------------------------------------
# Control block for command prompt input
# --------------------------------------------------------------

osword0_param_block:
    WORD INPBUF
    WORD INPEND - INPBUF
    WORD 0x20
    WORD 0xFF


# --------------------------------------------------------------

osWRCH:
    ld      r12, r0, r1status
    and     r12, r0, 0x40
    z.mov   pc, r0, osWRCH
    sto     r1, r0, r1data
    RTS     ()

# --------------------------------------------------------------

osRDCH:
    mov     r1, r0        # Send command &00 - OSRDCH
    JSR     (SendByteR2)

WaitCarryChar:            # Receive carry and A
    JSR     (WaitByteR2)
    add     r1, r0, 0xff80
    # fall through to...

# --------------------------------------------------------------

WaitByteR2:
    ld      r1, r0, r2status
    and     r1, r0, 0x80
    z.mov   pc, r0, WaitByteR2
    ld      r1, r0, r2data
    RTS     ()

# --------------------------------------------------------------

WaitByteR4:
    ld      r1, r0, r4status
    and     r1, r0, 0x80
    z.mov   pc, r0, WaitByteR4
    ld      r1, r0, r4data
    RTS     ()

# --------------------------------------------------------------

SendByteR2:
    ld      r12, r0, r2status     # Wait for Tube R2 free
    and     r12, r0, 0x40
    z.mov   pc, r0, SendByteR2
    sto     r1, r0, r2data        # Send byte to Tube R2
    RTS()

# --------------------------------------------------------------

SendStringR2:
    PUSH    (r13)
    PUSH    (r2)

SendStringR2Lp:
    ld      r1, r2
    JSR     (SendByteR2)
    mov     r2, r2, 1
    cmp     r1, r0, 0x0d
    nz.mov  pc, r0, SendStringR2Lp
    POP     (r2)
    POP     (r13)
    RTS     ()

# --------------------------------------------------------------
#
# print_string
#
# Prints the zero terminated ASCII string
#
# Entry:
# - r1 points to the zero terminated string
#
# Exit:
# - all other registers preserved

print_string:
    PUSH    (r2)
    PUSH    (r13)
    mov     r2, r1

ps_loop:
    ld      r1, r2
    and     r1, r0, 0xff
    z.mov   pc, r0, ps_exit
    JSR     (OSWRCH)
    mov     r2, r2, 0x0001
    mov     pc, r0, ps_loop

ps_exit:
    POP     (r13)
    POP     (r2)
    RTS     ()

# --------------------------------------------------------------

osnewl_code:
    PUSH    (r13)
    mov     r1, r0, 0x0a
    JSR     (OSWRCH)
    mov     r1, r0, 0x0d
    JSR     (OSWRCH)
    POP     (r13)
    RTS     ()

# -----------------------------------------------------------------------------
# Interrupts handlers
# -----------------------------------------------------------------------------

IRQ1Handler:
    ld      r1, r0, r4status
    and     r1, r0, 0x80
    nz.mov  pc, r0, r4_irq
    ld      r1, r0, r1status
    and     r1, r0, 0x80
    nz.mov  pc, r0, r1_irq
    ld      pc, r0, IRQ2V


# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R1
# -----------------------------------------------------------------------------

r1_irq:
    ld      r1, r0, r1data
    cmp     r1, r0, 0x80
    c.mov   pc, r0, r1_irq_escape

    PUSH   (r13)          # Save registers
    PUSH   (r2)
    PUSH   (r3)
    JSR    (LFE80)        # Get Y parameter from Tube R1
    mov    r3, r1
    JSR    (LFE80)        # Get X parameter from Tube R1
    mov    r2, r1
    JSR    (LFE80)        # Get event number from Tube R1
    JSR    (LFD36)        # Dispatch event
    POP    (r3)           # restore registers
    POP    (r2)
    POP    (r13)

    ld     r1, r0, TMP_R1 # restore R1 from tmp location
    rti    pc, pc         # rti

LFD36:
    ld     pc, r0, EVNTV

r1_irq_escape:
    add    r1, r1
    sto    r1, r0, ESCAPE_FLAG

    ld     r1, r0, TMP_R1 # restore R1 from tmp location
    rti    pc, pc         # rti

# -----------------------------------------------------------------------------
# Interrupt generated by data in Tube R4
# -----------------------------------------------------------------------------

r4_irq:

    ld      r1, r0, r4data
    cmp     r1, r0, 0x80
    nc.mov  pc, r0, LFD65  # b7=0, jump for data transfer

#
# Error    R4: &FF R2: &00 err string &00
#

    PUSH    (r2)
    PUSH    (r13)

    JSR     (WaitByteR2)   # Skip data in Tube R2 - should be 0x00

    mov    r2, r0, ERRBUF

    JSR     (WaitByteR2)   # Get error number
    sto     r1, r2
    mov     r2, r2, 1

err_loop:
    JSR     (WaitByteR2)   # Get error message bytes
    sto     r1, r2
    mov     r2, r2, 1
    cmp     r1, r0
    nz.mov  pc, r0, err_loop

# TODO, at this point the 6502 Client ROM jumps to a BRK which invokes the error handler
#
# That doesn't work for us, because we end up stuck in interrupt context
# (actually, we don't if isrv is ignored)

    # enable interrupts again (not sure if this is the best place...)
    EI      ()

    ld      pc, r0, BRKV
#
# The below also isn't very robust, because the main code I think is waiting for
# for an OSCLI to come back, and it never does. It's just luck (a race condition) that
# means this sometimes works.

#    mov     r1, r0, ERRBUF + 1
#    JSR     (print_string)
#    JSR     (OSNEWL)

#    POP     (r13)
#    POP     (r2)
#    ld      r1, r0, TMP_R1 # restore R1 from tmp location
#    rti     pc, pc         # rti

#
# Transfer R4: action ID block sync R3: data
#

LFD65:
    PUSH    (r13)
    PUSH    (r2)           # working register for transfer type
    PUSH    (r3)           # working register for transfer address
    mov     r2, r1
    JSR     (WaitByteR4)
    cmp     r2, r0, 0x05
    z.mov   pc, r0, release
    JSR     (WaitByteR4)   # block address MSB - ignored for now
    JSR     (WaitByteR4)   # block address ... - ignored for now
    JSR     (WaitByteR4)   # block address ...
    bswp    r1, r1
    mov     r3, r1
    JSR     (WaitByteR4)   # block address LSB
    or      r3, r1

    JSR     (WaitByteR4)   # sync

    add     r2, r0, transfer_handler_table
    ld      pc, r2

release:
    POP     (r3)
    POP     (r2)
    POP     (r13)

    ld      r1, r0, TMP_R1 # restore R1 from tmp location
    rti     pc, pc         # rti


transfer_handler_table:
    WORD    type_0
    WORD    type_1
    WORD    release
    WORD    release
    WORD    release
    WORD    release
    WORD    release
    WORD    release

# ============================================================
# Type 0 transfer: 1-byte parasite -> host (SAVE)
#
# r1 - scratch register
# r2 - data register (16-bit data value read from memory)
# r3 - address register (16-bit memory address)
# ============================================================

type_0:

    mov     r2, r0                # clean the odd byte flag (start with an even byte)

type_0_loop:
    ld      r1, r0, r4status      # Test for an pending interrupt signalling end of transfer
    and     r1, r0, 0x80
    nz.mov  pc, r0, release

    ld      r1, r0, r3status      # Wait for Tube R3 free
    and     r1, r0, 0x40
    z.mov   pc, r0, type_0_loop

    and     r2, r2                # test odd byte flag
    mi.mov  pc, r0, type_0_odd_byte

    ld      r2, r3                # Read word from memory, increment memory pointer
    mov     r3, r3, 1
    sto     r2, r0, r3data        # Send even byte to Tube R3
    bswp    r2, r2
    or      r2, r0, 0x8000        # set the odd byte flag
    mov     pc, r0, type_0_loop

type_0_odd_byte:
    sto     r2, r0, r3data        # Send odd byte to Tube R3
    mov     pc, r0, type_0        # loop back, clearing odd byte flag

# ============================================================
# Type 1 transfer: 1-byte host -> parasite (LOAD)
#
# r1 - scratch register
# r2 - data register (16-bit data value read from memory)
# r3 - address register (16-bit memory address)
# ============================================================

type_1:

    mov     r2, r0                # clean the odd byte flag (start with an even byte)

type_1_loop:
    ld      r1, r0, r4status      # Test for an pending interrupt signalling end of transfer
    and     r1, r0, 0x80
    nz.mov  pc, r0, release

    ld      r1, r0, r3status      # Wait for Tube R3 free
    and     r1, r0, 0x80
    z.mov   pc, r0, type_1_loop

    and     r2, r2                # test odd byte flag
    mi.mov  pc, r0, type_1_odd_byte

    ld      r2, r0, r3data        # Read the even byte from Tube T3
    or      r2, r0, 0x8000        # set the odd byte flag
    mov     pc, r0, type_1_loop

type_1_odd_byte:

    ld      r1, r0, r3data        # Read the odd byte from Tube T3
    bswp    r1, r1                # Shift it to the upper byte
    and     r2, r0, 0x00FF        # Clear the odd byte flag
    or      r2, r1                # Or the odd byte in the MSB

    sto     r2, r3                # Write word to memory, increment memory pointer
    mov     r3, r3, 1
    mov     pc, r0, type_1        # loop back, clearing odd byte flag

# Wait for byte in Tube R1 while allowing requests via Tube R4
# ============================================================
LFE80:
    ld      r12, r0, r1status
    and     r12, r0, 0x80
    nz.mov  pc, r0, LFE94

LFE85:
    ld      r12, r0, r4status
    and     r12, r0, 0x80
    z.mov   pc, r0, LFE80

# 6502 code uses re-entrant interrups at this point
#
# we'll need to think carefully about this case
#
#LDA $FC             # Save IRQ's A store in A register
#PHP                 # Allow an IRQ through to process R4 request
#CLI
#PLP
#STA $FC             # Restore IRQ's A store and jump back to check R1
#JMP LFE80

LFE94:
    ld     r1, r0, r1data    # Fetch byte from Tube R1 and return

    RTS    ()


ORG 0xFF00

InterruptHandler:
    psr     psr, r0   # disable interrupts (this also nukes the SWI bit, but that is broken at the moment)
    sto     r1, r0, TMP_R1
    psr     r1, psr
    and     r1, r0, 0x10
    nz.mov  pc, r0, SWIHandler
    ld      pc, r0, IRQ1V

SWIHandler:
    PUSH   (r13)
    mov    r1, r0, SWIMessage
    JSR    (print_string)
    POP    (r13)
    ld     r1, r0, TMP_R1 # restore R1 from tmp location
    rti    pc, pc         # rti

SWIMessage:
    STRING "SWI!"
    WORD 0x0a, 0x0d, 0x00

# DEFAULT VECTOR TABLE
# ====================

ORG 0xFF80

LFF80:
    WORD Unsupported    # &200 - USERV
    WORD ErrorHandler   # &202 - BRKV
    WORD IRQ1Handler    # &204 - IRQ1V
    WORD Unsupported    # &206 - IRQ2V
    WORD osCLI          # &208 - CLIV
    WORD osBYTE         # &20A - BYTEV
    WORD osWORD         # &20C - WORDV
    WORD osWRCH         # &20E - WRCHV
    WORD osRDCH         # &210 - RDCHV
    WORD osFILE         # &212 - FILEV
    WORD osARGS         # &214 - ARGSV
    WORD osBGET         # &216 - BGetV
    WORD osBPUT         # &218 - BPutV
    WORD osGBPB         # &21A - GBPBV
    WORD osFIND         # &21C - FINDV
    WORD Unsupported    # &21E - FSCV
    WORD NullReturn     # &220 - EVNTV
    WORD Unsupported    # &222 - UPTV
    WORD Unsupported    # &224 - NETV
    WORD Unsupported    # &226 - VduV
    WORD Unsupported    # &228 - KEYV
    WORD Unsupported    # &22A - INSV
    WORD Unsupported    # &22C - RemV
    WORD Unsupported    # &22E - CNPV
    WORD NullReturn     # &230 - IND1V
    WORD NullReturn     # &232 - IND2V
    WORD NullReturn     # &234 - IND3V

ORG 0xFFC8

NVRDCH:                      # &FFC8
    ld      pc, r0, osRDCH
    WORD    0x0000

NVWRCH:                      # &FFCB
    ld      pc, r0, osWRCH
    WORD    0x0000

OSFIND:                      # &FFCE
    ld      pc, r0, FINDV
    WORD    0x0000

OSGBPB:                      # &FFD1
    ld      pc, r0, GBPBV
    WORD    0x0000

OSBPUT:                      # &FFD4
    ld      pc, r0, BPUTV
    WORD    0x0000

OSBGET:                      # &FFD7
    ld      pc, r0, BGETV
    WORD    0x0000

OSARGS:                      # &FFDA
    ld      pc, r0, ARGSV
    WORD    0x0000

OSFILE:                      # &FFDD
    ld      pc, r0, FILEV
    WORD    0x0000

OSRDCH:                      # &FFE0
    ld      pc, r0, RDCHV
    WORD    0x0000

OSASCI:                      # &FFE3
    cmp     r1, r0, 0x0d
    nz.mov  pc, r0, OSWRCH

OSNEWL:                      # &FFE7
    mov     pc, r0, osnewl_code
    WORD    0x0000
    WORD    0x0000
    WORD    0x0000

OSWRCR:                      # &FFEC
    mov     r1, r0, 0x0D

OSWRCH:                      # &FFF1
    ld      pc, r0, WRCHV
    WORD    0x0000

OSWORD:                      # &FFEE
    ld      pc, r0, WORDV
    WORD    0x0000

OSBYTE:                      # &FFF4
    ld      pc, r0, BYTEV
    WORD    0x0000

OS_CLI:                      # &FFF7
    ld      pc, r0, CLIV
    WORD    0x0000

