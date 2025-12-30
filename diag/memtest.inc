		;------------------------------------------
		; Memory Test
		;
		; Note: I call these "banks", but on the 8086 board there are 9 even and 9 odd chips, so
		;          banks 0+1 = 18 ICs
		;          banks 2+3 = 18 ICs
		;	   banks 4+5 = 18 ICs
		;	   banks 6+7 = 18 ICs
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
		jmp 	ver1
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
		mov	ax, 4000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop4:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop4			; Decrement CX and repeat until CX is 0

		mov	bx, 5678h			; starting pattern to write
		mov	ax, 4000h
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
		jmp 	ver5
ver_Failed4:
		mov dh, BEEP_BANK4BAD
		jmp ram_error

		; memory test bank 5

ver5:
		mov	bx, 6789h			; starting pattern to write
		mov	ax, 5000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop5:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop5			; Decrement CX and repeat until CX is 0

		mov	bx, 6789h			; starting pattern to write
		mov	ax, 5000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop5:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed5 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop5       ; Decrement CX and repeat until CX is 0
		jmp 	ver6
ver_Failed5:
		mov dh, BEEP_BANK5BAD
		jmp ram_error

		; memory test bank 6

ver6:
		mov	bx, 789Ah			; starting pattern to write
		mov	ax, 6000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop6:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop6			; Decrement CX and repeat until CX is 0

		mov	bx, 789Ah			; starting pattern to write
		mov	ax, 6000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop6:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed6 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop6       ; Decrement CX and repeat until CX is 0
		jmp 	ver7
ver_Failed6:
		mov dh, BEEP_BANK6BAD
		jmp ram_error

		; memory test bank 7

ver7:
		mov	bx, 89ABh			; starting pattern to write
		mov	ax, 7000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
write_loop7:	mov	es:[di], bx			; Write BX to ES:DI
		add	di, 2				; Increment DI by 2 (BX is 2 bytes)
		add	bx, MEM_INC
		loop	write_loop7			; Decrement CX and repeat until CX is 0

		mov	bx, 89ABh			; starting pattern to write
		mov	ax, 7000h
		mov	es, ax
		mov	di, 0
		mov	cx, 07FFFh			; Set CX to 65535 (loop counter)
verify_loop7:
        	mov     ax, es:[di]        ; Read the value at ES:DI
        	cmp     ax, bx             ; Compare it with the expected value in BX
        	jne     ver_failed7 	   ; If not equal, jump to failure handler
        	add     di, 2              ; Increment DI by 2 (AX is 2 bytes)
        	add     bx, MEM_INC        ; Increment BX to match the next expected value
        	loop    verify_loop7       ; Decrement CX and repeat until CX is 0
		jmp 	verDone
ver_Failed7:
		mov dh, BEEP_BANK6BAD
		jmp ram_error				
		
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

verDone:	; this is where we exit memtest. Continue running in parent.