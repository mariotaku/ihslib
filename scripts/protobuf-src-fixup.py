#!/usr/bin/env python3

import os
import re
from typing import List

import sys


def process_gen_src(p: str):
    lines: List[str] = []
    with open(p) as fr:
        for line in fr.readlines():
            for match in re.findall(r'[ "]([\w_]+__)k_', line):
                line = line.replace(match, '')
            lines.append(line)
    with open(p, 'w') as fw:
        fw.writelines(lines)


for d in sys.argv[1:]:
    for fn in os.listdir(d):
        name, ext = os.path.splitext(fn)
        if ext in ['.c', '.h']:
            process_gen_src(os.path.join(d, fn))
