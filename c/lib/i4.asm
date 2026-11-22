; i4.asm - the 32-bit multiply and divide helpers Open Watcom calls for long
; arithmetic on a 16-bit target.
;
; __U4M, __I4M: DX:AX = DX:AX * CX:BX (the low 32 bits are the same signed or not)
; __U4D, __I4D: DX:AX = DX:AX / CX:BX, remainder in CX:BX
; (signed: the quotient rounds toward zero, the remainder has the dividend's sign)

        .8086

DGROUP  group   _TEXT
_TEXT   segment word public 'CODE'
        assume  cs:DGROUP

        public  __U4M, __I4M
__U4M:
__I4M:  push    cx
        push    si
        mov     si, ax                  ; a lo
        mov     ax, dx
        mul     bx                      ; a hi * b lo
        xchg    ax, cx                  ; cx = a hi * b lo, ax = b hi
        mul     si                      ; b hi * a lo
        add     cx, ax
        mov     ax, si
        mul     bx                      ; a lo * b lo
        add     dx, cx
        pop     si
        pop     cx
        ret

        public  __U4D
__U4D:  push    si
        push    di
        push    bp
        xor     si, si                  ; remainder, di:si
        xor     di, di
        mov     bp, 32
u4d1:   shl     ax, 1                   ; shift the next dividend bit into the remainder;
        rcl     dx, 1                   ; the quotient fills dx:ax from the bottom
        rcl     si, 1
        rcl     di, 1
        cmp     di, cx
        jb      u4d3
        ja      u4d2
        cmp     si, bx
        jb      u4d3
u4d2:   sub     si, bx
        sbb     di, cx
        inc     ax
u4d3:   dec     bp
        jnz     u4d1
        mov     bx, si
        mov     cx, di
        pop     bp
        pop     di
        pop     si
        ret

        public  __I4D
__I4D:  push    si
        push    di
        mov     si, dx                  ; the dividend's sign
        mov     di, dx
        xor     di, cx                  ; the quotient's sign
        test    dx, dx
        jns     i4d1
        neg     dx
        neg     ax
        sbb     dx, 0
i4d1:   test    cx, cx
        jns     i4d2
        neg     cx
        neg     bx
        sbb     cx, 0
i4d2:   call    __U4D
        test    di, di
        jns     i4d3
        neg     dx
        neg     ax
        sbb     dx, 0
i4d3:   test    si, si
        jns     i4d4
        neg     cx
        neg     bx
        sbb     cx, 0
i4d4:   pop     di
        pop     si
        ret

_TEXT   ends
        end
