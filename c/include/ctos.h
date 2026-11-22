/* ctos.h - CTOS requests and OS subroutines, called at their fixed far addresses.
 *
 * CTOS uses the PL/M LARGE convention: arguments are pushed left to right,
 * a pointer as segment then offset and a DWORD as high word then low word,
 * and the callee pops them. The error code comes back in AX. lib/ctos.asm
 * places each name at its address, so a call is a plain far call and the
 * program needs no CTOS library.
 *
 * SI, DI and ES are treated as changed by every call, to be safe.
 */
#ifndef CTOS_H
#define CTOS_H

typedef unsigned short ErcType;

#pragma aux CTOSCALL "*" parm reverse routine [] value [ax] modify [ax bx cx dx si di es];

/* Files. Read and Write move whole 512-byte pages at page-aligned file addresses. */
extern ErcType __far OpenFile(unsigned short __far *pFhRet, const char __far *pbFileSpec,
                              unsigned cbFileSpec, const char __far *pbPassword,
                              unsigned cbPassword, unsigned mode);
extern ErcType __far CreateFile(const char __far *pbFileSpec, unsigned cbFileSpec,
                                const char __far *pbPassword, unsigned cbPassword,
                                unsigned long lfaFileSize);
extern ErcType __far CloseFile(unsigned fh);
extern ErcType __far ChangeFileLength(unsigned fh, unsigned long lfaNewFileLength);
extern ErcType __far Read(unsigned fh, void __far *pBufferRet, unsigned sBufferMax,
                          unsigned long lfa, unsigned __far *pcbRet);
extern ErcType __far Write(unsigned fh, const void __far *pBuffer, unsigned sBuffer,
                           unsigned long lfa, unsigned __far *pcbRet);
#pragma aux (CTOSCALL) OpenFile;
#pragma aux (CTOSCALL) CreateFile;
#pragma aux (CTOSCALL) CloseFile;
#pragma aux (CTOSCALL) ChangeFileLength;
#pragma aux (CTOSCALL) Read;
#pragma aux (CTOSCALL) Write;

#define modeRead     0x6D72  /* 'mr' */
#define modeModify   0x6D6D  /* 'mm' */

/* Memory and time */
extern ErcType __far AllocMemorySL(unsigned cbSegment, void __far * __far *ppSegmentRet);
extern ErcType __far GetDateTime(unsigned long __far *pDateTimeRet);
#pragma aux (CTOSCALL) AllocMemorySL;
#pragma aux (CTOSCALL) GetDateTime;

/* Keyboard */
extern ErcType __far ReadKbd(char __far *pCharRet);
extern ErcType __far Beep(void);
#pragma aux (CTOSCALL) ReadKbd;
#pragma aux (CTOSCALL) Beep;

/* Video frames. Frame 0 is the Executive's main frame. */
extern ErcType __far PosFrameCursor(unsigned frame, unsigned col, unsigned line);
extern ErcType __far PutFrameAttrs(unsigned frame, unsigned col, unsigned line,
                                   unsigned attr, unsigned cb);
extern ErcType __far PutFrameChars(unsigned frame, unsigned col, unsigned line,
                                   const char __far *pb, unsigned cb);
extern ErcType __far ResetFrame(unsigned frame);
extern ErcType __far ScrollFrame(unsigned frame, unsigned firstLine, unsigned lineLimit,
                                 unsigned count, unsigned fUp);
#pragma aux (CTOSCALL) PosFrameCursor;
#pragma aux (CTOSCALL) PutFrameAttrs;
#pragma aux (CTOSCALL) PutFrameChars;
#pragma aux (CTOSCALL) ResetFrame;
#pragma aux (CTOSCALL) ScrollFrame;

#define attrReverse  4

/* Parameters: the ASCB's word at offset 2 points to the Variable-Length Parameter Block */
extern ErcType __far GetpASCB(void __far * __far *ppAscbRet);
#pragma aux (CTOSCALL) GetpASCB;

/* Leaving. Not declared "aborts": Watcom would then jump instead of call, and
 * CTOS would look for the argument one return address too far up the stack. */
extern void __far ErrorExit(ErcType erc);
extern void __far Exit(void);
#pragma aux (CTOSCALL) ErrorExit;
#pragma aux (CTOSCALL) Exit;

#endif
