# C for the Convergent AWS and NGEN

Cross-compiles C on Linux into CTOS run files that run on both the AWS (CTOS 9.11, 8086)
and the NGEN (CTOS II, 80186). It uses [Open Watcom](https://github.com/open-watcom/open-watcom-v2)
and a small C library written for CTOS. It needs nothing from Convergent's own C compiler or
libraries.

The AWS is the case that matters. The NGEN has a working native compiler, Unisys BTOS C, but
its run files and C library need CTOS II and an 80186, so nothing it builds runs on the AWS.

## Building

You need Open Watcom 2.0 (by default in `/opt/watcom`), GNU make and Python 3.

```sh
make                       # build/libc.lib, and build/X.run for each examples/X.c
make WATCOM=/path/to/watcom
```

To add a program, put it in `examples/` or link it the same way: `build/crt0.obj` first, then
the program's objects, then `build/libc.lib` (see the `%.bin` rule in the Makefile).

To put a run file on a CTOS floppy image, use
[ctostool](https://github.com/sbelectronics/ctostool):

```sh
python2 ctostool.py floppy.img add Sys hello.run build/hello.run
```

## How programs are built

- **8086 code** (`wcc -0`), so the same run file works on both machines.
- **One segment.** Code, data, the heap and the stack share one 64 KB segment (CS = DS = SS),
  as in a DOS `.COM` file. `lib/crt0.asm` puts every segment in `DGROUP`, starting with the
  code at offset 0, so code and data offsets agree. `wlink` writes a raw binary.
- **Run file.** `tools/mkrun.py` wraps the binary as a CTOS version 4 run file, entered at
  offset 0. It pads the image with zeros up to the stack top, since CTOS loads only what's in
  the file, so the BSS, heap and stack are zeroed without any start-up code. There are no
  relocations.
- **CTOS calls.** `include/ctos.h` declares the CTOS requests and OS subroutines as far
  functions with the PL/M calling convention: arguments pushed left to right, the callee pops
  them. `lib/ctos.asm` places each name at its fixed address in an absolute segment, so a call
  compiles to an ordinary far call and needs no CTOS library.

## The library

| Header | What's there |
|---|---|
| `stdio.h` | `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `rewind`, `feof`, `ferror`, `clearerr`, `fflush`, `fgetc`, `fputc`, `ungetc`, `fgets`, `fputs`, `remove`, `rename`, `perror`; the `printf` family; `getchar`, `putchar`, `puts` |
| `string.h` | the usual `mem*` and `str*` functions |
| `stdlib.h` | `exit`, `abort`, `abs`, `atoi`, `strtol`, `strtoul`, `rand`, `srand`, `malloc`, `calloc`, `realloc`, `free` |
| `errno.h` | `errno`, the CTOS error code of the last failure |
| `ctype.h` | the classification macros (they evaluate their argument more than once) |
| `ctos.h` | the CTOS calls listed below |

- **Console.** `stdout` is the Executive's video frame 0. The first output clears it and
  writes from the top, scrolling at the bottom. `stdin` reads a line at a time from the
  keyboard, with echo and BACKSPACE. RETURN or GO ends the line.
- **Files.** A `FILE` is a CTOS file, opened with the modes `r`, `w`, `a` and their `+`
  forms (`b` is accepted and ignored: there's no text translation). Names are CTOS file
  specs; a plain name is in the current path. Details below.
- **printf** handles `%d %i %u %x %X %o %c %s %p %%`, with `-`, `0`, `+`, space and `#` flags,
  width, precision, `*`, and `l` for longs. There's no floating point.
- **malloc** is first fit in a 16 KB heap inside the segment (`HEAP_SIZE` in `lib/stdlib.c`).
  Each open file takes 540 bytes of it.
- **Exit.** `exit(0)`, or returning 0 from `main`, calls `Exit`. A non-zero status calls
  `ErrorExit` with it.
- **Long arithmetic.** Watcom calls helpers for 32-bit multiply and divide; `lib/i4.asm`
  provides them.

### Arguments

`main(int argc, char **argv)` gets the fields of the command form.
- `argv[0]` is the command name. It's empty when the program is started with `Run`.
- Then come the words of each field in order. A field holding `a b` gives two arguments, and
  an empty field gives none, as on a Unix command line.

So a command made with `New Command` and fields such as `'[Input file]' '[Output file]'` gets
them as `argv[1]` and `argv[2]`. With `Run`, the arguments go in `[Param 1]`, `[Param 2]` and
so on.

### Files

CTOS reads and writes files in whole 512-byte pages, and keeps two lengths for each: its
allocated size, a multiple of 512, and its end of file, the exact byte count. `lib/file.c`
handles this:
- **Buffering:** each open file has a one-page buffer.
- **Growing:** writing past the allocated size grows it 4 KB at a time (`GROW`).
- **Closing:** `fclose` gives back the space past the end of file and records the exact
  length, so the file has precisely the bytes written.
- **Gaps:** writing past the end of file fills the gap with zeros.
- **Errors:** a failing call returns `NULL`, `EOF` or -1 and leaves the CTOS error code in
  `errno`. For example, 203 is "no such file".

Up to 8 files can be open at once (`FOPEN_MAX`). `exit` closes any still open.

### CTOS calls

The CTOS calls in `ctos.h`:
- files: `OpenFile`, `CreateFile`, `CloseFile`, `ChangeFileLength`, `Read`, `Write`,
  `DeleteFile`, `RenameFile`, `GetFileStatus`, `SetFileStatus`;
- memory and time: `AllocMemorySL`, `GetDateTime`;
- keyboard: `ReadKbd`, `Beep`;
- video: `PosFrameCursor`, `PutFrameAttrs`, `PutFrameChars`, `ResetFrame`, `ScrollFrame`;
- parameters: `GetpASCB`;
- leaving: `Exit`, `ErrorExit`.

Their addresses come from the formulas in `lib/ctos.asm`. To add one, give it a label there
and a declaration with `#pragma aux (CTOSCALL)` in `ctos.h`.

## Not done yet

- Floating point.
- File passwords: files are opened with an empty password.
- Programs bigger than 64 KB of code and data.

## Testing

The examples are the tests, run on the machine or in the
[simulator](https://github.com/sbelectronics/aws-simulator):

- **`selftest.run`** checks:
  - long multiply and divide, signed and unsigned (the helpers in `i4.asm`);
  - 22 `printf` formats and `snprintf` truncation;
  - `strtol`, the string functions, `malloc`, `realloc` and `calloc`, and `rand`.

  It prints each failure, then `selftest: 58 checks, 0 failed`, and exits with the number of
  failures.
- **`filetest.run`** prints its arguments, then checks file I/O:
  - writing and reading back a 200-line text file of about 5 KB;
  - `fseek`, `ftell` and `ungetc`, appending, and updating with `r+`;
  - binary `fread` and `fwrite`, emptying with `w`, and the zeros in a gap;
  - `rename`, `remove`, and `errno` for a missing file.

  Its first argument is the directory for its files, for example `[f0]<Zork>`. It ends with
  `filetest: 44 checks, 0 failed`. It leaves `keep.txt` behind, 63 bytes, so you can check
  from outside that the file has exactly the bytes written.
- **`hello.run`** prints a greeting and a line of formats, then reads a name from the keyboard:

  ```
  Hello from C on CTOS!
  -42 65535 beef 123456789 ok|    7|7    |00007
  What's your name? Scott
  Hello, Scott
  ```

All three pass on the AWS and NGEN simulators. Run with `[Param 1]` set to `[f0]<Zork>` and
`[Param 2]` to `two words`, `filetest` starts with `argc 4: [] [[f0]<Zork>] [two] [words]`.
A non-zero `return` from `main` shows as an error with that number on both machines.

On the AWS, check that a program uses no 80186 instructions. On an 8086 they decode as other
instructions, and the program crashes or misbehaves. `wcc -0` doesn't generate them, but code
written in assembly should be checked.
