#!/usr/bin/env bash
# gate.sh <cpp-bin-dir> [case ...] — run the three acceptance gates on a build.
#
#   gate 1/2: artifact and curve comparison vs the captured D baseline
#             (harness/compare.py: restart files element-wise, residuals, loads)
#   gate 3:   the case's own Ruby judgement, against the C++ binaries
#
# The C++ bin dir must contain an `e4shared` and the same Lua module tree the
# reference uses; the modules come from the reference install, so the Lua
# contract is a fixed input rather than something this port gets to redefine.
#
# Requires a bin dir with: e4shared (+ prep-gas etc. fall back to the reference).
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(dirname "$here")"
GDTK_REF="${GDTK_REF:-/home/tang/packages/gdtk}"
REV="${REV:-e587b235}"
bindir="${1:?usage: gate.sh <cpp-bin-dir> [case-name ...]}"
shift
bindir="$(cd "$bindir" && pwd)"

cases=("$@")
if [ ${#cases[@]} -eq 0 ]; then
    mapfile -t cases < <(ls "$repo/baselines/$REV" 2>/dev/null | grep -v INVALID)
fi
if [ ${#cases[@]} -eq 0 ]; then
    echo "no baselines under baselines/$REV; capture them first" >&2
    exit 2
fi

work="$(mktemp -d)"
report="$work/gate.md"
echo "# Gate report against D baseline $REV" > "$report"
echo "" >> "$report"
echo "candidate binaries: $bindir" >> "$report"
echo "" >> "$report"
fails=0
for c in "${cases[@]}"; do
    base="$repo/baselines/$REV/$c"
    [ -d "$base" ] || { echo "- **SKIP** $c (no baseline)"; continue; }
    if [ -f "$base/INVALID" ]; then
        echo "- **SKIP** $c (baseline invalid: $base/INVALID)" | tee -a "$report"
        continue
    fi
    # case dir = baseline name with dashes restored is not reversible, so the
    # mapping is recorded alongside the capture.
    casedir="$(cat "$base/case-dir" 2>/dev/null || true)"
    judgement="$(cat "$base/judgement" 2>/dev/null || true)"
    if [ -z "$casedir" ]; then
        echo "- **SKIP** $c (no case-dir recorded)" | tee -a "$report"
        continue
    fi
    out="$work/$c"
    "$repo/harness/run_case.sh" "$casedir" "$judgement" "$bindir" "$out" >/dev/null 2>&1
    if [ -f "$out/INVALID" ]; then
        echo "- **FAIL** $c: candidate install missing a binary the case uses" | tee -a "$report"
        fails=$((fails + 1))
        continue
    fi
    line="$(python3 "$repo/harness/compare.py" "$base" "$out" --report "$out/diff.md" 2>&1 | tail -1)"
    rc=$?
    if [ $rc -eq 0 ]; then
        echo "- **PASS** $c: $line" | tee -a "$report"
    else
        echo "- **FAIL** (rc=$rc) $c" | tee -a "$report"
        sed -n '1,12p' "$out/diff.md" >> "$report" 2>/dev/null
        fails=$((fails + 1))
    fi
done
echo "" >> "$report"
echo "failing cases: $fails" >> "$report"
echo
echo "report: $report"
[ "$fails" -eq 0 ] || exit 1
