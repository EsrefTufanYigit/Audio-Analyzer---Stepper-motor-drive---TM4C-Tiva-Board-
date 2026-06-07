; InputOutput.s
; Runs on TM4C123
; Contains IOInit, InChar, OutChar and OutStr
; This file is made to replace InChar.s, OutChar.s and OutStr.s

; Baud = 9600, 8-bit, No Parity
; 1 Stop bit, No flow control

; GPIO Registers *****************
RCGCGPIO		EQU 0x400FE608	; GPIO clock register
; PORT B base address	  = 0x40004000
PORTA_DEN		EQU 0x4000451C	; Digital Enable
PORTA_PCTL		EQU 0x4000452C	; Alternate function select
PORTA_AFSEL		EQU 0x40004420	; Enable Alt functions
PORTA_AMSEL		EQU 0x40004528	; Enable analog
	
; UART Registers *****************
RCGCUART		EQU	0x400FE618	; UART clock register
	
UART0_DR		EQU	0x4000C000	; UART0 data / base address
UART0_CTL		EQU	0x4000C030	; UART0 control register
UART0_IBRD		EQU	0x4000C024	; Baud rate divisor Integer part
UART0_FBRD		EQU	0x4000C028	; Baud rate divisor Fractional part
UART0_LCRH		EQU	0x4000C02C	; UART serial parameters
UART0_FR		EQU 0x4000C018	; UART status
UART0_CC		EQU 0x4000CFC8	; UART clock configuration

; PLL Registers *****************
SYSCTL_RCC2_R   EQU 0x400FE070	; PLL control


				AREA    |.text|, READONLY, CODE, ALIGN=2
				THUMB
				EXPORT  IOInit		; make section available to other programs
				EXPORT  InChar
				EXPORT  OutChar
				EXPORT  OutStr


; ########################################################################
; Initializes UART0 and Port A (PA0, PA1) for serial I/O
; Enables GPIOA and UART0 clocks, configures alternate functions, and sets UART parameters.
; Arguments: none
IOInit			PROC
				PUSH 	{R0-R3}
				
				; Setup UART registers and GPIO 
				; Enable UART clock
				LDR		R1, =RCGCUART
				LDR		R3, [R1]
				ORR		R3, R3, #0x01			; set bit 0 to enable UART0 clock
				STR		R3, [R1]
				NOP								; Let clock stabilize
				NOP
				NOP  

; Setup GPIO **********************************************************************
				; Enable GPIO clock to use debug USB as com port (PA0, PA1)
				LDR		R1, =RCGCGPIO
				LDR		R3, [R1]
				ORR		R3, R3, #0x01			; set bit 0 to enable port A clock
				STR		R3, [R1]
				NOP								; Let clock stabilize
				NOP
				NOP
				
				
				; Make PA0, PA1 digital
				LDR		R1, =PORTA_DEN
				LDR		R3, [R1]
				ORR		R3, R3, #0x03			; set bits 1,0 to enable alt functions on PA0, PA1
				STR		R3, [R1]
				
				; Disable analog on PA0, PA1
				LDR		R1, =PORTA_AMSEL
				LDR		R3, [R1]
				BIC		R3, R3, #0x03			; clear bits 1,0 to disable analog on PA0, PA1
				STR		R3, [R1]
				
				; Enable alternate functions selected
				LDR		R1, =PORTA_AFSEL
				LDR		R3, [R1]
				ORR		R3, R3, #0x03			; set bits 1,0 to enable alt functions on PA0, PA1
				STR		R3, [R1]
				

				; Select alternate function to be used (UART on PA0, PA1)
				LDR		R1, =PORTA_PCTL
				LDR		R3, [R1]
				ORR		R3, R3, #0x11			; set bits 4,0 to select UART Rx, Tx
				STR		R3, [R1]
	

; Setup UART *****************************************************************
				;UART0
				; Disable UART while setting up
				LDR		R1, =UART0_CTL
				LDR		R3, [R1]
				BIC		R3, R3, #0x01			; Clear bit 0 to disable UART0 while
				STR		R3, [R1]				; setting up
				
				; Set UART clock to be the 16MHz precision oscillator (bypass any PLL)
				LDR		R1, =UART0_CC
				LDR		R3, [R1]
				BIC     R3, R3, #0xF
				ORR     R3, R3, #0x5			; set to PIOSC
				STR		R3, [R1]
				
				; Set baud rate to 9600. Divisor = 16MHz/(16*9600)= 104.16666
				; Set integer part to 104
				LDR		R1, =UART0_IBRD
				MOV		R3, #104
				STR		R3, [R1]
				
				; Set fractional part
				;	0.16666*64+0.5 = 11.16666 => Integer = 11
				LDR		R1, =UART0_FBRD
				MOV		R3, #11
				STR		R3, [R1]
				
				; Set serial parameters
				LDR		R1, =UART0_LCRH
				MOV		R3, #0x70				; No stick parity, 8bit, FIFO enabled, 
				STR		R3, [R1]				; One stop bit, Disable parity, Normal use
				
				; Enable UART, TX, RX
				LDR		R1, =UART0_CTL
				LDR		R3, [R1]
				MOV		R2, #0x00000301			; Set bits 9,8,0
				ORR		R3, R3, R2
				STR		R3, [R1]	
									
				; let UART settle
				MOV		R2, #512				; wait for about 1 bit time (16.000.000/3/9600)
wait_uart		SUBS	R2, #1
				BNE		wait_uart
	
				POP 	{R0-R3}
				BX		LR
				ENDP  ; IOInit


; ########################################################################
; Capture ASCII character via 
; UART0 in PortA, then store
; character in R0
InChar			PROC
				PUSH 	{R1-R2}
				
				
				LDR		R2, =UART0_DR				; load UART data address
				LDR 	R1, =UART0_FR				; load UART status register address
				
				; check for incoming character
check			LDR		R0, [R1]					; 
				ANDS	R0,R0,#0x10					; check if char received (RXFE is 0)
				BNE		check						; if no character, check again, else
				LDR		R0, [R2]					; load received char into R0
	
				POP 	{R1-R2}
				BX		LR
				ENDP  ; InChar

; ########################################################################
; Sends ASCII character placed in R0
; out through UART0
OutChar			PROC
				PUSH 	{R0-R3}
				

				
				LDR 	R1, =UART0_FR				; load flag register location

				; check if UART is ready (buffer is empty)
waitR			LDR		R3, [R1]					; load UART status register address
				ANDS	R3,R3,#0x20                 ; check if TXFF = 1
				BNE 	waitR	                    ; If so, UART is full, so wait / check again
				
				
				LDR		R2, =UART0_DR				; load R2 with UART data address
				STR		R0,[R2]						; Put character in UART data register	
	
				; check if UART is done transmitting
waitD			LDR		R3, [R1]
				ANDS	R3,R3,#0x08                 ; check if BUSY = 1
				BNE 	waitD	                    ; If so, UART is busy, so wait / check again
		
				POP 	{R0-R3}
				BX		LR
				ENDP  ; OutChar



; ########################################################################
; Prints a string located in memory
; to UART0
; Starting address of string is passed
; to R0
OutStr			PROC
				PUSH 	{R0-R2, LR}
				
				MOV		R1, R0						; address is moved to R1

loop			LDRB	R0, [R1],#1					; load character, post inc address
				CMP		R0,#0x04					; has end character been reached?
				BEQ		done						; if so, end
				BL		OutChar						; write the char using OutChar
				B   	loop

done			POP 	{R0-R2, LR}
				BX		LR
				ENDP  ; OutStr


				ALIGN
				END ;