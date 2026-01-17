EXTRN		WriteBsREcord:FAR, WriteByte:FAR, ErrorExit:FAR
	
Main            SEGMENT WORD    'Code'
Main            ENDS

Const           SEGMENT WORD PUBLIC 'Const'

rgchMsg         DB      'Now is the time for all good men to come to the aid of their party'
cbMsg           DW      SIZE rgchMsg

Const           ENDS

Data            SEGMENT WORD    PUBLIC 'Data'
EXTRN           bsVid:BYTE

cloop           DW      0
cbWrittenRet    DW      ?

Data            ENDS

Stack           SEGMENT STACK   'Stack'
wLimStack       EQU     THIS    WORD
STACK           ENDS

Dgroup          GROUP   Const, Data, Stack

Main            SEGMENT
ASSUME          CS:Main

Begin:          mov     ax, Dgroup
                mov     ss, ax
ASSUME          SS:Dgroup
                mov     sp, offset DGroup:wLimStack
                mov     ds, ax
ASSUME          DS:DGroup

                MOV     cloop, 0
loops:
                push    ds
                lea     ax,bsVid
                push    ax
                push    ds
                lea     ax, rgchMsg
                push    ax
                push    cbMsg
                push    ds
                lea     ax, cbWrittenRet
                push    ax
                call    WriteBsRecord
                and     ax,ax
                jne     error

                mov     ax, cloop
                call    printhex
                inc     cloop

                push    ds
                lea     ax, bsVid
                push    ax
                mov     al, 0AH
                push    ax

                call    writeByte
                and     ax,ax
                jne     error

                jmp     loops

printHex        PROC    NEAR
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
                and     ax,ax
                jne     error

                pop     ax
                pop     cx
                loop    print1
                ret
printhex        ENDP

error:          push    ax
                call    ErrorExit

Main            ENDS

END             Begin
