#!/usr/bin/env bash
# compare_artefacts.sh <manifest> <artefact-dir>
#
# Recompute every artefact a demo wrote and require byte equality with the
# checksum manifest captured from the D reference. The mirror of harness/compare_text.sh
# for the FILE half of a demo's behaviour: compare_text.sh gates stdout, this gates the
# files, and a demo like sgrid_demo writes 3.4 MB of .vtk that reports/phase-0.md's rule
# versions as a manifest rather than as bytes.
#
# Manifest format (baselines/e587b235/probes/*.artefacts.txt, header comment lines
# starting with '#'):
#   kind <TAB> relative-path <TAB> bytes[ payload_bytes=N] <TAB> sha256
# kind is `bytes` or `payload`.
#   * bytes   -> the container itself is compared: size in column 3, sha256 in column 4.
#   * payload -> the DECOMPRESSED payload is compared against the `payload_bytes` value
#     and the sha256 in column 4. Column 3's leading number (the container size) is
#     informational and deliberately NOT compared: gzip stores the entry mtime in the
#     container, so two runs of the D binary itself differ in container bytes while the
#     payload digests are equal — measured, and recorded in the manifest headers
#     (geom_grid_sgrid_demo.artefacts.txt:16-17, geom_grid_usgrid_demo.artefacts.txt:16-17)
#     and in reports/wave-02-grid-brief.md section 9.3 item 6.
#
# Any mismatch (missing file, wrong size, wrong digest) is reported and the exit status
# is 1. The artefact directory must already contain the files a previous demo run wrote;
# this script runs nothing, so the CTest artefact case depends on the stdout case of the
# same demo as a fixture.
set -u
manifest="$1"
artefact_dir="$2"

if [[ ! -r "$manifest" ]]; then echo "compare_artefacts: cannot read manifest '$manifest'"; exit 2; fi
if [[ ! -d "$artefact_dir" ]]; then echo "compare_artefacts: no such directory '$artefact_dir'"; exit 2; fi

checked=0
bad=0
# The trailing line of a manifest has no final newline on some captures, so `read` must
# still process it.
while IFS=$'\t' read -r kind relpath col3 want_sha || [[ -n "${kind:-}" ]]; do
    [[ -z "$kind" || "$kind" == \#* ]] && continue
    want_bytes="${col3%% *}"
    payload_bytes=""
    if [[ "$col3" == *"payload_bytes="* ]]; then
        payload_bytes="${col3#*payload_bytes=}"
        payload_bytes="${payload_bytes%%[!0-9]*}"
    fi
    path="$artefact_dir/$relpath"
    if [[ ! -f "$path" ]]; then
        echo "MISSING $relpath (expected $want_bytes bytes, sha256 $want_sha)"
        bad=$((bad + 1)); checked=$((checked + 1)); continue
    fi
    if [[ "$kind" == "payload" ]]; then
        tmp="$(mktemp)"
        gunzip -c "$path" > "$tmp" || { echo "GUNZIP FAILED $relpath"; rm -f "$tmp"; bad=$((bad+1)); checked=$((checked+1)); continue; }
        got_bytes="$(wc -c < "$tmp" | tr -d ' ')"
        got_sha="$(sha256sum "$tmp" | cut -d' ' -f1)"
        want_bytes="${payload_bytes:-$want_bytes}"
        rm -f "$tmp"
    else
        got_bytes="$(wc -c < "$path" | tr -d ' ')"
        got_sha="$(sha256sum "$path" | cut -d' ' -f1)"
    fi
    checked=$((checked + 1))
    if [[ "$got_sha" != "$want_sha" || "$got_bytes" != "$want_bytes" ]]; then
        echo "MISMATCH $relpath"
        echo "  D reference: bytes=$want_bytes sha256=$want_sha"
        echo "  port:        bytes=$got_bytes sha256=$got_sha"
        bad=$((bad + 1))
    fi
done < "$manifest"

if [[ $bad -ne 0 ]]; then
    echo "$bad of $checked artefacts differ from the D reference: $(basename "$manifest")"
    exit 1
fi
echo "identical to the D reference: $checked artefacts ($(basename "$manifest"))"
exit 0
