; crt0.asm - start-up for a one-segment CTOS program (CS = DS = SS)
;
; The whole program, code included, is one group starting at offset 0, so
; code and data offsets agree and the run file needs no relocations. crt0
; must come first in the link: the run file is entered at offset 0.
;
; Word 2 of the image holds the stack top, for tools/mkrun.py. The run file
; holds only the code and initialised data; CTOS gives the program the memory
; above them, for _BSS and the stack, which crt0 clears.

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
        public  __bssbeg
__bssbeg label  word                    ; first in _BSS, as crt0 is linked first
_BSS    ends
STACK   segment para public 'STACK'
        db      4096 dup(?)
        public  __stktop
__stktop label  word
STACK   ends

        extrn   _cmain_:near

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
        mov     di, offset DGROUP:__bssbeg      ; clear _BSS and the stack
        mov     cx, offset DGROUP:__stktop
        sub     cx, di
        shr     cx, 1
        xor     ax, ax
        rep     stosw
        call    _cmain_                 ; start.c: builds argv, runs main, exits
_TEXT   ends

        end     _cstart_
