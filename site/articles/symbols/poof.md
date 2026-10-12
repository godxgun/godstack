+++
title = 'Poof symbols'
description = 'Types, macros, and functions declared by Poof.h.'
weight = 70
[params]
  id = 'symbols/poof'
  product = 'Poof'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Poof symbols

For setup and API workflows, read the [Poof guide](/docs/poof/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Poof.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### PoofTargetPlatform {#pooftargetplatform}

Source: `Poof.h:65`

```c
enum PoofTargetPlatform {
    POOF_TARGET_HOST  = 0,
    POOF_TARGET_WIN32 = 1,
    POOF_TARGET_LINUX = 1 << 1,
    POOF_TARGET_MACOS = 1 << 2,
};
```

#### PoofCCFlags {#poofccflags}

Source: `Poof.h:72`

```c
enum PoofCCFlags {
    POOF_CC_GCC     = 1,
    POOF_CC_CLANG   = 1 << 1,
    POOF_CC_MSVC    = 1 << 2,
    POOF_CC_MINGW   = 1 << 3,
};
```

#### PoofOptimizationFlags {#poofoptimizationflags}

Source: `Poof.h:79`

```c
enum PoofOptimizationFlags {
    POOF_O0       = 0,
    POOF_O1       = 1,
    POOF_O2       = 2,
    POOF_O3       = 3,
    POOF_MSSE     = 1 << 3,
    POOF_MSSE2    = 1 << 4,
    POOF_AVX      = 1 << 5,
    POOF_AVX2     = 1 << 6,
    POOF_AVX512F  = 1 << 7,
    POOF_BMI      = 1 << 8,
};
```

#### Poof_Cmd {#poof_cmd}

Source: `Poof.h:94`

```c
typedef struct Poof_Cmd {
    const char **items;
    size_t capacity;
    size_t count;
} Poof_Cmd;
```

#### Poof_CC {#poof_cc}

Source: `Poof.h:100`

```c
typedef struct Poof_CC {
    uint8_t compiler;               // enum PoofCCFlags
    uint8_t target_platform;        // enum PoofTargetPlatform
    uint32_t optimization;          // bitwise PoofOptimizationFlags
    bool debug_mode;
    Poof_Cmd inputs;
    const char *output;
    Poof_Cmd includes;
    Poof_Cmd lib_paths;
    Poof_Cmd libs;
    Poof_Cmd defines;
    Poof_Cmd extra_flags;

    /* Platform specific flags */
    Poof_Cmd win32_flags;
    Poof_Cmd linux_flags;
    Poof_Cmd macos_flags;
} Poof_CC;
```

#### Poof_Batch {#poof_batch}

Source: `Poof.h:119`

```c
typedef struct Poof_Batch {
    Poof_Cmd *cmds;
    size_t capacity;
    size_t count;
} Poof_Batch;
```

### Macros

#### POOF_ISATTY {#poof_isatty}

Source: `Poof.h:54`

```c
#define POOF_ISATTY(fd) (_isatty(fd))
```

#### POOF_ISATTY {#poof_isatty-1}

Source: `Poof.h:61`

```c
#define POOF_ISATTY(fd) (isatty(fd))
```

#### POOF_IS_SET {#poof_is_set}

Source: `Poof.h:92`

```c
#define POOF_IS_SET(var, flag) (((var) & (flag)) != 0)
```

#### poof_cmd_append {#poof_cmd_append}

Source: `Poof.h:127`

```c
#define poof_cmd_append(cmd, ...) poof_cmd_append_internal((cmd), __VA_ARGS__, NULL)
```

#### poof_cc_append {#poof_cc_append}

Source: `Poof.h:128`

```c
#define poof_cc_append(cc, ...)  poof_cmd_append_internal((cc), __VA_ARGS__, NULL)
```

#### poof_cc_append_win32 {#poof_cc_append_win32}

Source: `Poof.h:130`

```c
#define poof_cc_append_win32(cc, ...) poof_cmd_append_internal(&((cc)->win32_flags), __VA_ARGS__, NULL)
```

#### poof_cc_append_linux {#poof_cc_append_linux}

Source: `Poof.h:131`

```c
#define poof_cc_append_linux(cc, ...) poof_cmd_append_internal(&((cc)->linux_flags), __VA_ARGS__, NULL)
```

#### poof_cc_append_macos {#poof_cc_append_macos}

Source: `Poof.h:132`

```c
#define poof_cc_append_macos(cc, ...) poof_cmd_append_internal(&((cc)->macos_flags), __VA_ARGS__, NULL)
```

#### poof_batch_append {#poof_batch_append}

Source: `Poof.h:141`

```c
#define poof_batch_append(batch, cmd) poof_batch_append_cmd((batch), (cmd))
```

#### poof_support {#poof_support}

Source: `Poof.h:166`

```c
#define poof_support(...) poof_support_internal(__VA_ARGS__, NULL)
```

#### POOF_GO_REBUILD_URSELF {#poof_go_rebuild_urself}

Source: `Poof.h:172`

```c
#define POOF_GO_REBUILD_URSELF(argc, argv) poof_go_rebuild_urself_impl(argc, argv, __FILE__)
```

### Functions

#### poof_cmd_append_internal {#poof_cmd_append_internal}

Source: `Poof.h:126`

```c
static void poof_cmd_append_internal(Poof_Cmd *cmd, ...);
```

Command operations

#### poof_cmd_free {#poof_cmd_free}

Source: `Poof.h:134`

```c
static void poof_cmd_free(Poof_Cmd *cmd);
```

#### poof_cmd_clear {#poof_cmd_clear}

Source: `Poof.h:135`

```c
static void poof_cmd_clear(Poof_Cmd *cmd);
```

#### poof_cmd_run {#poof_cmd_run}

Source: `Poof.h:136`

```c
static bool poof_cmd_run(Poof_Cmd *cmd);
```

#### poof_batch_append_cmd {#poof_batch_append_cmd}

Source: `Poof.h:139`

```c
static void poof_batch_append_cmd(Poof_Batch *batch, Poof_Cmd cmd);
```

Batch operations

#### poof_batch_append_cc {#poof_batch_append_cc}

Source: `Poof.h:140`

```c
static void poof_batch_append_cc(Poof_Batch *batch, Poof_CC *cc);
```

#### poof_batch_free {#poof_batch_free}

Source: `Poof.h:142`

```c
static void poof_batch_free(Poof_Batch *batch);
```

#### poof_batch_run {#poof_batch_run}

Source: `Poof.h:143`

```c
static bool poof_batch_run(Poof_Batch *batch, const char *label);
```

#### poof_batch_run_parallel {#poof_batch_run_parallel}

Source: `Poof.h:144`

```c
static bool poof_batch_run_parallel(Poof_Batch *batch, const char *label, size_t max_jobs);
```

#### poof_cc_available {#poof_cc_available}

Source: `Poof.h:147`

```c
static uint8_t poof_cc_available(void);
```

Compiler abstraction

#### poof_cpu_available {#poof_cpu_available}

Source: `Poof.h:148`

```c
static uint32_t poof_cpu_available(void);
```

#### poof_has_cmd {#poof_has_cmd}

Source: `Poof.h:149`

```c
static bool poof_has_cmd(const char *name);
```

#### poof_cc_init {#poof_cc_init}

Source: `Poof.h:150`

```c
static void poof_cc_init(Poof_CC *cc, uint8_t compiler, uint8_t target_platform);
```

#### poof_cc_free {#poof_cc_free}

Source: `Poof.h:151`

```c
static void poof_cc_free(Poof_CC *cc);
```

#### poof_cc_run {#poof_cc_run}

Source: `Poof.h:152`

```c
static bool poof_cc_run(Poof_CC *cc);
```

#### poof_mkdir {#poof_mkdir}

Source: `Poof.h:155`

```c
static bool poof_mkdir(const char *path);
```

File and directory utilities

#### poof_touch {#poof_touch}

Source: `Poof.h:156`

```c
static bool poof_touch(const char *path);
```

#### poof_rm {#poof_rm}

Source: `Poof.h:157`

```c
static bool poof_rm(const char *path);
```

#### poof_rm_recursive {#poof_rm_recursive}

Source: `Poof.h:158`

```c
static bool poof_rm_recursive(const char *path);
```

#### poof_copy_file {#poof_copy_file}

Source: `Poof.h:159`

```c
static bool poof_copy_file(const char *src, const char *dst);
```

#### poof_needs_rebuild {#poof_needs_rebuild}

Source: `Poof.h:160`

```c
static bool poof_needs_rebuild(const char *target, const char **sources, size_t source_count);
```

#### poof_print {#poof_print}

Source: `Poof.h:163`

```c
static int poof_print(uint32_t col, const char *text, ...);
```

Output and pretty printing

#### poof_support_line {#poof_support_line}

Source: `Poof.h:164`

```c
static void poof_support_line(const char *name, int yes);
```

#### poof_support_internal {#poof_support_internal}

Source: `Poof.h:165`

```c
static uint32_t poof_support_internal(const char *label, ...);
```

#### poof_progress_bar {#poof_progress_bar}

Source: `Poof.h:167`

```c
static int poof_progress_bar(const char *label, float value, float min, float max, float width);
```

#### poof_go_rebuild_urself_impl {#poof_go_rebuild_urself_impl}

Source: `Poof.h:170`

```c
static void poof_go_rebuild_urself_impl(int argc, char **argv, const char *source_file);
```

Rebuild self macro

<!-- END GENERATED API -->
