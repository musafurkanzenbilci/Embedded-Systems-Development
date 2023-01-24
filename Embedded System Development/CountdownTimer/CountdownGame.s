PROCESSOR 18F8722
    
#include <xc.inc>

; configurations
CONFIG OSC = HSPLL, FCMEN = OFF, IESO = OFF, PWRT = OFF, BOREN = OFF, WDT = OFF, MCLRE = ON, LPT1OSC = OFF, LVP = OFF, XINST = OFF, DEBUG = OFF

; global variable declarations
GLOBAL var1
GLOBAL pb
GLOBAL tempa4
GLOBAL tempe4
GLOBAL statusB
GLOBAL statusC
GLOBAL statusD
GLOBAL countdown
GLOBAL stateless

; allocating memory for variables
PSECT udata_acs
    var1:
	DS     1    ; allocates 1 byte
    l1:
	DS     1    ; allocates 1 byte
    l2:
	DS     1    ; allocates 1 byte
    l3:
	DS     1
        ; allocates 1 byte
    pb:
	DS     1
        ; allocates 1 byte
    pc:
	DS     1
        ; allocates 1 byte
    pd:
	DS     1
    tempa4:
	DS	1  
    tempe4:
	DS	1
    statusB:
	DS	1
    statusC:
	DS	1
    statusD:
	DS	1
    selectedPort:
	DS	1
    firsttime:
	DS	1
    blink:
	DS	1
    countdown:
	DS	1
    stateless:
	DS	1
    

PSECT resetVec,class=CODE,reloc=2
resetVec:
    goto    main

; DO NOT DELETE OR MODIFY
; 500ms pass check for test scripts
ms500_passed:
    nop
    return

; DO NOT DELETE OR MODIFY
; 1sec pass check for test scripts
ms1000_passed:
    nop
    return

    
configC:
    btg statusC, 0
    btg statusC, 1
    btfsc blink, 0
    movff statusC, PORTC
    goto end3
    
configB:
    movlw 0x00
    btfsc statusB, 3
    movwf statusB
    rlncf statusB
    movlw 0x01
    addwf statusB
    btfsc blink, 0
    movff statusB, PORTB
    goto end3
    
    
stateC:
    bsf selectedPort, 0
    movff statusB, PORTB
    goto end3

delay500:
    btfss firsttime, 0
    goto begin
    btfss  selectedPort, 0
    goto checkB
    goto checkC0
    checkB:
	btfss PORTB, 0
	goto fillB
	goto clearB
    checkC0:
	btfss PORTC, 0
	goto checkC1
	goto clearC
    checkC1:
	btfss PORTC, 1
	goto fillC
	goto clearC
    clearB:
	movlw 0x00
	movwf PORTB
	movlw 0x00
	movwf blink
	goto begin
    clearC:
	movlw 0x00
	movwf PORTC
	movlw 0x00
	movwf blink
	goto begin
    fillB:
	movff statusB, PORTB
	movlw 0x01
	movwf blink
	goto begin
    fillC:
	movff statusC, PORTC
	movlw 0x01
	movwf blink
	goto begin
    begin:
    bsf firsttime, 1
    movlw 0x80
    movwf l1
    hloop1:
	movlw 0xF0
	movwf l2
	hloop2:
	    movlw 0x08
	    movwf l3
	    hloop3:
		btfsc tempe4, 0
		goto re4cl
		btfsc tempa4, 0
		goto ra4cl
		goto re4l
		re4l:
		    btfss  PORTE, 4
		    goto	ra4l
		    goto	re4cl
		ra4l:
		    btfss  PORTA, 4
		    goto	end3
		    goto	ra4cl
		ra4cl: ;check for press of RA4
		    bsf tempa4, 0
		    btfsc PORTA, 4
		    goto end3
		    bcf tempa4, 0
		    btfsc selectedPort, 0
		    goto configC
		    goto configB
		re4cl: ; check for release
		    ;tempe4 =  1
		    bsf tempe4, 0
		    btfsc PORTE, 4
		    goto end3
		    ;presse4 = 0
		    bcf tempe4, 0
		    btfsc selectedPort, 0
		    goto stateD
		    goto stateC
		end3:
		    decfsz l3, 1
		    goto hloop3
		    decfsz l2, 1
		    goto hloop2
		    decfsz l1, 1
		    goto hloop1
		    return

delay500only:
    movlw 0x80
    movwf l1
    loop1ly:
	movlw 0xF0
	movwf l2
	loop2ly:
	    movlw 0x30
	    movwf l3
	    loop3ly:
		decfsz l3, 1
		goto loop3ly
		decfsz l2, 1
		goto loop2ly
		decfsz l1, 1
		goto loop1ly
		return
		    
		    
delay:
    movlw 0xFF
    movwf l1
    loop1:
	movlw 0xF0
	movwf l2
	loop2:
	    movlw 0x35
	    movwf l3
	    loop3:
		decfsz l3, 1
		goto loop3
		decfsz l2, 1
		goto loop2
		decfsz l1, 1
		goto loop1
		return

		
delayNoState:
    movlw 0x80
    movwf l1
    loop1ns:
	movlw 0xF0
	movwf l2
	loop2ns:
	    movlw 0x10
	    movwf l3
	    loop3ns:
		btfss stateless, 0
		goto  re4wons
		goto  re4cwons
		re4wons:
		   btfss PORTE, 4
		   goto endns
		   bsf stateless, 0
		   goto re4cwons
	        re4cwons: ; check for release
		   btfss PORTE, 4
		   goto loop
		   goto endns
		endns:
		decfsz l3, 1
		goto loop3ns
		decfsz l2, 1
		goto loop2ns
		decfsz l1, 1
		goto loop1ns
		return
	
PSECT CODE

main:
    CLRF PORTA ;setting up registers and such
    CLRF PORTB
    CLRF PORTC
    CLRF PORTD
    CLRF PORTE
    CLRF pb
    CLRF pc
    CLRF pd
    CLRF tempa4
    CLRF tempe4
    CLRF statusB
    CLRF statusD
    CLRF statusC
    CLRF selectedPort
    CLRF firsttime
    CLRF blink
    CLRF countdown
    CLRF stateless
    
    movlw 0x00
    movwf stateless
    movlw 0x01
    movwf blink
    movlw 0x01
    movwf statusC
    movlw 0x01
    movwf statusB
    movlw 0x00
    movwf selectedPort
    movlw 0x00
    movwf firsttime
    movlw 0x00
    movwf tempe4
    movlw 0x00
    movwf tempa4
    
    movlw 0xF0
    movwf TRISB ;set all RB[0-3]  as output
    movlw 0xFC
    movwf TRISC ;set all RC0,RC1  as output
    movlw 0x00
    movwf TRISD ;set all PORTD  as output
    ; some code to initialize and wait 1000ms here, maybe
    movlw 0x0F
    movwf PORTB
    movlw 0x03
    movwf PORTC
    movlw 0xFF
    movwf PORTD
    call delay ;delay subroutine
    
    
    
    call ms1000_passed
    
    
    
    movlw 0x10
    movwf TRISE; set RE4 as input
    movlw 0x10
    movwf TRISA; set RA4 as input
    
    defaultums:
    movlw 0xF1;set to default configuration
    movwf PORTB
    movlw 0xFD
    movwf PORTC
    movlw 0x00
    movwf PORTD
    loopss:
	call delayNoState
	call ms500_passed
	goto loopss
    
    stateD:
	movff statusC, PORTC ; if PORTC is off
	; obtain the level point
	btfsc PORTB, 3
	goto por3
	btfsc PORTB, 2
	goto por2
	btfsc PORTB, 1
	goto por1
	btfsc PORTB, 0
	goto por0
	por3:
	    movlw 0x04
	    movwf statusB
	    goto    mode
	por2:
	    movlw 0x03
	    movwf statusB
	    goto    mode
	por1:
	    movlw 0x02
	    movwf statusB
	    goto    mode
	por0:
	    movlw 0x01
	    movwf statusB
	mode:
	btfss PORTC, 1
	goto attack
	goto defence
	attack: ; multiply PORT B by 1 led(500ms)
	    movlw 0x01
	    mulwf statusB ; result is in PRODL
	    goto assign
	defence: ; multiply PORTB by 2 led(1s)
	    movlw 0x02
	    mulwf statusB
	assign:
	movff PRODL, countdown
	goto Dloop
	
    Dloop:
	;configure PORTD
	
	innerLoop:
	    ;configure statusD : leftshift and add 1
	    rlncf statusD
	    movlw 0x01
	    addwf statusD
	    decfsz countdown, 1
	    goto innerLoop
	movff statusD, PORTD
	;500ms delay
	startCD:
	    call delay500only
	    after500:
	    btfss statusD, 0
	    goto default
	    bcf STATUS, 0
	    rrcf statusD
	    movff statusD, PORTD
	    goto startCD
    default:
	movlw 0xF1;set to default configuration
	movwf PORTB
	movlw 0xFD
	movwf PORTC
	movlw 0x00
	movwf PORTD
	goto loopss
    
    ; a loop here, maybe
    loop:
        ; inside the loop, once it is confirmed 500ms passed
	call delay500
        call ms500_passed
        goto loop

end resetVec