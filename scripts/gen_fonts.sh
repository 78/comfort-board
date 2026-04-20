#!/usr/bin/env bash
#
# Regenerate the LVGL fonts used by the comfort-board UI.
#
# Prereq: install lv_font_conv (v1.5+).
#   npm i -g lv_font_conv
#   or via pnpm: pnpm add -g lv_font_conv
#
# Fonts are rendered at bpp=1 (no anti-aliasing) because the 4-color e-paper
# panel renders 1-bit glyphs cleanly without intermediate shades.
#
# Edit DIGITS_TTF / LABEL_TTF below if swapping fonts. Same TTF can be used
# for both sizes.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO_ROOT/main/gui/fonts"
mkdir -p "$OUT"

DIGITS_TTF="$REPO_ROOT/assets/FuturaHandwritten.ttf"
LABEL_TTF="$REPO_ROOT/assets/Nasalization Rg.otf"

# Size knobs kept in one place so it's easy to try different scales.
DIGITS_SIZE=72
LABEL_SIZE=21

lv_font_conv --bpp 1 --size "$DIGITS_SIZE" \
  --font "$DIGITS_TTF" \
  --symbols "0123456789.-°%" \
  --format lvgl --no-compress \
  --lv-include "lvgl.h" \
  -o "$OUT/font_digits.c"

lv_font_conv --bpp 1 --size "$LABEL_SIZE" \
  --font "$LABEL_TTF" \
  --range 0x20-0x7E --symbols "°" \
  --format lvgl --no-compress \
  --lv-include "lvgl.h" \
  -o "$OUT/font_label.c"

echo "Fonts regenerated in $OUT"
