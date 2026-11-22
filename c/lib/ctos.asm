; ctos.asm - the fixed addresses of the CTOS calls declared in ctos.h
;
; Requests and OS subroutines are entered by far calls to fixed CS:IP
; addresses. Each name here is a label in an absolute segment, so the linker
; resolves calls to it without a relocation.
;
; Request rq:    CS = -(rq AND 0FFF8h), IP = 210h + (rq AND 0FFF8h)*10h + ((rq*2) AND 0Fh)
; OS subroutine: CS = 0FFFFh - 16*(n/8), IP = 230h + 100h*(n/8) + 2n - 10h*(n/8)

C0000   segment at 00000h
        org     0218h
        public  OpenFile
OpenFile label far
        org     021Ah
        public  CreateFile
CreateFile label far
        org     021Ch
        public  DeleteFile
DeleteFile label far
        org     021Eh
        public  RenameFile
RenameFile label far
C0000   ends

CFFF8   segment at 0FFF8h
        org     0290h
        public  GetFileStatus
GetFileStatus label far
        org     0292h
        public  SetFileStatus
SetFileStatus label far
        org     0294h
        public  CloseFile
CloseFile label far
        org     029Ah
        public  ChangeFileLength
ChangeFileLength label far
        org     029Ch
        public  GetDateTime
GetDateTime label far
CFFF8   ends

CFFE0   segment at 0FFE0h
        org     0416h
        public  Read
Read    label far
        org     0418h
        public  Write
Write   label far
CFFE0   ends

CFFD8   segment at 0FFD8h
        org     0494h
        public  AllocMemorySL
AllocMemorySL label far
CFFD8   ends

CFFD0   segment at 0FFD0h
        org     0518h
        public  Beep
Beep    label far
        org     051Ah
        public  ReadKbd
ReadKbd label far
CFFD0   ends

CFFDF   segment at 0FFDFh
        org     0432h
        public  PosFrameCursor
PosFrameCursor label far
        org     0434h
        public  PutFrameAttrs
PutFrameAttrs label far
        org     0436h
        public  PutFrameChars
PutFrameChars label far
        org     043Ah
        public  ResetFrame
ResetFrame label far
        org     043Ch
        public  ScrollFrame
ScrollFrame label far
CFFDF   ends

CFFCF   segment at 0FFCFh
        org     053Eh
        public  GetpASCB
GetpASCB label far
CFFCF   ends

CFFEF   segment at 0FFEFh
        org     0334h
        public  ErrorExit
ErrorExit label far
        org     0336h
        public  Exit
Exit    label far
CFFEF   ends

        end
