EQU NUM_VECTORS, 27

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


ORG 0xF800

reset:

    mov     r14, r0, STACK              # setup the stack

    mov     r2, r0, NUM_VECTORS         # copy the vectors
    mov     r3, r0, NUM_VECTORS * 2
init_vec_loop:
    ld      r1,  r2, LFF80
    sto     r1,  r3, USERV
    sub     r3,  r0, 2
    sub     r2,  r0, 1
    pl.mov  pc,  r0, init_vec_loop


    mov     r1, r0, banner              # send the reset message
    JSR     (print_string)

    mov     r1, r0                      # send the terminator
    JSR     (OSWRCH)

    JSR     (WaitByte)                  # wait for the response and ignore

cmd_prompt:

cmd_os_loop:
    mov     r1, r0, 0x2a
    JSR     (OSWRCH)

    mov     r1, r0
    mov     r2, r0, osword0_param_block
    JSR     (OSWORD)

    mov     r1, r0, INPBUF
    JSR     (OS_CLI)
    mov     pc, r0, cmd_os_loop

banner:
    WORD    0x4f0a
    BSTRING "PC5LS Co Processor"
    WORD    0x0a0a
    WORD    0x000d

# --------------------------------------------------------------

SendCommand:
SendByte:
        ld      r12, r0, r2status     # Wait for Tube R2 free
        and     r12, r0, 0x40
        z.mov   pc, r0, SendByte
        sto     r1, r0, r2data        # Send byte to Tube R2
        RTS()


SendStringR2:
        PUSH    (r13)
        PUSH    (r2)
SendStringLp:
        ld      r1, r2
        JSR     (SendByte)
        mov     r2, r2, 1
        cmp     r1, r0, 0x0d
        nz.mov  pc, r0, SendStringLp
        POP     (r2)
        POP     (r13)
        RTS     ()

# --------------------------------------------------------------
# MOS interface
# --------------------------------------------------------------

ErrorHandler:
    RTS     ()

IRQ1Handler:
    RTS     ()

osARGS:
    RTS     ()

osBGET:
    RTS     ()

osBPUT:
    RTS     ()

osBYTE:
    RTS     ()

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
    JSR     (SendCommand)
    JSR     (SendStringR2)          # Send string pointed to by r2

osCLI_Ack:
    JSR     (WaitByte)
    cmp     r1, r0, 0x80
    c.mov   pc, r0, enterCode

enterCode:
    POP     (r13)
    RTS     ()


osFILE:
    RTS     ()

osFIND:
    RTS     ()

osGBPB:
    RTS     ()

# --------------------------------------------------------------

osRDCH:
    mov     r1, r0        # Send command &00 - OSRDCH
    JSR     (SendCommand)

WaitCarryChar:            # Receive carry and A
    JSR     (WaitByte)
    add     r1, r0, 0xff80

WaitByte:
    ld      r12, r0, r2status
    and     r12, r0, 0x80
    z.mov   pc, r0, WaitByte
    ld      r1, r0, r2data

NullReturn:
    RTS     ()

# --------------------------------------------------------------

osWORD:
    cmp     r1, r0
    z.mov   pc, r0, RDLINE
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
    JSR     (SendCommand) # Send command &0A - RDLINE

    ld      r1, r2, 3     # Send <char max>
    JSR     (SendByte)
    ld      r1, r2, 2     # Send <char min>
    JSR     (SendByte)
    ld      r1, r2, 1     # Send <buffer len>
    JSR     (SendByte)
    mov     r1, r0, 0x07  # Send <buffer addr MSB>
    JSR     (SendByte)
    mov     r1, r0        # Send <buffer addr LSB>
    JSR     (SendByte)
    JSR     (WaitByte)    # Wait for response &FF [escape] or &7F
    ld      r3, r0        # initialize response length to 0
    cmp     r1, r0, 0x80  # test for escape
    c.mov   pc, r0, RdLineEscape

    ld      r2, r2        # Load the local input buffer from the control block
RdLineLp:
    JSR     (WaitByte)    # Receive a response byte
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

Unsupported:
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
    PUSH    (r13)
    PUSH    (r2)
    mov     r2, r1

ps_loop:
    ld      r1, r2
    and     r1, r0, 0xff
    z.mov   pc, r0, ps_exit
    JSR     (OSWRCH)
    ld      r1, r2
    bswp    r1, r1
    and     r1, r0, 0xff
    z.mov   pc, r0, ps_exit
    JSR     (OSWRCH)
    mov     r2, r2, 0x0001
    mov     pc, r0, ps_loop

ps_exit:
    POP     (r1)
    POP     (r13)
    RTS     ()



osnewl_code:
    PUSH    (r13)
    mov     r1, r0, 0x0a
    JSR     (OSWRCH)
    mov     r1, r0, 0x0d
    JSR     (OSWRCH)
    POP     (r13)
    RTS     ()


# DEFAULT VECTOR TABLE
# ====================

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

