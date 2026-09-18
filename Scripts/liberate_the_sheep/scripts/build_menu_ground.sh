#!/bin/zsh

set -euo pipefail

if [[ $# -ne 2 ]]; then
    print -u2 "usage: $0 <source.png> <output.png>"
    exit 64
fi

readonly source_path="$1"
readonly output_path="$2"

if [[ ! -f "$source_path" ]]; then
    print -u2 "error: source image does not exist: $source_path"
    exit 66
fi

# The generated source is 2128x739, but the illustrated grass and soil occupy
# rows 279...454. Keep the complete horizontal artwork while removing the large
# transparent bands that made the runtime Sprite look like an empty rectangle.
/usr/bin/sips \
    --cropToHeightWidth 176 2128 \
    --cropOffset 279 0 \
    "$source_path" \
    --out "$output_path" \
    >/dev/null

readonly dimensions="$(/usr/bin/sips \
    --getProperty pixelWidth \
    --getProperty pixelHeight \
    --oneLine \
    "$output_path")"

if [[ "$dimensions" != *"pixelWidth: 2128"* || "$dimensions" != *"pixelHeight: 176"* ]]; then
    print -u2 "error: unexpected output dimensions: $dimensions"
    exit 65
fi

print "wrote menu ground 2128x176: $output_path"
