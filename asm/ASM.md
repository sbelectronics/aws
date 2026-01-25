Assembly Language Notes

Write to screen

Can use bsVid for video pBSWA and bsKbd for keyboard pBSWA

OpenByteStream(pbSWA, pbFileSpec, cbFileSpec, pbPassword, cvPassword, mode, pBufferArea, sBufferArea): ErcTYpe

   pbSWA - addr of 130 byte work area
   pbFileSpec, cbFilespec - pointer and size to device -- "[Vid]" or "[Kbd]"
   pbPassword, cbPassword - pointer and size of password
   mode - "mw" encoded into 16-bit with m in the high order byte
   pBufferArea, sBufferArea - pointer and size of 1kb buffer

WriteByte (pBSWA, b): ErcType

WriteBsRecord (pBSWA, pb, cb, pcbRet): ErcType
   pb, cb = pointer and size of bytes to write
   pcbRet - pointer to return count of written bytes

ReadByte (pBSWA, pbRet): ErcType
   pbRet - memory address for byte return
   returns status code 1 on Eof (finish key) or 4 on Cancel

CloseByteString (pBSWA): ErcType

Reading parameters

Param (0,0) is always the command name.

CParam(): WORD - returns count of params

CSubParams(iparam): WORD - returns count of subparams for param

RgParam (iParam, jParam, pSDRet): ErcType
   iParam - index of parameter
   jParam - index of subparameter
   pSDRet - point to 6 bytes, holds 4 byte addr followed by 2-byte len

