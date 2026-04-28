#!/bin/sh
set -eu

dir="${1:-assets}"

fix_png8() {
  name="$1"
  size="$2"

  src="$dir/$name"
  out="$dir/$name.clean.png"

  echo "Fixing $src -> $size"

  magick "$src" \
    -resize "$size!" \
    -alpha off \
    -strip \
    -colors 256 \
    "PNG8:$out"

  mv "$out" "$src"
}

fix_png8 icon0.png 128x128
fix_png8 pic0.png 960x544
fix_png8 bg.png 840x500
fix_png8 startup.png 280x158

echo
identify "$dir/icon0.png" "$dir/pic0.png" "$dir/bg.png" "$dir/startup.png"