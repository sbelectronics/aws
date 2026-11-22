; serio.asm - port input and output for serial.c, and the interrupt routines
; CTOS needs when a program takes a serial line. serial.c turns the line's
; interrupts off and polls instead, so these only end an interrupt if one
; comes. 38h is the 7201/8274 end-of-interrupt command.

        .8086

DGROUP  group   _TEXT
_TEXT   segment word public 'CODE'
        assume  cs:DGROUP

        public  ser_inb_, ser_outb_, ser_isr_init_, ser_isr_set_

; void __far *ser_isr_init(void), *ser_isr_set(void): the routines' far
; addresses, made at run time because the run file has no relocations
ser_isr_init_:
        mov     dx, cs
        mov     ax, offset DGROUP:SerIsrInit
        ret

ser_isr_set_:
        mov     dx, cs
        mov     ax, offset DGROUP:SerIsrSet
        ret

; unsigned ser_inb(unsigned port): AX = port
ser_inb_:
        push    dx
        mov     dx, ax
        in      al, dx
        xor     ah, ah
        pop     dx
        ret

; void ser_outb(unsigned port, unsigned v): AX = port, DX = v
ser_outb_:
        push    dx
        xchg    ax, dx
        out     dx, al
        pop     dx
        ret

; For InitCommLine (CTOS II): DX is the line's command port.
SerIsrInit proc far
        mov     al, 38h
        out     dx, al
        ret     4
SerIsrInit endp

; For SetCommISR (the AWS's CTOS): the line number is on the stack, and the
; end of interrupt goes to port A's command port.
SerIsrSet proc far
        push    ax
        mov     al, 38h
        out     0A9h, al
        pop     ax
        ret     2
SerIsrSet endp

_TEXT   ends
        end
