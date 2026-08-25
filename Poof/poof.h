/* ===========================================================================   
 * POOF - The Universal Build System - Copyright (c) 2026 Vasco Alves
 *
 * PREFIX: Poof_ (types) or poof_ (functions & variables)
 *
 * DESCRIPTION:
 * Platform independent-ish build system!
 * Performant, multi-threaded and mesmerizing.
 *
 * See LICENSE at the end of the file.
 * =========================================================================== */

#ifndef _POOF_H_
#define _POOF_H_

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#define POOF_MAJOR 0
#define POOF_MINOR 1
#define POOF_PATCH 2

/* CHANGE LOG
 * 0.1.1 - @vasco - rebuild finds poof.h from more paths
 * 0.1.2 - @vasco - include poof.c; no POOF_IMPLEMENTATION
 */

#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <dirent.h>
    #include <errno.h>
#endif

enum PoofTargetPlatform {
    POOF_TARGET_HOST  = 0,
    POOF_TARGET_WIN32 = 1,
    POOF_TARGET_LINUX = 1 << 1,
    POOF_TARGET_MACOS = 1 << 2,
};

enum PoofCCFlags {
    POOF_CC_GCC     = 1,
    POOF_CC_CLANG   = 1 << 1,
    POOF_CC_MSVC    = 1 << 2,
    POOF_CC_MINGW   = 1 << 3,
};

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
};

#define POOF_IS_SET(var, flag) (((var) & (flag)) != 0)

typedef struct Poof_Cmd {
    const char **items;
    size_t capacity;
    size_t count;
} Poof_Cmd;

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

typedef struct Poof_Batch {
    Poof_Cmd *cmds;
    size_t capacity;
    size_t count;
} Poof_Batch;

/* Command operations */
extern void poof_cmd_append_internal(Poof_Cmd *cmd, ...);
#define poof_cmd_append(cmd, ...) poof_cmd_append_internal((cmd), __VA_ARGS__, NULL)
 #define poof_cc_append(cc, ...)  poof_cmd_append_internal((cc), __VA_ARGS__, NULL)

#define poof_cc_append_win32(cc, ...) poof_cmd_append_internal(&((cc)->win32_flags), __VA_ARGS__, NULL)
#define poof_cc_append_linux(cc, ...) poof_cmd_append_internal(&((cc)->linux_flags), __VA_ARGS__, NULL)
#define poof_cc_append_macos(cc, ...) poof_cmd_append_internal(&((cc)->macos_flags), __VA_ARGS__, NULL)

extern void poof_cmd_free(Poof_Cmd *cmd);
extern void poof_cmd_clear(Poof_Cmd *cmd);
extern bool poof_cmd_run(Poof_Cmd *cmd);

/* Batch operations */
extern void poof_batch_append_cmd(Poof_Batch *batch, Poof_Cmd cmd);
extern void poof_batch_append_cc(Poof_Batch *batch, Poof_CC *cc);
#define poof_batch_append(batch, cmd) poof_batch_append_cmd((batch), (cmd))
extern void poof_batch_free(Poof_Batch *batch);
extern bool poof_batch_run(Poof_Batch *batch, const char *label);
extern bool poof_batch_run_parallel(Poof_Batch *batch, const char *label, size_t max_jobs);

/* Compiler abstraction */
extern uint8_t poof_cc_available(void);
extern void poof_cc_init(Poof_CC *cc, uint8_t compiler, uint8_t target_platform);
extern void poof_cc_free(Poof_CC *cc);
extern bool poof_cc_run(Poof_CC *cc);

/* File and directory utilities */
extern bool poof_mkdir(const char *path);
extern bool poof_touch(const char *path);
extern bool poof_rm(const char *path);
extern bool poof_rm_recursive(const char *path);
extern bool poof_copy_file(const char *src, const char *dst);
extern bool poof_needs_rebuild(const char *target, const char **sources, size_t source_count);

/* Output and pretty printing */
extern int poof_print(uint32_t col, const char *text, ...);
extern int poof_progress_bar(const char *label, float value, float min, float max, float width);

/* Rebuild self macro */
extern void poof_go_rebuild_urself_impl(int argc, char **argv, const char *source_file);

#define POOF_GO_REBUILD_URSELF(argc, argv) poof_go_rebuild_urself_impl(argc, argv, __FILE__)

#endif // _POOF_H_

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2026 Vasco Alves
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
