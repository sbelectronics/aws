#!/usr/bin/env python3
"""mkrun.py - wrap a one-segment program image as a CTOS version 4 run file.

usage: mkrun.py in.bin out.run [name]

in.bin is the linker's raw binary: code and data in one segment (CS = DS =
SS), entered at offset 0. lib/crt0.asm puts the stack top in word 2. The
file holds only the image; cparMinAlloc asks CTOS for the memory above it,
up to the stack top, which the loader gives as the rest of the segment.
crt0 clears it, since it isn't necessarily zero.

The program needs no relocations: the loader relocates saStack and saStart
itself, and CTOS calls are far calls to fixed addresses.
"""
import os, struct, sys

src, dst = sys.argv[1], sys.argv[2]
name = sys.argv[3] if len(sys.argv) > 3 else os.path.splitext(os.path.basename(dst))[0]
img = bytearray(open(src, 'rb').read())
if len(img) < 4 or img[0] != 0xEB:
    sys.exit('mkrun: %s does not start with crt0' % src)
sp = struct.unpack_from('<H', img, 2)[0]
if sp & 1 or sp < len(img):
    sys.exit('mkrun: bad stack top %#x (image is %#x bytes)' % (sp, len(img)))

HDR = 0x200
hdr = bytearray(HDR)
total = (HDR + len(img) + 511) // 512 * 512
img += bytes(total - HDR - len(img))
extra = max(0, (sp - len(img) + 15) // 16)      # paragraphs above the loaded image
fields = [
    0x5747,          # 0  signature 'GW'
    4,               # 2  run file version
    total // 512,    # 4  cpnRes: file size in 512-byte pages
    0,               # 6  irleMax: relocation entries
    HDR // 16,       # 8  cparDirectory: header size in paragraphs
    extra, extra,    # 10 cparMinAlloc, 12 cparMaxAlloc
    0, sp,           # 14 saStack, 16 raStackInit
    0,               # 18 checksum
    0, 0,            # 20 raStart, 22 saStart
    0x50,            # 24 rbrgrle: relocation table offset
    0,               # 26 iovMax
    0xFFFF,          # 28 constant
    1,               # 30 verAlt
    0x50,            # 32 end of the (empty) relocation table, as the CTOS linker writes it
]
struct.pack_into('<%dH' % len(fields), hdr, 0, *fields)
ver = ('Ver ' + name)[:40].encode('latin1')
hdr[0x24:0x24 + len(ver)] = ver
run = bytearray(hdr + img)
s = sum(struct.unpack_from('<%dH' % (len(run) // 2), run, 0)) & 0xffff
struct.pack_into('<H', run, 18, (0xffff - s) & 0xffff)
open(dst, 'wb').write(run)
print('mkrun: %s: %d bytes of code and data, stack top %#x, file %d bytes'
      % (dst, os.path.getsize(src), sp, len(run)))
