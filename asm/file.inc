; Functions for working with files.
; Scott Baker, https://www.smbaker.com/
;
; Assumes "buffer", "sBuffer", "bwsa", and "sBwsa" are defined elsewhere.

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

		mov	ax, 06D72h		; Arg 4: mode. "mr" encoded as an integer.
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

readFileByte	PROC	NEAR
		push	ax
		push	bx
		push	cx
		push	dx
		push	si
		push	di

		push	ds			; Arg 1: address pf Byte Stream Work Area
		lea	ax, bswa
		push	ax

		push	ds			; Arg 2: address of Byte
		lea	ax, bReadRet
		push	ax

		call	ReadByte
		cmp	ax, 1
		jne	rfbNotEof
		jmp	eof
rfbNotEof:	%CheckError(ax)

		pop	di
		pop	si
		pop	dx
		pop	cx
		pop	bx
		pop	ax

		ret
readFileByte	ENDP
