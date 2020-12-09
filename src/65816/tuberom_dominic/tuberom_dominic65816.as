; 65816 tube host code (c) 2015 Dominic Beesley
		; Taken from JGH Dissasembly
		; REM Code copyright Acorn Computer
		; Commentary copyright J.G.Harston

		NMIV_NAT	= $01FFEA

		.segment "ZP"
		.org $E4
		; Memory addresses
ZP_MON_PTR:	.res 2; monitor PTR
ZP_MON_ACC:	.res 1; monitor accumulator
		.res 7; spare
ZP_PROG: 	.res 2; $ee/F = PROG   - Current program
ZP_NUM:		.res 2; $f0/1 = NUM    - hex accumulator
ZP_MEMTOP:	.res 2; $f2/3 = MEMTOP - top of memory
ZP_NMIADDR:	.res 2; $f4/5 = address of byte transfer address, NMIAddr or ADDR
ZP_ADDR:	.res 2; $f6/7 = ADDR   - Data transfer address
ZP_OSWORDCTL:	.res 2; $f8/9 = String pointer, OSWORD control block
ZP_CTL:		.res 2; $fa/B = CTRL   - OSFILE, OSGBPB control block, PrText string pointer
ZP_IRQA:	.res 1; $fc   = IRQ A store
ZP_LAST_E:	.res 2; $fd/E => last error
ZP_ESC:		.res 1; $ff   = Escape flag


		.segment "PAD"

		.segment "CODE0"
		.P816
		.a8
		.i8

		USERV=$200
		BRKV=$202
		IRQ1V=$204
		IRQ2V=$206
		CLIV=$208
		BYTEV=$20a
		WORDV=$20c
		WRCHV=$20e
		RDCHV=$210
		FILEV=$212
		ARGSV=$214
		BGetV=$216
		BPutV=$218
		GBPBV=$21a
		FINDV=$21c
		FSCV=$21e
		EVNTV=$220
		UPTV=$222
		NETV=$224
		VduV=$226
		KEYV=$228
		INSV=$22a
		RemV=$22c
		CNPV=$22e
		IND1V=$230
		IND2V=$232
		IND3V=$234

		ERRBUF=$236
		INPBUF=$236


		TubeS1=$fef8
		TubeR1=$fef9
		TubeS2=$fefa
		TubeR2=$fefb
		TubeS3=$fefc
		TubeR3=$fefd
		TubeS4=$fefe
		TubeR4=$feff

		; processor flags
		PF_A16 = $20
		PF_I16 = $10

		; OS CONSTANTS
		OSWORD_INPUT_LINE = $00

		OSBYTE_ESC_ACK = $7e

		ERR_ESC = 17

		.org	 $f300
ROMSTART:
MON_LAST_DUMP:	.res	2
MON_REGSAVE:
MON_REGSAVE_NE:	.res	1	; flags with C set to Nat/Emu
MON_REGSAVE_P:	.res	1	; flags when COP occurred
MON_REGSAVE_PC:	.res	2	; PC when COP occurred in PCL, PCH (adjusted for COP)
MON_REGSAVE_K:	.res	1	; K program bank when COP occurred
MON_REGSAVE_D:	.res	2	; DP when COP occurred
MON_REGSAVE_B:	.res	1	; Databank when COP occurred
MON_REGSAVE_S:	.res	2	; SP when COP occurred (adjusted for COP)
MON_REGSAVE_A:	.res	2	; 16 bit C register
MON_REGSAVE_X:	.res	2	; 16 bit X
MON_REGSAVE_Y:	.res	2	; 16 bit Y


		; ------------------------------------------------------------
		; 65816 monitor - Dossytronics 2015
		; ------------------------------------------------------------
		; Monitor Command - a monitor command has been entered at the command line / OSCLI
		; monitor commands all start '%<LETTER>'
		; on entry (ZP_OSWORDCTL), Y points at the '%'
		; native mode , i8, a8
MonitorCommand:	.a8
		.i8

		jsr		PrText
		.byte		"Monitor", 13, 10
		nop

		jsr		SkipSpaces
		and		#$DF
		sta		ZP_MON_ACC
		ldx		#0
@lp1:		lda		MonCmds, X
		beq		@sk1
		cmp		ZP_MON_ACC
		beq		@sk2
		inx
		inx
		inx					; next command
		bra		@lp1
@sk2:		inx					; found jump to command
		pea		OSCLIRET - 1		; push return address to OSCLIRET
		jmp		(MonCmds, X)
@sk1:		lda		ZP_MON_ACC
		jsr		OSWRCH
		jsr		PrText
		.byte		"? H", 13, 10
		nop
		jmp		OSCLIRET

MonCmds:	.byte	'R'
		.addr	monRegs
		.byte	'D'
		.addr   monDump
		.byte	'B'
		.addr	monBreakPoint
		.byte	'H'
		.addr	monHelp
		.byte	0
tblFlagsNames:	.byte		"CZIDXMVN"


		; ============================================================
		; monRegs- show registers captured at last breakpoint
		; ============================================================
monRegs:	jsr		PrText
		.byte "PC="
		nop
		lda		MON_REGSAVE_K
		jsr		Hex8			; show program bank
		lda		#':'
		jsr		OSWRCH
		lda		MON_REGSAVE_PC + 1
		jsr		Hex8
		lda		MON_REGSAVE_PC
		jsr		Hex8

		lda		MON_REGSAVE_NE
		and		#1			; test if em flag set
		bne		@sk_em
		jsr		PrText
		.byte ", NAT"
		nop
		bra		@sknem
@sk_em:		jsr		PrText
		.byte ", EMU"
		nop
@sknem:		jsr		PrText
		.byte ", F="
		nop
		lda		MON_REGSAVE_P
		sta		ZP_MON_ACC
		ldy		#7
@lp1:		lda		tblFlagsNames, Y
		asl		ZP_MON_ACC
		bcs		@sk2
		ora		#$20			; to lower if flag not set
@sk2:		jsr		OSWRCH
		lda		#' '
		jsr		OSWRCH
		dey
		bpl		 @lp1
		jsr		OSNEWL
		jsr		PrText
		.byte "A="
		nop
		lda		MON_REGSAVE_A + 1
		jsr		Hex8
		lda		MON_REGSAVE_A
		jsr		Hex8
		jsr		PrText
		.byte " X="
		nop
		lda		MON_REGSAVE_X + 1
		jsr		Hex8
		lda		MON_REGSAVE_X
		jsr		Hex8
		jsr		PrText
		.byte " Y="
		nop
		lda		MON_REGSAVE_Y + 1
		jsr		Hex8
		lda		MON_REGSAVE_Y
		jsr		Hex8
		jsr		PrText
		.byte " S="
		nop
		lda		MON_REGSAVE_S + 1
		jsr		Hex8
		lda		MON_REGSAVE_S
		jsr		Hex8
		jsr		PrText
		.byte " B="
		nop
		lda		MON_REGSAVE_B
		jsr		Hex8
		jsr		PrText
		.byte " DP="
		nop
		lda		MON_REGSAVE_D + 1
		jsr		Hex8
		lda		MON_REGSAVE_D
		jmp		Hex8



monDump:
		jsr		inYSkipSpaces
		cmp		#$D
		beq		@sk1
		jsr		ScanHex
		rep		#PF_I16
		.i16
		ldx		ZP_NUM
		beq		@sk1
		stx		ZP_MON_PTR
		bra		@sk2
@sk1:		rep		#PF_I16
		ldx		MON_LAST_DUMP
		stx		ZP_MON_PTR


@sk2:		ldx		#8
@lp1:		phx
		jsr		dumpline
		plx
		dex
		bne		@lp1

		ldx		ZP_MON_PTR
		stx		MON_LAST_DUMP

		sep		#PF_I16
		.i8
		rts

		.i16
dumpline:	lda		#0
		jsr		Hex8
		lda		ZP_MON_PTR + 1
		jsr		Hex8
		lda		ZP_MON_PTR
		jsr		Hex8
		lda		#':'
		jsr		OSWRCH
		ldy		#0
@lp1:		lda		(ZP_MON_PTR), Y
		jsr		Hex8
		lda		#' '
		jsr		OSWRCH
		iny
		cpy		#8
		bne		@lp1
		lda		#'|'
		jsr		OSWRCH
		ldy		#0
@lp2:		lda		(ZP_MON_PTR), Y
		cmp		#' '
		bcc		@sk1
		cmp		#$7F
		bcc		@sk2
@sk1:		lda		#'.'
@sk2:		jsr		OSWRCH
		iny
		cpy		#8
		bne		@lp2

		lda		#$86
		jsr		OSBYTE			; read cursor
		cpx		#0
		beq		@sk3

		jsr		OSNEWL

@sk3:		rep		#PF_A16
		.a16
		lda		ZP_MON_PTR
		clc
		adc		#8
		sta		ZP_MON_PTR
		sep		#PF_A16
		.a8
		rts
		.i8

monBreakPoint:
monHelp:
		jsr	PrText

		.byte  "R - dump registers", 13, 10
		.byte  "D[<addr>] - dump memory next/addr", 13, 10
		.byte  "B[<addr>] [C] - set/clr/lst breakpoints", 13, 10
		.byte  "H - help", 13, 10
		nop
		rts



MonCOPHandler:
		clc					; save all regs and state (flags already stored on stack)
		xce					; switch to native mode
		php					; store old nat / em mode in C
		phb
		phd

		;reset direct page
		pea		0
		pld
		;reset bank register
		pea		0
		plb
		plb

		rep		#PF_A16 + PF_I16	; switch to big regs
		.a16
		.i16
		sta		MON_REGSAVE_A
		stx		MON_REGSAVE_X
		sty		MON_REGSAVE_Y
		tsc
		clc
		adc		#8			; number of extra bytes on stack including those pushed by COP
		sta		MON_REGSAVE_S		; stack adjusted (nat mode)
		pla
		sta		MON_REGSAVE_D
		sep		#PF_A16
		.a8
		pla
		sta		MON_REGSAVE_B
		pla
		sta		MON_REGSAVE_NE
		pla
		sta		MON_REGSAVE_P
		plx
		stx		MON_REGSAVE_PC
		lda		MON_REGSAVE_NE
		and		#1			; check if was in em mode
		bne		@skem
		pla
		sta		MON_REGSAVE_K		; native mode - pull program bank
		bra		@sknat
@skem:		stz		MON_REGSAVE_K		; em mode - zero program bank
		lda		MON_REGSAVE_S		; readjust stack for one less byte
		bne		@skem2
		dec		MON_REGSAVE_S + 1
@skem2:		dec		MON_REGSAVE_S
@sknat:		sep		#PF_A16 + PF_I16
		.a8
		.i8

		jsr		PrText
		.byte "Breakpoint hit", 13, 10
		nop

		pea		CmdOSLoop - 1
		jmp		monRegs


RESET:
		; ------------------------------------------------------------
		; 65816 tube client adapted from JGH dissambly 2015
		; ------------------------------------------------------------
		; switch to Native mode
		clc
		xce

		; Twiddle the bank registers to select 512KB mode and map in the ROM
		; banking=1100
		lda     #4
		sta     $fef0
		lda     #4
		sta     $fef0
		lda     #5
		sta     $fef0
		lda     #5
		sta     $fef0
		; turn on the def override
		lda     #0
		sta     $fef0

		; set direct page to 0
		pea 		0
		pld

		.i16
		.a16
		; 16 bit XY, C
		rep		#PF_A16 + PF_I16

		; Copy NAT_VECT to top of bank 1 RAM
		lda		#$20 - 1
		ldx		#NAT_VECT
		ldy		#$FFE0
		; CA65 vesion 2.19 uses <src>,<dst>
		mvn		#0, #1

		; copy top page (OS and CPU vectors from ROM to RAM)
		lda		#$100 - 1
		ldx		#$ff00
		ldy		#$ff00
		mvn		#0, #0


		; Set up default vectors
		lda		#TBL_DefVectorsEnd - TBL_DefVectors - 1
		ldx		#TBL_DefVectors
		ldy		#USERV
		mvn		#0, #0

		; Clear stack (set x to $FF)
		ldx		#$1FF
		txs

		; Copy rest of ROM to RAM
		lda		#$FEF0 - ROMSTART - 1
		ldx		#ROMSTART
		ldy		#ROMSTART
		mvn		#0, #0

		; Copy jump code to $100
		lda		#(bootBounceEnd - bootoff) - 1
		ldx		#bootoff
		ldy		#$100
		mvn		#0, #0

		lda		ZP_PROG			; Copy ZP_PROG to ZP_ADDR
		sta		ZP_ADDR

		; back to 8 bit A
		.a8
		SEP		#PF_A16
		stz		ZP_ESC

		; Set memtop to start of ROM
		stz		ZP_MEMTOP
		lda		#>ROMSTART
		sta		ZP_MEMTOP + 1

		.i8
		.a8
		; 8 bit XY, C
		sep		#PF_A16 + PF_I16

		; Jump via low memory to page ROM out
		; Executed in low memory to page ROM out
		; --------------------------------------
		jmp		$0100
bootoff:
		; banking=0100 - unmap the ROM
		lda     #4
		sta     $fef0
		lda     #4
		sta     $fef0
		lda     #5
		sta     $fef0
		lda     #4
		sta     $fef0
		lda		TubeS1
		cli
		; Check Tube R1 status to page ROM out
rstBannerOrLoop:
		jmp		PrBanner                ; Jump to initilise I/O with banner
							; or straight to cmd loop - this gets replaced in next
							; part of code

bootBounceEnd:

PrBanner:	jsr		PrText                 	; Display startup banner
		.byte 		10, "Dossytronics 65816", 10, 10, 13, 0
		nop

		rep		#PF_I16
		.i16
		ldx		#CmdOSLoop		; banner not printed
		stx		rstBannerOrLoop + 1	; next time RESET is soft entered,
		sep		#PF_I16
		.i8

		jsr		WaitByte                ; Wait for Acknowledge
		cmp		#$80
		beq		EnterCode     		; If $80, jump to enter code

		; Otherwise, enter command prompt loop
		; Minimal Command prompt
		; ======================
CmdOSLoop:
		lda		#'*'
		jsr		OSWRCH    		; Print '*' prompt
		ldx		#<OSWORD_INPBUF
		ldy		#>OSWORD_INPBUF
		lda		#OSWORD_INPUT_LINE
		jsr		OSWORD    		; Read line to INPBUF
		bcs		CmdOSEscape
		ldx		#<INPBUF
		ldy		#>INPBUF		; Execute command
		jsr		OS_CLI
		jmp		CmdOSLoop   		;  and loop back for another

CmdOSEscape:	lda		#OSBYTE_ESC_ACK
		jsr		OSBYTE        		; Acknowledge Escape state
		brk
		.byte 		ERR_ESC, "Escape"
		brk

		; Enter Code pointed to by ZP_ADDR
		; ==============================
		; Checks to see if code has a ROM header, and verifies
		; it if it has
		; TODO make this for em mode round call, if so remove force from SaveProgEnterCode in OSCLI
EnterCode:	rep	#PF_A16
		.a16
		lda		ZP_ADDR
		sta		ZP_PROG
		sta		ZP_MEMTOP			; Set current program and memtop
		ldy		#$07
		lda		(ZP_PROG), Y     		; Get copyright offset
		tay
		tya						; get bottom 8 bits
		cld
		clc
		adc		ZP_PROG
		sta		ZP_LAST_E			; ZP_LAST_E=>copyright message
		.a8
		sep	#PF_A16

		; Now check for $00,"(C)"
		; =======================
		ldy		#$00
		lda		(ZP_LAST_E), Y
		bne		@NotROM			; Jump if no initial $00
		iny
		lda		(ZP_LAST_E), Y
		cmp		#'('
		bne		@NotROM 		; Jump if no '('
		iny
		lda		(ZP_LAST_E), Y
		cmp		#'C'
		bne		@NotROM			; Jump if no 'C'
		iny
		lda		(ZP_LAST_E), Y
		cmp		#')'
		bne		@NotROM 		; Jump if no ')'

		; $00,"(C)" exists - check if a language
		;==================
		ldy		#$06
		lda		(ZP_PROG), Y 		; Get ROM type
		and		#$4f
		cmp		#$40
		bcc		NotLanguage  		; b6=0, not a language
		and		#$0d
		bne		Not6502Code  		; type<>0 and <>2, not 6502 code
							; TODO add 65816 type here?
@NotROM:
		lda		#$01
		sec
		xce
		jmp		(ZP_MEMTOP)		; Enter code with A=1 in emulation mode

NotLanguage:
		; Any existing error handler will probably have been overwritten
		; Set up new error handler before generating an error
		jsr		setupErrorHandler
		brk
		.byte 0, "This is not a language", 0
Not6502Code:	jsr		setupErrorHandler
		brk
		.byte 0, "I cannot run this code", 0

setupErrorHandler:
		rep		#PF_A16
		.a16
		lda		#ErrorHandler
		sta		BRKV 			; Claim error handler
		sep		#PF_A16
		.a8
		rts

ErrorHandler:
		ldx		#$ff
		txs					; Clear stack
		jsr		OSNEWL
		ldy		#$01
@lp1:		lda		(ZP_LAST_E), Y
		beq		@sk1      		; Print error string
		jsr		OSWRCH
		iny
		bne		@lp1
@sk1:		jsr		OSNEWL
		jmp		CmdOSLoop   		; Jump to command prompt

OSWORD_INPBUF:
		; Control block for command prompt input
		; --------------------------------------
		.addr INPBUF             		; Input text to INPBUF at $236
		.byte $ca          			; Up to $ca characters
		.byte $20
		.byte $ff          			; Min=$20, Max=$ff

		; MOS INTERFACE
		; =============
		;
		;

do_OSWRCH:
		; OSWRCH - Send character to output stream
		; ========================================
		; On entry, A =character
		; On exit,  A =preserved
		;
		; Tube data  character  --
		;
		; DB This function is now native mode agnostic

		php
		sep		#PF_A16			; switch to a8 mode
@lp1:		bit		TubeS1			; Read Tube R1 status
		nop
		bvc		@lp1			; Loop until b6 set
		sta		TubeR1
		plp					; pull old a8/16 mode
		rts

do_OSRDCH:
		; Send character to Tube R1
		; OSRDCH - Wait for character from input stream
		; =============================================
		; On exit, A =char, Cy=Escape flag
		;
		; Tube data  $00  --  Carry Char
		;
		; DB This function is now native mode agnostic

		clc					; clear carry
		php
		sep		#PF_A16			; switch to a8 mode

		lda		#$00
		jsr		SendCommand 		; Send command $00 - OSRDCH

		jsr		WaitCarryChar
		bcs		@retcs
		plp					; pull old a8/16 mode
		rts
@retcs:		plp					; pull old a8/a16 mode and set carry
		sec
		rts


WaitCarryChar:	jsr		WaitByte		; Wait for Carry and A
		asl		A         		; Wait for carry
WaitByte:	bit		TubeS2
		bpl		WaitByte    		; Loop until Tube R2 has data
		lda		TubeR2         		; Fetch character


NullReturn:	rts


		; Skip Spaces
		; ===========
inYSkipSpaces:	iny
SkipSpaces:	lda		(ZP_OSWORDCTL), Y
		cmp		#$20
		beq		inYSkipSpaces
		rts

		; Scan hex
		; ========
ScanHex:	ldx		#$00
		stx		ZP_NUM
		stx		ZP_NUM + 1   		; Clear hex accumulator
@s2:		lda		(ZP_OSWORDCTL), Y	; Get current character
		cmp		#$30
		bcc		@skhex			; <'0', exit
		cmp		#$3a
		bcc		@s1			; '0'..'9', add to accumulator
		and		#$df
		sbc		#$07
		bcc		@skhex			; Convert letter, if <'A', exit
		cmp		#$40
		bcs		@skhex			; >'F', exit
@s1:
		asl		A
		asl		A
		asl		A
		asl		A    			; *16
		ldx		#$03 			; Prepare to move 3+1 bits
@s3:		asl		A
		rol		ZP_NUM
		rol		ZP_NUM + 1     		; Move bits into accumulator
		dex
		bpl		@s3         		; Loop for four bits, no overflow check
		iny
		bne		@s2			; Move to next character
@skhex:		rts


SendString:	; Send string to Tube R2
		; ======================
		stx		ZP_OSWORDCTL
		sty		ZP_OSWORDCTL + 1	; Set ZP_OSWORDCLT to string
SendStringF8:	ldy		#$00
@l1:		bit		TubeS2
		bvc		@l1			; Wait for Tube R2 free
		lda		(ZP_OSWORDCTL), Y
		sta		TubeR2     		; Send character to Tube R2
		iny
		cmp		#$0d
		bne		@l1			; Loop until <cr> sent
		ldy		ZP_OSWORDCTL + 1	; Restore Y from ZP_OSWORDCTL + 1 and return
		rts

do_OSCLI:	; OSCLI - Execute command
		; =======================
		; On entry, XY=>command string
		; On exit,  XY= preserved
		;
		; DB - make native mode agnostic

		clc
		xce
		php					; enter native mode if not already and save M/X flags
		rep		#PF_A16 + PF_I16	; save regs in 16bit mode
		pha
		xba
		pha
		phx
		phy
		sep		#PF_A16 + PF_I16
		.a8
		.i8
		stx		ZP_OSWORDCTL
		sty		ZP_OSWORDCTL + 1	; Save A, ZP_OSWORDCTL=>command string
		ldy		#$00
@l1:		jsr		SkipSpaces
		iny
		cmp		#'*'
		beq		@l1			; Skip spaces and stars
		cmp		#'%'
		bne		@skmon
		jmp		MonitorCommand
@skmon:		and		#$df
		tax					; Ignore case, and save in X
		lda		(ZP_OSWORDCTL), Y	; Get next character
		cpx		#'G'
		beq		CmdGO			; Jump to check '*GO'
		cpx		#'H'
		bne		do_OSCLI_IO		; Not "H---", jump to pass to Tube
		cmp		#'.'
		beq		CmdHELP			; "H.", jump to do * unlistable token
		and		#$df			; Ignore case
		cmp		#'E'
		bne		do_OSCLI_IO		; Not "HE---", jump to pass to Tube
		iny
		lda		(ZP_OSWORDCTL), Y	; Get next character
		cmp		#'.'
		beq		CmdHELP			; "HE.", jump to do * unlistable token
		and		#$df			; Ignore case
		cmp		#'L'
		bne		do_OSCLI_IO		; Not "HEL---", jump to pass to Tube
		iny
		lda		(ZP_OSWORDCTL), Y	; Get next character
		cmp		#'.'
		beq		CmdHELP			; "HEL.", jump to do * unlistable token
		and		#$df			; Ignore case
		cmp		#'P'
		bne		do_OSCLI_IO		; Not "HELP---", jump to pass to Tube
		iny
		lda		(ZP_OSWORDCTL), Y	; Get next character
		and		#$df			; Ignore case
		cmp		#'A'
		bcc		CmdHELP			; "HELP" terminated by non-letter, do * unlistable token
		cmp		#'['
		bcc		do_OSCLI_IO


CmdHELP:
		; "HELP" followed by letter, pass to Tube
		; *Help - Display help information
		; --------------------------------
		jsr		PrText                 	; Print help message
		.byte 10, 13, "65816 tube 0.01", 10, 13
		nop
		; Continue to pass '* unlistable token ' command to Tube

do_OSCLI_IO:
		; OSCLI - Send command line to host
		; =================================
		; On entry, ZP_OSWORDCTL(f8/9)=>command string
		;
		; Tube data  $02 string $0d  --  $7f or $80
		;
		sec
		xce					; DB: Always switch to EM mode at this point! TODO: Make this native after writing native mode NMI handlers?
		php
		lda		#$02
		jsr		SendCommand   		; Send command $02 - OSCLI
		jsr		SendStringF8  		; Send command string at ZP_OSWORDCTL
do_OSCLI_Ack:	jsr		WaitByte		; Wait for acknowledgement
		plp
		xce					; Switch back to native / em mode
		cmp		#$80
		beq		SavePROGEnterCode	; Jump if code to be entered
		bra		OSCLIRET


CmdGO:
		; *GO - call machine code
		; -----------------------
		and		#$df    		; Ignore case
		cmp		#'O'
		bne		do_OSCLI_IO		; Not '*GO', jump to pass to Tube
		jsr		inYSkipSpaces		; Move past any spaces
		jsr		ScanHex
		jsr		SkipSpaces 		; Read hex value and move past spaces
		cmp		#$0d
		bne		do_OSCLI_IO   		; More parameters, pass to Tube to deal with
		txa
		beq		SavePROGEnterCode	; If no address given, jump to current program
		lda		ZP_NUM
		sta		ZP_ADDR			; Set program start to address read
		lda		ZP_NUM + 1
		sta		ZP_ADDR + 1
SavePROGEnterCode:
		; DB - need to enter emulated mode here and save all registers
		sec
		xce
		php					; save nat / em mode in C

		; TODO - save bank regs and get a long address

		lda		ZP_PROG + 1
		pha
		lda		ZP_PROG
		pha					; Save current program
		jsr		EnterCode
		pla
		sta		ZP_PROG
		sta		ZP_MEMTOP        	; Restore current program and
		pla
		sta		ZP_PROG + 1
		sta		ZP_MEMTOP + 1		;  set address top of memory to it
		plp
		xce					; back to nat / em mode
OSCLIRET:	rep		#PF_A16 + PF_I16
		ply
		plx
		pla
		xba
		pla
		plp
		xce					; back to whatever mode we were in before
		rts


CheckAck:	beq		do_OSCLI_Ack


do_OSBYTE:
		; OSBYTE - Byte MOS functions
		; ===========================
		; On entry, A, X, Y=OSBYTE parameters
		; On exit,  A  preserved
		;           If A<$80, X=returned value
		;           If A>$7f, X, Y, Carry=returned values
		;
		; DB	- make nat/em agnostic

		php
		sep		#PF_A16 + PF_I16

		cmp		#$80
		bcs		ByteHigh      ; Jump for long OSBYTEs

		;
		; Tube data  $04 X A    --  X
		;
		pha
		lda		#$04
@l1:		bit		TubeS2
		bvc		@l1		; Wait for Tube R2 free
		sta		TubeR2          ; Send command $04 - OSBYTELO
@l2:		bit		TubeS2
		bvc		@l2       	; Wait for Tube R2 free
		stx		TubeR2		; Send single parameter
		pla
@l3:		bit		TubeS2
		bvc		@l3       	; Wait for Tube R2 free
		sta		TubeR2          ; Send function
@l4:		bit		TubeS2
		bpl		@l4		; Wait for Tube R2 data present
		ldx		TubeR2		; Get return value
		plp
		rts


ByteHigh:	cmp		#$82
		beq		Byte82		; Read memory high word
		cmp		#$83
		beq		Byte83		; Read bottom of memory
		cmp		#$84
		beq		Byte84		; Read top of memory

		;
		; Tube data  $06 X Y A  --  Cy Y X
		;
		pha
		lda		#$06
@l1:
		bit		TubeS2
		bvc		@l1		; Wait for Tube R2 free
		sta		TubeR2		; Send command $06 - OSBYTEHI
@l2:		bit		TubeS2
		bvc		@l2		; Wait for Tube R2 free
		stx		TubeR2		; Send parameter 1
@l3:		bit		TubeS2
		bvc		@l3       	; Wait for Tube R2 free
		sty		TubeR2    	; Send parameter 2
		pla
@l4:		bit		TubeS2
		bvc		@l4		; Wait for Tube R2 free
		sta		TubeR2		; Send function
		cmp		#$8e		; If select language, check to enter code
		beq		CheckAck
		cmp		#$9d
		beq		@sk1		; Fast return with Fast BPUT
		pha				; Save function
@l5:		bit		TubeS2
		bpl		@l5		; Wait for Tube R2 data present
		lda		TubeR2
		asl		A
		pla				; Get Carry
@l6:		bit		TubeS2
		bpl		@l6		; Wait for Tube R2 data present
		ldy		TubeR2		; Get return high byte
@l7:		bit		TubeS2
		bpl		@l7		; Wait for Tube R2 data present
		ldx		TubeR2		; Get return low byte
		plp
@sk1:		rts

Byte84:
		ldx		ZP_MEMTOP	; Read top of memory from $f2/3
		ldy		ZP_MEMTOP + 1
		plp
		rts
Byte83:
		ldx		#$00		; Read bottom of memory
		ldy		#$08
		plp
		rts
Byte82:
		ldx		#$00		; Return $0000 as memory high word
		ldy		#$00
		plp
		rts

do_OSWORD:
		; OSWORD - Various functions
		; ==========================
		; On entry, A =function
		;           XY=>control block
		;
		; DB make em/nat agnostic
		; TODO - trashes X, Y, A high bytes

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode

		stx		ZP_OSWORDCTL
		sty		ZP_OSWORDCTL + 1	; ZP_OSWORDCTL=>control block
		tay
		beq		RDLINE			; OSWORD 0, jump to read line
		pha
		ldy		#$08
@l1:		bit		TubeS2
		bvc		@l1			; Loop until Tube R2 free
		sty		TubeR2			; Send command $08 - OSWORD
@l2:		bit		TubeS2
		bvc		@l2			; Loop until Tube R2 free
		sta		TubeR2			; Send function
		tax
		bpl		@WordSendLow		; Jump with functions<$80
		ldy		#$00
		lda		(ZP_OSWORDCTL), Y	; Get send block length from control block
		tay
		jmp		@WordSend		; Jump to send control block
@WordSendLow:	ldy		WordLengthsLo-1, X      ; Get send block length from table
		cpx		#$15
		bcc		@WordSend      		; Use this length for OSWORD 1 to $14
		ldy		#$10          		; Send 16 bytes for OSWORD $15 to $7f
@WordSend:	bit		TubeS2
		bvc		@WordSend    		; Wait until Tube R2 free
		sty		TubeR2			; Send send block length
		dey
		bmi		@sk2			; Zero or $81..$ff length, nothing to send
@l3:		bit		TubeS2
		bvc		@l3       		; Wait for Tube R2 free
		lda		(ZP_OSWORDCTL), Y
		sta		TubeR2			; Send byte from control block
		dey
		bpl		@l3			; Loop for number to be sent
@sk2:		txa
		bpl		@WordRecvLow        	; Jump with functions<$80
		ldy		#$01
		lda		(ZP_OSWORDCTL), Y	; Get receive block length from control block
		tay
		jmp		@WordRecv		; Jump to receive control block
@WordRecvLow:	ldy		WordLengthsHi-1, X	; Get receive length from table
		cpx		#$15
		bcc		@WordRecv		; Use this length for OSWORD 1 to $14
		ldy		#$10			; Receive 16 bytes for OSWORD $15 to $7f
@WordRecv:	bit		TubeS2
		bvc		@WordRecv    		; Wait for Tube R2 free
		sty		TubeR2                 	; Send receive block length
		dey
		bmi		@restoreXY              ; Zero of $81..$ff length, nothing to receive
@l4:		bit		TubeS2
		bpl		@l4			; Wait for Tube R2 data present
		lda		TubeR2
		sta		(ZP_OSWORDCTL), Y	; Get byte to control block
		dey
		bpl		@l4			; Loop for number to receive
@restoreXY:	ldy		ZP_OSWORDCTL + 1	; Restore registers
		ldx		ZP_OSWORDCTL
		pla
		plp
		rts


RDLINE:
		; RDLINE - Read a line of text
		; ============================
		; On entry, A =0
		;           XY=>control block
		; On exit,  A =undefined
		;           Y =length of returned string
		;           Cy=0 ok, Cy=1 Escape
		;
		; Tube data  $0a block  --  $ff or $7f string $0d
		;
		lda		#$0a
		jsr		SendCommand		; Send command $0a - RDLINE
		ldy		#$04
@l1:		bit		TubeS2
		bvc		@l1			; Wait for Tube R2 free
		lda		(ZP_OSWORDCTL), Y
		sta		TubeR2     		; Send control block
		dey
		cpy		#$01
		bne		@l1   			; Loop for 4, 3, 2
		lda		#$07
		jsr		SendByte		; Send $07 as address high byte
		lda		(ZP_OSWORDCTL), Y
		pha					; Get text buffer address high byte
		dey
@l2:		bit		TubeS2
		bvc		@l2			; Wait for Tube R2 free
		sty		TubeR2			; Send $00 as address low byte
		lda		(ZP_OSWORDCTL), Y
		pha					; Get text buffer address low byte
		ldx		#$ff
		jsr		WaitByte		; Wait for response
		cmp		#$80
		bcs		RdLineEscape		; Jump if Escape returned
		pla
		sta		ZP_OSWORDCTL
		pla
		sta		ZP_OSWORDCTL + 1	; Set ZP_OSWORDCTL=>text buffer
		ldy		#$00
RdLineLp:	bit		TubeS2
		bpl		RdLineLp		; Wait for Tube R2 data present
		lda		TubeR2
		sta		(ZP_OSWORDCTL), Y	; Store returned character
		iny
		cmp		#$0d
		bne		RdLineLp  		; Loop until <cr>
		lda		#$00
		dey
		inx					; Return A=0, Y=len, X=00, Cy=0
		plp
		clc
		rts
RdLineEscape:	pla
		pla
		lda		#$00           		; Return A=0, Y=len, X=FF, Cy=1
		plp					; DB Reset aXX, iXX if in nat mode
		rts

do_OSARGS:	; OSARGS - Read info on open file
		; ===============================
		; On entry, A =function
		;           X =>data word in zero page
		;           Y =handle
		; On exit,  A =returned value
		;           X  preserved
		;           Y  preserved
		;
		; Tube data  $0c handle block function  --  result block
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode
		pha
		lda		#$0c
		jsr		SendCommand 		; Send command $0c - OSARGS
@l1:		bit		TubeS2
		bvc		@l1			; Loop until Tube R2 free
		sty		TubeR2			; Send handle
		lda		$03, X
		jsr		SendByte		; Send data word
		lda		$02, X
		jsr		SendByte
		lda		$01, X
		jsr		SendByte
		lda		$00, X
		jsr		SendByte
		pla
		jsr		SendByte		; Send function
		jsr		WaitByte
		pha					; Get and save result
		jsr		WaitByte
		sta		$03, X			; Receive data word
		jsr		WaitByte
		sta		$02, X
		jsr		WaitByte
		sta		$01, X
		jsr		WaitByte
		sta		$00, X
		pla
		plp					; restore aXX, iXX nat mode
		rts					; Get result back and return


do_OSFIND:
		; OSFIND - Open of Close a file
		; =============================
		; On entry, A =function
		;           Y =handle or XY=>filename
		; On exit,  A =zero or handle
		;
		; Tube data  $12 function string $0d  --  handle
		;            $12 $00 handle  --  $7f
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode

		pha
		lda		#$12
		jsr		SendCommand 		; Send command $12 - OSFIND
		pla
		jsr		SendByte		; Send function
		cmp		#$00
		bne		OPEN			; If <>0, jump to do OPEN
		pha
		tya
		jsr		SendByte		; Send handle
		jsr		WaitByte
		pla
		plp					; restore aXX, iXX nat mode
		rts					; Wait for acknowledge, restore regs and return
OPEN:
		jsr		SendString              ; Send pathname
		jsr		WaitByte
		plp					; restore aXX, iXX nat mode
		rts


do_OSBGET:
		; Wait for and return handle
		; OSBGet - Get a byte from open file
		; ==================================
		; On entry, Y =handle
		; On exit,  A =byte Read
		;           Y =preserved
		;           Cy set if EOF
		;
		; Tube data  $0e handle --  Carry byte
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode
		lda		#$0e
		jsr		SendCommand   		; Send command $0e - OSBGET
		tya
		jsr		SendByte           	; Send handle
		jsr		WaitCarryChar		; Jump to wait for Carry and byte
		plp					; restore aXX, iXX nat mode
		rts


do_OSBPUT:
		; OSBPut - Put a byte to an open file
		; ===================================
		; On entry, A =byte to write
		;           Y =handle
		; On exit,  A =preserved
		;           Y =preserved
		;
		; Tube data  $10 handle byte  --  $7f
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode
		pha
		lda		#$10
		jsr		SendCommand 		; Send command $10 - OSBPUT
		tya
		jsr		SendByte		; Send handle
		pla
		jsr		SendByte		; Send byte
		pha
		jsr		WaitByte
		pla
		plp					; restore aXX, iXX nat mode
		rts					; Wait for acknowledge and return


SendCommand:
SendByte:
		; Send a byte to Tube R2
		; ======================
		bit		TubeS2
		bvc		SendByte    		; Wait for Tube R2 free
		sta		TubeR2			; Send byte to Tube R2
		rts

do_OSFILE:
		; OSFILE - Operate on whole files
		; ===============================
		; On entry, A =function
		;           XY=>control block
		; On exit,  A =result
		;           control block updated
		;
		; Tube data  $14 block string <cr> function  --  result block
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		sec
		xce
		php

		sty		ZP_CTL + 1
		stx		ZP_CTL              	; ZP_CTL=>control block
		pha
		lda		#$14
		jsr		SendCommand 		; Send command $14 - OSFILE
		ldy		#$11
@lp1:		lda		(ZP_CTL), Y
		jsr		SendByte   		; Send control block
		dey
		cpy		#$01
		bne		@lp1			; Loop for $11..$02
		dey
		lda		(ZP_CTL), Y
		tax
		iny
		lda		(ZP_CTL), Y
		tay					; Get pathname address to XY
		jsr		SendString		; Send pathname
		pla
		jsr		SendByte		; Send function
		jsr		WaitByte
		pha					; Wait for result
		ldy		#$11
@lp2:		jsr		WaitByte
		sta		(ZP_CTL), Y   		; Get control block back
		dey
		cpy		#$01
		bne		@lp2			; Loop for $11..$02
		ldy		ZP_CTL + 1
		ldx		ZP_CTL            	; Restore registers
		pla					; Get result and return
		plp					; restore aXX, iXX nat mode
		xce
		rts


do_OSGBPB:
		; OSGBPB - Multiple byte Read and write
		; =====================================
		; On entry, A =function
		;           XY=>control block
		; On exit,  A =returned value
		;              control block updated
		;
		; Tube data  $16 block function  --   block Carry result
		;
		; DB Make em / nat agnostic
		; TODO X, Y high bytes lost in NAT MODE

		php
		sep		#PF_A16 + PF_I16	; set a8, i8 if in nat mode

		sty		ZP_CTL + 1
		stx		ZP_CTL			; ZP_CTL=>control block
		pha
		lda		#$16
		jsr		SendCommand 		; Send command $16 - OSGBPB
		ldy		#$0c
@lp1:		lda		(ZP_CTL), Y
		jsr		SendByte   		; Send control block
		dey
		bpl		@lp1 			; Loop for $0c..$00
		pla
		jsr		SendByte		; Send function
		ldy		#$0c
@lp2:		jsr		WaitByte
		sta		(ZP_CTL), Y   		; Get control block back
		dey
		bpl		@lp2			; Loop for $0c..$00
		ldy		ZP_CTL + 1
		ldx		ZP_CTL			; Restore registers
		jsr		WaitCarryChar          	; Jump to get Carry and result
		plp					; restore aXX, iXX nat mode
		rts

Unsupported:
		brk
		.byte 255, "Bad", 0

		; OSWORD control block lengths
		; ============================
WordLengthsLo:
		.byte $00, $05, $00, $05
		.byte $04, $05, $08, $0e
		; ** Different, 6502 TUBE sends only 2 bytes for =IO
		.byte $04, $01, $01, $05
		.byte $00, $01, $20, $10
		.byte $0d, $00, $04, $80
WordLengthsHi:	.byte $05, $00, $05, $00
		.byte $05, $00, $00, $00
		.byte $05, $09, $05, $00
		.byte $08, $18, $00, $01
		.byte $0d, $80, $04, $80

		; Interrupt Handler - Native mode
		; ===============================
		; DB - new handler if IRQs in native mode
InterruptHandlerNAT:
		.a16
		.i16
		rep		#PF_A16 + PF_I16
		pha
		phx
		phy					; push all regs as 16 bits

		; re-raise as a bogus emulated mode interrupt from here
		.a8
		.i8
		sec					; emulated mode with a phoney IRQ stack
		xce
		pea		NATIRQRET		; RTI will return to NATIRQRET
		php

		bra		IntCallIRQ1V		; jump into E mode irq handler


NATIRQRET:	clc
		xce					; back to native mode

		.a16
		.i16
		rep		#PF_A16 + PF_I16
		ply
		plx
		pla
		rti


BrkHandlerNAT:	; TODO need an API for native mode break - this will only handler breaks in 1st bank
		.a16
		.i16
		rep		#PF_A16 + PF_I16
		lda		2, S			; Get BRK address from stack
		;	cld = not needed
		sec
		sbc		#$01
		sta		ZP_LAST_E		; ZP_LAST_E=>after BRK opcode
		sec					; go to emulated mode! = TODO - what to do?
		xce
		cli
		jmp		(BRKV)          	; Restore IRQs, jump to Error Handler


		.a8
		.i8

		; Interrupt Handler
		; =================
InterruptHandler:
		; note we no longer store A at ZP_IRQA, that is bogus and
		; only used to satisfy IRQ1/2V capturers
		sta		ZP_IRQA			; Save A
		lda		1, S			; get flags ; DB - use new stack rel addressing
		and		#$10
		bne		BRKHandler		; If BRK, jump to BRK handler
IntCallIRQ1V:	jmp		(IRQ1V)         	; Continue via IRQ1V handler
IRQ1Handler:	bit		TubeS4
		bmi		irqR4Trans		; If data in Tube R4, jump to process errors and transfers
		bit		TubeS1
		bmi		irqR1Event		; If data in Tube R1, jump to process Escape and Events
		jmp		(IRQ2V)         	; Pass on to IRQ2V


BRKHandler:	lda		2, S; Get low byte of error address from stack ; DB - use new stack rel
		;	 cld ; DB - not needed
		sec
		sbc		#$01
		sta		ZP_LAST_E
		lda		3, S
		sbc		#$00
		sta		ZP_LAST_E + 1 		; ZP_LAST_E=>after BRK opcode
		lda		ZP_IRQA			; pull old A
		cli
		jmp		(BRKV)          	; Restore IRQs, jump to Error Handler

irqR1Event:
		; Interrupt generated by data in Tube R1
		; --------------------------------------
		lda		TubeR1
		bmi		irqSetEsc       	; b7=1, jump to set Escape state
		phy
		phx					; Save registers
		jsr		waitR1noblockR4
		tay					; Get Y parameter from Tube R1
		jsr		waitR1noblockR4
		tax					; Get X parameter from Tube R1
		jsr		waitR1noblockR4 	; Get event number from Tube R1
		jsr		jmp_ind_EVENTV		; Dispatch event, restore registers
		plx							; DB use new pl* instructions
		ply
		lda		ZP_IRQA
		rti					; Restore A, return from interrupt
jmp_ind_EVENTV:
		jmp		(EVNTV)

irqSetEsc:	asl		A
		sta		ZP_ESC			; Set Escape flag from b6
		lda		ZP_IRQA
		rti					; Restore A, return from interrupt

irqR4Trans:
		; Interrupt generated by data in Tube R4
		; --------------------------------------
		lda		TubeR4
		bpl		irqDataTrans		; b7=0, jump for data transfer
		; An error occurred cause a BRK
		cli
@lp1:		bit		TubeS2
		bpl		@lp1			; Wait for data in Tube R2
		lda		TubeR2
		lda		#$00
		sta		ERRBUF			; Store BRK opcode in error buffer
		tay
		jsr		WaitByte
		sta		ERRBUF+1  		; Get error number
@lp2:		iny
		jsr		WaitByte        	; Store bytes fetched from Tube R2
		sta		ERRBUF+1, Y
		bne		@lp2   			; Loop until final zero
		jmp		ERRBUF         		; Jump to error block to generate error

irqDataTrans:
		; Data transfer initiated by IRQ via Tube R4
		; ------------------------------------------
		phy					; save Y
		tay					; put trans type in Y
		lda		Tbl_DataTransLO, Y
		sta		NMIV+0			; get NMI routine address from table
		sta		NMIV_NAT + 0
		lda		Tbl_DataTransHI, Y
		sta		NMIV+1			; and point NMIV to it
		sta		NMIV_NAT + 1
		lda		Tbl_DataAddrLO, Y
		sta		ZP_NMIADDR      	; Point ZP_NMIADDR to transfer address field
		lda		Tbl_DataAddrHI, Y
		sta		ZP_NMIADDR + 1
@lp1:		bit		TubeS4
		bpl		@lp1       		; Wait until Tube R4 data present
		lda		TubeR4			; Get called ID from Tube R4
		cpy		#$05
		beq		irqDataTransExit	; If 'TubeRelease', jump to exit
		phy					; Save transfer type
		ldy		#$01
@lp2:		bit		TubeS4
		bpl		@lp2       		; Wait for Tube R4 data present
		lda		TubeR4          	; Fetch and disgard address byte 4 TODO: We need this?
@lp3:		bit		TubeS4
		bpl		@lp3       		; Wait for Tube R4 data present
		lda		TubeR4          	; Fetch and disgard address byte 3 TODO: We need this?
@lp4:		bit		TubeS4
		bpl		@lp4       		; Wait for Tube R4 data present
		lda		TubeR4
		sta		(ZP_NMIADDR), Y 	; Fetch address byte 2, store in address
		dey
@lp5:		bit		TubeS4
		bpl		@lp5       		; Wait for Tube R4 data present
		lda		TubeR4
		sta		(ZP_NMIADDR), Y		; Fetch address byte 1, store in address
		bit		TubeR3
		bit		TubeR3      		; Read from Tube R3 twice
@lp6:		bit		TubeS4
		bpl		@lp6			; Wait for Tube R4 data present
		lda		TubeR4			; Get sync byte from Tube R4
		pla					; Get back trans type
		cmp		#$06
		bcc		irqDataTransExit	; Exit if not 256-byte transfers
		bne		irqDataTransRd256	; Jump with 256-byte read

		; Send 256 bytes to Tube via R3
		; -----------------------------
		ldy		#$00
irqDataTransLP7:lda		TubeS3
		and		#$80
		bpl		irqDataTransLP7		; Wait for Tube R3 free
NMI6Addr:	lda		$ffff, Y
		sta		TubeR3     		; Fetch byte and send to Tube R3
		iny
		bne		irqDataTransLP7		; Loop for 256 bytes


@lp8:		bit		TubeS3
		bpl		@lp8			; Wait for Tube R3 free
		sta		TubeR3			; Send final sync byte
irqDataTransExit:
		ply					; Restore registers and return
		lda		ZP_IRQA
		rti


		; Read 256 bytes from Tube via R3
		; -------------------------------
irqDataTransRd256:
		ldy		#$00
irqDataTransLP8:lda		TubeS3
		and		#$80
		bpl		irqDataTransLP8		; Wait for Tube R3 data present
		lda		TubeR3			; Fetch byte from Tube R3
NMI7Addr:	sta		$ffff, Y
		iny
		bne		irqDataTransLP8		; Store byte and loop for 256 bytes
		beq		irqDataTransExit	; Jump to restore registers and return


;		; Transfer 0 - Transfer single byte to Tube
;		; -----------------------------------------
;NMI0:		pha					; Save A
;NMI0Addr:	lda		$ffff
;		sta		TubeR3       		; Get byte and send to Tube R3
;		inc		NMI0Addr+1
;		bne		@sk2			; Increment transfer address
;		inc		NMI0Addr+2
;@sk2:		pla
;		rti					; Restore A and return

		; Transfer 0 - Transfer a single byte from memory to TUBE
NMI0:		sep		#PF_A16			; force 8 bit mode if in native mode
		pha
NMI0Addr:	lda		$FFFF
		sta		TubeR3			; Get byte and send to Tube R3
		inc		NMI0Addr + 1
		bne		@sk1
		inc		NMI0Addr + 2
@sk1:		pla
		rti


;		; Transfer 1 - Transfer single byte from Tube
;		; -------------------------------------------
;NMI1:		pha
;		lda		TubeR3             	; Save A, get byte from Tube R3
;NMI1Addr:	sta		$ffff                  	; Store byte
;		inc		NMI1Addr+1
;		bne		@sk3			; Increment transfer address
;		inc		NMI1Addr+2
;@sk3:		pla					; Restore A and return
;		rti

		; Transfer 1 - Transfer single byte from Tube
		; -------------------------------------------
NMI1:		sep		#PF_A16			; force 8 bit mode if in Native
		pha
		lda		TubeR3             	; Save A, get byte from Tube R3
NMI1Addr:	sta		$ffff                  	; Store byte
		inc		NMI1Addr + 1
		bne		@sk3
		inc		NMI1Addr + 2
@sk3:		pla					; Restore A and return
		rti


;NMI2:		; Transfer 2 - Transfer two bytes to Tube
;		; ---------------------------------------
;		pha										; 3	1
;		phy					; Save registers			; 3	1
;		ldy		#$00       							; 2	2
;		lda		(ZP_ADDR), Y							; 5	2
;		sta		TubeR3     		; Get byte and send to Tube R3		; 4	3
;		inc		ZP_ADDR								; 5	2
;		bne		@sk1								; 3	2
;		inc		ZP_ADDR + 1		; Increment transfer address		; -	2
;@sk1:		lda		(ZP_ADDR), Y							; 5	2
;		sta		TubeR3     		; Get byte and send to Tube R3		; 4	3
;		inc		ZP_ADDR								; 5	2
;		bne		@sk4								; 3	2
;		inc		ZP_ADDR + 1		; Increment transfer address		; -	2
;@sk4:		ply					; Restore registers and return		; 3	1
;		pla										; 3	1
;		rti									;======= 48	28


NMI2:		; Transfer 2 - Transfer two bytes to Tube
		; ---------------------------------------
		sep		#PF_A16			; force 8 bit mode			; 3	2
		pha										; 3	1
		lda		(ZP_ADDR)							; 5	2
		sta		TubeR3     		; Get byte and send to Tube R3		; 4	3
		inc		ZP_ADDR								; 5	2
		bne		@sk1								; 3	2
		inc		ZP_ADDR + 1		; Increment transfer address		; -	2
@sk1:		lda		(ZP_ADDR)							; 5	2
		sta		TubeR3     		; Get byte and send to Tube R3		; 4	3
		inc		ZP_ADDR								; 5	2
		bne		@sk4								; 3	2
		inc		ZP_ADDR + 1		; Increment transfer address		; -	2
@sk4:		pla										; 3	1
		rti									;======= 43	26


;NMI3:		; Transfer 3 - Transfer two bytes from Tube
;		; -----------------------------------------
;		pha
;		phy
;		ldy		#$00       		; Save registers
;		lda		TubeR3
;		sta		(ZP_ADDR), Y     	; Get byte from Tube R3 and store
;		inc		ZP_ADDR
;		bne		@sk5
;		inc		ZP_ADDR + 1  		; Increment transfer address
;@sk5:		lda		TubeR3
;		sta		(ZP_ADDR), Y		; Get byte from Tube R3 and store
;		inc		ZP_ADDR
;		bne		@sk6
;		inc		ZP_ADDR + 1		; Increment transfer address
;@sk6:		ply					; Restore registers and return
;		pla
;		rti

NMI3:		; Transfer 3 - Transfer two bytes from Tube
		; -----------------------------------------
		sep		#PF_A16			; force 8 bit mode
		pha
		lda		TubeR3
		sta		(ZP_ADDR)	     	; Get byte from Tube R3 and store
		inc		ZP_ADDR
		bne		@sk5
		inc		ZP_ADDR + 1  		; Increment transfer address
@sk5:		lda		TubeR3
		sta		(ZP_ADDR)		; Get byte from Tube R3 and store
		inc		ZP_ADDR
		bne		@sk6
		inc		ZP_ADDR + 1		; Increment transfer address
@sk6:		pla
		rti

		; Data transfer address pointers
		; ------------------------------
Tbl_DataAddrLO:	.lobytes (NMI0Addr+1),(NMI1Addr+1), ZP_ADDR, ZP_ADDR, ZP_ADDR, ZP_ADDR, (NMI6Addr+1), (NMI7Addr+1)
Tbl_DataAddrHI:	.hibytes (NMI0Addr+1),(NMI1Addr+1), ZP_ADDR, ZP_ADDR, ZP_ADDR, ZP_ADDR, (NMI6Addr+1), (NMI7Addr+1)

		; Data transfer routine addresses
		; -------------------------------
Tbl_DataTransLO:.lobytes NMI0, NMI1, NMI2, NMI3, NMI_Ack, NMI_Ack, NMI_Ack, NMI_Ack
Tbl_DataTransHI:.hibytes NMI0, NMI1, NMI2, NMI3, NMI_Ack, NMI_Ack, NMI_Ack, NMI_Ack

		; Wait for byte in Tube R1 while allowing requests via Tube R4
		; ============================================================
waitR1noblockR4:
		bit		TubeS1
		bmi		lda_TubeR1      	; If data in Tube R1, jump to fetch it
		bit		TubeS4          	; Check if data present in Tube R4
		bpl		waitR1noblockR4 	; If nothing there, jump back to check Tube R1
		lda		ZP_IRQA			; Save IRQ's A store in A register
		php
		cli
		plp					; Allow an IRQ through to process R4 request
		sta		ZP_IRQA
		jmp		waitR1noblockR4 	; Restore IRQ's A store and jump back to check R1

lda_TubeR1:	lda		TubeR1			; Fetch byte from Tube R1 and return
		rts

PrText:		; =====================
		; Print embedded string
		; =====================
		php
		sep		#PF_A16			; ensure 8 bit A
		rep		#PF_I16			; 16 bit Y
		phy
		.a8
		.i16
		ldy		#0
inc_FAFB:	clc
		lda		4, S
		adc		#1
		sta		4, S
		lda		5, S
		adc		#0
		sta		5, S
print_str_FA:	lda		(4, S), Y		; Print character and loop back for more
		bmi		jmp_ind_FA      	; Get character, exit if >$7f
		jsr		OSWRCH
		bra		inc_FAFB
jmp_ind_FA:	ply
		plp					; restore reg sizes
		rts                			; Jump back to code after string

NMI_Ack:	sta		TubeR3			; Store to TubeR3 to acknowlege NMI
		rti

COPHandler:	jmp		(IND1V)

Hex4:		; output low nibble of A as hex char
		and		#$F
		cmp		#$A
		bcs		@sk1
		adc		#'0'
		jmp		OSWRCH
@sk1:		adc		#'A' - 11
		jmp		OSWRCH

Hex8:		pha
		ror		A
		ror		A
		ror		A
		ror		A
		jsr		Hex4
		pla
		jmp		Hex4

STACK_TOP:	.res		2


TEST_VECTO:

		.a8
		.i8
brkNatVec:	sec
		xce
		brk
		.byte		45, "Native Vector Called", 0
		nop
		stp

NAT_VECT:	.addr		TEST_VECTO		; native XXX 	FFE0
		.addr		TEST_VECTO		; native XXX 	FFE2
		.addr		COPHandler		; native COP 	FFE4
		.addr		BrkHandlerNAT		; native BRK 	FFE6
		.addr		TEST_VECTO		; native ABORT 	FFE8
		.addr		NMI0			; native NMI	FFEA
		.addr		TEST_VECTO		; native XXX	FFEC
		.addr		InterruptHandlerNAT	; native IRQ 	FFEE

		.addr		TEST_VECTO		; emu XXX 	FFF0
		.addr		TEST_VECTO		; emu XXX 	FFF2
		.addr		COPHandler		; emu COP 	FFF4
		.addr		TEST_VECTO		; emu BRK 	FFF6
		.addr		TEST_VECTO		; emu ABORT 	FFF8
		.addr		NMI0			; emu NMI	FFFA
		.addr		RESET			; emu RESET	FFFC
		.addr		InterruptHandler	; emu IRQ 	FFFE

		.segment	"TUBE_REGS"
		.org		$FEF0
		.byte		"Ishbel Beesley15"

		.segment	"DEF_VEC"
		.org		$FF00
		.res		$80
TBL_DefVectors:
		.addr Unsupported      		; $200 - USERV
		.addr ErrorHandler     		; $202 - BRKV
		.addr IRQ1Handler      		; $204 - IRQ1V
		.addr Unsupported      		; $206 - IRQ2V
		.addr do_OSCLI            	; $208 - CLIV
		.addr do_OSBYTE           	; $20a - BYTEV
		.addr do_OSWORD           	; $20c - WORDV
		.addr do_OSWRCH           	; $20e - WRCHV
		.addr do_OSRDCH           	; $210 - RDCHV
		.addr do_OSFILE           	; $212 - FILEV
		.addr do_OSARGS           	; $214 - ARGSV
		.addr do_OSBGET           	; $216 - BGetV
		.addr do_OSBPUT           	; $218 - BPutV
		.addr do_OSGBPB           	; $21a - GBPBV
		.addr do_OSFIND           	; $21c - FINDV
		.addr Unsupported      		; $21e - FSCV
		.addr NullReturn       		; $220 - EVNTV
		.addr Unsupported      		; $222 - UPTV
		.addr Unsupported      		; $224 - NETV
		.addr Unsupported      		; $226 - VduV
		.addr Unsupported      		; $228 - KEYV
		.addr Unsupported      		; $22a - INSV
		.addr Unsupported      		; $22c - RemV
		.addr Unsupported      		; $22e - CNPV
		.addr MonCOPHandler    		; $230 - IND1V			; DB: This is now used as the COP vector both N and E
		.addr NullReturn       		; $232 - IND2V
		.addr NullReturn       		; $234 - IND3V
TBL_DefVectorsEnd:


		.SEGMENT "OS_VEC"
		.org		$FFB6
VECDEF:	 	.byte		TBL_DefVectorsEnd - TBL_DefVectors
		.addr		TBL_DefVectors
OSXXX0:	 	jmp		Unsupported	; $ffb9
OSXXX1:	 	jmp		Unsupported	; $ffbc
OSXXX2:	 	jmp		Unsupported	; $ffbf
OSXXX3:	 	jmp		Unsupported	; $ffc2
OSXXX4:	 	jmp		Unsupported	; $ffc5
NVRDCH:		jmp		do_OSRDCH	; $ffc8
NVWRCH:	 	jmp		do_OSWRCH	; $ffcb
OSFIND:	 	jmp		(FINDV)		; $ffce
OSGBPB:	 	jmp		(GBPBV)		; $ffd1
OSBPUT:	 	jmp		(BPutV)		; $ffd4
OSBGET:	 	jmp		(BGetV)		; $ffd7
OSARGS:	 	jmp		(ARGSV)		; $ffda
OSFILE:	 	jmp		(FILEV)		; $ffdd
OSRDCH:	 	jmp		(RDCHV)		; $ffe0
OSASCI:	 	cmp		#$0d		; $ffe3
		bne		OSWRCH
OSNEWL:	 	lda		#$0a		; $ffe7
		jsr		OSWRCH
OSWRCR:	 	lda		#$0d		; $ffec
OSWRCH:		jmp		(WRCHV)		; $ffee
OSWORD:		jmp		(WORDV)		; $fff1
OSBYTE:	 	jmp		(BYTEV) 	; $fff4
OS_CLI:	 	jmp		(CLIV)		; $fff7


		.segment "CPU_VEC"
		.org		$FFFA
NMIV:	   	.addr 		NMI0  		; 6502 NMI Vector ; $fffa
RESETV:	 	.addr 		RESET           ; RESET Vector ; $fffc
IRQV:	   	.addr 		InterruptHandler; IRQ Vector ; $fffe
