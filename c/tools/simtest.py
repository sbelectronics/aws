#!/usr/bin/env python3
"""simtest.py - run the example programs, hexedit, the tools and the games on
the AWS and NGEN simulators and check what they do.

usage: simtest.py [-m aws|ngen] [-k DIR] [-s SECONDS] [test ...]

It needs:
  - the simulator, https://github.com/sbelectronics/aws-simulator, built, in
    the directory SIMDIR names, with its disk images (aws.emu, ngen.emu);
  - ctostool, https://github.com/sbelectronics/ctostool, in the directory
    CTOSTOOL names, and the Python 2 that runs it in PYTHON2 (default python2);
  - the programs, built by make.

Each test gets a floppy made from test/blank.img.gz, with its program in
directory <C>. Every test runs in its own simulator, all at once, and is started with
the Executive's Run command, typed in. On the AWS, any 80186 instruction is a
failure (awssim -warn186).
"""
import argparse, binascii, gzip, os, re, shutil, subprocess, sys, tempfile, zlib

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(HERE, 'build')

# Each test: the arguments for Run's [Param n] fields; what to type once the
# program is running (seconds after GO, text; 'type_ngen' on the NGEN only);
# raw AWS key codes to press (seconds after GO, hex; ngensim can only type);
# how long to let it run; and regular expressions the output must match,
# anywhere ('out') or as a whole screen line at some point ('screen', and
# 'screen_aws' or 'screen_ngen' on one machine). The final screen isn't
# enough: when a program ends, the Executive scrolls its top lines away.
# 'serialin' is text arriving at a serial port (seconds after GO, port,
# text), and 'sent' what the program must send from port A. 'keep' is what
# the file 'keepname' (default keep.txt) must hold afterwards. Each test has
# its own floppy, with its program, the programs of other tests it runs
# ('needs'), and 'files' (each the bytes, or a file's path from here). A
# program that isn't an example names its run file with 'prog'. Instead of
# Run, a test can type 'commands': seconds to wait after the last one's GO,
# the command's name, and what to type in its fields.

# 70000 bytes, more than a segment, with NEEDLE at 10F00h
BIG = bytearray(i % 251 for i in range(70000))
BIG[0x10F00:0x10F06] = b'NEEDLE'
BIG_SAVED = b'Z' + BIG[:0x10F00] + b'A' + BIG[0x10F01:]

# for the tools in utils/. The Executive expands * in parameters itself, and
# keeps what's in single quotes as one parameter, as it stands.
TEXT_A = b'alpha\nbar baz\ngamma\nfoo bar zed\n'
TEXT_B = b'alpha\nbar baz\nGAMMA\nfoo bar zed\nend\n'
TEXTS = {'a.txt': TEXT_A, 'b.txt': TEXT_B}
TOOLS = ['grep', 'find', 'diff', 'sort', 'wc', 'du', 'sum', 'calc', 'sysinfo', 'xmodem']

# XMODEM, as the other end sends it: blocks with a CRC, 128 or 1024 bytes
def xm_block(n, data, size=128):
    data = data + b'\x1a' * (size - len(data))
    crc = binascii.crc_hqx(data, 0)
    return bytes([1 if size == 128 else 2, n, 255 - n]) + data + bytes([crc >> 8, crc & 255])

def serial_text(data):
    """bytes for 'serialin', in the simulators' escapes"""
    return ''.join(chr(c) if 32 <= c < 127 and chr(c) not in ';\\' else '\\x%02X' % c for c in data)

XM_TEXT = b''.join(b'line %03d of the file sent with XMODEM\n' % i for i in range(25))
XM_BAD = bytearray(xm_block(2, XM_TEXT[128:], 1024))
XM_BAD[500] ^= 0x40

TESTS = {
    'selftest': dict(run=40, out=[r'selftest: \d+ checks, 0 failed']),
    'hello': dict(type=[(20, 'Scott\\n')], run=40,
                  out=[r'Hello from C on CTOS!',
                       re.escape('-42 65535 beef 123456789 ok|    7|7    |00007'),
                       r'Hello, Scott']),
    'filetest': dict(params=['[f0]<C>', 'two words'], run=70,
                     out=[r'filetest: \d+ checks, 0 failed',
                          re.escape('argc 4: [] [[f0]<C>] [two] [words]')],
                     keep=b'The quick brown fox.\n' * 3),
    'screentest': dict(type=[(20, 'ab'), (30, '\\n')], type_ngen=[(34, 'cd')],
                       awskeys=[(24, 0x01), (25, 0x0B), (26, 0x0E), (27, 0x0F),
                                (34, 0x01), (35, 0x0B)],
                       run=55,
                       screen=[r'^.{30}screen test', r'^rows \d+', r'^.{75}clipp$', r'^poll -1',
                               r'^delay [234]$',
                               r'^date \w{3} \w{3} [ \d]\d \d\d:\d\d:\d\d (1983|1987)$',
                               r'^printf on line 8$', r'^bottom line$'],
                       screen_aws=[r'^keys: 61 62 01 0B 0E 12$', r'^raw: 01 81 0B 8B$'],
                       screen_ngen=[r'^keys: 61 62$', r'^raw:( [0-9A-F]{2}){4}$']),
    'ctostest': dict(params=['[f0]'], needs=['filetest'], run=90,
                     out=[r'ctostest: \d+ checks, 0 failed',
                          re.escape('argc 3: [filetest] [[f0]<C>] [via execv]'),
                          r'filetest: \d+ checks, 0 failed'],
                     keep=b'The quick brown fox.\n' * 3),
    'charset': dict(type=[(15, '\\n')], run=30, screen=[r'a box']),
    'fonttest': dict(params=['[f0]<C>'], type=[(20, '\\n')], run=45,
                     screen_ngen=[r'^load 0, restore 0$'],
                     screen_aws=[r'^load -?\d+, restore -?\d+$']),
    # hexedit, keys by their AWS codes: TAB 09, f1-f6 15-1A, f7-f10 1C-1F,
    # right 0F, DELETE 7F, HELP 00
    'hexedit': dict(prog='hexedit/hexedit.run', params=['[f0]<C>hex.dat'],
                    files={'hex.dat': b'The quick brown fox jumps over the lazy dog.\n'},
                    type=[(20, '4a'),                       # overtype T with 4A
                          (23, r'\t!!'),                    # text side: overtype "he"
                          (26, r'\x18XY'),                  # insert XY
                          (29, r'\x7F'),                    # delete the space
                          (31, r'\x19\x19\x1A'),            # undo twice, redo
                          (35, r'\x16"lazy"\n'),            # find
                          (41, r'\x1C\x0F\x0F\x0F\x1D'),    # mark "lazy", copy
                          (46, r'\x150\n'),                 # go to 0
                          (49, r'\x1E\x18'),                # paste, back to overtype
                          (52, r'\x1F'),                    # save: a new file
                          (62, r'\x150\n\t4c'),             # hex side: overtype l with 4C
                          (68, r'\x00'), (71, 'x'),         # help and back
                          (74, r'\x1F'),                    # save in place
                          (82, r'\f')],                     # leave
                    run=92,
                    screen=[r'HexEdit +\[f0\]<C>hex\.dat', r'Found at 00000025',
                            r'^ Saved 51 bytes', r'^ *. +Moving +Changing',
                            r'^ Offset 00000000 = 0 +Byte 4C  76 .L. 01001100  Word 614C  Long 797A614C'],
                    keepname='hex.dat',
                    keep=b'LazyJ!!XY quick brown fox jumps over the lazy dog.\n'),
    'hexbig': dict(prog='hexedit/hexedit.run', params=['[f0]<C>big.dat'],
                   files={'big.dat': bytes(BIG)},
                   type=[(20, r'\x16"NEEDLE"\n'),           # find, past 64 KB
                         (34, '41'),                        # overtype N with 41
                         (37, r'\x0C\x0C\x05'),             # NEXT PAGE twice, PREV PAGE
                         (41, r'\x1F'),                     # save in place
                         (52, r'\x15#100\n'),               # go to decimal 100
                         (57, r'\x150\n\x18\tZ'),           # go to 0, insert Z
                         (66, r'\x1F'),                     # save: a new file
                         (110, r'\f')],                     # leave
                   run=125,
                   screen=[r'Found at 00010F00', r'^ Saved 70000 bytes',
                           r'^ Offset 00000064 = 100 ', r'^ Saved 70001 bytes'],
                   keepname='big.dat', keep=BIG_SAVED),
    'memtest': dict(run=60, out=[r'memtest: \d+ blocks, 0 overlap', r'0 bytes of _BSS not zero']),
    # the tools in utils/
    'grep': dict(prog='utils/grep.run', params=['-n', "'b.*z'", '[f0]<C>*.txt'], files=TEXTS, run=35,
                 screen=[r'^\[f0\]<C>a\.txt:2:bar baz$', r'^\[f0\]<C>a\.txt:4:foo bar zed$',
                         r'^\[f0\]<C>b\.txt:4:foo bar zed$']),
    'find': dict(prog='utils/find.run', params=['-l', "'*.txt'", '[f0]'], files=TEXTS, run=35,
                 screen=[r'^ +32  \d{4}-\d\d-\d\d \d\d:\d\d  \[f0\]<C>a\.txt$',
                         r'^ +36  \S+ \S+  \[f0\]<C>b\.txt$']),
    'diff': dict(prog='utils/diff.run', params=['[f0]<C>a.txt', '[f0]<C>b.txt'], files=TEXTS,
                 run=35, screen=[r'^3c3$', r'^< gamma$', r'^---$', r'^> GAMMA$', r'^4a5$', r'^> end$']),
    'sort': dict(prog='utils/sort.run', params=['-n', '[f0]<C>n.txt', '>[f0]<C>s.txt'],
                 files={'n.txt': b'10\n-3\n7\n100\n2\n'}, run=35,
                 keepname='s.txt', keep=b'-3\n2\n7\n10\n100\n'),
    'wc': dict(prog='utils/wc.run', params=['[f0]<C>*.txt'], files=TEXTS, run=35,
               screen=[r'^ +4 +7 +32  \[f0\]<C>a\.txt$', r'^ +5 +8 +36  \[f0\]<C>b\.txt$',
                       r'^ +9 +15 +68  total$']),
    'du': dict(prog='utils/du.run', params=['[f0]'], files=TEXTS, run=35,
               screen=[r'^ +\d+ +\d+ +3  <C>$', r'(?i)^ +\d+ +\d+ +\d+  <Sys>$', r'KB free$']),
    # the same two on the hard disk
    'finddisk': dict(prog='utils/find.run', params=["'exec.run'", '[d0]'], run=40,
                     screen=[r'(?i)^\[d0\]<sys>exec\.run$']),
    'dudisk': dict(prog='utils/du.run', params=['[d0]'], run=40,
                   screen=[r'(?i)^ +\d+ +\d+ +\d+  <sys>$', r'directories on \[d0\]',
                           r'^ +\d+ KB free of \d+ KB$']),
    'sum': dict(prog='utils/sum.run', params=['[f0]<C>a.txt'], files=TEXTS, run=35,
                screen=[r'^%08X +32  \[f0\]<C>a\.txt$' % zlib.crc32(TEXT_A)]),
    'calc': dict(prog='utils/calc.run', params=["'6 * 7'"], run=35,
                 screen=[r"^  42   hex 2A   oct 52   '\*'$", r'^  bin 0010 1010$']),
    'calci': dict(prog='utils/calc.run', type=[(15, 'x = 0FFh\\n'), (22, 'x >> 4 | 0b1\\n'), (29, '\\n')],
                  run=40, screen=[r'^  255   hex FF', r'^  15   hex F   oct 17$']),
    'sysinfo': dict(prog='utils/sysinfo.run', params=['-v'], type=[(25, '\\n'), (30, '\\n')], run=45,
                    screen=[r'^Booted from  \[d0\]$', r'^Path         \[d0\]<Sys>$|^Path         \[Sys\]<Sys>$',
                            r'^\[f0\] +CTest ', r'24C pOPcbRun +[0-9A-F]{4}:[0-9A-F]{4}'],
                    screen_aws=[r'^CTOS         a240ClstrLfsSp-9\.11$', r'^Processor    8086 or 8088$',
                                r'^\[d0\] +ScottAWS '],
                    screen_ngen=[r'^CTOS         t1ClstrLfs-II-3\.2\.0$', r'^Processor    80186 or 80188$',
                                 r'^\[d0\] +Win ']),
    # Install.sub makes the tools commands; then the commands' forms work
    'install': dict(prog='utils/grep.run', run=40,
                    files=dict([('%s.run' % t, 'utils/%s.run' % t) for t in TOOLS] +
                               [('Install.sub', 'utils/Install.sub'), ('mkcmd.run', 'utils/mkcmd.run'),
                                ('n.txt', b'10\n-3\n7\n')] +
                               list(TEXTS.items())),
                    commands=[(0, 'path', ['[f0]', 'C']),
                              (5, 'submit', ['Install.sub']),
                              (220, 'grep', ["'b.*z'", '*.txt', '', 'yes']),
                              (25, 'sort lines', ['n.txt', '', 'yes', '', '', '', '', 's.txt']),
                              (25, 'sysinfo', [])],
                    screen=[r'^a\.txt:2:bar baz$', r'^b\.txt:4:foo bar zed$', r'^Booted from  \[d0\]$'],
                    keepname='s.txt', keep=b'-3\n7\n10\n'),
    # xmodem: the other end's side is scripted, at fixed times
    'xmrecv': dict(prog='utils/xmodem.run', params=['-r', '-t', '[f0]<C>keep.txt'], run=55,
                   serialin=[(10, 'A', serial_text(xm_block(1, XM_TEXT[:128]))),
                             (14, 'A', serial_text(bytes(XM_BAD))),
                             (20, 'A', serial_text(xm_block(2, XM_TEXT[128:], 1024))),
                             (26, 'A', serial_text(b'\x04'))],
                   sent='\x06\x15\x06\x06', keep=XM_TEXT,
                   screen=[r'^xmodem: received \[f0\]<C>keep\.txt, %d bytes$' % len(XM_TEXT)]),
    'xmsend': dict(prog='utils/xmodem.run', params=['-s', '[f0]<C>x.txt'], files={'x.txt': XM_TEXT[:200]},
                   run=50, serialin=[(14, 'A', 'C'), (18, 'A', '\\x15'), (22, 'A', '\\x06'),
                                     (26, 'A', '\\x06'), (30, 'A', '\\x06')],
                   sent=(2 * xm_block(1, XM_TEXT[:128]) + xm_block(2, XM_TEXT[128:200]) +
                         b'\x04').decode('latin1'),
                   screen=[r'^xmodem: sent \[f0\]<C>x\.txt, 200 bytes$']),
    # chess: the move generator, a move typed in and a bad one, then a mate in 2
    'chess': dict(prog='games/chess.run', params=['-d', '2'],
                  type=[(15, 'perft 2\\n'), (35, 'e4\\n'), (55, 'e5e6\\n'), (62, r'\f')], run=72,
                  screen=[r'perft 2: 400 positions', r'  1\. e4 +\S+', r"e5e6 isn't a legal move here"]),
    'chessmate': dict(prog='games/chess.run', params=['-b', '-d', '3', "'k7/8/2K5/8/8/8/8/7R w'"],
                      type=[(35, 'Kb8\\n'), (55, r'\f')], run=65,
                      screen=[r'  1\. Kb6 +Kb8', r'  2\. Rh8#', r'Checkmate: White wins']),
    # patrol, with the Reach of seed 7: into a raider's zone, a dart, the chart
    'patrol': dict(prog='games/patrol.run', params=['-s', '7'],
                   type=[(15, 'm 0 10\\n'), (22, 'm 270 8\\n'), (29, 'shields 1200\\n'), (36, 'd\\n'),
                         (43, 'chart\\n'), (50, 'q\\n')], run=60,
                   screen=[r'Zone 7,5: 1 raider here!', r'The raider at 6,3 is destroyed\.',
                           r'Raiders left 18', r'1d <- > -  +Shields 1200']),
    'sertest': dict(serialin=[(10, 'A', 'ping from the far end\\n')], run=30,
                    out=[r'got: ping from the far end', r'sertest done'],
                    sent='hello serial\r\necho: ping from the far end\r\n'),
}

MACHINES = {
    # sim name, extra options, the typing that signs on, when the Executive is
    # ready. The floppy goes in after the hard disk has booted: with it in the
    # drive at power-on, the boot ROM would try to boot from it.
    'aws': ('awssim', ['-warn186', '-insert', '35'], '30:\\g', 40),
    'ngen': ('ngensim', ['-insert', '55'], '30:\\n;31:\\n;32:Thu Nov 5, 1987 1:00 pm;50:\\g', 62),
}


def run_file(test):
    prog = TESTS[test].get('prog')
    return os.path.join(HERE, prog) if prog else os.path.join(BUILD, test + '.run')


def sim_exe(simdir, name):
    for n in (name + '.exe', name):
        p = os.path.join(simdir, 'out', n)
        if os.path.exists(p):
            return p
    sys.exit('simtest: no %s in %s/out (build the simulator first)' % (name, simdir))


def ctostool(args, **kw):
    tool = os.path.join(os.environ['CTOSTOOL'], 'ctostool.py')
    return subprocess.run([os.environ.get('PYTHON2', 'python2'), tool] + args,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True, **kw).stdout


def typing(machine, test, spec):
    """The -type script: sign on, Run with the run file and the parameters
    (or the test's commands), then the test's input. Returns it, the time of
    the last GO, and when to stop."""
    _, _, signon, t = MACHINES[machine]
    parts = [signon]
    commands = spec.get('commands') or [
        (0, 'run', ['[f0]<C>%s.run' % test, '', ''] + spec.get('params', []))]
    for wait, name, fields in commands:
        t += wait
        parts.append('%d:%s%s' % (t, name, '\\n' if fields else '\\g'))
        t += 2
        for i, text in enumerate(fields):
            end = '\\g' if i == len(fields) - 1 else '\\n'
            parts.append('%d:%s%s' % (t, text, end))
            t += 2 + len(text) * 0.3     # typing takes about 0.2 s a character
            t = int(t + 0.99)
    go = t
    for dt, text in spec.get('type', []) + spec.get('type_' + machine, []):
        parts.append('%d:%s' % (go + dt, text))
    return ';'.join(parts), go, go + spec['run']


def final_screen(log):
    i = log.rfind('final screen:')
    return log[i:].splitlines()[1:] if i >= 0 else []


def check(machine, test, spec, log, floppy):
    problems = []
    for pat in spec.get('out', []):
        if not re.search(pat, log):
            problems.append('no output matching %r' % pat)
    lines = [line.rstrip() for line in log.splitlines()]
    for pat in spec.get('screen', []) + spec.get('screen_' + machine, []):
        if not any(re.search(pat, line) for line in lines):
            problems.append('no screen line matching %r' % pat)
    screen = [line.rstrip() for line in final_screen(log)]
    for line in log.splitlines():
        if '80186 instruction' in line:
            problems.append(line.strip())
    if 'sent' in spec:
        sent = ''.join(chr(int(h, 16)) for h in re.findall(r'serial A sent ([0-9A-F]{2})', log))
        if spec['sent'] not in sent:
            problems.append('sent %r on port A' % sent[:80])
    if 'keep' in spec:
        keepname = spec.get('keepname', 'keep.txt')
        try:
            got = ctostool([floppy, 'extract', 'C', keepname])
            if got != spec['keep']:
                problems.append('%s holds %r' % (keepname, got[:80]))
        except subprocess.CalledProcessError:
            problems.append('no %s on the floppy' % keepname)
        if b'0 errors' not in ctostool([floppy, 'chkdsk']):
            problems.append('the floppy fails ctostool chkdsk')
    return problems, screen


def main():
    ap = argparse.ArgumentParser(description='Run the example programs on the simulators.')
    ap.add_argument('-m', '--machine', choices=sorted(MACHINES), action='append',
                    help='only this machine (default both)')
    ap.add_argument('-k', '--keep', help='keep the floppies and logs in this directory')
    ap.add_argument('-s', '--shot', type=float, metavar='SECONDS',
                    help='stop this long after GO and save a screenshot (with -k)')
    ap.add_argument('tests', nargs='*', help='tests to run (default all): %s' % ', '.join(TESTS))
    a = ap.parse_args()
    for var in ('SIMDIR', 'CTOSTOOL'):
        if var not in os.environ:
            sys.exit('simtest: set %s (see the top of %s)' % (var, sys.argv[0]))
    simdir = os.environ['SIMDIR']
    machines = a.machine or sorted(MACHINES)
    tests = a.tests or list(TESTS)
    for t in tests:
        if t not in TESTS:
            sys.exit('simtest: no test %s' % t)
    for t in TESTS:
        if not os.path.exists(run_file(t)):
            sys.exit('simtest: %s isn\'t built (run make)' % run_file(t))

    work = a.keep or tempfile.mkdtemp(prefix='simtest')
    os.makedirs(work, exist_ok=True)
    with gzip.open(os.path.join(HERE, 'test', 'blank.img.gz')) as f:
        blank = f.read()
    for t in tests:
        img = os.path.join(work, t + '.img')
        with open(img, 'wb') as f:
            f.write(blank)
        for p in [t] + TESTS[t].get('needs', []):
            ctostool([img, 'add', 'C', p + '.run', run_file(p)])
        for name, data in TESTS[t].get('files', {}).items():
            if isinstance(data, str):
                path = os.path.join(HERE, data)
            else:
                path = os.path.join(work, name)
                with open(path, 'wb') as f:
                    f.write(data)
            ctostool([img, 'add', 'C', name, path])

    runs = []
    for m in machines:
        name, opts, _, _ = MACHINES[m]
        exe = sim_exe(simdir, name)
        for t in tests:
            spec = TESTS[t]
            floppy = os.path.join(work, '%s-%s.img' % (m, t))
            after = os.path.join(work, '%s-%s-after.img' % (m, t))
            shutil.copy(os.path.join(work, t + '.img'), floppy)
            script, go, stop = typing(m, t, spec)
            args = [exe, '-floppy', floppy, '-flsave', after, '-type', script] + opts
            if a.shot is not None:
                stop = go + a.shot
                args += ['-png', os.path.join(work, '%s-%s.png' % (m, t))]
            args += ['-seconds', str(stop)]
            if spec.get('serialin'):
                args += ['-serial', '-serialin',
                         ';'.join('%d:%s:%s' % (go + dt, port, text) for dt, port, text in spec['serialin'])]
            if m == 'aws' and spec.get('awskeys'):
                args += ['-keys', ','.join('%d:%x' % (go + dt, k) for dt, k in spec['awskeys'])]
            logname = os.path.join(work, '%s-%s.log' % (m, t))
            log = open(logname, 'wb')
            runs.append((m, t, after, logname, log, subprocess.Popen(args, cwd=simdir, stdout=log,
                                                                     stderr=subprocess.STDOUT)))

    failed = 0
    for m, t, after, logname, log, p in runs:
        p.wait()
        log.close()
        text = open(logname, encoding='latin1').read()
        problems, screen = check(m, t, TESTS[t], text, after)
        print('%-5s %-11s %s' % (m, t, 'FAILED' if problems else 'ok'))
        if problems:
            failed += 1
            for pr in problems:
                print('      ' + pr)
            print('      screen at the end:')
            for line in screen:
                if line:
                    print('      | ' + line)
    if not a.keep:
        shutil.rmtree(work, ignore_errors=True)
    else:
        print('floppies and logs in %s' % work)
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
