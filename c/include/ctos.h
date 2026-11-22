/* ctos.h - CTOS requests and OS subroutines, called at their fixed far addresses.
 *
 * CTOS uses the PL/M LARGE convention: arguments are pushed left to right,
 * a pointer as segment then offset and a DWORD as high word then low word,
 * and the callee pops them. The error code comes back in AX. lib/ctos.asm
 * places each name at its address, so a call is a plain far call and the
 * program needs no CTOS library.
 *
 * The manuals don't say which registers CTOS keeps, so SI, DI and ES are
 * treated as changed by every call.
 */
#ifndef CTOS_H
#define CTOS_H

typedef unsigned short ErcType;

#pragma aux CTOSCALL "*" parm reverse routine [] value [ax] modify [ax bx cx dx si di es];

/* Files. Read and Write move whole 512-byte pages at page-aligned file addresses. */
extern ErcType __far OpenFile(unsigned __far *pFhRet, const char __far *pbFileSpec,
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
extern ErcType __far DeleteFile(unsigned fh);
extern ErcType __far RenameFile(unsigned fh, const char __far *pbNewFileSpec,
                                unsigned cbNewFileSpec, const char __far *pbPassword,
                                unsigned cbPassword);
extern ErcType __far GetFileStatus(unsigned fh, unsigned statusCode, void __far *pStatusRet,
                                   unsigned sStatusMax);
extern ErcType __far SetFileStatus(unsigned fh, unsigned statusCode, const void __far *pStatus,
                                   unsigned sStatus);
#pragma aux (CTOSCALL) OpenFile;
#pragma aux (CTOSCALL) CreateFile;
#pragma aux (CTOSCALL) CloseFile;
#pragma aux (CTOSCALL) ChangeFileLength;
#pragma aux (CTOSCALL) Read;
#pragma aux (CTOSCALL) Write;
#pragma aux (CTOSCALL) DeleteFile;
#pragma aux (CTOSCALL) RenameFile;
#pragma aux (CTOSCALL) GetFileStatus;
#pragma aux (CTOSCALL) SetFileStatus;

#define modeRead     0x6D72  /* 'mr' */
#define modeModify   0x6D6D  /* 'mm' */

/* A file has two lengths: its allocated size, a multiple of 512 set by
 * ChangeFileLength (and CreateFile), and its end of file, the exact byte count. */
#define statusFileLength 0   /* allocated size, 4 bytes */
#define statusEOF        6   /* end of file, 4 bytes */

#define ercEOF          1
#define ercNoSuchFile   203

/* Directories. ReadDirSector reads page iSector (512 bytes) of a directory. */
extern ErcType __far CreateDir(const char __far *pbDirSpec, unsigned cbDirSpec,
                               const char __far *pbVolPassword, unsigned cbVolPassword,
                               const char __far *pbDirPassword, unsigned cbDirPassword,
                               unsigned cSectors, unsigned defaultFileProtectionLevel);
extern ErcType __far DeleteDir(const char __far *pbDirSpec, unsigned cbDirSpec,
                               const char __far *pbPassword, unsigned cbPassword);
extern ErcType __far ReadDirSector(const char __far *pbDirSpec, unsigned cbDirSpec,
                                   const char __far *pbPassword, unsigned cbPassword,
                                   unsigned iSector, void __far *pBufferRet);
#pragma aux (CTOSCALL) CreateDir;
#pragma aux (CTOSCALL) DeleteDir;
#pragma aux (CTOSCALL) ReadDirSector;

/* Memory. Short-lived memory goes when the program ends; long-lived memory
 * survives Chain. Short-lived segments are freed in the reverse order of
 * allocation. A segment's offset is always 0. */
extern ErcType __far AllocMemorySL(unsigned cbSegment, void __far * __far *ppSegmentRet);
extern ErcType __far DeallocMemorySL(void __far *pSegment, unsigned cbSegment);
extern ErcType __far AllocMemoryLL(unsigned cbSegment, void __far * __far *ppSegmentRet);
extern ErcType __far QueryMemAvail(unsigned __far *pcParagraphRet);
#pragma aux (CTOSCALL) AllocMemorySL;
#pragma aux (CTOSCALL) DeallocMemorySL;
#pragma aux (CTOSCALL) AllocMemoryLL;
#pragma aux (CTOSCALL) QueryMemAvail;

/* Time */
extern ErcType __far GetDateTime(unsigned long __far *pDateTimeRet);
#pragma aux (CTOSCALL) GetDateTime;

/* Replaces this program with the run file. Long-lived memory, which holds
 * the parameter block, is kept. priority: 0 (highest) to 254. */
extern ErcType __far Chain(const char __far *pbFileSpec, unsigned cbFileSpec,
                           const char __far *pbPassword, unsigned cbPassword,
                           unsigned priority, ErcType ercTermination, unsigned fDebug);
#pragma aux (CTOSCALL) Chain;

/* Keyboard. ReadKbdDirect mode 1 returns at once, with ercNoCharAvailable if no
 * key is waiting; mode 0 waits. */
extern ErcType __far ReadKbd(char __far *pCharRet);
extern ErcType __far ReadKbdDirect(unsigned mode, char __far *pCharRet);
extern ErcType __far SetKbdUnencodedMode(unsigned fOn);
extern ErcType __far Beep(void);
#pragma aux (CTOSCALL) ReadKbd;
#pragma aux (CTOSCALL) ReadKbdDirect;
#pragma aux (CTOSCALL) SetKbdUnencodedMode;
#pragma aux (CTOSCALL) Beep;

#define ercNoCharAvailable 602

/* Waits n tenths of a second */
extern ErcType __far Delay(unsigned n);
#pragma aux (CTOSCALL) Delay;

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

/* LoadFontRam loads the character shapes from an open font file, where the
 * font is in RAM (the NGEN, not the AWS). SetScreenVidAttr(1, 0) turns the
 * display off and (1, 0xFF) on again, for while it's loading. */
extern ErcType __far LoadFontRam(unsigned fh, void __far *pBuffer, unsigned sBuffer);
extern ErcType __far SetScreenVidAttr(unsigned iAttr, unsigned fOn);
#pragma aux (CTOSCALL) LoadFontRam;
#pragma aux (CTOSCALL) SetScreenVidAttr;

/* Character attributes for PutFrameAttrs */
#define attrHalfBright 1
#define attrUnderline  2
#define attrReverse    4
#define attrBlink      8

/* Parameters: the far pointer at offset 2 of the ASCB points to the
 * Variable-Length Parameter Block */
extern ErcType __far GetpASCB(void __far * __far *ppAscbRet);
#pragma aux (CTOSCALL) GetpASCB;

/* Leaving. Not declared "aborts": Watcom would then jump instead of call, and
 * CTOS would look for the argument one return address too far up the stack. */
extern void __far ErrorExit(ErcType erc);
extern void __far Exit(void);
#pragma aux (CTOSCALL) ErrorExit;
#pragma aux (CTOSCALL) Exit;

#endif
