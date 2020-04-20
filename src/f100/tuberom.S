.equ        WORK 0x0600      ; system jump blocks (512 locations) below ROM
.equ        CODE 0x0800      ; This is the location of main F100 ROM
.equ        TUBE 0x7EF8      ; Hardware mapped in here - tube registers
.equ         MOS 0x7FC8      ; This is a small additional ROM at top of memory
.equ         APP 0x1000      ; Application RAM starts here        
.equ  END_MARKER 0x8000      ; makes the end of each command -ve

;; ; These are passed in directly from the makefile 
;; .equ     MEM_BOT _MEM_BOT_
;; .equ     MEM_TOP _MEM_TOP_
;; .equ       STACK _STACK_

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
        

; -----------------------------------------------------------------------------
; Memory from 0x0000 to 0x00FF is reserved for system use
; -----------------------------------------------------------------------------
.equ      NUM_VECTORS     27            ; number of vectors in DefaultVectors table

.equ       USERV WORK + 0x0010 + 0x0010 
.equ       WRCHV WORK + 0x0017 + 0x0017 
                                        
.equ      ERRBUF WORK + 0x0030 + 0x0030 
.equ      INPBUF WORK + 0x0030 + 0x0030 
.equ      INPEND WORK + 0x00F6 + 0x00F6 
                                        
.equ        ADDR WORK + 0x00F6 + 0x00F6 ; tube execution address
.equ      TMP_R1 WORK + 0x00FC + 0x00FC ; tmp store for R1 during IRQ
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
        LDA     ,DefaultVectors
        STO     R1
        LDA     ,USERV
        STO     R2        
InitVecLoop:        
        LDA     /R1+
        STO     /R2+
        ICZ     R0 InitVecLoop
        
        LDA     ,BannerMessage
        STO     R1
        CAL     .print_string

        LDA     ZERO            ; send the terminator
        STO     R1
        CAL     .OSWRCH        

Idle:
        JMP     .Idle           ; Comment out this instruction to finish emulation run on HALT
        HALT   ,123             
        
; --------------------------------------------------------------
;
; print_string
;
; Prints the zero terminated ASCII string
;
; Entry:
; - r1 points to the zero terminated string
;
; Exit:
; - r1,r2 trashed, all other registers preserved

print_string:
        LDA     R1
        STO     R2
print_string_loop:
        LDA     /R2+
        AND     ,0xFF
        CMP     ,0
        STO     R1
        JBS     ZERO CR print_string_exit
        CAL     .OSWRCH        
        JMP     .print_string_loop
print_string_exit:
        RTN

; --------------------------------------------------------------
;
; OSWRCH
;
; Prints single character
;
; Entry:
; - R1 holds byte to be written
;
; Exit:
; - all registers preserved
       
mosWRCH:
        LDA     .r1status       
        AND     ,0x40
        JBS     ZERO CR mosWRCH
        LDA     R1
        STO     .r1data
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
        ; .STRING "Escape"
        .WORD    0x00

; -----------------------------------------------------------------------------
; TUBE ULA registers
; -----------------------------------------------------------------------------

.org TUBE
        .WORD 0x0000     ; 0xFEF8
        .WORD 0x0000     ; 0xFEF9
        .WORD 0x0000     ; 0xFEFA
        .WORD 0x0000     ; 0xFEFB
        .WORD 0x0000     ; 0xFEFC
        .WORD 0x0000     ; 0xFEFD
        .WORD 0x0000     ; 0xFEFE
        .WORD 0x0000     ; 0xFEFF

; -----------------------------------------------------------------------------
; DEFAULT VECTOR TABLE
; -----------------------------------------------------------------------------

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
; MOS interface - normal for Acorn to be at the top of the memory map
;               - see http://mdfs.net/Software/Tube/Z80/ClientZ80.src for Z80 impl.
; -----------------------------------------------------------------------------

.org MOS

.org MOS + (0xEE-0xC8)

OSWRCH:                      ; &FFEE
        JMP     .WRCHV       


