EXTRN		OpenByteStream:FAR, WriteByte:FAR, WriteBsRecord:FAR, ErrorExit:FAR, Exit:FAR

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

Begin:		mov 	ax, Dgroup
		mov 	SS, AX
ASSUME		SS: Dgroup
		mov	SP, OFFSET Dgroup:wLimStack

		mov	DS, AX
ASSUME		DS: Dgroup

		lea	ax, banner
		mov	bx, bannerLen
		call	printString

		call	Exit

		; printstring
		;    ax contains offset of string
		;    bx contains length of string

printString:	push	cx
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
		pop	cx
		ret

		; printbyte
		;       ax contains byte

printChar:	push	cx
		push	ds
		lea	cx, bsVid
		push	cx
		push	ax
		call	WriteByte
		and	ax,ax
		jne	error
		pop	cx
		ret

error:		push	ax
		call	ErrorExit

Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'

EXTRN		bsVid:BYTE			; existing video bytestream
	
cbWrittenRet	DW	?

Data		ENDS

Const		SEGMENT WORD	PUBLIC	'Const'

banner		DB	'Scott Was Here',0Ah
bannerLen	DW	SIZE banner

Const 		ENDS

Stack		SEGMENT STACK
wLimStack	EQU	THIS	WORD
STACK 		ENDS

		END Begin