#!/usr/bin/env python3
"""simtest.py - run the example programs on the AWS and NGEN simulators and
check what they do.

usage: simtest.py [-m aws|ngen] [-k DIR] [test ...]

It needs:
  - the simulator, https://github.com/sbelectronics/aws-simulator, built, in
    the directory SIMDIR names, with its disk images (aws.emu, ngen.emu);
  - ctostool, https://github.com/sbelectronics/ctostool, in the directory
    CTOSTOOL names, and the Python 2 that runs it in PYTHON2 (default python2);
  - the programs, built by make.

The programs go on a floppy made from test/blank.img.gz, in its directory
<C>. Every test runs in its own simulator, all at once, and is started with
the Executive's Run command, typed in. On the AWS, any 80186 instruction is a
failure (awssim -warn186).
"""
import argparse, gzip, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(HERE, 'build')

# Each test: the arguments for Run's [Param n] fields; what to type once the
# program is running (seconds after GO, text); raw AWS key codes to press
# (seconds after GO, hex); how long to let it run; and regular expressions
# the output must match, anywhere ('out') or as a whole screen line at some
# point ('screen', and 'screen_aws' on the AWS only). The final screen isn't
# enough: when a program ends, the Executive scrolls its top lines away.
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
    'screentest': dict(type=[(20, 'ab'), (30, '\\n')], awskeys=[(24, 0x01), (25, 0x0B), (26, 0x0E), (27, 0x0F)],
                       run=45,
                       screen=[r'^.{30}screen test', r'^rows \d+', r'^.{75}clipp$', r'^poll -1',
                               r'^delay [234]$',
                               r'^date \w{3} \w{3} [ \d]\d \d\d:\d\d:\d\d (1983|1987)$',
                               r'^printf on line 8$', r'^bottom line$'],
                       screen_aws=[r'^keys: 61 62 01 0B 0E 12$'],
                       screen_ngen=[r'^keys: 61 62$']),
}

MACHINES = {
    # sim name, extra options, the typing that signs on, when the Executive is
    # ready. The floppy goes in after the hard disk has booted: with it in the
    # drive at power-on, the boot ROM would try to boot from it.
    'aws': ('awssim', ['-warn186', '-insert', '35'], '30:\\g', 40),
    'ngen': ('ngensim', ['-insert', '55'], '30:\\n;31:\\n;32:Thu Nov 5, 1987 1:00 pm;50:\\g', 62),
}


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
    for dt, text in spec.get('type', []):
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
    if 'keep' in spec:
        try:
            got = ctostool([floppy, 'extract', 'C', 'keep.txt'])
            if got != spec['keep']:
                problems.append('keep.txt holds %r' % got[:80])
        except subprocess.CalledProcessError:
            problems.append('no keep.txt on the floppy')
        if b'0 errors' not in ctostool([floppy, 'chkdsk']):
            problems.append('the floppy fails ctostool chkdsk')
    return problems, screen


def main():
    ap = argparse.ArgumentParser(description='Run the example programs on the simulators.')
    ap.add_argument('-m', '--machine', choices=sorted(MACHINES), action='append',
                    help='only this machine (default both)')
    ap.add_argument('-k', '--keep', help='keep the floppies and logs in this directory')
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
        if not os.path.exists(os.path.join(BUILD, t + '.run')):
            sys.exit('simtest: %s.run isn\'t built (run make)' % t)

    work = a.keep or tempfile.mkdtemp(prefix='simtest')
    os.makedirs(work, exist_ok=True)
    blank = os.path.join(work, 'c.img')
    with gzip.open(os.path.join(HERE, 'test', 'blank.img.gz')) as f, open(blank, 'wb') as g:
        g.write(f.read())
    for t in tests:
        ctostool([blank, 'add', 'C', t + '.run', os.path.join(BUILD, t + '.run')])

    runs = []
    for m in machines:
        name, opts, _, _ = MACHINES[m]
        exe = sim_exe(simdir, name)
        for t in tests:
            spec = TESTS[t]
            floppy = os.path.join(work, '%s-%s.img' % (m, t))
            after = os.path.join(work, '%s-%s-after.img' % (m, t))
            shutil.copy(blank, floppy)
            script, go, stop = typing(m, t, spec)
            args = [exe, '-floppy', floppy, '-flsave', after, '-seconds', str(stop),
                    '-type', script] + opts
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
