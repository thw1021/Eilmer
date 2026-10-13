#!/usr/bin/env bash
# compare_text.sh <expected.txt> <program...>
# Runs the program, normalizes its stdout, and requires byte equality with the
# captured D reference. Used by the demo-equivalence CTest cases.
set -u
expected="$1"; shift
here="$(cd "$(dirname "$0")" && pwd)"
actual="$(mktemp)"
"$@" > "$actual" || { echo "program failed: $*"; rm -f "$actual"; exit 2; }
if python3 "$here/normalize_log.py" "$expected" > "$expected.norm" 2>/dev/null; then
    python3 "$here/normalize_log.py" "$actual" > "$actual.norm"
    if diff -u "$expected.norm" "$actual.norm" > "$here/.last_diff" 2>&1; then
        echo "identical after normalization: $(basename "$expected")"
        rm -f "$actual" "$actual.norm"; exit 0
    fi
    echo "DIFFERS from D reference: $(basename "$expected")"
    head -40 "$here/.last_diff"
    rm -f "$actual" "$actual.norm"; exit 1
fi
# Normalizer unavailable: fall back to raw comparison.
if cmp -s "$expected" "$actual"; then echo "identical: $(basename "$expected")"; rm -f "$actual"; exit 0; fi
echo "DIFFERS (raw): $(basename "$expected")"; rm -f "$actual"; exit 1
