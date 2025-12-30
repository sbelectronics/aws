		assume CS:CODE, DS:CODE, SS:CODE, ES:CODE

MEM_INC		equ 0377h  ; 377h == 887d == some prime number to use as a memory increment, to assure a good pattern

BEEP_OKAY	equ 1
BEEP_BANK0BAD	equ 2
BEEP_BANK1BAD	equ 3
BEEP_BANK2BAD	equ 4
BEEP_BANK3BAD	equ 5
BEEP_BANK4BAD	equ 6
BEEP_BANK5BAD	equ 7
BEEP_BANK6BAD	equ 8
BEEP_BANK7BAD	equ 9
BEEP_NMI	equ 10

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

		mov 	ax, 0h				; install NMI handler to 0000:0008
		mov     es, ax
		mov	bx, nmi
		mov	es:[0008h], bx			; remember, it's little endian, so offset first
		mov	bx, 0FF00h
		mov	es:[000Ah], bx			; then segment

		cli					; turn off interrupts, for good measure...

		;in	al,0F4h				; disable parity. It's automatically disabled on reset.

							; XXX I think reading from ROM will generate parity errors.
		;in      al,0F0h 			; enable parity if you dare. This uses NMI and is not maskable.

kbd_setup:      xor     dx,dx                         ; setup the keyboard channel B for setting LEDS on the 7201, from BIOS
                mov     al,18h
                out     66h,al
                mov     al,4h
                out     66h,al
                mov     al,0C4h
                out     66h,al
                mov     al,3h
                out     66h,al
                mov     al,0C1h
                out     66h,al
                mov     al,1h
                out     66h,al
                dec     ax
                out     66h,al
                mov     al,2h
                out     62h,al
                xor     al,al
                out     62h,al
                mov     al,5h
                out     66h,al
                mov     al,68h
                out     66h,al
                mov     al,92h
                out     64h,al

kbd_ledo:	mov al, 0b8h			; over type
		out 64h, al
		mov	cx, 0FFFFh
led_delayo:	loop	led_delayo

kbd_ledc:	mov al, 0b4h			; caps lock
		out 64h, al
		mov	cx, 0FFFFh
led_delayc:	loop	led_delayc

kbd_led0:	mov al, 0b2h			; F1
		out 64h, al
		mov	cx, 0FFFFh
led_delay0:	loop	led_delay0

kbd_led1:	mov al, 0b1h			; F2
		out 64h, al
		mov	cx, 0FFFFh
led_delay1:	loop	led_delay1

kbd_led2:	mov al, 0b0h			; turn off prior LEDs
		out 64h, al
		mov al, 0A8h			; F3
		out 64h, al
		mov	cx, 0FFFFh
led_delay2:	loop	led_delay2

kbd_led3:       mov al, 0A4h			; F7
		out 64h, al
		mov	cx, 0FFFFh
led_delay3:	loop	led_delay3

kbd_led4:       mov al, 0A2h			; F8
		out 64h, al
		mov	cx, 0FFFFh
led_delay4:	loop	led_delay4

kbd_led5:       mov al, 0A1h			; F9
		out 64h, al
		mov	cx, 0FFFFh
led_delay5:	loop	led_delay5

		mov al, 0A0h			; turn off prior LEDs
		out 64h, al

		include "memtest.inc"

		; memtest will jump to the next instruction after the "include"

okay:		mov dh, BEEP_OKAY
		jmp beep_count

nmi:		mov	dh, BEEP_NMI
		jmp	beep_count

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
