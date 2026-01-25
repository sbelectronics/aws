; Functions for printing stuff.
; Scott Baker, https://www.smbaker.com/

		; doPrintstring
		;    ax contains offset of string
		;    bx contains length of string

doPrintString	PROC	NEAR
		push	cx
		push	dx
		push	si
		push	di
		push	ds			; push address of bsVid
		lea	cx, bsVid
		push	cx
		push	ds			; push address of string
		push	ax
		push	bx			; push length of string
		push 	ds			; push address of cbWrittenRet
		lea	ax, cbWrittenRet
		push	ax
		call	WriteBsRecord
		%CheckError(ax)
		pop	di
		pop	si
		pop	dx
		pop	cx
		ret
doPrintString	ENDP

		; printbyte
		;       ax contains byte

doPrintChar	PROC	NEAR
		push	bx
		push	cx
		push	dx
		push	si
		push	di
		push	ds
		lea	cx, bsVid
		push	cx
		push	ax
		call	WriteByte
		%CheckError(ax)
		pop 	di
		pop	si
		pop	dx
		pop	cx
		pop	bx
		ret
doPrintChar	ENDP

		; printHex
		;       print hex value in AX

doPrinthex      PROC    NEAR
		push	bx
		push    cx
		push	dx
		push	si
		push	di

                mov     cx,4
print1:         push    cx
                mov     cl, 4
                rol     ax, cl
                push    ax

                mov     bx,ax
                and     bl,0FH
                add     bl,'0'
                cmp     bl,'9'
                jbe     print2
                add     bl,'A'-'0'-10
print2:
                push    ds
                lea     ax,bsVid
                push    ax
                push    bx
                call    writeByte
		%CheckError(ax)

                pop     ax
                pop     cx
                loop    print1

		pop	di
		pop	si
		pop	dx
		pop	cx
		pop	bx
                ret
doPrinthex      ENDP

		; printHex2
		;       print hex value in AL

doPrinthex2     PROC    NEAR
		push	bx
		push    cx
		push	dx
		push	si
		push	di

		mov	ah, al

                mov     cx,2
print21:        push    cx
                mov     cl, 4
                rol     ax, cl
                push    ax

                mov     bx,ax
                and     bl,0FH
                add     bl,'0'
                cmp     bl,'9'
                jbe     print22
                add     bl,'A'-'0'-10
print22:
                push    ds
                lea     ax,bsVid
                push    ax
                push    bx
                call    writeByte
		%CheckError(ax)

                pop     ax
                pop     cx
                loop    print21

		pop	di
		pop	si
		pop	dx
		pop	cx
		pop	bx
                ret
doPrinthex2     ENDP
