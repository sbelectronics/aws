EXTRN		OpenByteStream:FAR, WriteByte:FAR, WriteBsRecord:FAR, ErrorExit:FAR, Exit:FAR
EXTRN		CParams:FAR, CSubParams:FAR, RgParam:FAR

Main		SEGMENT WORD	'Code'
Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'
Data		ENDS

Const		SEGMENT WORD	PUBLIC	'Const'
Const		ENDS

Stack		SEGMENT STACK   'Stack'
STACK 		ENDS

Dgroup		GROUP	Const, Data, Stack

Main		SEGMENT
ASSUME		CS: Main

%*DEFINE (PrintString(strOffset,strLen))(
		push	ax
		push	bx
		lea	ax, %strOffset
		mov	bx, %strLen
		call	doPrintString
		pop	bx
		pop	ax
)

%*DEFINE (PrintChar(theChar))(
		push	ax
		mov	ax, %theChar
		call	doPrintChar
		pop	ax
)

%*DEFINE (PrintHex(theValue))(
		push	ax
		mov	ax, %theValue
		call	doPrintChar
		pop	ax
)

Begin:		mov 	ax, Dgroup
		mov 	SS, AX
ASSUME		SS: Dgroup
		mov	SP, OFFSET Dgroup:wLimStack

		mov	DS, AX
ASSUME		DS: Dgroup

		%PrintString(banner, bannerLen)
		mov	ax, 1234h
		%PrintHex(ax)
		%PrintChar(0Ah)

		call	Exit

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
		and	ax,ax
		jne	error
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
		and	ax,ax
		jne	error
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

error:		push	ax
		call	ErrorExit

Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'

EXTRN		bsVid:BYTE			; existing video bytestream
	
cbWrittenRet	DW	?

Data		ENDS

Const		SEGMENT WORD	PUBLIC	'Const'

banner		DB	'Parameter Passing Demo',0Ah
bannerLen	DW	SIZE banner

Const 		ENDS

Stack		SEGMENT STACK
wLimStack	EQU	THIS	WORD
STACK 		ENDS

		END Begin