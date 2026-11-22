; crt0.asm - start-up for a one-segment CTOS program (CS = DS = SS)
;
; The whole program, code included, is one group starting at offset 0, so
; code and data offsets agree and the run file needs no relocations. crt0
; must come first in the link: the run file is entered at offset 0.
;
; Word 2 of the image holds the stack top, for tools/mkrun.py. Everything
; past the initialised data is zeros in the run file, so _BSS needs no
; clearing.

        .8086

DGROUP  group   _TEXT, CONST, CONST2, _DATA, _BSS, STACK

_TEXT   segment word public 'CODE'
_TEXT   ends
CONST   segment word public 'DATA'
CONST   ends
CONST2  segment word public 'DATA'
CONST2  ends
_DATA   segment word public 'DATA'
_DATA   ends
_BSS    segment word public 'BSS'
_BSS    ends
STACK   segment para public 'STACK'
        db      4096 dup(?)
        public  __stktop
__stktop label  word
STACK   ends

        extrn   main_:near
        extrn   exit_:near

_TEXT   segment
        assume  cs:DGROUP, ds:DGROUP, ss:DGROUP

        public  _cstart_
_cstart_:
        jmp     short start
        dw      offset DGROUP:__stktop
start:  cli
        mov     ax, cs
        mov     ds, ax
        mov     ss, ax
        mov     sp, offset DGROUP:__stktop
        sti
        mov     es, ax
        cld
        xor     ax, ax                  ; argc = 0, argv = NULL
        xor     dx, dx
        call    main_
        call    exit_                   ; AX = main's result
_TEXT   ends

        end     _cstart_
