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
make                       # build/libc.lib, build/X.run for each examples/X.c,
                           # hexedit/hexedit.run, the tools in utils/ and the
                           # games in games/
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
  offset 0. The file holds only the code and initialised data. The header's `cparMinAlloc`
  asks CTOS for the rest of the segment, up to the stack top, for the BSS, heap and stack,
  and `crt0` clears it. So `hexedit.run` is 22 KB rather than the 51 KB the whole segment
  would take. There are no relocations.
- **CTOS calls.** `include/ctos.h` declares the CTOS requests and OS subroutines as far
  functions with the PL/M calling convention: arguments pushed left to right, the callee pops
  them. `lib/ctos.asm` places each name at its fixed address in an absolute segment, so a call
  compiles to an ordinary far call and needs no CTOS library.

## The library

| Header | What's there |
|---|---|
| `stdio.h` | `fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `ftell`, `rewind`, `feof`, `ferror`, `clearerr`, `fflush`, `fgetc`, `fputc`, `ungetc`, `fgets`, `fputs`, `remove`, `rename`, `perror`; the `printf` and `scanf` families; `getchar`, `putchar`, `puts` |
| `string.h` | the usual `mem*` and `str*` functions, `strtok`, `strdup`, and `strerror`, which knows the text of the common CTOS error codes |
| `stdlib.h` | `exit`, `abort`, `abs`, `labs`, `div`, `ldiv`, `atoi`, `atol`, `strtol`, `strtoul`, `rand`, `srand`, `malloc`, `calloc`, `realloc`, `free`, `qsort`, `bsearch` |
| `time.h` | `time`, `clock`, `mktime`, `gmtime`, `localtime`, `asctime`, `ctime`, and `ctos_time` for the dates in CTOS's own structures |
| `screen.h` | full-screen output, single keys and raw keys, line-drawing characters, and fonts (below) |
| `setjmp.h` | `setjmp`, `longjmp` |
| `dirent.h`, `sys/stat.h` | `opendir`, `readdir`, `rewinddir`, `closedir`, `mkdir`, `rmdir`; `stat` for a file's size and dates |
| `unistd.h` | `getcwd`, the current path as `[d0]<Sys>`, and `getopt` for options like `-n` |
| `process.h` | `execv` and `execf`, to run another program in place of this one, with arguments; `arg_field`, the form field each argument came from |
| `farmem.h` | memory beyond the 64 KB segment (below) |
| `serial.h` | the RS-232 ports (below) |
| `ctosmsg.h` | CTOS exchanges and messages, and `DoRequest` for any CTOS request (below) |
| `assert.h`, `limits.h`, `ctype.h`, `errno.h`, `stdarg.h`, `stddef.h` | as usual. The `ctype.h` classes are functions, not macros, so `tolower(*p++)` is safe. `errno` holds the CTOS error code of the last failure, and `char` is unsigned. |
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
  `ErrorExit` with it, which the Executive shows as an error with that CTOS code's message:
  1 is "End of file encountered". So a program that has already said what went wrong should
  still end with 0.
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

The Executive expands wildcards itself: a word with `*` becomes the names of the files it
matches, in directory order, or nothing if none do. Single quotes stop that, and keep spaces:
`'b.*z'` arrives as `b.*z`, and `'x y'` as one argument.

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
  if no key is waiting. The `KEY_` names give the codes of the special keys: the arrows,
  `RETURN`, `GO`, `BACKSPACE`, `TAB`, `DELETE`, `HELP`, `CANCEL`, `FINISH`, `MARK`, `BOUND`,
  `PREVPAGE`, `NEXTPAGE`, `SCROLLUP`, `SCROLLDOWN` and `F1` to `F10`. They're the same on
  both machines.
- **The cursor:** `scr_cursor(row, col)` puts it there, and
  `scr_cursor(SCR_NOCURSOR, SCR_NOCURSOR)` takes it away. The AWS's cursor is a reverse
  block, so on a character shown in reverse it cancels out; the NGEN's is an underline.
- **Raw keys:** `scr_rawkeys(1)` makes those return each key's own code going down, and the
  code plus `KEY_RELEASED` coming up, so a game can tell which keys are held.
- **Timing:** `delay(ms)` waits, in steps of 100 ms.
- **Mixing with printf:** `printf` output continues on the line set by `scr_line(row)`, or at
  the top after `scr_clear`.

When the program ends, the Executive scrolls the frame up to put its own prompt below, so
the top lines go out of sight.

**Line drawing.** `BOX_H`, `BOX_V`, the corners `BOX_TL`, `BOX_TR`, `BOX_BL` and `BOX_BR`, the
tees and `BOX_X` are the single-line box characters of the CTOS character set, and
`SHADE_1` to `SHADE_3` its shading. `BLOCK` is a solid block. `examples/charset.c` shows all
256 characters. Both machines show them. On the AWS, though, they come from an alternate font,
switched in by an attribute, and a screen line can only change attributes 16 times:
- each separate run of these characters uses two of them (`BLOCK` uses none);
- so does each `scr_attr` run.

A box costs little along its top and bottom, but its sides cost two a line each. Past the
limit, a character shows as its plain 7-bit neighbour.

**Fonts.** On the NGEN the character shapes are in RAM. `scr_loadfont(name)` loads a font
file: 256 characters of 16 words, each 9 dots wide, in the low 9 bits of words 1-15. The font
stays after the program ends, so load `[Sys]<Sys>t1Sys.font` back before leaving. The AWS's
font is in ROM, so this does nothing there.

### Memory beyond 64 KB

`farmalloc(n)` gives a far pointer to a block of up to 65520 bytes outside the program's
segment, from CTOS's short-lived memory, and `farfree` gives it back. `_fmemcpy`, `_fmemmove`,
`_fmemset` and `_fmemcmp` work on far memory, and `farcoreleft()` says how much CTOS has left.
`FP_SEG`, `FP_OFF` and `MK_FP` take far pointers apart and make them.

CTOS returns short-lived memory only in the reverse order of allocation. So `farfree` hands
a block back once every block allocated after it has been freed too; free in reverse order
to get memory back at once.

### Directories and running programs

`opendir("[f0]<C>")` and `readdir` list a directory (`opendir("")` the current one), and
`mkdir` and `rmdir` make and remove one. `stat` gives a file's size in bytes, its allocated
size, and when it was created and last changed.

`execv(runfile, argv)` runs another program in place of this one, which gets `argv` as its
`argc` and `argv`, the same as from a command form. An argument may hold spaces. Open files
are closed first, and `execv` returns only if CTOS can't start the program.

A command form's fields each give their words as arguments, so `argv` alone doesn't say
where one field ended. `arg_field(i)` gives the field `argv[i]` came from: 0 for the command
name, then 1, 2 and on. `execf(runfile, argv, field)` is `execv` with each argument's field
given, so a field can hold several words, as when typed with spaces between.

### Serial ports

`ser_open(SER_A, 9600)` takes port A (or `SER_B`) from CTOS, at 8 data bits, no parity and 1
stop bit. `ser_putc`, `ser_puts` and `ser_getc` send and receive, and `ser_close` gives the
port back.
- **Polling:** the library reads and writes the serial chip directly, with its interrupts
  off. `ser_getc` returns -1 at once if no byte has come, so call it often enough: at 9600
  baud a byte comes about every millisecond, and the chip holds three.
- **The two CTOS versions:**
  - On the NGEN, CTOS II's `InitCommLine` takes the port and sets the baud rate.
  - The AWS's CTOS has only `SetCommISR`, so the library sets the baud rate itself, on the
    second 8253 (port A's at AEh, B's at ADh), and uses the 7201 at A8h-ABh.

### Messages

`ctosmsg.h` has CTOS's exchanges and messages: `AllocExch`, `DeallocExch`, `Send`, `Wait`,
`Check` and `QueryDefaultRespExch`. A message is any 4 bytes, usually a far pointer, queued at
an exchange. Programs in different partitions, on a CTOS with them, find each other's exchanges
through CTOS's partition calls. Between cluster workstations, CTOS's documented route is the
Queue Manager on the master. Neither is wrapped here, and the simulators can't run a cluster.

`DoRequest(&rq)` sends a request block and waits for the reply. That reaches any CTOS request,
including those numbered 1000h and up, which have no fixed address to call. `serial.c` uses
it for `InitCommLine` (1006h).

### CTOS calls

The CTOS calls in `ctos.h` and `ctosmsg.h`:
- files: `OpenFile`, `CreateFile`, `CloseFile`, `ChangeFileLength`, `Read`, `Write`,
  `DeleteFile`, `RenameFile`, `GetFileStatus`, `SetFileStatus`;
- directories: `CreateDir`, `DeleteDir`, `ReadDirSector`;
- memory: `AllocMemorySL`, `DeallocMemorySL`, `AllocMemoryLL`, `QueryMemAvail`;
- time: `GetDateTime`, `Delay`;
- keyboard: `ReadKbd`, `ReadKbdDirect`, `SetKbdUnencodedMode`, `Beep`;
- video: `PosFrameCursor`, `PutFrameAttrs`, `PutFrameChars`, `ResetFrame`, `ScrollFrame`,
  `LoadFontRam`, `SetScreenVidAttr`;
- programs: `GetpASCB`, `Chain`, `Exit`, `ErrorExit`;
- messages: `AllocExch`, `DeallocExch`, `QueryDefaultRespExch`, and the kernel's `Send`,
  `Wait`, `Check` and `Request`.

Their addresses come from the formulas in `lib/ctos.asm` and `lib/kernel.asm`. To add one,
give it a label there and a declaration with `#pragma aux (CTOSCALL)`, or build its request
block for `DoRequest`.

## hexedit

`hexedit/` is a full-screen hex editor, and the biggest program built with the library.

```
hexedit [file]
```

Run it with the Executive's `Run` command, giving the file as the first parameter, or make
it a command with `New Command`. Without a file name it asks for one. A file that doesn't
exist yet is made when it's saved, and one that can't be opened for changing is shown read
only.

The screen has the offset, 16 bytes in hex and the same bytes as text on each line, with the
cursor shown on both sides. `TAB` moves between them. Bytes changed since the last save are
underlined. The line below shows the byte at the cursor in hex, decimal, as a character and
in binary, and the word and long word starting there (low byte first). The keys are on the
bottom line, and `HELP` shows them all:
- **Moving:** the arrows, `PREV PAGE` and `NEXT PAGE`, `SCROLL UP` and `SCROLL DOWN`, and
  `f1` to go to an offset: hex, decimal after `#`, `+n` or `-n` from the cursor, or `$` for
  the end.
- **Changing:** hex digits on the hex side, characters on the text side. `f4` switches
  between overtype and insert. `DELETE` removes the byte at the cursor; `BACKSPACE` moves
  back, removing the byte before in insert mode. `f5` undoes and `f6` redoes, back as far
  as the last save.
- **Blocks:** `f7` or `MARK` starts a block at the cursor, and moving the cursor sets its
  other end. `f8` copies it, `f9` pastes, over the block if there is one, and `DELETE`
  removes it. `f7` again or `CANCEL` drops it.
- **Finding:** `f2` finds hex bytes and quoted text, mixed: `4D 5A`, `"PK" 03 04`. `f3`
  finds the next one. Both carry on from the start when they reach the end.
- **Leaving:** `f10` saves, and `FINISH` leaves, asking first if there are unsaved changes.

It works on files bigger than memory. The file stays on disk, read 4 KB at a time, and the
editor keeps a list of pieces: runs of bytes from the file, or from a buffer holding what was
typed. An edit replaces some pieces with others, and undo keeps both lists. The limits are on
changes between saves, not on the file: at most 32 KB of typing and 4000 pieces, less if
memory is short. `HELP` shows how much is left.

Saving writes only the changed pages when every byte from the file is still where it was.
Otherwise, after an insert or a delete, it writes the whole file as `name-new` and renames it
over the old one, so the disk needs room for both for a moment.

The AWS shows reverse and underline together as underline alone, and its cursor is a reverse
block, so the editor uses neither: its cursor is drawn with attributes, the same on both
machines. It also counts the attribute runs on each line, and leaves out the underlines on
changed bytes on a line that would have more than the AWS's 16.

## The tools

`utils/` has tools CTOS doesn't have, in the style of the Unix ones:

| Tool | What it does |
|---|---|
| `grep [-invclhF] 'pattern' file...` | lines matching a regular expression: `.`, `[a-z]`, `[^0-9]`, `*`, `+`, `?`, `^`, `$`, and `\s`, `\d`, `\w` |
| `find [-l] 'pattern' [volume...]` | files by name, such as `'*.c'` or `'<Sys>*.run'`, in every directory of a volume; `-l` adds sizes and dates |
| `diff [-uiwqb] file1 file2` | the fewest changes between two text files, as a classic or (`-u`) unified diff, or where two binary files differ |
| `sort [-rnfu] [-k field] [-t c] file...` | lines in order: reversed, as numbers, ignoring case, without repeats, on a field |
| `wc [-lwc] file...` | lines, words and bytes, with totals |
| `du [volume or [vol]<dir>]` | the space each directory uses and the volume's free space, or each file in a directory, biggest first |
| `sum file...` | the CRC-32 of each file, the same as zip's and Python's `zlib.crc32`, to check a copy or a transfer |
| `calc ['expression']` | a 32-bit programmer's calculator, with C's operators and hex, octal, binary and characters; without an expression it asks for one after another |
| `sysinfo [-v]` | the CTOS version, boot device, processor, date, path, free memory, screen and each disk volume; `-v` adds each volume's home block and CTOS's System Common table |
| `xmodem -s\|-r [-k] [-t] [-p B] [-b baud] file` | sends or receives a file over a serial port with XMODEM, to or from a PC's terminal program |

They share a few habits:
- **Wildcards.** File names take `*` and `?`, as in `[f0]<C>*.txt`. The Executive expands
  them before the tool starts, and the tool expands any that reach it in quotes. A pattern
  with a `*` or a space, for `grep`, `find` or `calc`, needs single quotes, or the Executive
  takes it as a file name.
- **Output.** It stops when the screen is full, until RETURN; CANCEL ends the tool. An
  argument `>name` sends it to a file instead, since the Executive has no redirection. The
  file is made after the input is read, so `sort list >list` sorts a file into itself.
- **Endings.** They end normally even when they find nothing or something fails, and say so,
  since the Executive shows any other ending as a CTOS error with an unrelated message.

**Installing them as commands.** `make` also writes `utils/Install.sub`, a Submit file. Copy
it, `mkcmd.run` and the tools' run files to a directory, set the Path to that directory, and
`Submit Install.sub`. It copies each tool to `[Sys]<Tools>`, a directory of its own so that
nothing of CTOS's is replaced, and makes its command, with a form:

```
Grep
  Pattern                     'b.*z'
  File list                   *.txt
  [Ignore case?]
  [Line numbers?]             yes
  [Lines that do not match?]
  [Count only?]
  [File names only?]
  [Pattern is plain text?]
  [Output file]
```

The commands are the tools' names, except `Sort Lines`: CTOS II has a `Sort` already. A
command that's already there is left alone, so the Submit file can be run again to update
the run files. Started by its own command, a tool turns its form's yes-or-no fields into its
options; with `Run`, it takes options as before.

Each tool's form is one string in its source, `FORM`: what it does, then each field's name,
and for the ones that aren't arguments, the option it gives (`[Ignore case?] -i`) or `>` for
the output file. `tools/mksub.py` writes the Submit file from those strings, and the tool
reads its fields with them. `New Command`'s own form takes only what fits on the screen line,
too few for nine field names, so the Submit file runs `mkcmd`, which gives them to New
Command's program directly: `Command.run` on the AWS, `CmdFileEditor.run` on the NGEN.

`find` and `du` read the volume's `<Sys>FileHeaders.Sys` straight through, which holds every
file's name, directory, size and dates, rather than opening each directory and file. They
stop once they have seen as many headers as the volume says are in use, so a whole hard disk
takes seconds. `du` also reads `<Sys>Mfd.Sys` for the directories with nothing in them. CTOS
compares names ignoring case, and so do they: its own files give their directory as `<sys>`.

`sysinfo` gets each volume's home block with `GetVHB`, the current path with `GetUCB`, and the
version and boot device from System Common, a table of pointers to CTOS's structures at
`0000:0240`. The names of its entries come from CTOS's `SysCom.Edf`. The version string and
boot device name are the same on CTOS 9 and CTOS II; what the other entries point to differs
between versions, so it only shows them.

`xmodem` moves files to and from a PC without a floppy. Start it, then the PC's side: a
terminal program's XMODEM send or receive, such as Tera Term's, or `sx` and `rx` from lrzsz.
It uses a CRC-16 when the other end can, a checksum otherwise, takes 128-byte and 1 KB blocks,
and sends 1 KB blocks with `-k`. A damaged block is sent again. It's 9600 baud on port A
unless told otherwise, and it polls the serial chip, so nothing is written to the screen
while a block is coming in. XMODEM pads a file to a whole block with ^Z; `-t` takes them off,
which is right for text.

## The games

`games/` has two, each a full-screen program:
- **`chess [-b] [-t seconds] [-d depth] ['position']`** plays you at chess, you as White,
  or Black with `-b`. Type moves as `e4`, `Nf3`, `exd5`, `O-O` and `e8=Q`, or as squares,
  `e2e4`. It thinks for up to 10 seconds a move, or `-t`; `-d` searches a fixed number of
  moves ahead instead. A position in FEN, in single quotes, sets up the board. At its prompt,
  `new`, `undo`, `switch` (it takes your side), `level` and `depth` change the game, and
  `perft N` counts the positions N moves ahead. The engine searches with alpha-beta and
  iterative deepening, follows captures to the end, and keeps a table of positions it has
  seen in far memory. It knows all the rules, en passant and underpromotion among them,
  and draws by repetition and the fifty-move rule. Its move generator gives the standard
  perft counts for the usual test positions. Black's pieces are shown in brackets: in the
  AWS's font a `p` looks like a `P`.
- **`patrol [-s seed] [-e]`** is a game of hunting raiders. You command the cutter Kestrel in
  the Reach, 8 by 8 zones of 8 by 8 sectors, and have to destroy every raider before the
  deadline. You `move` by compass heading and distance, fire the `lance`, whose energy is
  split among the raiders in the zone, or a `dart`, which aims itself, and keep up your
  `shields`, since the raiders close in and fire back. The `chart` shows what's known of the
  zones around, `nav` gives the heading to a place, and docking next to a depot restocks and
  mends the Kestrel. `-e` is easier; `-s` plays a given Reach again.

## Not done yet

- Floating point.
- `strftime`.
- File passwords: files are opened with an empty password.
- Programs bigger than 64 KB of code and data.

## Testing

```sh
make test
```

runs each example, `hexedit`, the tools and the games on the AWS and NGEN simulators and
checks what they do. It needs:
- **The simulator**, [aws-simulator](https://github.com/sbelectronics/aws-simulator), built
  with its disk images, a version whose `awssim` has `-warn186`, `-flsave` and `-serial`,
  and whose typing and `-serialin` take `\xHH` for any key or byte.
  Set `SIMDIR` to its directory.
- **ctostool**, [ctostool](https://github.com/sbelectronics/ctostool), to make the test
  floppy and read it back afterwards. Set `CTOSTOOL` to its directory, and `PYTHON2` to the
  Python 2 that runs it if that isn't `python2`.

`tools/simtest.py` does the work, and runs on any machine with those. `-m aws` or `-m ngen`
picks one machine, names after the options pick tests, and `-k DIR` keeps the floppies and
logs. `-s SECONDS` stops each run that long after it starts the program, and saves a
screenshot in the `-k` directory. Here's what it does:
1. **Makes the test floppies:** for each test it unpacks `test/blank.img.gz`, an empty CTOS
   floppy, and adds the program, any program it runs, and its data files, in directory `<C>`.
2. **Runs everything at once:** each program runs in its own simulator, started by typing the
   Executive's `Run` command.
3. **Checks the results:**
   - the output each program should give;
   - on the AWS, no 80186 instructions (`awssim -warn186`);
   - for `filetest`, `hexedit` and `sort`, the files as the program left them (`-flsave`);
   - for `sertest` and `xmodem`, what went out of the serial port (`-serial`), with bytes
     sent in (`-serialin`).

The tests:
The programs are in `examples/`, and each is also an example of what it checks, apart from
`hexedit`.
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
  the arrows on the AWS, then raw key codes, along with `delay` and the date.
- **`ctostest`** checks far memory, messages, `strerror`, directories and `stat`. Then it runs
  `filetest` with `execv`, giving it `[f0]<C>` and `via execv`, which `filetest` prints as
  `argc 3: [filetest] [[f0]<C>] [via execv]` before passing its own checks.
- **`charset`** shows the character set and a box drawn with the `BOX_` characters.
- **`fonttest`** loads a font with a checkered `A`, then the system font again, and prints
  what each returned: `load 0, restore 0`.
- **`memtest`** checks that CTOS gives a program the memory above its run file, for the BSS
  and stack: it takes every block of memory CTOS will give, and none may overlap them.
- **`hexedit`** edits a 45-byte file with keys typed at it: overtyping in hex and text,
  inserting, deleting, undo and redo, finding text, copying a block and pasting it, help,
  and saving twice, once as a new file and once in place. The runner checks the file.
- **`hexbig`** does the same with a 70,000-byte file: finding bytes past 64 KB, paging, a
  save in place, going to a decimal offset, an insert at the start and a save of the whole
  file.
- **`sertest`** sends `hello serial` from port A, waits for a line to come in, and sends it
  back after `echo: `. It runs on both machines.
- **`install`** runs `Install.sub` on a floppy with the tools, then uses the `Grep`, `Sort
  Lines` and `Sysinfo` commands it made, filling in their forms, with an output file.
- **The tools** each have a test with files on the floppy: `grep` with a quoted pattern and
  a wildcard, `find -l` and `du` on the floppy and on the hard disk (`finddisk`, `dudisk`),
  `diff`, `sort -n` into a file with `>`, `wc` with totals, `sum` against Python's CRC-32,
  `calc` given an expression and typed at (`calci`), and `sysinfo -v`, with a key for its
  second page.
- **`xmrecv`** receives a file with `xmodem -r -t`, from blocks sent in at set times: a
  128-byte one, a damaged 1 KB one, which it must ask for again, the 1 KB one whole, then the
  end. **`xmsend`** sends a file of two blocks, and is asked for the first one twice. Both
  check every byte that went out, and `xmrecv` the file.
- **`chess`** counts the positions two moves ahead (400), then takes `e4` and refuses
  `e5e6`. **`chessmate`** sets up a mate in two for the computer, plays `Kb8` for Black, and
  checks for `Kb6`, `Rh8#` and the checkmate.
- **`patrol`** plays the Reach of seed 7: into a zone with a raider, shields up, a dart that
  destroys it, and the chart.

`find`, `du` and `sysinfo` need the simulators' disks as they are: they look for the
volumes' names and some of their files.

On the AWS, code written in assembly should be checked for 80186 instructions: they decode
as other instructions on its 8086. `wcc -0` doesn't generate them.
