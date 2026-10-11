# Type verification

Run from the godstack root with `sh tests/run.sh`. No downloads, package installs,
font discovery or external libraries are used. Required tools are gcc, clang,
POSIX sh and libc/libm. The script compiles with C99, Wall/Wextra/Werror and
`-ffp-contract=off`. It runs units and synthetic robustness at O0/O2 with both
compilers; two ASan/UBSan mutation runs of 200,000 inputs each; sanitized robustness;
and clang libFuzzer for 300 seconds when a separate compiler support probe succeeds.
All generated files live under `tests/.build`; each run removes its private directory.
`TYPE_FUZZ_COUNT` and `TYPE_LIBFUZZ_SECONDS` override durations for development.
Leak detection is disabled because sandbox ptrace prevents LeakSanitizer startup;
Type performs no heap allocation. Address/undefined checks remain enabled.

`unit.c` checks the seven pinned Sigma fonts: units/em, hhea metrics, Unicode cmap
hits/misses, known advances, decoded vertex counts/coordinates, bitmap origins,
diagnostic names, and every glyph's advance/outline. `determinism.c` hashes 608
rasters across 8, 12, 16, 24, 32, 48, 64 and 96 px, including quadratic composites,
CFF/CFF2 cubic curves, accented CFF composition and a CFF2 default variation blend.
`robust.c` covers bounded global/local subroutine loops, composite cycles/deep
chains, TTC selection, cmap validation, scratch rollback, shaped runs and shelves.
`fuzz.c` exports `LLVMFuzzerTestOneInput` and also has a deterministic standalone
mutation driver. Bit flips, every-length truncations on synthetic data, random
truncations, directory/offset/count corruption and nested loops are exercised.

The original implementation was saved outside the final tree before replacement.
A one-off tool compiled that original header and called its
metrics, cmap, outline and bitmap functions on the pinned fixtures. The resulting
seven text files and 70 coverage byte arrays are in `reference/`; no original
library code is shipped in these tests. The codepoints are U+0020, U+0041, U+0067,
U+00E9, U+03A9, U+0416, U+05D0, U+0644, U+0627 and U+1F600. Missing codepoints
compare glyph zero. Bitmaps are tightly packed grayscale coverage at 32 px/em.
Each text file starts with upem/ascent/descent/gap/glyph-count, followed for each
codepoint by cp/gid/advance/lsb/vertex-count, that many op/x/y/cx/cy/cx1/cy1 rows,
and x0/y0/x1/y1. Unused reference control fields are ignored (the old decoder did
not initialize them). Implied quadratic midpoint coordinates allow 0.51 font-unit
error because the original decoder rounded them to integers. Coverage tolerances
are at most 96/255 per pixel and less than 4% total ink difference; comparisons
print the actual maxima. Shape and metric equality is separate from raster tolerance.

The requested Sigma assets/fonts directory does not exist in this checkout;
these exact fonts and their accompanying licence files came from Sigma's assets/.
The six Noto faces retain the notices in NOTO-FONTS-LICENSE.txt,
NOTO-HEBREW-LICENSE.txt and NOTO-ARABIC-LICENSE.txt. Liberation Sans retains
LIBERATION-FONTS-LICENSE.txt. The two small synthetic OTF files are original data
emitted by synthetic.h; they have the same licence as the original test code.
All original tests, scripts and synthetic fixtures are GPL-3.0-only OR a separately
negotiated commercial license under the repository's licensing terms. Font licences
remain separate. See report.md for the completed verification results.
