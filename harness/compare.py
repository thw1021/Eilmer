#!/usr/bin/env python3
"""compare.py — numeric and textual equivalence gate between D and C++ run output.

    compare.py <d_dir> <cpp_dir> [--rel 1e-13] [--report out.md]

Artifacts are compared by kind:

  * text-format solver files (flow/grid/config/hist/loads) are tokenized and
    compared number-by-number. Numbers are parsed, never diffed as text, because
    "1.0000000001e+03" and "1.0000000002e+03" are the same value to 1e-10 and
    two spellings of the same value are a formatting question, not a numeric one.
    The formatting question is still answered - the log comparison below catches
    any change in how a value is printed.
  * binary files are compared byte-wise, with the first differing offset reported.
  * *.log files are normalized (volatile fields placeholderized) and compared
    byte-for-byte, with the first divergent line reported.

Exit status: 0 if every artifact is within tolerance, 1 otherwise, 2 if an
artifact exists on one side and not the other (that is a functional difference,
not a numerical one).
"""
import argparse
import gzip
import os
import re
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import normalize_log  # noqa: E402

NUM_RE = re.compile(r"[-+]?(?:\d+\.?\d*(?:[eE][-+]?\d+)?|\.?\d+)", re.ASCII)
TEXT_EXT = ("", ".txt", ".log", ".dat")
BINARY_HINTS = (".bin",)


def tokens(path):
    """Split a solver text file into (float_or_string) tokens."""
    if path.endswith((".gz", ".zip")):
        data = read_compressed(path)
    else:
        with open(path, "rb") as fh:
            data = fh.read()
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return None
    out = []
    for m in re.finditer(r"[^\s]+", text):
        t = m.group(0)
        try:
            out.append(float(t))
        except ValueError:
            out.append(t)
    return out


def compare_numbers(dpath, cpath, rel):
    dt, ct = tokens(dpath), tokens(cpath)
    if dt is None or ct is None:
        return compare_bytes(dpath, cpath)
    if len(dt) != len(ct):
        return False, f"token count differs: D {len(dt)} vs C++ {len(ct)}"
    worst_rel, worst_abs, where = 0.0, 0.0, None
    for i, (a, b) in enumerate(zip(dt, ct)):
        if isinstance(a, str) or isinstance(b, str):
            if a != b:
                # D writes some config files by iterating an associative array,
                # whose order druntime reseeds per process. Verified against the
                # reference itself: config/cone20.fluidBlockArrays changes
                # between two runs of the same D binary while cone20.list does
                # not. So a token-order difference is only equivalent if the
                # multisets agree - and it must be labelled, not hidden.
                if sorted(map(str, dt)) == sorted(map(str, ct)):
                    return True, (f"order-insensitive match ({len(dt)} tokens as a set; "
                                  "D itself varies run to run here)")
                return False, f"token {i} differs textually: D '{a}' vs C++ '{b}'"
            continue
        diff = abs(a - b)
        scale = max(abs(a), abs(b), 1e-300)
        r = diff / scale
        if diff > worst_abs:
            worst_abs, where_abs = diff, i
        if r > worst_rel:
            worst_rel, where = r, i
    ok = worst_rel <= rel
    return ok, (f"max rel {worst_rel:.3e} (token {where}), max abs {worst_abs:.3e}"
                if dt else "no numeric tokens")


def read_compressed(path):
    """Payload of a gz or zip container.

    Both wrappers store the entry timestamp, so their bytes differ between two
    runs of the same binary even when the contained data is identical (measured:
    CellData .zip files differ at byte 10, the mtime field). The gate compares
    the payload, which is what the solver wrote.
    """
    if path.endswith(".gz"):
        with gzip.open(path, "rb") as fh:
            return fh.read()
    if path.endswith(".zip"):
        with zipfile.ZipFile(path) as zf:
            return b"".join(zf.read(n) for n in sorted(zf.namelist()))
    with open(path, "rb") as fh:
        return fh.read()


def compare_payload(dpath, cpath):
    da, db = read_compressed(dpath), read_compressed(cpath)
    if da == db:
        return True, f"{len(da)} payload bytes identical"
    for i, (x, y) in enumerate(zip(da, db)):
        if x != y:
            return False, f"payload first difference at byte {i}: D 0x{x:02x} vs C++ 0x{y:02x}"
    return False, f"payload length differs: D {len(da)} vs C++ {len(db)}"


def compare_bytes(dpath, cpath):
    with open(dpath, "rb") as a, open(cpath, "rb") as b:
        da, db = a.read(), b.read()
    if da == db:
        return True, f"{len(da)} bytes identical"
    for i, (x, y) in enumerate(zip(da, db)):
        if x != y:
            return False, f"first difference at byte {i}: D 0x{x:02x} vs C++ 0x{y:02x}"
    return False, f"length differs: D {len(da)} vs C++ {len(db)}"


def compare_logs(dpath, cpath):
    dn = normalize_log.normalize(open(dpath, encoding="utf-8", errors="replace").read())
    cn = normalize_log.normalize(open(cpath, encoding="utf-8", errors="replace").read())
    if dn == cn:
        return True, f"identical after normalization ({len(dn.splitlines())} lines)"
    dl, cl = dn.splitlines(), cn.splitlines()
    for i in range(min(len(dl), len(cl))):
        if dl[i] != cl[i]:
            return False, f"line {i + 1}:\n    D:  {dl[i]!r}\n    C++:{cl[i]!r}"
    return False, f"length differs after line {min(len(dl), len(cl))}"


# Files the harness itself writes into a capture directory. Comparing these would
# make every gate compare fail against a baseline captured by an older harness.
# SHA256SUMS.txt belongs here: it is the baseline's own integrity manifest, written
# after the capture, so a fresh candidate run never has one and the gate would
# otherwise report "MISSING IN C++" for a file the solver did not produce.
METADATA = {"case-dir", "judgement", "diff.md", "INVALID", "run.log", "SHA256SUMS.txt"}
# Python bytecode caches and mktemp scratch directories record when and where
# they were made; they are not solver output and never will be comparable.
IGNORE_DIRS = {"__pycache__"}
IGNORE_PREFIX = ("tmp.",)


def list_artifacts(d):
    found = set()
    for root, dirs, files in os.walk(d):
        dirs[:] = [x for x in dirs
                   if x not in IGNORE_DIRS and not x.startswith(IGNORE_PREFIX)]
        for f in files:
            rel = os.path.relpath(os.path.join(root, f), d)
            if os.path.basename(rel) in METADATA and "/" not in rel:
                continue
            found.add(rel)
    return sorted(found)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("d_dir")
    ap.add_argument("cpp_dir")
    ap.add_argument("--rel", type=float, default=1e-13)
    ap.add_argument("--report")
    args = ap.parse_args()

    d_files, c_files = set(list_artifacts(args.d_dir)), set(list_artifacts(args.cpp_dir))
    only_d, only_c = d_files - c_files, c_files - d_files
    rows, all_ok = [], True

    for rel_path in sorted(d_files & c_files):
        dp, cp = os.path.join(args.d_dir, rel_path), os.path.join(args.cpp_dir, rel_path)
        ext = os.path.splitext(rel_path)[1]
        if ext == ".log" or ext == ".lua":
            # .lua carries the prep-gas generation timestamp in a comment; the
            # code below is compared after normalization, same as the logs.
            ok, msg = compare_logs(dp, cp)
        elif ext in BINARY_HINTS or ext in (".gz", ".zip"):
            ok, msg = compare_bytes(dp, cp) if ext not in (".gz", ".zip") else compare_payload(dp, cp)
        else:
            ok, msg = compare_numbers(dp, cp, args.rel)
        all_ok = all_ok and ok
        rows.append((rel_path, ok, msg))

    lines = [f"# comparison: {args.d_dir} vs {args.cpp_dir}", ""]
    lines.append(f"tolerance: relative {args.rel:g}")
    lines.append("")
    for rel_path, ok, msg in rows:
        lines.append(f"- **{'PASS' if ok else 'FAIL'}** `{rel_path}`: {msg}")
    for f in sorted(only_d):
        lines.append(f"- **MISSING IN C++** `{f}`")
    for f in sorted(only_c):
        lines.append(f"- **EXTRA IN C++** `{f}`")
    report = "\n".join(lines) + "\n"
    if args.report:
        with open(args.report, "w") as fh:
            fh.write(report)
    print(report)
    if only_d or only_c:
        return 2
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
