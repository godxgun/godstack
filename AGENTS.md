# Godstack

Header is a black box.
Read `foo.h`. Call it.
Do not open `foo.c` unless you are changing that library, or the header was used correctly and the process still dies.

Vendored by copy. `-I` the library directory. Include `foo.h`, then `foo.c`. That `.c` pulls the rest of the TU.

| Lib | Header | `.c` pulls |
|-----|--------|------------|
| Peak | `Peak/peak.h` | platform + `p_log.c` |
| Rend | `Rend/rend.h` | `rend_vk.c` |
| Grit | `Grit/grit.h` | itself |
| Fuse | `Fuse/fuse.h` | itself |
| Poof | `Poof/poof.h` | itself |
| Cool | `Cool/cool.h` | itself |

Peak before Rend. `peak.h` sets the Vulkan WSI macros when `PEAK_VULKAN` is on.

`rg` first. Offset reads. Never dump a library `.c` to understand it.

Build: `./build` in this directory (includes `Poof/poof.c`).

## Skills

Full record in `skills/`. One directory per skill, `SKILL.md` inside. Do not restate them here.

- `skills/algorithm/SKILL.md` — compress, simplify, delete; then repeat
- `skills/style/SKILL.md` — C layout, names, lint (`lint.py`)
- `skills/debug/SKILL.md` — `MASSERT` / recoverable return / `MOD_TODO`
- `skills/commit/SKILL.md` — `<ModuleName> MAJOR.MINOR.PATCH`
