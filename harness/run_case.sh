#!/usr/bin/env bash
# run_case.sh <case-dir-under-examples> <judgement.rb> <bin-dir> <out-dir>
#
# Copies one example case to scratch, replays the exact command sequence its
# Ruby judgement uses, and collects every produced artifact plus the combined
# stdout into <out-dir>. Same Lua module tree on both sides by construction: the
# modules come from the reference install, only the binaries change.
set -uo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$here")"
GDTK_REF="${GDTK_REF:-/home/tang/packages/gdtk}"

case_dir="$1"; judgement="$2"; bindir="$3"; outdir="$4"
# Absolutize before any cd: the scratch directory below is elsewhere.
mkdir -p "$outdir"
outdir="$(cd "$outdir" && pwd)"
bindir="$(cd "$bindir" && pwd)"

scratch="$(mktemp -d)"
cp -r "$GDTK_REF/examples/eilmer/$case_dir/." "$scratch/"

export DGD="$GDTK_REF/build"
export PATH="$bindir:$DGD/bin:$PATH"
export DGD_LUA_PATH="$DGD/lib/?.lua"
export DGD_LUA_CPATH="$DGD/lib/?.so"
export LD_LIBRARY_PATH="$DGD/lib:${LD_LIBRARY_PATH:-}"

cd "$scratch"
log="$outdir/run.log"
: > "$log"
while IFS= read -r cmd; do
    [ -z "$cmd" ] && continue
    echo "+ $cmd" >> "$log"
    # Each command's stdout and stderr go to the log: the gate compares text, and
    # solver diagnostics arrive on both streams.
    timeout "${CASE_TIMEOUT:-1800}" bash -c "$cmd" >> "$log" 2>&1
    echo "rc=$? for: $cmd" >> "$log"
done < <(python3 "$here/extract_cmds.py" "$scratch/$(basename "$judgement")")

# Record which example directory and judgement this capture came from, so gate.sh
# can replay any baseline without a name-reversible mapping.
echo "$case_dir" > "$outdir/case-dir"
echo "$(basename "$judgement")" > "$outdir/judgement"

# A baseline captured with a missing binary is worse than no baseline: it looks
# complete and silently compares nothing. rc=127 means the case invoked a solver
# variant the reference install does not have (e4-nk-shared, e4-cht-shared, e4mpi).
if grep -q 'rc=127 for:' "$log"; then
    echo "INVALID: reference install is missing a binary this case uses:" >&2
    grep 'rc=127 for:' "$log" | sed 's/^/  /' >&2
    echo "INVALID: see run.log" > "$outdir/INVALID"
    grep 'rc=127 for:' "$log" >> "$outdir/INVALID"
    rm -rf "$scratch"
    exit 3
fi

# Collect artifacts: the whole scratch tree. The solver writes into config/, grid/,
# flow/, hist/ and loads/ subdirectories, so a top-level file scan would silently
# drop the restart files that gate (1) is about.
cp -a "$scratch" "$outdir/tree"
rm -rf "$scratch"
echo "captured $(ls "$outdir" | wc -l) files into $outdir"
