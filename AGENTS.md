# Godstack

Header is a black box.
Read `foo.h`. Call it.
Do not open `foo.c` unless you are changing that library, or the header was used correctly and the process still dies.

Vendored by copy. `-I` the library directory. Include `foo.h`, then `foo.c`. That `.c` pulls the rest of the TU.

| Lib | Header | `.c` pulls |
|-----|--------|------------|
| Peak | `Peak/peak.h` | platform + `p_log.c` |
| Rend | `Rend/rend.h` | `rend_vk14.c` (if `PEAK_VULKAN`) and `rend_cpu.c` |
| Grit | `Grit/grit.h` | itself |
| Fuse | `Fuse/fuse.h` | itself |
| Poof | `Poof/poof.h` | itself |
| Cool | `Cool/cool.h` | itself |
| Term | `Term/term.h` | itself |

Peak before Rend. `peak.h` sets the Vulkan WSI macros when `PEAK_VULKAN` is on.

`rg` first. Offset reads. Never dump a library `.c` to understand it.

Build: `./build` in this directory (includes `Poof/poof.c`).

## Docs (read on demand)

When asked about a topic, read the file completely and follow links.
Do not dump `**/*.c` to learn an API — read the header.

| Topic | File |
|-------|------|
| tests | `docs/tests.md` |

## Tests

`./build test` builds then runs. `tests/rend_cpu` is offscreen Rend CPU
raster (no window, no Vulkan). Headless Rend demos use `--headless`.
Do not scrape a display. How to add a check: `docs/tests.md`.

## Skills

Full record in `skills/`. One directory per skill, `SKILL.md` inside. Do not restate them here.

- `skills/algorithm/SKILL.md` — compress, simplify, delete; performance first; then repeat
- `skills/style/SKILL.md` — C layout, names, lint
- `skills/debug/SKILL.md` — `MASSERT` / recoverable return / `MOD_TODO`
- `skills/commit/SKILL.md` — `<ModuleName> MAJOR.MINOR.PATCH`
