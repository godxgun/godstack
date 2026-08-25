---
name: style
description: >
  Apply this project's C style. Use when asked to style, restyle, lint,
  or fix style. One-shot: lint pass, then stop.
---

If the target is unclear, ask once. Else start now.
Do not rename public APIs or existing names.
Lint, edit only reported lines, stop. No drive-by edits.

MACROS: MODULE_MACRO
TYPES: ModuleType
FUNCTION: module_object_action()
LOCAL VARIABLES: module_variable
LOCAL VARIABLES: variable

# Lintable

Do not read whole files.

    uv run --project <this-skill-dir> python lint.py PATH

Re-run until clean or only false positives. No extra restyling.

Reports: return type, name, and `{` each on their own line; `(void)` if no params;
`int *p`; space after `if` `for` `while` `switch`, none inside `()`;
decls at top of block; `if {` same line; `switch` cases unindented;
system includes first, alphabetical, then a blank line and locals.

# Nuanced

When writing or asked. Do not rewrite a clean file for these.

C99 only.

**sizeof** — `sizeof *buffer`. Parens only for a type, with a space: `sizeof (BufferType)`.

**Names and linkage**
- Types: `ModuleType` (`PeakWindow`, `GritArena`). Never `Module_Type` or `Module_Type_Whatever`.
- Functions: `module_object_action` (`peak_window_open`). Internal: `module_internal_object_action`
- Forward-declare every function once: header if exported, else this file
- TU-local: `static` on the declaration only

    static void usage(void);

    void
    usage(void)
    {
    	...
    }

**File layout**
1. File note
2. `* major.minor.patch - @user - description`
3. Headers, macros, types
4. Declarations (param names; optional if short; grouped)
5. Globals
6. Definitions in declaration order
7. `main`

**Comments** — one `/* NOTE(user): ... */` at the function top, not sprinkled.

**Blocks**
`{` same line after a space (not functions). `}` alone unless `else`/`while` continues.
Brace a single statement if a sibling or nested arm needs braces.

    for (;;) {
    	if (foo) {
    		bar;
    		baz;
    	}
    }

    if (foo) {
    	bar;
    } else {
    	baz;
    	qux;
    }

**Switch** — `/* FALLTHROUGH */`. Cases not extra-indented (also linted).

    switch (value) {
    case 0: /* FALLTHROUGH */
    case 1:
    	break;
    default:
    	break;
    }

**Headers and types**
Comment load-bearing include order. No cyclic includes. Unity builds often need none.
Typedef structs `ModuleType`. Opaque handles are a pointer typedef to that name. Do not typedef builtins or rename existing types.

**Line length** — one statement per line. No arbitrary wrap.
