---
name: style
description: >
  Apply this project's C style. Use when asked to style, restyle, lint,
  or fix style. Lint pass, then Nuanced rg pass, then stop.
  Clean lint.py is not done: it does not cover Nuanced.
---

If the target is unclear, ask once. Else start now.
Do not rename public APIs or existing names.
Lint, then Nuanced. Edit only reported / rg-hit lines. No drive-by edits.
Do not stop because lint.py is clean.

MACROS: MODULE_MACRO
TYPES: ModuleType
FUNCTION: module_object_action()
LOCAL VARIABLES: module_variable
LOCAL VARIABLES: variable

# Lintable

Do not read whole files. Do not use tree-sitter.

    python3 <this-skill-dir>/lint.py PATH

Line-oriented. Does not preprocess; unity `#include "foo.c"` is just a line.
Re-run until clean or only false positives. No extra restyling.
Then do # Nuanced even if lint.py printed nothing.

List file-scope functions the same way the linter does:

    rg '^[A-Za-z_].*\(' PATH

Column-0 lines with `(` are prototypes or definition *names*.
A type-only column-0 line above a name is the return type of a definition.

**Definitions** — return type, name, `{` each on their own line:

    void *
    peak_file_alloc(const char *path, unsigned long *buf_size)
    {
    	...
    }

Not `void foo(void) {`, not `void foo(void)` then `{`, not `void foo()` .
`*` on a return-type line trails the type (`void *`). Empty params are `(void)`.
No `static` / `extern` on the definition. Linkage lives on the declaration only.

**Prototypes** — type and name on one line (params may wrap):

    static void term_screen_init(TermScreen *s, uint32_t cols, uint32_t rows);
    void *grit_arena_alloc(GritArena *a, size_t size);

Also reports: `int *p` (not `int* p` / `int * p`); space after `if` `for` `while`
`switch`, none inside `()`; `{` of those on the same line as the closing `)` of
the condition (multiline conditions are fine); decls at top of each block;
`switch` cases unindented; contiguous `#include <...>` runs alphabetical.
Unity / own-header includes are ignored for include order.

# Nuanced

Required after lint, including when lint.py reported no issues.
`lint.py` does not cover this section. Clean lint is not clean style.
`rg` on column-0 / tokens is enough; do not add another linter.
Run every pattern below. Hit → edit that line. No drive-by restyle.
Stop only after every pattern has been run.

    rg '^[A-Za-z_].*\(' PATH
    # proto or def *name*. A type-only line above a name is the def return type.

    rg '^static |^extern ' PATH
    # linkage once: `;` on the hit → declaration (keep). Else it is on a
    # definition — drop `static` / `extern`. Example bad / good:
    #   static void          void
    #   usage(void)          usage(void)

    rg 'sizeof\s*\(' PATH
    # parens only for a type, with a space: `sizeof (BufferType)`.
    # object: `sizeof *buffer`, never `sizeof(buffer)` / `sizeof(*buffer)`.

    rg 'typedef struct|typedef enum|typedef union' PATH
    # `typedef struct { ... } ModuleType;`. Never `Module_Type`.

    rg '/\* NOTE|/\*|//' PATH
    # one `/* NOTE(user): ... */` at the function top, not sprinkled.

    rg '^\s+(case |default:)' PATH
    # cases must sit at switch indent (lintable). Adjacent cases need
    # `/* FALLTHROUGH */` on the first.

    rg '^#include' PATH
    # system `<...>` run alphabetical; own-header / unity ignored.

C99 only.

**sizeof** — `sizeof *buffer`. Parens only for a type, with a space: `sizeof (BufferType)`.

**Names**
- Types: `ModuleType` (`PeakWindow`, `GritArena`). Never `Module_Type` or `Module_Type_Whatever`.
- Functions: `module_object_action` (`peak_window_open`). Internal: `module_internal_object_action`
- Forward-declare every function once: header if exported, else this file

**Linkage** — declared once. The definition does not repeat it.
C keeps the linkage from the prior declaration (`static` / `extern` / none).
`static` on the prototype and again on the definition is repetition.

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
