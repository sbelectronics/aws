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
