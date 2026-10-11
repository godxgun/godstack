# Independent Type CPU fixtures

Run from the Sigma repository root. These fixtures are intentionally not wired
into Sigma's build or active text pipeline. No font/dependency fetching occurs.

- `contracts.c`: fail-closed admission, directory truncation/malformed data,
  caller scratch ownership/exhaustion, glyph IDs, shaped positions and metrics.
- `shaping.c`: actual local HarfBuzz GSUB/GPOS and FriBidi run reordering using
  the existing Noto Latin/Arabic/Hebrew fonts and their notices in `assets/`.
- `harfbuzz_bridge.h`: experimental logical-run adapter with caller-owned
  dependency objects; internal dependency allocation is **not arena-qualified**.
- `fixture_support.h`: fixed standalone fixture storage and bounded local file
  reads, not the application's arena implementation.

See [`docs/type-integration.md`](../../../docs/type-integration.md) for API
contracts, intentional compatibility breaks, trust limitations and unrun GCC,
Clang and sanitizer acceptance commands. Do not disable assertions.

Neither these fixtures nor the sfnt directory preflight make imported fonts
safe. Malicious nested data must not reach the explicit trusted entry points.
