#!/usr/bin/env bash
# =========================================================================
#  MESQUITE PHASE 2 - per-node pod build  (addresses NODE-05, §4.2 BLOCKER)
#
#  Generates one firmware image per bone id from a single source, with the
#  id injected as a build flag. Replaces the Phase 1 workflow of editing a
#  comment block 17 times, which is how duplicate sendIDs get created --
#  a failure that is SILENT at runtime.
#
#  Usage:
#     tools/build_pods.sh                 # build all 17
#     tools/build_pods.sh 3 4 5           # build only these ids
#     FQBN=... tools/build_pods.sh        # override board
#
#  Requires arduino-cli on PATH. Outputs to build/pods/pod_<id>_<NAME>/
# =========================================================================
set -euo pipefail
cd "$(dirname "$0")/.."

SKETCH="Device code/Pod_Watch_Binary"
OUTROOT="build/pods"
FQBN="${FQBN:-esp32:esp32:twatch}"

NAMES=(Head Spine HipsAlt LeftArm LeftForeArm LeftHand RightArm RightForeArm \
       RightHand LeftUpLeg LeftLeg LeftFoot RightUpLeg RightLeg RightFoot \
       LeftShoulder RightShoulder)

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "ERROR: arduino-cli not found on PATH." >&2
  echo "  Install it, or build manually with:" >&2
  echo "    -DMESQ_POD_ID=<id>  as an extra build flag" >&2
  exit 127
fi

IDS=("$@")
if [ ${#IDS[@]} -eq 0 ]; then IDS=($(seq 0 16)); fi

mkdir -p "$OUTROOT"
MANIFEST="$OUTROOT/MANIFEST.txt"
: > "$MANIFEST"
echo "# Mesquite pod build manifest - $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$MANIFEST"
echo "# id  name           sha256(bin)" >> "$MANIFEST"

fail=0
for id in "${IDS[@]}"; do
  if [ "$id" -lt 0 ] || [ "$id" -gt 16 ]; then
    echo "SKIP invalid id $id" >&2; fail=1; continue
  fi
  name="${NAMES[$id]}"
  out="$OUTROOT/pod_${id}_${name}"
  echo "==> building id=$id ($name)"
  mkdir -p "$out"
  if arduino-cli compile \
        --fqbn "$FQBN" \
        --build-property "build.extra_flags=-DMESQ_POD_ID=${id}" \
        --output-dir "$out" \
        "$SKETCH" >"$out/build.log" 2>&1; then
    bin=$(ls "$out"/*.bin 2>/dev/null | head -1 || true)
    if [ -n "$bin" ]; then
      sum=$(shasum -a 256 "$bin" | awk '{print $1}')
      printf "%-4s %-14s %s\n" "$id" "$name" "$sum" >> "$MANIFEST"
      echo "    ok  $sum"
    else
      echo "    WARNING: no .bin produced" >&2; fail=1
    fi
  else
    echo "    BUILD FAILED - see $out/build.log" >&2
    tail -5 "$out/build.log" >&2
    fail=1
  fi
done

echo
echo "Manifest: $MANIFEST"
# Duplicate-binary check: two ids producing an identical image means the
# build flag did not take effect, which is exactly the NODE-05 failure.
dups=$(awk 'NF==3 && $1!="#"{print $3}' "$MANIFEST" | sort | uniq -d | wc -l | tr -d ' ')
if [ "$dups" != "0" ]; then
  echo "ERROR: $dups duplicate binaries in manifest -- MESQ_POD_ID did not take effect." >&2
  exit 1
fi
echo "No duplicate images. Every id produced a distinct binary."
exit $fail
