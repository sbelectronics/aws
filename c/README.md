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
make lib                   # just the library and crt0
make WATCOM=/path/to/watcom
```

### Your own programs

A program in another directory includes `ctos.mk` and lists the objects for each run file:

```make
CTOS_C = /path/to/aws/c
include $(CTOS_C)/ctos.mk

all: eliza.run
eliza.run: eliza.obj words.obj
```

Each `.obj` is compiled from the `.c` of the same name, and each `.run` is linked from the
objects listed for it. The library is built first if it needs to be. Add compiler options
with `CFLAGS`.

To put a run file on a CTOS floppy image, use
[ctostool](https://github.com/sbelectronics/ctostool):

```sh
python2 ctostool.py floppy.img add Sys eliza.run eliza.run
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
| `stdio.h` | `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `rewind`, `feof`, `ferror`, `clearerr`, `fflush`, `fgetc`, `fputc`, `ungetc`, `fgets`, `fputs`, `remove`, `rename`, `perror`; the `printf` and `scanf` families; `getchar`, `putchar`, `puts` |
| `string.h` | the usual `mem*` and `str*` functions, `strtok`, `strdup` |
| `stdlib.h` | `exit`, `abort`, `abs`, `labs`, `div`, `ldiv`, `atoi`, `atol`, `strtol`, `strtoul`, `rand`, `srand`, `malloc`, `calloc`, `realloc`, `free`, `qsort`, `bsearch` |
| `time.h` | `time`, `clock`, `mktime`, `gmtime`, `localtime`, `asctime`, `ctime` |
| `screen.h` | full-screen output and single keys (below) |
| `setjmp.h` | `setjmp`, `longjmp` |
| `assert.h`, `limits.h`, `ctype.h`, `errno.h`, `stdarg.h`, `stddef.h` | as usual. The `ctype.h` macros evaluate their argument more than once, `errno` holds the CTOS error code of the last failure, and `char` is unsigned. |
| `ctos.h` | the CTOS calls listed below |

- **Console.** `stdout` is the Executive's video frame 0. The first output clears it and
  writes from the top, scrolling at the bottom. `stdin` reads a line at a time from the
  keyboard, with echo and BACKSPACE. RETURN or GO ends the line.
- **Files.** A `FILE` is a CTOS file, opened with the modes `r`, `w`, `a` and their `+`
  forms (`b` is accepted and ignored: there's no text translation). Names are CTOS file
  specs; a plain name is in the current path. Details below.
- **printf** handles `%d %i %u %x %X %o %c %s %p %%`, with `-`, `0`, `+`, space and `#` flags,
  width, precision, `*`, and `l` for longs. **scanf** handles `%d %i %u %o %x %c %s %[...] %n
  %%`, with widths, `*`, `h` and `l`. There's no floating point.
- **malloc** is first fit in a 16 KB heap inside the segment (`HEAP_SIZE` in `lib/stdlib.c`).
  Each open file takes 540 bytes of it.
- **Time.** `time_t` counts seconds from 1970, from the CTOS date and time. CTOS has no time
  zones, so `localtime` is `gmtime`. `clock` counts whole seconds. There's no `difftime`,
  since that returns a `double`: subtract the `time_t` values instead.
- **Exit.** `exit(0)`, or returning 0 from `main`, calls `Exit`. A non-zero status calls
  `ErrorExit` with it.
- **Long arithmetic.** Watcom calls helpers for 32-bit multiply and divide; `lib/i4.asm`
  provides them.

Each part of the library is its own object, so a program links only what it uses.

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

### The screen

`screen.h` is for programs that draw the screen rather than print lines. Rows count from 0 at
the top of frame 0, and it's `SCR_COLS` (80) wide; `scr_rows()` gives its height.
- **Drawing:** `scr_clear`, `scr_puts(row, col, s)`, `scr_putc` and `scr_cursor`. Text past
  the right edge is cut off.
- **Attributes:** `scr_attr(row, col, n, attr)` with `ATTR_REVERSE`, `ATTR_UNDERLINE`,
  `ATTR_HALFBRIGHT` and `ATTR_BLINK`, which combine.
- **Keys:** `scr_getkey()` waits for a key, without echo. `scr_pollkey()` returns -1 at once
  if no key is waiting. `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_RETURN`,
  `KEY_GO` and `KEY_BACKSPACE` are their codes. (The arrows' codes were found on the AWS.)
- **Timing:** `delay(ms)` waits, in steps of 100 ms.
- **Mixing with printf:** `printf` output continues on the line set by `scr_line(row)`, or at
  the top after `scr_clear`.

When the program ends, the Executive scrolls the frame up to put its own prompt below, so
the top lines go out of sight.

### CTOS calls

The CTOS calls in `ctos.h`:
- files: `OpenFile`, `CreateFile`, `CloseFile`, `ChangeFileLength`, `Read`, `Write`,
  `DeleteFile`, `RenameFile`, `GetFileStatus`, `SetFileStatus`;
- memory and time: `AllocMemorySL`, `GetDateTime`, `Delay`;
- keyboard: `ReadKbd`, `ReadKbdDirect`, `Beep`;
- video: `PosFrameCursor`, `PutFrameAttrs`, `PutFrameChars`, `ResetFrame`, `ScrollFrame`;
- parameters: `GetpASCB`;
- leaving: `Exit`, `ErrorExit`.

Their addresses come from the formulas in `lib/ctos.asm`. To add one, give it a label there
and a declaration with `#pragma aux (CTOSCALL)` in `ctos.h`.

## Not done yet

- Floating point.
- `strftime`.
- File passwords: files are opened with an empty password.
- Programs bigger than 64 KB of code and data.

## Testing

```sh
make test
```

runs each example on the AWS and NGEN simulators and checks what it does. It needs:
- **The simulator**, [aws-simulator](https://github.com/sbelectronics/aws-simulator), built
  with its disk images. Set `SIMDIR` to its directory.
- **ctostool**, [ctostool](https://github.com/sbelectronics/ctostool), to make the test
  floppy and read it back afterwards. Set `CTOSTOOL` to its directory, and `PYTHON2` to the
  Python 2 that runs it if that isn't `python2`.

`tools/simtest.py` does the work, and runs on any machine with those. `-m aws` or `-m ngen`
picks one machine, names after the options pick tests, and `-k DIR` keeps the floppies and
logs. Here's what it does:
1. **Makes a test floppy:** it unpacks `test/blank.img.gz`, an empty CTOS floppy, and adds the
   programs to its directory `<C>`.
2. **Runs everything at once:** each program runs in its own simulator, started by typing the
   Executive's `Run` command.
3. **Checks the results:**
   - the output each program should give;
   - on the AWS, no 80186 instructions (`awssim -warn186`);
   - for `filetest`, the floppy as the program left it (`-flsave`).

The tests:
- **`selftest`** checks long arithmetic, 22 `printf` formats, `scanf`, `strtol`, the
  string functions, `qsort` and `bsearch`, `setjmp` and `longjmp`, `malloc`, the date
  conversions and `rand`. It prints each failure, then `selftest: 90 checks, 0 failed`.
- **`filetest`** gets `[f0]<C>` and `two words` as arguments and prints them (`argc 4: []
  [[f0]<C>] [two] [words]`). It then checks file I/O:
  - writing and reading back a 200-line text file of about 5 KB;
  - seeking, `ungetc`, appending and updating with `r+`;
  - binary reads and writes, emptying with `w`, and the zeros in a gap;
  - `rename`, `remove` and `errno`.

  It leaves `keep.txt` behind, and the runner checks that the file holds exactly the 63
  bytes written.
- **`hello`** prints a greeting and a line of formats, then reads a name typed at it.
- **`screentest`** draws on the screen and shows the code of each key typed at it, including
  the arrows on the AWS, along with `delay` and the date.

On the AWS, code written in assembly should be checked for 80186 instructions: they decode
as other instructions on its 8086. `wcc -0` doesn't generate them.
