; Hexdump Tool
; Scott Baker, https://www.smbaker.com/

EXTRN		OpenByteStream:FAR, ReadByte:FAR, ReadBytes:FAR, WriteByte:FAR, WriteBsRecord:FAR, ErrorExit:FAR, Exit:FAR
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

$INCLUDE(macros.inc)

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

readLoop:	call	readFileByte
		mov	al, [bReadRet]

gotBytes:	call	doPrintHex2
		%PrintChar(020h)
		dec	dx
		jnz	moreColumns
		%PrintChar(0Ah)
		mov	dx, 10h
moreColumns:	jmp	readLoop

eof:
		call	Exit

insufficientParams:
		%PrintString(strInsuff,lenInsuff)
		call	Exit

$INCLUDE(file.inc)
$INCLUDE(print.inc)

Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'

EXTRN		bsVid:BYTE			; existing video bytestream
	
cbWrittenRet	DW	?
bReadRet	DB	?

paramOfs	DW 	?
paramSeg	DW	?
paramLen	DW	?

		EVEN
bswa		DB	sBSWA	DUP(?)
		EVEN
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