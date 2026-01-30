; Maze Generator
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

COLS		EQU	79
ROWS		EQU	21
WALL		EQU	'X'  ; character to use for walls
PATH		EQU	' '  ; character to use for paths

Main		SEGMENT
ASSUME		CS: Main

$INCLUDE(macros.inc)

Begin:		mov 	ax, Dgroup
		mov 	SS, AX
ASSUME		SS: Dgroup
		mov	SP, OFFSET Dgroup:wLimStack

		mov	DS, AX
ASSUME		DS: Dgroup

		; call	enableLiteral

		%PrintString(banner, bannerLen)

		; Initialize grid
		call	initGrid

		; Start at 1,1
		mov	ax, 1
		mov	bx, 1
		call	setPath
		
		; Push (1,1)
		call	stackPush

mazeLoop:	
		call	stackEmpty
		cmp	ax, 1
		je	mazeDone

		; Peek current
		call	stackPeek
		; AL = X, AH = Y, but stackPeek returns AX=X, BX=Y in my plan?
		; Let's assume stackPeek returns X in AX, Y in BX context.
		; Wait, I need to design those helpers.
		
		; Let's execute logic inline or call a step function.
		call	doMazeStep
		jmp	mazeLoop

mazeDone:
		; Create Entry at (0, 1)
		mov	ax, 0
		mov	bx, 1
		call	setPath

		; Create Exit at (COLS-1, ROWS-2)
		mov	ax, COLS-1
		mov	bx, ROWS-2
		call	setPath
	
		call	printGrid

		call	Exit

; --------------------------------------
; Maze Logic
; --------------------------------------

doMazeStep	PROC	NEAR
		; Peek current cell
		call	stackPeek
		mov	si, ax		; SI = X
		mov	di, bx		; DI = Y

		; Check neighbors
		; We test 4 directions. To randomize, we can't just check
		; sequentially. We need a list of candidates.
		
		; I'll use a simpler approach: Pick a random direction, try it.
		; If fail, try next (mod 4). If all 4 fail, pop.
		; Ideally we should shuffle directions, but that's complex in ASM.
		; A "try random, then scan" approach is easier.
		
		; Better: Collect valid neighbors.
		; Store in temp array.
		xor	cx, cx		; Count of valid neighbors
		
		; Check North (x, y-2)
		mov	ax, si
		mov	bx, di
		sub	bx, 2
		call	isValidNeighbor
		or	ax, ax
		jz	notNorth
		mov	dl, 0	; 0 = North
		call	pushNeighbor
notNorth:
		; Check South (x, y+2)
		mov	ax, si
		mov	bx, di
		add	bx, 2
		call	isValidNeighbor
		or	ax, ax
		jz	notSouth
		mov	dl, 1 ; 1 = South
		call	pushNeighbor
notSouth:
		; Check West (x-2, y)
		mov	ax, si
		sub	ax, 2
		mov	bx, di
		call	isValidNeighbor
		or	ax, ax
		jz	notWest
		mov	dl, 2 ; 2 = West
		call	pushNeighbor
notWest:
		; Check East (x+2, y)
		mov	ax, si
		add	ax, 2
		mov	bx, di
		call	isValidNeighbor
		or	ax, ax
		jz	notEast
		mov	dl, 3 ; 3 = East
		call	pushNeighbor
notEast:

		cmp	cx, 0
		je	noNeighbors

		; Pick random index 0 to CX-1
		push	cx
		call	Rand
		pop	cx
		xor	dx, dx
		div	cx		; DX = AX mod CX
		
		; Get direction from neighborArr[DX]
		mov	bx, dx
		mov	al, neighborArr[bx]
		
		; Move there
		; SI=CurX, DI=CurY
		cmp	al, 0
		je	goNorth
		cmp	al, 1
		je	goSouth
		cmp	al, 2
		je	goWest
		jmp	goEast

goNorth:	call	removeWallNorth
		mov	ax, si
		mov	bx, di
		sub	bx, 2
		jmp	doMove

goSouth:	call	removeWallSouth
		mov	ax, si
		mov	bx, di
		add	bx, 2
		jmp	doMove

goWest:		call	removeWallWest
		mov	ax, si
		sub	ax, 2
		mov	bx, di
		jmp	doMove

goEast:		call	removeWallEast
		mov	ax, si
		add	ax, 2
		mov	bx, di
		jmp	doMove

doMove:		
		; Mark new cell
		call	setPath
		
		; Push to stack (ax=x, bx=y)
		call	stackPush		
		ret


noNeighbors:
		call	stackPop
		ret
doMazeStep	ENDP

; Helper: Push DL to neighborArr
pushNeighbor	PROC	NEAR
		push	bx
		mov	bx, cx
		mov	neighborArr[bx], dl
		inc	cx
		pop	bx
		ret
pushNeighbor	ENDP

; Helper: Check if (AX, BX) is valid unvisited neighbor
; Returns AX=1 if valid, 0 if not
isValidNeighbor	PROC	NEAR
		; Check bounds
		cmp	ax, 1
		jl	invalid
		cmp	ax, COLS-2
		jg	invalid
		cmp	bx, 1
		jl	invalid
		cmp	bx, ROWS-2
		jg	invalid
		
		; Check if unvisited (is WALL)
		call	getCell
		cmp	al, WALL
		je	valid
invalid:	xor	ax, ax
		ret
valid:		mov	ax, 1
		ret
isValidNeighbor	ENDP

; Helper: Remove wall relative to SI, DI
removeWallNorth	PROC	NEAR
		mov	ax, si
		mov	bx, di
		dec	bx
		call	setPath
		ret
removeWallNorth	ENDP
removeWallSouth	PROC	NEAR
		mov	ax, si
		mov	bx, di
		inc	bx
		call	setPath
		ret
removeWallSouth	ENDP
removeWallWest	PROC	NEAR
		mov	ax, si
		dec	ax
		mov	bx, di
		call	setPath
		ret
removeWallWest	ENDP
removeWallEast	PROC	NEAR
		mov	ax, si
		inc	ax
		mov	bx, di
		call	setPath
		ret
removeWallEast	ENDP


; --------------------------------------
; Grid Funcs
; --------------------------------------

; Fill grid with WALLs
initGrid	PROC	NEAR
		mov	cx, COLS*ROWS
		mov	di, 0
		mov	al, WALL
initL:		mov	mazeGrid[di], al
		inc	di
		loop	initL
		ret
initGrid	ENDP

; Set cell (AX, BX) to PATH. AX=Col, BX=Row
setPath		PROC	NEAR
		push	ax
		push	bx
		push	dx
		
		; Offset = BX * COLS + AX
		push	ax
		mov	ax, bx
		mov	dx, COLS
		mul	dx
		mov	bx, ax
		pop	ax
		add	bx, ax
		
		mov	mazeGrid[bx], PATH
		
		pop	dx
		pop	bx
		pop	ax
		ret
setPath		ENDP

; Get cell (AX, BX). Returns char in AL.
getCell		PROC	NEAR
		push	bx
		push	dx
		
		; Offset = BX * COLS + AX
		push	ax
		mov	ax, bx
		mov	dx, COLS
		mul	dx
		mov	bx, ax
		pop	ax
		add	bx, ax
		
		mov	al, mazeGrid[bx]
		
		pop	dx
		pop	bx
		ret
getCell		ENDP


printGrid	PROC	NEAR
		mov	cx, ROWS
		mov	bx, 0	; row index
pLoop:		push	cx
		push	bx
		
		; Print row BX
		; Calculate offset
		mov	ax, bx
		mov	dx, COLS
		mul	dx
		
		; Print string at mazeGrid[AX], len COLS
		lea	bx, mazeGrid
		add	bx, ax
		
		mov	ax, bx
		mov	bx, COLS
		call	doPrintString
		
		%PrintChar(0Ah)
		
		pop	bx
		pop	cx
		inc	bx
		loop	pLoop
		ret
printGrid	ENDP

; --------------------------------------
; Stack Funcs
; --------------------------------------

; Push AX, BX
stackPush	PROC	NEAR
		mov	di, stackPtr
		mov	stackBuf[di], ax
		mov	stackBuf[di+2], bx
		add	di, 4
		mov	stackPtr, di
		ret
stackPush	ENDP

; Pop (discard)
stackPop	PROC	NEAR
		mov	di, stackPtr
		sub	di, 4
		mov	stackPtr, di
		ret
stackPop	ENDP

; Peek (ret AX=X, BX=Y)
stackPeek	PROC	NEAR
		mov	di, stackPtr
		mov	ax, stackBuf[di-4]
		mov	bx, stackBuf[di-2]
		ret
stackPeek	ENDP

stackEmpty	PROC	NEAR
		mov	ax, stackPtr
		cmp	ax, 0
		je	isEmpty
		mov	ax, 0
		ret
isEmpty:	mov	ax, 1
		ret
stackEmpty	ENDP

; --------------------------------------
; RNG
; --------------------------------------
; Returns random AX
Rand		PROC	NEAR
		push	dx
		push	bx
		mov	ax, randSeed
		mov	dx, 25173
		mul	dx
		add	ax, 13849
		mov	randSeed, ax
		pop	bx
		pop	dx
		ret
Rand		ENDP

$INCLUDE(print.inc)

Main		ENDS

Data		SEGMENT WORD	PUBLIC	'Data'

EXTRN		bsVid:BYTE			; existing video bytestream
	
cbWrittenRet	DW	?
bReadRet	DB	?

mazeGrid	DB	2000 DUP(?)
neighborArr	DB	4 DUP(?)

stackBuf	DW	4000 DUP(?) 
stackPtr	DW	0

Data		ENDS

Const		SEGMENT WORD	PUBLIC	'Const'

randSeed	DW	12345

banner		DB	'Maze Generator',0Ah
bannerLen	DW	SIZE banner

Const 		ENDS

Stack		SEGMENT STACK
wLimStack	EQU	THIS	WORD
STACK 		ENDS

		END Begin