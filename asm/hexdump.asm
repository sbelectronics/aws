; Parameter Passing Experiment
; Scott Baker, https://www.smbaker.com/

EXTRN		OpenByteStream:FAR, ReadBytes:FAR, WriteByte:FAR, WriteBsRecord:FAR, ErrorExit:FAR, Exit:FAR
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

sBSWA		EQU	130
sBuffer		EQU	1024

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
		call	doPrintHex
		pop	ax
)

%*DEFINE (CheckError(theRegister)) LOCAL noError (
		xor	%theRegister,%theRegister
		je	%noError
		push	%theRegister
		call	ErrorExit
%noError:
)

Begin:		mov 	ax, Dgroup
		mov 	SS, AX
ASSUME		SS: Dgroup
		mov	SP, OFFSET Dgroup:wLimStack

		mov	DS, AX
ASSUME		DS: Dgroup

		%PrintString(banner, bannerLen)

		call	CParams
		cmp	ax, 1
		jge	haveEnoughParams
		jmp	insufficientParams
haveEnoughParams:
		push	paramNum
		call	CSubParams
		cmp	ax, 1
		jge	haveEnoughSubParams
		jmp	insufficientParams
haveEnoughSubparams:

		push	paramNum		; parameter index 1
		push	subParamNum		; subparameter index 0
		push	ds
		lea	ax, paramOfs
		push	ax
		call 	RgParam
		%CheckError(ax)

		%PrintString(strFileName, lenFileName)

		push	ds			; print the filename
		lea	ax, bsVid
		push	ax
		push	paramSeg		; push segment and offset of string
		push	paramOfs
		push	paramLen		; push length of string
		push 	ds			; push address of cbWrittenRet
		lea	ax, cbWrittenRet
		push	ax
		call	WriteBsRecord

		%PrintChar(0Ah)

		call	openFile

		mov	dx, 10h			; dx is column counter

blockLoop:	call	readBuffer

		mov	cx, [cReadRet]
		mov	si, [bReadAddrOfs]

bufferLoop:	cmp	cx, 0
		jne	gotBytes
		jmp	blockLoop		; go read the next block
gotBytes:	lodsb
		call	doPrintHex2
		%PrintChar(020h)
		dec	dx
		jnz	moreColumns
		%PrintChar(0Ah)
		mov	dx, 10h
		dec	cx
moreColumns:	jmp	bufferLoop

eof:
		call	Exit

openFile	PROC	NEAR
		push	ds			; Arg 1: address pf Byte Stream Work Area
		lea	ax, bswa
		push	ax

		push	paramSeg		; Arg 2: address and size of file spec
		push	paramOfs
		push	paramLen

		push 	ds			; Arg 3: address and size of password
		lea	ax, password
		push	ax
		mov	ax, 0			; length of password
		push	ax

		mov	ax, 06D77h		; Arg 4: mode. "mw" encoded as an integer.
		push	ax

		push	ds			; Arg 5: address and size of buffer
		lea	ax, buffer
		push	ax
		mov	ax, sBuffer
		push	ax

		call	OpenByteStream
		%CheckError(ax)
		ret
openFile	ENDP

readBuffer	PROC	NEAR
		push	ds			; Arg 1: address pf Byte Stream Work Area
		lea	ax, bswa
		push	ax

		mov	ax, 010h		; Arg 2: amount of data to return
		push	ax

		push	ds			; Arg 3: buffer pointer
		lea	ax, bReadAddrOfs
		push	ax

		push	ds			; Arg 4: number of characters read
		lea	ax, cReadRet
		push	ax

		call	ReadBytes
		cmp	ax, 1
		jne	notEof
		jmp	eof
notEof:		%CheckError(ax)
		ret
readBuffer	ENDP

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

error:		push	ax
		call	ErrorExit

insufficientParams:
		%PrintString(strInsuff,lenInsuff)
		call	Exit

Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'

EXTRN		bsVid:BYTE			; existing video bytestream
	
cbWrittenRet	DW	?
cReadRet	DW	?
bReadAddrOfs	DW	?
bReadAddrSeg	DW	?

paramOfs	DW 	?
paramSeg	DW	?
paramLen	DW	?

bswa		DB	sBSWA	DUP(?)
buffer  	DB	sBuffer	DUP(?)

Data		ENDS

Const		SEGMENT WORD	PUBLIC	'Const'

password	DB	0

paramNum	DW	1
subParamNum	DW	0

banner		DB	'Hexdump',0Ah
bannerLen	DW	SIZE banner

strFileName	DB	'File Name: '
lenFileName	DW	SIZE strFileName

strInsuff	DB	'Insufficient Parameters',0Ah
lenInsuff	DW	SIZE strInsuff

Const 		ENDS

Stack		SEGMENT STACK
wLimStack	EQU	THIS	WORD
STACK 		ENDS

		END Begin