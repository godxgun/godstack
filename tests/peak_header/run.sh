#!/bin/sh
set -eu

header=${1:-Peak/peak.h}
if [ "$(uname -s)" != Linux ]; then
    echo "skip Peak single-header test (Linux only)"
    exit 0
fi

header_path=$(CDPATH= cd -- "$(dirname -- "$header")" && pwd)/$(basename -- "$header")

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/peak-header.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
cp "$header_path" "$tmp_dir/peak.h"
cp tests/peak_header/*.c "$tmp_dir/"
cd "$tmp_dir"

"${CC:-cc}" -std=c99 -Wall -Werror -Wno-deprecated-declarations -DPEAK_NO_AUDIO \
    main.c declarations_a.c declarations_b.c implementation.c \
    -ldl -lm -o peak_header_test
./peak_header_test
