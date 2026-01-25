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
		and	%theRegister,%theRegister
		je	%noError
		push	%theRegister
		call	ErrorExit
%noError:
)

%*DEFINE (SetCTOSSegments(theGroup, theStack)) (
		cli
		mov	ax, %theGroup
		mov	ds, ax		; DS = dgroup
		mov	ss, ax		; SS = dgroup
ASSUME DS:	%theGroup
ASSUME SS:	%theGroup
		mov	ax, sp
        	mov	SP, OFFSET %theGroup:%theStack
		push 	ax		; push the old stack pointer
		sti
)

%*DEFINE (RestoreSegments(theGroup)) (
	cli
	pop	ax
	mov	sp, ax		; restore old stack pointer
	mov	ax, %theGroup
	mov	ds, ax		; DS = CS
	mov	ss, ax		; SS = CS
ASSUME	DS:	%theGroup
ASSUME	SS:	%theGroup
	sti
)