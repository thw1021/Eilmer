#!/usr/bin/env python3
# probes/geom/misc/gen_d_copies.py
#
# Regenerate the three D measurement copies this stage has to build because the
# pinned compiler cannot build the originals:
#
#   ldc2 1.40.0 (the compiler the frozen reference reports in its own banner) will not
#   instantiate nm/brent.d's `solve`, because solve is declared @nogc and throws
#   `new Exception` at brent.d:51 and brent.d:123:
#
#     nm/brent.d(51): Error: cannot use `new` in `@nogc` function
#                     `geom.misc.univariatefunctions.VinokurFunction.this.solve!(zeroFun,double).solve`
#
#   The failure is triggered by any module that calls brent.solve, so
#   geom/misc/univariatefunctions.d (whose two Vinokur classes call it from their
#   constructors) and geom/misc/univariatefunctions_demo.d cannot be compiled or run
#   at all — see reports/backlog.md. Wave 1 recorded the identical blocker for
#   nm.brent's own unittest (src/nm/brent.hpp:16-20).
#
#   Therefore the D-side baseline for this module is captured from mechanical copies:
#     brent_copy.d               = nm/brent.d, minus one `@nogc` attribute line.
#     univariatefunctions_copy.d = geom/misc/univariatefunctions.d, with the module
#                                  name changed, `import nm.brent` -> `import
#                                  brent_copy` (2 call sites), and 3 statements added
#                                  to count the loops D does not expose.
#     univariatefunctions_demo_copy.d = the demo, import line only.
#   No expression, operand order, tolerance, branch or message text is touched. Run
#   with --check to print the diffs and verify the committed copies are current.
#
# Usage:
#   python3 probes/geom/misc/gen_d_copies.py          # regenerate
#   python3 probes/geom/misc/gen_d_copies.py --check  # diff only, exit 1 on drift
import difflib
import os
import sys

GDTK = os.environ.get("GDTK_SRC", "/home/tang/packages/gdtk/src")
HERE = os.path.dirname(os.path.abspath(__file__))


def read(p):
    with open(p) as f:
        return f.read()


def write_local(name, text):
    with open(os.path.join(HERE, name), "w") as f:
        f.write(text)


def report(name, orig, copy):
    diff = [l for l in difflib.unified_diff(orig.splitlines(), copy.splitlines(),
                                            lineterm="", n=0)
            if l[:1] in "+-" and not l.startswith(("+++", "---"))]
    print("%s: %d changed/added lines" % (name, len(diff)))
    for l in diff:
        print("    " + l)
    return diff


def make_brent():
    b = read(os.path.join(GDTK, "nm/brent.d"))
    b = b.replace("module nm.brent;", "// brent_copy.d — copy of nm/brent.d; the ONLY change is that `@nogc` is dropped\n"
                                      "// from solve (see gen_d_copies.py). No arithmetic or message text differs.\n"
                                      "module brent_copy;")
    assert "@nogc\nT solve(alias f, T)(" in b, "nm/brent.d no longer has the expected @nogc on solve"
    b = b.replace("@nogc\nT solve(alias f, T)(", "T solve(alias f, T)(")
    return b


def make_uni():
    s = read(os.path.join(GDTK, "geom/misc/univariatefunctions.d"))
    s = s.replace(
        "module geom.misc.univariatefunctions;",
        "// univariatefunctions_copy.d — mechanical copy of geom/misc/univariatefunctions.d.\n"
        "// Changes: module name, import nm.brent -> import brent_copy, and 3 iteration\n"
        "// counters. See gen_d_copies.py for the full diff.\n"
        "module univariatefunctions_copy;\n\n"
        "import brent_copy;")
    s = s.replace("import nm.brent;\n", "")
    s = s.replace("nm.brent.solve!", "brent_copy.solve!")
    for marker, stmt in (("        foreach(n; 0 .. 100){\n", "            lam_iters = n + 1;\n"),
                         ("        foreach(n; 0 .. 100) {\n", "            gauss_iters = n + 1;\n"),
                         ("            iterations += 1;\n", "            xsw_iters = iterations;\n")):
        k = s.index(marker) + len(marker)
        s = s[:k] + stmt + s[k:]
    marker = "import std.format;\n"
    k = s.index(marker) + len(marker)
    s = s[:k] + ("\n// Iteration counters added by this copy (the original exposes none).\n"
                 "int lam_iters = 0;\nint gauss_iters = 0;\nint xsw_iters = 0;\n") + s[k:]
    return s


def make_demo():
    d = read(os.path.join(GDTK, "geom/misc/univariatefunctions_demo.d"))
    d = d.replace("import geom.misc.univariatefunctions;", "import univariatefunctions_copy;")
    return d


def main():
    check = "--check" in sys.argv
    specs = [("brent_copy.d", make_brent, "nm/brent.d"),
             ("univariatefunctions_copy.d", make_uni, "geom/misc/univariatefunctions.d"),
             ("univariatefunctions_demo_copy.d", make_demo, "geom/misc/univariatefunctions_demo.d")]
    drift = 0
    for name, fn, srcname in specs:
        copy = fn()
        orig = read(os.path.join(GDTK, srcname))
        report(name, orig, copy)
        committed_path = os.path.join(HERE, name)
        if check:
            if not os.path.exists(committed_path) or read(committed_path) != copy:
                print("DRIFT in %s — regenerate with gen_d_copies.py" % name)
                drift = 1
        else:
            write_local(name, copy)
    sys.exit(drift)


main()
