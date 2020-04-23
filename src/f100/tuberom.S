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
        LDA     ,DefaultVectors-1
        STO     R1
        LDA     ,USERV-1
        STO     R2        
InitVecLoop:        
        LDA     /R1+
        STO     /R2+
        ICZ     R0 InitVecLoop
        
        LDA     ,BannerMessage 
        STO     R0
        CAL     .print_string

        CAL     .PI
        CAL     .NEWLINE        

        LDA     ,0            ; send the terminator
        STO     R0
        CAL     .OSWRCH        

Idle:
        HALT   ,123             
        
; --------------------------------------------------------------
;
; print_string
;
; Prints the zero terminated ASCII string
;
; Entry:
; - r0 points to the location of the zero terminated string
;
; Exit:
; - r0,r1 trashed, all other registers preserved

print_string:
        LDA     ,1
        SUB     R0 ; Need addr-1 in R1 because we will use pre-incr pointer to address the string
        STO     R1
print_string_loop:
        LDA     /R1+
        AND     ,0xFF
        JBS     ZERO CR print_string_exit
        STO     R0
        CAL     .OSWRCH        
        JMP     .print_string_loop
print_string_exit:
        RTN
; --------------------------------------------------------------
;
; NEWLINE
;
; emit a newline LF+CR pair
;
; Entry:
; - None
;
; Exit:
; - R0 trashed
        
NEWLINE:
        LDA     ,0x0A
        STO     R0
        CAL     .OSWRCH
        LDA     ,0x0D
        STO     R0
        CAL     .OSWRCH
        RTN

; --------------------------------------------------------------
;
; OSWRCH
;
; Prints single character
;
; Entry:
; - R0 holds byte to be written
;
; Exit:
; - all registers preserved
OSWRDIG:                        ; Alternative entry with digit in Acc
        ADD     ,48             ; make ASCII
        STO     R0              ; save in R0
mosWRCH:
        LDA     .r1status       
        AND     ,0x40
        JBS     ZERO CR mosWRCH
        LDA     R0
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
.equ  DIGITS  32        
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
        CMP     ,0
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
        CMP     ,0
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
        CMP     ,0
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

.org MOS + (0xEE-0xC8)

OSWRCH:                      ; &FFEE
        JMP     .WRCHV       


