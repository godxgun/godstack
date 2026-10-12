#!/bin/sh
# MIT License; see LICENSE.
# Offline C99 tests. All generated products/corpora stay in tests/.build.
set -eu
ulimit -c 0
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p tests/.build
out=$(mktemp -d "$root/tests/.build/run.XXXXXX")
cleanup() { rm -r "$out"; rmdir "$root/tests/.build" 2>/dev/null || :; }
trap cleanup EXIT HUP INT TERM
flags='-std=c99 -Wall -Wextra -Werror -ffp-contract=off -pipe'
for cc in gcc clang; do
 command -v "$cc" >/dev/null 2>&1 || { echo "$cc required" >&2; exit 1; }
 for opt in 0 2; do
  "$cc" $flags -O"$opt" tests/unit.c -lm -o "$out/unit"
  "$out/unit" > "$out/$cc-O$opt.txt"
  sed -n '/^digest=/p' "$out/$cc-O$opt.txt" > "$out/$cc-O$opt.digest"
  "$cc" $flags -O"$opt" tests/determinism.c -lm -o "$out/determinism"
  "$out/determinism" >> "$out/$cc-O$opt.digest"
  cmp "$out/gcc-O0.digest" "$out/$cc-O$opt.digest"
  "$cc" $flags -O"$opt" tests/robust.c -lm -o "$out/robust"
  "$out/robust"
  echo "$cc -O$opt: unit, robustness and determinism passed"
 done
 "$cc" $flags -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined tests/fuzz.c -lm -o "$out/fuzz"
 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$out/fuzz" "${TYPE_FUZZ_COUNT:-200000}"
 "$cc" $flags -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined tests/robust.c -lm -o "$out/robust-san"
 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$out/robust-san"
done
case $(uname -m) in
 x86_64|i?86)
  gcc $flags -O2 -mfpmath=387 tests/determinism.c -lm -o "$out/determinism-x87"
  "$out/determinism-x87" > "$out/x87.digest"
  sed -n '/^multisize /p' "$out/gcc-O2.digest" > "$out/sse.digest"
  cmp "$out/sse.digest" "$out/x87.digest"
  echo 'excess precision: x87 digest equals SSE/compiler matrix'
 ;;
esac
cat "$out/gcc-O2.txt" "$out/gcc-O2.digest"
echo 'determinism: gcc/clang O0/O2 digests identical'
printf 'int LLVMFuzzerTestOneInput(const unsigned char *p, __SIZE_TYPE__ n) { (void)p; (void)n; return 0; }\n' > "$out/probe.c"
if clang -fsanitize=fuzzer "$out/probe.c" -o "$out/probe" 2>"$out/probe.log"; then
 clang $flags -O1 -g -DTYPE_LIBFUZZER -fsanitize=fuzzer,address,undefined tests/fuzz.c -lm -o "$out/libfuzz"
 mkdir "$out/corpus"
 cp tests/fonts/*.ttf tests/fonts/*.otf "$out/corpus/"
 "$out/robust" "$out/corpus"
 if ! ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "$out/libfuzz" "$out/corpus" -seed=42 -max_len=2000000 -max_total_time="${TYPE_LIBFUZZ_SECONDS:-300}" -rss_limit_mb=1024 -timeout=15 -artifact_prefix="$out/" -print_final_stats=1 > "$out/libfuzzer.log" 2>&1; then
  cat "$out/libfuzzer.log"; exit 1
 fi
 tail -12 "$out/libfuzzer.log"
else
 echo 'libFuzzer unavailable (compiler does not support -fsanitize=fuzzer)'
fi
echo 'all Type tests passed; generated build/corpus artifacts removed on exit'
