Some of these are old notes. Not sure if for the AWS or something else...


Dot Clock possible 17.820 MHz
19.8KHz HSYnC ?
330 scan lines
50Hz or 60Hz vertical?
RS422 differential


video board -- see PDF page, saved to folder

J1- 3,6,8,11,15,19 - GND

J1-1,2 +27V
J1-5,4 Horizontal (+,-)
J1-9,10 Video (+,-)
J1-12,13 Halfbright (+,-)
J1-16,17 Vertical (+,-)

Motherboard connectors

P1-5,6,15,16,25,26,35,36 - GND
P1-27 Halfbright+
P1-28 Halfbright-
P1-29 VERDR+
P1-30 VERDR-
P1-31 HORDR+
P1-32 HORDR-
P1-33 VIDEO+
P1-34 VIDEO-

P3-1,2 +24V
P3-3,6,8,11,15,19 GND
P3-5 HORDR+
P3-4 HORDR-
P3-9 VIDEO+
P3-10 VIDEO-
P3-12 Halfbright-
P3-13 Halfbright+
P3-16 VERDR+
P3-17 VERDR-

AWS Port numbers

Ports
00 - 1F DMA
20 - 3F CRT
40 - 5F 8253
60 - 7F 7201 SIO
80 - 9F FDC/HDC
A0 - BF FDC/HDC
E0 - FF Decoder 3F (parity stuff)
C0 - DF Advanced video

E0 - E3 read parity address latch
E4 - E7 read parity address latch
E8 - EB read parity address latch
F0 - F3 (read) enables parity
F4 - F7 (read) disables parity
