#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NPEG_VIDEO="${1:-/mnt/data/video.npeg}"
NPEG_PREP="${2:-/mnt/data/prep2.npeg}"
for f in "$NPEG_VIDEO" "$NPEG_PREP"; do
  if [ -f "$f" ]; then
    "$ROOT/npeg-convert" "$f" --info
    out="$(mktemp /tmp/npeg-XXXXXX.ppm)"
    "$ROOT/npeg-convert" "$f" --frame 0 --ppm "$out"
    test "$(head -c 2 "$out")" = P6
    rm "$out"
  else
    echo "Skip absent fixture: $f"
  fi
done
