; kernel.asm - the fixed addresses of the CTOS calls in ctosmsg.h
;
; Kernel primitive k: CS = 0FFFFh - 10h*(k/4), IP = 210h + 100h*(k/4) + 4*(k mod 4)
; (Kernel_All.Asm in the CTOS source). Send is 0, Wait 1, Check 2, Request 4.
; AllocExch and DeallocExch are requests 40 and 41; QueryDefaultRespExch is
; OS subroutine 0. The formulas for those are in ctos.asm.

KFFFF   segment at 0FFFFh
        org     0210h
        public  Send
Send    label far
        org     0214h
        public  Wait
Wait    label far
        org     0218h
        public  Check
Check   label far
        org     0230h
        public  QueryDefaultRespExch
QueryDefaultRespExch label far
KFFFF   ends

KFFEF   segment at 0FFEFh
        org     0310h
        public  Request
Request label far
KFFEF   ends

KFFD8   segment at 0FFD8h
        org     0490h
        public  AllocExch
AllocExch label far
        org     0492h
        public  DeallocExch
DeallocExch label far
KFFD8   ends

        end
