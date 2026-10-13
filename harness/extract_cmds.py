#!/usr/bin/env python3
"""extract_cmds.py <case-test.rb> — the command sequence a judgement actually runs.

The Ruby cases invoke `prep-gas` and `e4shared` by name, so replaying the same
list against a different bin directory gives an apples-to-apples D vs C++ run
without duplicating 70 command lists here. Order is preserved (some cases sed
their own script between runs), and each command is emitted on its own line.
"""
import ast
import re
import sys

PAT = re.compile(r'^\s*cmd\s*=\s*(.+?)\s*$')


def main():
    with open(sys.argv[1], encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = PAT.match(line)
            if not m:
                continue
            expr = m.group(1)
            try:
                value = ast.literal_eval(expr)
            except (ValueError, SyntaxError):
                # Interpolated or multi-part strings: not replayable verbatim.
                print(f"# SKIPPED (not a literal): {expr}", file=sys.stderr)
                continue
            if isinstance(value, str):
                print(value)


if __name__ == "__main__":
    main()
