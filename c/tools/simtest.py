#!/usr/bin/env python3
"""simtest.py - run the example programs and hexedit on the AWS and NGEN
simulators and check what they do.

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
import argparse, gzip, os, re, shutil, subprocess, sys, tempfile

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
# ('needs'), and 'files'. A program that isn't an example names its run file
# with 'prog'.

# 70000 bytes, more than a segment, with NEEDLE at 10F00h
BIG = bytearray(i % 251 for i in range(70000))
BIG[0x10F00:0x10F06] = b'NEEDLE'
BIG_SAVED = b'Z' + BIG[:0x10F00] + b'A' + BIG[0x10F01:]

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
    """The -type script: sign on, Run with the run file and the parameters,
    then the test's input. Returns it, the time of GO, and when to stop."""
    _, _, signon, t = MACHINES[machine]
    parts = [signon, '%d:run\\n' % t]
    fields = ['[f0]<C>%s.run' % test, '', ''] + spec.get('params', [])
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
