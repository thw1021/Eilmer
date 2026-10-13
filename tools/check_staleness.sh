#!/usr/bin/env bash
# check_staleness.sh — report D files whose content changed after their port.
#
# gdtk is read-only for porting work but it is a live checkout, so the reference
# can move under the port. Every manifest record stores the D blob hash; this
# re-hashes and lists drift, which is how a stale translation gets found before
# it turns into an unexplained residual difference.
set -uo pipefail
GDTK="${1:-/home/tang/packages/gdtk}"
here="$(cd "$(dirname "$0")/.." && pwd)"
manifest="$here/provenance/manifest.yml"
[ -f "$manifest" ] || { echo "no manifest at $manifest; run tools/gen_manifest.py" >&2; exit 2; }
drift=0
current=""
while IFS= read -r line; do
    case "$line" in
        -\ cpp:*) current="${line#- cpp: }" ;;
        \ \ d_blob:*)
            stored="${line#  d_blob: }"
            [ "$stored" = "UNTRACKED" ] && continue
            dpath="${line#  d_blob: }"
            ;;
        \ \ d:*)
            d="${line#  d: }"
            [ "${d:0:1}" = "#" -o "$d" = "null" ] && continue
            found_d="$d"
            ;;
    esac
    if [ -n "${found_d:-}" ] && [ -n "${current:-}" ]; then
        actual="$(git -C "$GDTK" hash-object -- "$found_d" 2>/dev/null)"
        stored="$(awk -v f="$current" '
            $0 == "- cpp: " f {grab=1; next}
            grab && /^  d_blob:/ {split($0,a,": "); print a[2]; exit}
            grab && /^- cpp:/ {exit}' "$manifest")"
        if [ -n "$stored" ] && [ -n "$actual" ] && [ "$stored" != "$actual" ]; then
            echo "DRIFT: $current  <- $found_d changed in gdtk"
            drift=$((drift + 1))
        fi
        found_d=""
    fi
done < "$manifest"
if [ "$drift" -eq 0 ]; then echo "no drift: every ported file matches its recorded D blob"; fi
exit $((drift > 0))
