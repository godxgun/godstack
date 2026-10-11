#!/bin/sh
set -eu

if [ "$(uname -s)" != Linux ]; then
    echo "skip: Peak safe-I/O implementation is Linux-only" >&2
    exit 0
fi
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/peak-safe-io.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L \
    -I"$repo" "$repo/demos/peak-safe-io/main.c" \
    -pthread -ldl -lm -o "$tmp/peak-safe-io"
"$tmp/peak-safe-io"
