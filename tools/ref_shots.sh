#!/usr/bin/env bash
# Capture reference screenshots from fixed Paldiski viewpoints for visual regression.
# Requires a built client: cmake -B build && cmake --build build
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$ROOT/build/game"
OUT="${1:-$ROOT/ref_shots}"
mkdir -p "$OUT"

PRESETS=(shore bog forest ridge golden)
for name in "${PRESETS[@]}"; do
  echo "capturing $name -> $OUT/ref_${name}.ppm"
  FPS_MAP=paldiski FPS_REF="$name" FPS_SHOT="$OUT/ref_${name}.ppm" "$GAME"
done

echo "done — ${#PRESETS[@]} frames in $OUT"
