# Poof

Documentation: https://godxgun.github.io/godstack/docs/poof/
Description: Construct process commands and compiler configurations with Poof.


# Poof

Poof lets you write a project's build script in C. You keep a `build.c` next to your sources,
compile it once with `cc build.c -o build`, and from then on run `./build`. Poof provides
commands, a compiler description that becomes GCC, Clang, MinGW or MSVC command lines, parallel
batches, file helpers and a self-rebuild macro.

Every function is `static`, so no implementation macro is needed. Include `Poof.h` before any C
library header: it defines `_POSIX_C_SOURCE` (and `_DARWIN_C_SOURCE` on macOS) for the process
APIs it uses.

## Running a Command

The first thing to try is running one program and checking that it succeeded:

```c
#include "Poof.h"

int main(void)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "cc", "--version");
    bool ok = poof_cmd_run(&cmd);
    poof_cmd_free(&cmd);
    return ok ? 0 : 1;
}
```

`Poof_Cmd` is a growable list of argument pointers. `poof_cmd_append()` takes any number of
strings; it stores the pointers, not copies, so each string must stay alive while the command is
used. `poof_cmd_run()` joins the arguments into one command line, quoting arguments that contain
spaces, prints it, and runs it with the system shell. It returns `true` when the exit status is
zero and then empties the command, keeping its storage for reuse. `poof_cmd_free()` releases the
storage.

Because the command line goes through the shell, do not build commands from untrusted input.

## Compiling a Program

A `Poof_CC` describes one compilation: inputs, output, include paths, defines, libraries and
flags. `poof_cc_run()` turns it into a compiler command line, runs it, and frees the
configuration:

```c
Poof_CC cc;
poof_cc_init(&cc, poof_has_cmd("clang") ? POOF_CC_CLANG : POOF_CC_GCC, POOF_TARGET_HOST);
cc.output = "app";
cc.optimization = POOF_O2;
cc.debug_mode = false;
poof_cmd_append(&cc.inputs, "app.c");
poof_cmd_append(&cc.includes, ".");
poof_cmd_append(&cc.defines, "NDEBUG");
poof_cmd_append(&cc.libs, "m");
poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wextra");
if (!poof_cc_run(&cc)) return 1;
```

With Clang this runs roughly
`clang -target ... -O2 -DNDEBUG -I. app.c -o app -lm -std=c99 -Wall -Wextra`. Includes, defines
and libraries are written without their `-I`, `-D` and `-l` prefixes; Poof adds the right
spelling for the compiler, including `/I`, `/D` and `name.lib` for MSVC. Flags in `extra_flags`
are passed through unchanged.

`poof_cc_init()` starts in debug mode (`-g`) at `POOF_O0`. `optimization` combines one level
(`POOF_O0` to `POOF_O3`) with optional instruction-set flags such as `POOF_AVX2` or `POOF_BMI`.
The compiler argument is a bit mask; when several bits are set, Poof uses the first of GCC,
Clang, MinGW and MSVC. `poof_cc_available()` probes which compilers are installed and returns
that mask, and `poof_has_cmd()` checks for any program on the `PATH`.

## Platform Flags and Cross Targets

Flags that only apply to one operating system go into `win32_flags`, `linux_flags` or
`macos_flags`, usually through the matching macros. Only the collection for the target being
built is used:

```c
Poof_CC cc;
poof_cc_init(&cc, POOF_CC_CLANG, POOF_TARGET_LINUX | POOF_TARGET_WIN32);
cc.output = "tool";
poof_cmd_append(&cc.inputs, "tool.c");
poof_cc_append_linux(&cc, "-ldl", "-lpthread");
poof_cc_append_win32(&cc, "-lgdi32");
if (!poof_cc_run(&cc)) return 1;
```

`POOF_TARGET_HOST` builds for the running system. A mask with several targets produces one
compiler command per target and suffixes the output names (`tool_linux`, `tool_win32.exe`).
Clang selects targets with `-target`; GCC switches to `x86_64-w64-mingw32-gcc` for Windows. macOS
builds compile sources as Objective-C so that AppKit headers work.

## Building in Parallel

A `Poof_Batch` collects commands and runs them concurrently. `poof_batch_append_cc()` converts a
compiler description into commands and frees the description; `poof_batch_append()` adds a
`Poof_Cmd`, and the batch takes ownership of it:

```c
Poof_Batch batch = {0};

Poof_Cmd shader = {0};
poof_cmd_append(&shader, "slangc", "ui.slang", "-target", "spirv", "-o", "ui.spv");
poof_batch_append(&batch, shader);

const char *programs[] = {"server", "client"};
const char *sources[] = {"server.c", "client.c"};
for (int i = 0; i < 2; ++i) {
    Poof_CC cc;
    poof_cc_init(&cc, POOF_CC_GCC, POOF_TARGET_HOST);
    cc.output = programs[i];
    poof_cmd_append(&cc.inputs, sources[i]);
    poof_batch_append_cc(&batch, &cc);
}

if (!poof_batch_run(&batch, "build")) return 1;
```

`poof_batch_run()` starts one job per CPU, draws a progress bar labelled `build`, waits for every
command, and frees the batch. It returns `false` if any command failed.
`poof_batch_run_parallel()` takes an explicit job limit. Commands in one batch must not depend on
each other; run dependent steps in separate batches.

## Skipping Up-to-Date Work

`poof_needs_rebuild()` compares modification times and returns `true` when the target is missing
or older than any source. The file helpers return `true` on success:

```c
const char *inputs[] = {"app.c", "app.h"};
if (!poof_mkdir("bin")) return 1;
if (poof_needs_rebuild("bin/app", inputs, 2)) {
    Poof_CC cc;
    poof_cc_init(&cc, POOF_CC_GCC, POOF_TARGET_HOST);
    cc.output = "bin/app";
    poof_cmd_append(&cc.inputs, "app.c");
    if (!poof_cc_run(&cc)) return 1;
}
if (!poof_copy_file("README.md", "bin/README.md")) return 1;
```

`poof_touch()`, `poof_rm()` and `poof_rm_recursive()` cover the remaining file housekeeping.

## Rebuilding the Build Script

Put `POOF_GO_REBUILD_URSELF(argc, argv)` at the start of `main()`. When the build script's source
file is newer than the running executable, Poof renames the old binary to `<name>.old`,
recompiles the script with `gcc`, and executes the new binary with the same arguments:

```c
#include "Poof.h"

int main(int argc, char **argv)
{
    POOF_GO_REBUILD_URSELF(argc, argv);

    Poof_CC cc;
    poof_cc_init(&cc, POOF_CC_GCC, POOF_TARGET_HOST);
    cc.output = "app";
    poof_cmd_append(&cc.inputs, "app.c");
    return poof_cc_run(&cc) ? 0 : 1;
}
```

After the first `cc build.c -o build`, editing `build.c` and running `./build` is enough.

## Printing Status

`poof_print()` is `printf` with a 24-bit `0xRRGGBB` color that is applied only when standard
output is a terminal. `poof_support()` prints a list of name and yes/no pairs, then the host's
SIMD support, and returns the SIMD flags as `POOF_*` optimization bits:

```c
poof_print(0x00FF88, "[build] %d targets\n", 2);
uint32_t simd = poof_support("app", "Vulkan", poof_has_cmd("glslangValidator"));
if (POOF_IS_SET(simd, POOF_AVX2)) poof_print(0x888888, "using AVX2 paths\n");
```

This repository's own [`build.c`](https://github.com/godxgun/godstack/blob/main/build.c) is a
full example: run `cc build.c -o build` and then `./build demo <name>` from the repository root.

See [Poof symbols](/godstack/symbols/poof/) for all declarations.
