# Agent Contract
- Unit tests are illegal, considered a crime. Instead run demos/ that stress test the public API.
## Files
- Header is a black box.
- Read `foo.h`. Call it.
- Do not open `foo.c` unless you are changing that library, or the header was used correctly and the process still dies.
- Vendored by copy. `-I` the library directory. Include `foo.h`, then `foo.c`. That `.c` pulls the rest of the TU. Poof is header-only: include `poof.h`, with no implementation macro.
## Navigation
- `rg` first.
- `read` with offset/limit.
- Never dump a library `.c` to learn an API — read the header first.
## Build
- `./build` in this directory (includes `Poof/poof.h`).
- `./build` builds demos, tools, and GPU artifacts; `./build run` runs CPU workloads and headless GPU demos (GPU required).
- `./build test` is a backwards-compatible alias for demo runs, never a suite.
- `./build cpu` / `./build cpu run` build/run Cast, Fuse, Grit, and Peak public-API workloads without display/GPU at runtime; Linux Peak placement still needs Vulkan headers/loader linkage.
- `./build peak run`, `./build rend run`, and `./build rend2 run` run their corresponding stress demos.
- `./build snake run` performs repeated offscreen output and compiled shader-reflection checks.


# Coding Style

## File Structure
- One header file (i.e. pixelpro.h)
- Multiple .c files per module (i.e. p_nodes.c, p_history.c, etc.)
- One main .c file (i.e. pixelpro.c) that includes the other .c files (unity build)
- main.c
- unity build

## File Layout
- Comment with LICENSE and possibly short explanation of file.
- Headers
- Macros
- Types
- Function declarations:
    - Functions declared in a header file dont need to be declared again.
    - Helper functions only used in this file will be declared at the top in the order they are defined.
    - Group/order in logical manner.
    - All declarations static.
    - Function named like `module_object_action` for easy auto complete and grep.
- Global variables.
- Function definitions in same order as declarations.
    - Definitions do not require linkage keywords as the standard guarrantees they match the declaration.
    - Return type on a different line.
```
static void hello();

void
hello()
{
    printf("Hello\n");
}
- main (if main.c)

## C Features
- Use C99.

## Leading Whitespace
- Use tabs for indentation and spaces for alignment.
- This ensures everything will line up independent of tab size.

## Functions
- Return type and modifiers on own line.
- Function name and argument list on next line. This allows to grep for function names simply using grep ^functionname(.
- Opening { on own line (function definitions are a special case of blocks as they cannot be nested).
- Functions not used outside translation unit should be declared and defined static.
- Functions are as long as they need to be: 500 lines is not a sufficient reason to split up a function.

### Example:

static void
usage(void)
{
	eprintf("usage: %s [file ...]\n", argv0);
}

## Variables
Global variables not used outside translation unit should be declared static.
In declaration of pointers the * is adjacent to variable name, not type.

## Keywords
Use a space after if, for, while, switch (they are not function calls).
Do not use a space after the opening ( and before the closing ).
Preferably use () with sizeof.
Do not use a space with sizeof().

## Switch
Do not indent cases another level.
Comment cases that FALLTHROUGH.
Example:

switch (value) {
case 0: /* FALLTHROUGH */
case 1:
case 2:
	break;
default:
	break;
}

## Headers
- Unity build.
- Place system/libc headers first in alphabetical order.
- If headers must be included in a specific order add a comment to explain.
- Place local headers after an empty line.
- When writing and using local headers.
- Try to avoid cyclic header inclusion dependencies.
- Instead ensure they are included where and when they are needed.

## User Defined Types
- Do not use type_t naming (it is reserved for POSIX and less readable).
- Typedef opaque structs.
- Do not typedef builtin types.
- Use CamelCase for typedef'd types.

## Line Length
- Lines can be as long as needed, even 500 chars wide.

# Tests and Boolean Values
- Do not use C99 bool types (stick to integer types).
- Otherwise use compound assignment and tests unless the line grows too long:
if (!(p = malloc(sizeof(*p))))
	hcf();
## Handling Errors
When functions return -1 for error test against 0 not -1:
if (func() < 0)
	hcf();

- Use goto to unwind and cleanup when necessary instead of multiple nested levels.
- return or exit early on failures instead of multiple nested levels.
- Unreachable code should have a NOTREACHED comment.
- Think long and hard on whether or not you should cleanup on fatal errors.
