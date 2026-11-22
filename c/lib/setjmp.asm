; setjmp.asm - setjmp and longjmp for Open Watcom's register convention: the
; first argument comes in AX, the second in DX, and a function keeps every
; register but AX. So jmp_buf saves BX, CX, DX, SI, DI, BP, SP and the return
; address, and longjmp puts them all back.

        .8086

DGROUP  group   _TEXT
_TEXT   segment word public 'CODE'
        assume  cs:DGROUP, ds:DGROUP

        public  setjmp_, longjmp_

; int setjmp(jmp_buf env): AX = env
setjmp_:
        xchg    ax, bx                  ; bx = env, ax = the caller's bx
        mov     [bx], ax
        mov     [bx+2], cx
        mov     [bx+4], dx
        mov     [bx+6], si
        mov     [bx+8], di
        mov     [bx+10], bp
        mov     si, sp
        mov     ax, [si]                ; the return address
        mov     [bx+14], ax
        lea     ax, [si+2]              ; sp after the return
        mov     [bx+12], ax
        mov     si, [bx+6]
        mov     bx, [bx]
        xor     ax, ax
        ret

; void longjmp(jmp_buf env, int val): AX = env, DX = val. setjmp then returns
; val, or 1 if val is 0.
longjmp_:
        mov     bx, ax
        mov     ax, dx
        or      ax, ax
        jnz     lj1
        inc     ax
lj1:    mov     sp, [bx+12]
        push    word ptr [bx+14]
        mov     cx, [bx+2]
        mov     dx, [bx+4]
        mov     si, [bx+6]
        mov     di, [bx+8]
        mov     bp, [bx+10]
        mov     bx, [bx]
        ret

_TEXT   ends
        end
