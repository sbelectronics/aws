#!/usr/bin/env python3
"""mksub.py - write the Submit file that installs the tools as commands.

usage: mksub.py tool.c... >Install.sub

Each tool's source has a FORM string: what the tool does, then its command
form's fields, separated by |, each perhaps ending in its role (see
utils/util.h). For each tool, the Submit file runs mkcmd.run, from the
current path, which copies the tool to [Sys]<Tools> and makes its command.

A Submit file is what would be typed: a command's name, RETURN to get its
form, RETURN between fields, and GO (ESC, 1Bh) to run it.
"""
import os, re, sys

RETURN, GO = '\n', '\x1b'

# The command is the tool's name, except where CTOS has a command by that
# name already: the NGEN has a Sort.
NAMES = {'sort': 'Sort Lines'}


def form_of(path):
    src = open(path, encoding='latin1').read()
    m = re.search(r'#define FORM ((?:"(?:[^"\\]|\\.)*"\s*\\?\s*)+)', src)
    if not m:
        sys.exit('mksub: no FORM in %s' % path)
    return ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))


def field_name(field):
    words = field.split(' ')
    if len(words) > 1 and words[-1][:1] in ('-', '>'):
        words = words[:-1]
    return ' '.join(words)


def command(name, *fields):
    return name + RETURN + RETURN.join(fields) + GO


out = []
for path in sys.argv[1:]:
    tool = os.path.splitext(os.path.basename(path))[0]
    form = form_of(path).split('|')
    name = NAMES.get(tool, tool.capitalize())
    # Run's fields: the run file, [Case], [Command], then [Param 1] and on,
    # each short enough to fit on its line
    out.append(command('Run', 'mkcmd.run', '', '', "'%s'" % name, tool, "'%s'" % form[0],
                       *["'%s'" % field_name(f) for f in form[1:]]))
sys.stdout.buffer.write(''.join(out).encode('latin1'))
