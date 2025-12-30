		assume CS:CODE, DS:CODE, SS:CODE, ES:CODE

MEM_INC		equ 0377h  ; 377h == 887d == some prime number to use as a memory increment, to assure a good pattern

BEEP_OKAY	equ 1
BEEP_BANK0BAD	equ 2
BEEP_BANK1BAD	equ 3
BEEP_BANK2BAD	equ 4
BEEP_BANK3BAD	equ 5
BEEP_BANK4BAD	equ 6

		org 0h					; start of 2732 EPROM

		; THIS IS WHERE WE START

		; do a couple tones on startup
		; do this without any CALLs, in case RAM is bad

boot_sound:	mov	al, 36h				; turn on speaker and make tone for 8000 loop cycles
		out 	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 6h
		out	40h, al
		mov	cx, 8000h
snd_delay0:	loop	snd_delay0
		mov	al, 36h
		out	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 5h
		out	40h, al
		mov	cx, 8000h
snd_delay1:	loop	snd_delay1
		mov	al, 36h
		out	46h, al
		mov	cx, 0F000h			; wait for silence
snd_delay2:	loop	snd_delay2

		;------------------------------------------
		; Memory Test
		;------------------------------------------

		; memory test bank 0

ver0:
		mov	bx, 1234h			; starting pattern to write
		mov	ax, 0000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop0:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop0			; Decrement CX and repeat until CX is 0

		mov	bx, 1234h			; starting pattern to write
		mov	ax, 0000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop0:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed0 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop0       ; Decrement CX and repeat until CX is 0
		jmp 	ver1 ; xxx
ver_Failed0:
		mov dh, BEEP_BANK0BAD
		jmp ram_error

		; memory test bank 1

ver1:
		mov	bx, 2345h			; starting pattern to write
		mov	ax, 1000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop1:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop1			; Decrement CX and repeat until CX is 0

		mov	bx, 2345h			; starting pattern to write
		mov	ax, 1000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop1:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed1 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop1       ; Decrement CX and repeat until CX is 0
		jmp 	ver2
ver_Failed1:
		mov dh, BEEP_BANK1BAD
		jmp ram_error

		; memory test bank 2

ver2:
		mov	bx, 3456h			; starting pattern to write
		mov	ax, 2000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop2:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop2			; Decrement CX and repeat until CX is 0

		mov	bx, 3456h			; starting pattern to write
		mov	ax, 2000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop2:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed2 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop2       ; Decrement CX and repeat until CX is 0
		jmp 	ver3
ver_Failed2:
		mov dh, BEEP_BANK2BAD
		jmp ram_error

		; memory test bank 3

ver3:
		mov	bx, 4567h			; starting pattern to write
		mov	ax, 3000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop3:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop3			; Decrement CX and repeat until CX is 0

		mov	bx, 4567h			; starting pattern to write
		mov	ax, 3000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop3:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed3 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop3       ; Decrement CX and repeat until CX is 0
		jmp 	ver4
ver_Failed3:
		mov dh, BEEP_BANK3BAD
		jmp ram_error

		; memory test bank 4

ver4:
		mov	bx, 5678h			; starting pattern to write
		mov	ax, 5000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop4:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop4			; Decrement CX and repeat until CX is 0

		mov	bx, 5678h			; starting pattern to write
		mov	ax, 5000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop4:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed4 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop4       ; Decrement CX and repeat until CX is 0
		jmp 	verDone
ver_Failed4:
		mov dh, BEEP_BANK4BAD
		jmp ram_error						

verDone:	mov dh, BEEP_OKAY
		jmp beep_count

		;------------------------------------------
		; ram_error - 
		;    1. Beep out the code in DH
		;    2. Compare AX and BX. For each of the 16 bits that differs, emit a high tone. For each bit that is the same, output a low tone. 
		;------------------------------------------

ram_error:
		xor bx, ax 				; BX = BX xor AX

beep_count3:	mov	al, 36h				; turn on speaker and make tone for 8000 loop cycles
		out 	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 4h
		out	40h, al
		mov	cx, 8000h
snd_delay3:	loop	snd_delay3
		mov	al, 36h				; turn off speaker
		out	46h, al
		mov	cx, 8000h			; wait for silence
sil_delay3:	loop	sil_delay3
		dec	dh
		cmp	dh, 0
		jne	beep_count3

		mov 	dh, 10h
beep_bit:	ror	bx, 1
		jnc	lowbit

		mov	al, 36h				; turn on speaker and make tone for 8000 loop cycles
		out 	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 5h
		out	40h, al
		mov	cx, 8000h
snd_delay4:	loop	snd_delay4
		jmp	nextbit

lowbit:		mov	al, 36h				; turn on speaker and make tone for 8000 loop cycles
		out 	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 6h
		out	40h, al
		mov	cx, 8000h
snd_delay5:	loop	snd_delay5

nextbit:	mov	al, 36h				; turn off speaker
		out	46h, al
		mov	cx, 0FFFFh			; wait for silence
sil_delay6:	loop	sil_delay6

		dec	dh
		cmp	dh, 0
		jnz	beep_bit			; walk through the remaining bits

		jmp 	forever


		;------------------------------------------
		; beep_count - beep the number of times in dh, then stop running
		;------------------------------------------

 beep_count:	mov	al, 36h				; turn on speaker and make tone for 8000 loop cycles
		out 	46h, al
		mov	al, 0h
		out	40h, al
		mov	al, 4h
		out	40h, al
		mov	cx, 8000h
snd_delay:	loop	snd_delay
		mov	al, 36h				; turn off speaker
		out	46h, al
		mov	cx, 8000h			; wait for silence
sil_delay:	loop	sil_delay
		dec	dh
		cmp	dh, 0
		jne	beep_count
forever:	jmp	forever

		org 0FF0h

		jmpf	0FF00h:0h			; this is where the 8086 starts
		db	0ffh, 82h, 00h, 04dh		; copied this stuff from original bios
		db	0ffh, 05h			; ... this probably doesn't matter....
		db	00h, 04h
		db	00h, 082h, 00h
