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
| `stdio.h` | `printf`, `sprintf`, `snprintf` and the `v` forms; `putchar`, `puts`, `fputs`, `getchar`, `fgets`. Only `stdout` and `stdin`: there's no file I/O yet. |
| `string.h` | the usual `mem*` and `str*` functions |
| `stdlib.h` | `exit`, `abort`, `abs`, `atoi`, `strtol`, `strtoul`, `rand`, `srand`, `malloc`, `calloc`, `realloc`, `free` |
| `ctype.h` | the classification macros (they evaluate their argument more than once) |
| `ctos.h` | the CTOS calls listed below |

- **Console.** `stdout` is the Executive's video frame 0. The first output clears it and
  writes from the top, scrolling at the bottom. `stdin` reads a line at a time from the
  keyboard, with echo and BACKSPACE. RETURN or GO ends the line.
- **printf** handles `%d %i %u %x %X %o %c %s %p %%`, with `-`, `0`, `+`, space and `#` flags,
  width, precision, `*`, and `l` for longs. There's no floating point.
- **malloc** is first fit in a 16 KB heap inside the segment (`HEAP_SIZE` in `lib/stdlib.c`).
- **Exit.** `exit(0)`, or returning 0 from `main`, calls `Exit`. A non-zero status calls
  `ErrorExit` with it.
- **Long arithmetic.** Watcom calls helpers for 32-bit multiply and divide; `lib/i4.asm`
  provides them.

The CTOS calls in `ctos.h`:
- files: `OpenFile`, `CreateFile`, `CloseFile`, `ChangeFileLength`, `Read`, `Write`;
- memory and time: `AllocMemorySL`, `GetDateTime`;
- keyboard: `ReadKbd`, `Beep`;
- video: `PosFrameCursor`, `PutFrameAttrs`, `PutFrameChars`, `ResetFrame`, `ScrollFrame`;
- parameters: `GetpASCB`;
- leaving: `Exit`, `ErrorExit`.

Their addresses come from the formulas in `lib/ctos.asm`. To add one, give it a label there
and a declaration with `#pragma aux (CTOSCALL)` in `ctos.h`.

## Not done yet

- `argc` and `argv` from the command form. `main` gets 0 and `NULL`; `GetpASCB` leads to the
  parameter block.
- File I/O through `stdio` (`fopen` and so on), on top of `OpenFile`, `Read` and `Write`.
- Floating point.
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
- **`hello.run`** prints a greeting and a line of formats, then reads a name from the keyboard:

  ```
  Hello from C on CTOS!
  -42 65535 beef 123456789 ok|    7|7    |00007
  What's your name? Scott
  Hello, Scott
  ```

Both give exactly this output on the AWS and NGEN simulators. A non-zero `return` from `main`
shows as an error with that number on both.

On the AWS, check that a program uses no 80186 instructions. On an 8086 they decode as other
instructions, and the program crashes or misbehaves. `wcc -0` doesn't generate them, but code
written in assembly should be checked.
