#!/bin/sh
set -eu

header=${1:-Peak.h}
if [ "$(uname -s)" != Linux ]; then
    echo "skip Peak single-header workload (Linux only)"
    exit 0
fi

header_path=$(CDPATH= cd -- "$(dirname -- "$header")" && pwd)/$(basename -- "$header")
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/peak-header.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
cp "$header_path" "$tmp_dir/Peak.h"
cp "$source_dir"/*.c "$tmp_dir/"
cp "$source_dir/../demo.c" "$tmp_dir/workload.c"
cd "$tmp_dir"

"${CC:-cc}" -std=c99 -Wall -Werror -Wno-deprecated-declarations -DPEAK_NO_AUDIO \
    main.c declarations_a.c declarations_b.c implementation.c \
    -ldl -lm -o peak_header_demo
./peak_header_demo

# Exercise the current context, caller storage and memory-domain APIs with only
# the copied header. No display or GPU is opened, even with Vulkan WSI enabled.
"${CC:-cc}" -std=c99 -Wall -Werror -Wno-deprecated-declarations \
    workload.c -ldl -lvulkan -lm -o peak_workload
./peak_workload
