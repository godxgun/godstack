/* ===========================================================================   
 * POOF - The Universal Build System - Copyright (c) 2026 Vasco Alves
 *
 * PREFIX: Poof_ (types) or poof_ (functions & variables)
 *
 * DESCRIPTION:
 * Platform independent-ish build system!
 * Performant, multi-threaded and mesmerizing.
 * Header-only: include poof.h before libc headers; all functions are static.
 *
 * See LICENSE at the end of the file.
 * =========================================================================== */

#pragma once

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#define POOF_MAJOR 0
#define POOF_MINOR 2
#define POOF_PATCH 1

/* CHANGE LOG
 * 0.1.1 - @vasco - rebuild finds poof.h from more paths
 * 0.1.2 - @vasco - include poof.c; no POOF_IMPLEMENTATION
 * 0.1.3 - @vasco - macos clang -x objective-c before sources
 * 0.1.4 - @vasco - macos _SC_NPROCESSORS_ONLN via _DARWIN_C_SOURCE
 * 0.2.0 - @vasco - host SIMD probe, poof_has_cmd, POOF_BMI, poof_support
 * 0.2.1 - header-only static implementation; compile-only output naming
 */

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#if defined(_MSC_VER)
    #include <intrin.h>
#endif

#if defined(_WIN32)
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define POOF_ISATTY(fd) (_isatty(fd))
    #define POOF_DEV_NULL "nul 2>&1"
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <dirent.h>
    #define POOF_ISATTY(fd) (isatty(fd))
    #define POOF_DEV_NULL "/dev/null 2>&1"
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
    POOF_BMI      = 1 << 8,
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
static void poof_cmd_append_internal(Poof_Cmd *cmd, ...);
#define poof_cmd_append(cmd, ...) poof_cmd_append_internal((cmd), __VA_ARGS__, NULL)
 #define poof_cc_append(cc, ...)  poof_cmd_append_internal((cc), __VA_ARGS__, NULL)

#define poof_cc_append_win32(cc, ...) poof_cmd_append_internal(&((cc)->win32_flags), __VA_ARGS__, NULL)
#define poof_cc_append_linux(cc, ...) poof_cmd_append_internal(&((cc)->linux_flags), __VA_ARGS__, NULL)
#define poof_cc_append_macos(cc, ...) poof_cmd_append_internal(&((cc)->macos_flags), __VA_ARGS__, NULL)

static void poof_cmd_free(Poof_Cmd *cmd);
static void poof_cmd_clear(Poof_Cmd *cmd);
static bool poof_cmd_run(Poof_Cmd *cmd);

/* Batch operations */
static void poof_batch_append_cmd(Poof_Batch *batch, Poof_Cmd cmd);
static void poof_batch_append_cc(Poof_Batch *batch, Poof_CC *cc);
#define poof_batch_append(batch, cmd) poof_batch_append_cmd((batch), (cmd))
static void poof_batch_free(Poof_Batch *batch);
static bool poof_batch_run(Poof_Batch *batch, const char *label);
static bool poof_batch_run_parallel(Poof_Batch *batch, const char *label, size_t max_jobs);

/* Compiler abstraction */
static uint8_t poof_cc_available(void);
static uint32_t poof_cpu_available(void);
static bool poof_has_cmd(const char *name);
static void poof_cc_init(Poof_CC *cc, uint8_t compiler, uint8_t target_platform);
static void poof_cc_free(Poof_CC *cc);
static bool poof_cc_run(Poof_CC *cc);

/* File and directory utilities */
static bool poof_mkdir(const char *path);
static bool poof_touch(const char *path);
static bool poof_rm(const char *path);
static bool poof_rm_recursive(const char *path);
static bool poof_copy_file(const char *src, const char *dst);
static bool poof_needs_rebuild(const char *target, const char **sources, size_t source_count);

/* Output and pretty printing */
static int poof_print(uint32_t col, const char *text, ...);
static void poof_support_line(const char *name, int yes);
static uint32_t poof_support_internal(const char *label, ...);
#define poof_support(...) poof_support_internal(__VA_ARGS__, NULL)
static int poof_progress_bar(const char *label, float value, float min, float max, float width);

/* Rebuild self macro */
static void poof_go_rebuild_urself_impl(int argc, char **argv, const char *source_file);

#define POOF_GO_REBUILD_URSELF(argc, argv) poof_go_rebuild_urself_impl(argc, argv, __FILE__)


/* Implementation */

static bool
poof__probe_cmd(const char *cmd)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s > " POOF_DEV_NULL, cmd);
    return (system(buffer) == 0);
}

static uint8_t
poof_cc_available(void)
{
    uint8_t available = 0;

    poof_print(0x00FF88, "[POOF] Probing available compilers:\n");
    if (poof__probe_cmd("gcc --version")) {
        available |= POOF_CC_GCC;
        poof_print(0x00FF88, "  - GCC: available\n");
    }

    if (poof__probe_cmd("clang --version")) {
        available |= POOF_CC_CLANG;
        poof_print(0x00FF88, "  - CLANG: available\n");
    }

    if (poof__probe_cmd("x86_64-w64-mingw32-gcc --version") || poof__probe_cmd("mingw32-gcc --version")) {
        available |= POOF_CC_MINGW;
        poof_print(0x00FF88, "  - MinGW: available\n");
    }

    if (poof__probe_cmd("cl")) {
        available |= POOF_CC_MSVC;
        poof_print(0x00FF88, "  - MSVC: available\n");
    }

    return available;
}

static bool
poof_has_cmd(const char *name)
{
    char buffer[256];

    if (!name || !name[0]) return false;
#if defined(_WIN32)
    snprintf(buffer, sizeof(buffer), "where %s > " POOF_DEV_NULL, name);
#else
    snprintf(buffer, sizeof(buffer), "command -v %s > " POOF_DEV_NULL, name);
#endif
    return (system(buffer) == 0);
}

static uint32_t
poof_cpu_available(void)
{
    uint32_t available = 0;

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_cpu_supports("sse"))     available |= POOF_MSSE;
    if (__builtin_cpu_supports("sse2"))    available |= POOF_MSSE2;
    if (__builtin_cpu_supports("avx"))     available |= POOF_AVX;
    if (__builtin_cpu_supports("avx2"))    available |= POOF_AVX2;
    if (__builtin_cpu_supports("avx512f")) available |= POOF_AVX512F;
    if (__builtin_cpu_supports("bmi"))     available |= POOF_BMI;
#elif defined(_MSC_VER)
    int info[4] = {0};
    __cpuid(info, 1);
    if (info[3] & (1 << 25)) available |= POOF_MSSE;
    if (info[3] & (1 << 26)) available |= POOF_MSSE2;
    if (info[2] & (1 << 28)) available |= POOF_AVX;
    __cpuidex(info, 7, 0);
    if (info[1] & (1 << 3))  available |= POOF_BMI;
    if (info[1] & (1 << 5))  available |= POOF_AVX2;
    if (info[1] & (1 << 16)) available |= POOF_AVX512F;
#endif
#endif
    return available;
}

static void
poof_support_line(const char *name, int yes)
{
    if (!name) return;
    if (yes)
        poof_print(0x00FF88, "  - %s: yes\n", name);
    else
        poof_print(0x888888, "  - %s: no\n", name);
}

static uint32_t
poof_support_internal(const char *label, ...)
{
    uint32_t simd;
    va_list args;
    const char *name;

    poof_print(0xFE9900, "[%s] support:\n", label ? label : "POOF");

    va_start(args, label);
    while ((name = va_arg(args, const char *)) != NULL) {
        int yes = va_arg(args, int);
        poof_support_line(name, yes);
    }
    va_end(args);

    simd = poof_cpu_available();
    poof_support_line("AVX2", POOF_IS_SET(simd, POOF_AVX2));
    poof_support_line("AVX", POOF_IS_SET(simd, POOF_AVX));
    poof_support_line("SSE", POOF_IS_SET(simd, POOF_MSSE));
    poof_support_line("SSE2", POOF_IS_SET(simd, POOF_MSSE2));
    poof_support_line("AVX512F", POOF_IS_SET(simd, POOF_AVX512F));
    poof_support_line("BMI", POOF_IS_SET(simd, POOF_BMI));
    return simd;
}

static void
poof_cmd_append_internal(Poof_Cmd *cmd, ...)
{
    va_list args;
    va_start(args, cmd);
    const char *arg = NULL;
    while ((arg = va_arg(args, const char *)) != NULL) {

    if (cmd->count >= cmd->capacity) {
        size_t new_cap = cmd->capacity == 0 ? 16 : cmd->capacity * 2;
        const char **new_items = (const char **)realloc(cmd->items, new_cap * sizeof(const char *));
        if (!new_items) return;
        cmd->items = new_items;
        cmd->capacity = new_cap;
    }
    cmd->items[cmd->count++] = arg;
    }
    va_end(args);
}


static void
poof_cmd_clear(Poof_Cmd *cmd)
{
    /* clear count while keeping capacity */
    cmd->count = 0;
}

static void
poof_cmd_free(Poof_Cmd *cmd)
{
    if (cmd->items) {
        free(cmd->items);
        cmd->items = NULL;
    }
    cmd->capacity = 0;
    cmd->count = 0;
}

static char *
poof__cmd_to_string(const Poof_Cmd *cmd)
{
    if (!cmd || cmd->count == 0) return NULL;

    size_t total_len = 0;
    for (size_t i = 0; i < cmd->count; ++i) {
        total_len += strlen(cmd->items[i]) + 3; // quotes and space
    }

    char *cmdline = (char *)malloc(total_len + 1);
    if (!cmdline) return NULL;
    cmdline[0] = '\0';

    for (size_t i = 0; i < cmd->count; ++i) {
        if (i > 0) strcat(cmdline, " ");
        bool need_quote = (strchr(cmd->items[i], ' ') != NULL);
        if (need_quote) strcat(cmdline, "\"");
        strcat(cmdline, cmd->items[i]);
        if (need_quote) strcat(cmdline, "\"");
    }

    return cmdline;
}

static bool
poof_cmd_run(Poof_Cmd *cmd)
{
    char *cmdline = poof__cmd_to_string(cmd);
    if (!cmdline) return false;

    printf("[POOF] Executing: ");
    poof_print(0xA9B665, "%s\n", cmdline);

    int status = system(cmdline);
    free(cmdline);
    poof_cmd_clear(cmd);
    return (status == 0);
}

static void
poof_cc_init(Poof_CC *cc, uint8_t compiler, uint8_t target_platform)
{
    memset(cc, 0, sizeof(Poof_CC));
    cc->compiler = compiler;
    cc->target_platform = target_platform;
    cc->optimization = POOF_O0;
    cc->debug_mode = true;
}

static void
poof_cc_free(Poof_CC *cc)
{
    poof_cmd_free(&cc->inputs);
    poof_cmd_free(&cc->includes);
    poof_cmd_free(&cc->lib_paths);
    poof_cmd_free(&cc->libs);
    poof_cmd_free(&cc->defines);
    poof_cmd_free(&cc->extra_flags);
    poof_cmd_free(&cc->win32_flags);
    poof_cmd_free(&cc->linux_flags);
    poof_cmd_free(&cc->macos_flags);
}

static void
poof_batch_append_cmd(Poof_Batch *batch, Poof_Cmd cmd)
{
    if (batch->count >= batch->capacity) {
        size_t new_cap = batch->capacity == 0 ? 16 : batch->capacity * 2;
        Poof_Cmd *new_cmds = (Poof_Cmd *)realloc(batch->cmds, new_cap * sizeof(Poof_Cmd));
        if (!new_cmds) return;
        batch->cmds = new_cmds;
        batch->capacity = new_cap;
    }
    batch->cmds[batch->count++] = cmd;
}

static Poof_Cmd
poof__cc_build_cmd(const Poof_CC *cc, uint8_t target_platform, bool multi_target)
{
    Poof_Cmd cmd = {0};

    // Pick compiler binary
    if (cc->compiler & POOF_CC_GCC) {
        if (target_platform == POOF_TARGET_WIN32) {
            poof_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
        } else {
            poof_cmd_append(&cmd, "gcc");
        }
    } else if (cc->compiler & POOF_CC_CLANG) {
        poof_cmd_append(&cmd, "clang");
        if (target_platform == POOF_TARGET_WIN32) {
            poof_cmd_append(&cmd, "-target");
            poof_cmd_append(&cmd, "x86_64-pc-windows-gnu");
        } else if (target_platform == POOF_TARGET_LINUX) {
            poof_cmd_append(&cmd, "-target");
            poof_cmd_append(&cmd, "x86_64-unknown-linux-gnu");
        } else if (target_platform == POOF_TARGET_MACOS) {
            poof_cmd_append(&cmd, "-target");
            poof_cmd_append(&cmd, "x86_64-apple-darwin");
        }
    } else if (cc->compiler & POOF_CC_MINGW) {
        poof_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
    } else if (cc->compiler & POOF_CC_MSVC) {
        poof_cmd_append(&cmd, "cl");
    } else {
        poof_cmd_append(&cmd, "gcc");
    }

    bool is_msvc = (cc->compiler & POOF_CC_MSVC) != 0;
    bool compile_only = false;
    const Poof_Cmd *plat_cmd = NULL;
    if (target_platform == POOF_TARGET_WIN32) plat_cmd = &cc->win32_flags;
    else if (target_platform == POOF_TARGET_LINUX) plat_cmd = &cc->linux_flags;
    else if (target_platform == POOF_TARGET_MACOS) plat_cmd = &cc->macos_flags;
    const Poof_Cmd *flags[] = {&cc->extra_flags, plat_cmd};
    for (size_t i = 0; i < 2; ++i) {
        if (!flags[i]) continue;
        for (size_t j = 0; j < flags[i]->count; ++j) {
            if (!strcmp(flags[i]->items[j], is_msvc ? "/c" : "-c")) compile_only = true;
        }
    }

    // Debug flags
    if (cc->debug_mode) {
        poof_cmd_append(&cmd, is_msvc ? "/Zi" : "-g");
    }

    // Optimization flags
    uint32_t opt_level = cc->optimization & 3;
    if (opt_level == POOF_O1) poof_cmd_append(&cmd, is_msvc ? "/O1" : "-O1");
    else if (opt_level == POOF_O2) poof_cmd_append(&cmd, is_msvc ? "/O2" : "-O2");
    else if (opt_level == POOF_O3) poof_cmd_append(&cmd, is_msvc ? "/O2" : "-O3");
    else poof_cmd_append(&cmd, is_msvc ? "/Od" : "-O0");

    // SIMD / Arch extensions
    if (POOF_IS_SET(cc->optimization, POOF_MSSE)) poof_cmd_append(&cmd, is_msvc ? "/arch:SSE" : "-msse");
    if (POOF_IS_SET(cc->optimization, POOF_MSSE2)) poof_cmd_append(&cmd, is_msvc ? "/arch:SSE2" : "-msse2");
    if (POOF_IS_SET(cc->optimization, POOF_AVX)) poof_cmd_append(&cmd, is_msvc ? "/arch:AVX" : "-mavx");
    if (POOF_IS_SET(cc->optimization, POOF_AVX2)) poof_cmd_append(&cmd, is_msvc ? "/arch:AVX2" : "-mavx2");
    if (POOF_IS_SET(cc->optimization, POOF_AVX512F)) poof_cmd_append(&cmd, is_msvc ? "/arch:AVX512" : "-mavx512f");
    if (POOF_IS_SET(cc->optimization, POOF_BMI) && !is_msvc) poof_cmd_append(&cmd, "-mbmi");

    // Defines
    for (size_t i = 0; i < cc->defines.count; ++i) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s%s", is_msvc ? "/D" : "-D", cc->defines.items[i]);
        poof_cmd_append(&cmd, strdup(buf));
    }

    // Include dirs
    for (size_t i = 0; i < cc->includes.count; ++i) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s%s", is_msvc ? "/I" : "-I", cc->includes.items[i]);
        poof_cmd_append(&cmd, strdup(buf));
    }

    /* AppKit is ObjC. -x is positional and must precede the source. */
    if (target_platform == POOF_TARGET_MACOS && !is_msvc) {
        poof_cmd_append(&cmd, "-x");
        poof_cmd_append(&cmd, "objective-c");
    }

    // Input files
    for (size_t i = 0; i < cc->inputs.count; ++i) {
        poof_cmd_append(&cmd, cc->inputs.items[i]);
    }

    // Output target
    if (cc->output) {
        char out_path[512];
        if (target_platform == POOF_TARGET_WIN32) {
            if (compile_only) {
                snprintf(out_path, sizeof(out_path), multi_target ? "%s_win32" : "%s", cc->output);
            } else if (strstr(cc->output, ".exe")) {
                snprintf(out_path, sizeof(out_path), "%s", cc->output);
            } else {
                snprintf(out_path, sizeof(out_path), multi_target ? "%s_win32.exe" : "%s.exe", cc->output);
            }
        } else if (target_platform == POOF_TARGET_MACOS) {
            snprintf(out_path, sizeof(out_path), multi_target ? "%s_macos" : "%s", cc->output);
        } else if (target_platform == POOF_TARGET_LINUX) {
            snprintf(out_path, sizeof(out_path), multi_target ? "%s_linux" : "%s", cc->output);
        } else {
            snprintf(out_path, sizeof(out_path), "%s", cc->output);
        }

        if (is_msvc) {
            char buf[512];
            snprintf(buf, sizeof(buf), compile_only ? "/Fo:%s" : "/Fe:%s", out_path);
            poof_cmd_append(&cmd, strdup(buf));
        } else {
            poof_cmd_append(&cmd, "-o");
            poof_cmd_append(&cmd, strdup(out_path));
        }
    }

    // Lib paths & libs
    for (size_t i = 0; i < cc->lib_paths.count; ++i) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s%s", is_msvc ? "/LIBPATH:" : "-L", cc->lib_paths.items[i]);
        poof_cmd_append(&cmd, strdup(buf));
    }

    for (size_t i = 0; i < cc->libs.count; ++i) {
        if (is_msvc) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s.lib", cc->libs.items[i]);
            poof_cmd_append(&cmd, strdup(buf));
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "-l%s", cc->libs.items[i]);
            poof_cmd_append(&cmd, strdup(buf));
        }
    }

    // Extra flags
    for (size_t i = 0; i < cc->extra_flags.count; ++i) {
        poof_cmd_append(&cmd, cc->extra_flags.items[i]);
    }

    // Platform specific flags
    if (plat_cmd) {
        for (size_t i = 0; i < plat_cmd->count; ++i) {
            poof_cmd_append(&cmd, plat_cmd->items[i]);
        }
    }

    return cmd;
}

static void
poof_batch_append_cc(Poof_Batch *batch, Poof_CC *cc)
{
    uint8_t mask = cc->target_platform;
    if (mask == 0) {
#if defined(_WIN32)
        mask = POOF_TARGET_WIN32;
#elif defined(__APPLE__)
        mask = POOF_TARGET_MACOS;
#else
        mask = POOF_TARGET_LINUX;
#endif
    }

    uint8_t targets[3] = { POOF_TARGET_WIN32, POOF_TARGET_LINUX, POOF_TARGET_MACOS };
    int target_count = 0;
    for (int i = 0; i < 3; ++i) {
        if (mask & targets[i]) target_count++;
    }

    bool multi_target = target_count > 1;

    for (int i = 0; i < 3; ++i) {
        if (mask & targets[i]) {
            Poof_Cmd cmd = poof__cc_build_cmd(cc, targets[i], multi_target);
            poof_batch_append_cmd(batch, cmd);
        }
    }

    poof_cc_free(cc);
}

static void
poof_batch_free(Poof_Batch *batch)
{
    if (batch->cmds) {
        for (size_t i = 0; i < batch->count; ++i) {
            poof_cmd_free(&batch->cmds[i]);
        }
        free(batch->cmds);
        batch->cmds = NULL;
    }
    batch->capacity = 0;
    batch->count = 0;
}

static bool
poof_batch_run_parallel(Poof_Batch *batch, const char *label, size_t max_jobs)
{
    if (!batch || batch->count == 0) return true;
    if (!label) label = "BATCH";

#if defined(_WIN32)
    if (max_jobs == 0) {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        max_jobs = sysinfo.dwNumberOfProcessors > 0 ? sysinfo.dwNumberOfProcessors : 4;
    }
    if (max_jobs > MAXIMUM_WAIT_OBJECTS) max_jobs = MAXIMUM_WAIT_OBJECTS;

    size_t total = batch->count;
    size_t next_job = 0;
    size_t completed = 0;
    bool success = true;

    HANDLE *handles = (HANDLE *)calloc(max_jobs, sizeof(HANDLE));
    if (!handles) return false;
    size_t active_jobs = 0;

    poof_progress_bar(label, 0.0f, 0.0f, (float)total, 30.0f);

    while (completed < total) {
        while (active_jobs < max_jobs && next_job < total) {
            char *cmdline = poof__cmd_to_string(&batch->cmds[next_job]);
            if (cmdline) {
                char win_cmd[1024];
                snprintf(win_cmd, sizeof(win_cmd), "cmd.exe /c \"%s\"", cmdline);

                STARTUPINFOA si = { sizeof(si) };
                PROCESS_INFORMATION pi = {0};

                if (CreateProcessA(NULL, win_cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hThread);
                    handles[active_jobs++] = pi.hProcess;
                } else {
                    success = false;
                }
                free(cmdline);
            }
            next_job++;
        }

        if (active_jobs == 0) break;

        DWORD dwWait = WaitForMultipleObjects((DWORD)active_jobs, handles, FALSE, INFINITE);
        if (dwWait >= WAIT_OBJECT_0 && dwWait < WAIT_OBJECT_0 + active_jobs) {
            DWORD idx = dwWait - WAIT_OBJECT_0;
            DWORD exit_code = 0;
            GetExitCodeProcess(handles[idx], &exit_code);
            if (exit_code != 0) success = false;
            CloseHandle(handles[idx]);

            for (size_t i = idx; i < active_jobs - 1; ++i) {
                handles[i] = handles[i + 1];
            }
            active_jobs--;
            completed++;
            poof_progress_bar(label, (float)completed, 0.0f, (float)total, 30.0f);
        } else {
            break;
        }
    }

    free(handles);
    poof_batch_free(batch);
    return success;
#else
    if (max_jobs == 0) {
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        max_jobs = (nprocs > 0) ? (size_t)nprocs : 4;
    }

    size_t total = batch->count;
    size_t next_job = 0;
    size_t active_jobs = 0;
    size_t completed = 0;
    bool success = true;

    poof_progress_bar(label, 0.0f, 0.0f, (float)total, 30.0f);

    while (completed < total) {
        while (active_jobs < max_jobs && next_job < total) {
            char *cmdline = poof__cmd_to_string(&batch->cmds[next_job]);
            if (cmdline) {
                pid_t pid = fork();
                if (pid == 0) {
                    execl("/bin/sh", "sh", "-c", cmdline, (char *)NULL);
                    _exit(127);
                } else if (pid > 0) {
                    active_jobs++;
                } else {
                    success = false;
                }
                free(cmdline);
            }
            next_job++;
        }

        if (active_jobs == 0) break;

        int status = 0;
        pid_t done_pid = wait(&status);
        if (done_pid > 0) {
            active_jobs--;
            completed++;
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                success = false;
            }
            poof_progress_bar(label, (float)completed, 0.0f, (float)total, 30.0f);
        } else if (done_pid < 0 && errno == ECHILD) {
            break;
        }
    }

    poof_batch_free(batch);
    return success;
#endif
}

static bool
poof_batch_run(Poof_Batch *batch, const char *label)
{
    return poof_batch_run_parallel(batch, label, 0);
}

static bool
poof__cc_run_single(const Poof_CC *cc, uint8_t target_platform, bool multi_target)
{
    Poof_Cmd cmd = poof__cc_build_cmd(cc, target_platform, multi_target);
    bool result = poof_cmd_run(&cmd);
    poof_cmd_free(&cmd);
    return result;
}

static bool
poof_cc_run(Poof_CC *cc)
{
    uint8_t mask = cc->target_platform;
    if (mask == 0) {
#if defined(_WIN32)
        mask = POOF_TARGET_WIN32;
#elif defined(__APPLE__)
        mask = POOF_TARGET_MACOS;
#else
        mask = POOF_TARGET_LINUX;
#endif
    }

    uint8_t targets[3] = { POOF_TARGET_WIN32, POOF_TARGET_LINUX, POOF_TARGET_MACOS };
    int target_count = 0;
    for (int i = 0; i < 3; ++i) {
        if (mask & targets[i]) target_count++;
    }

    bool multi_target = target_count > 1;
    bool success = true;

    for (int i = 0; i < 3; ++i) {
        if (mask & targets[i]) {
            if (!poof__cc_run_single(cc, targets[i], multi_target)) {
                success = false;
            }
        }
    }

    poof_cc_free(cc);
    return success;
}


static bool
poof_mkdir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

static bool
poof_touch(const char *path)
{
    FILE *f = fopen(path, "a");
    if (!f) return false;
    fclose(f);
    return true;
}

static bool
poof_rm(const char *path)
{
    return remove(path) == 0;
}

static bool
poof_rm_recursive(const char *path)
{
#if defined(_WIN32)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", path);
    return system(cmd) == 0;
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
    return system(cmd) == 0;
#endif
}

static bool
poof_copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        fwrite(buffer, 1, bytes, out);
    }

    fclose(in);
    fclose(out);
    return true;
}

static time_t
poof__get_mtime(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) != 0) return 0;
    return attr.st_mtime;
}

static bool
poof_needs_rebuild(const char *target, const char **sources, size_t source_count)
{
    time_t target_mtime = poof__get_mtime(target);
    if (target_mtime == 0) return true; // Target does not exist

    for (size_t i = 0; i < source_count; ++i) {
        time_t src_mtime = poof__get_mtime(sources[i]);
        if (src_mtime > target_mtime) return true;
    }
    return false;
}

static int
poof_print(uint32_t col, const char *text, ...)
{
    uint8_t r = (col >> 16) & 0xFF;
    uint8_t g = (col >> 8) & 0xFF;
    uint8_t b = col & 0xFF;

    int color = POOF_ISATTY(1);

    if (color)
        printf("\033[38;2;%u;%u;%um", r, g, b);

    va_list args;
    va_start(args, text);
    int res = vprintf(text, args);
    va_end(args);

    if (color)
        printf("\033[0m");

    fflush(stdout);
    return res;
}

static int
poof_progress_bar(const char *label, float value, float min, float max, float width)
{
    float norm = (value - min) / (max - min);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    int filled = (int)(norm * width);
    int total = (int)width;

    char bar[256];
    int pos = 0;
    for (int i = 0; i < total && pos < (int)sizeof(bar) - 1; ++i) {
        if (i < filled) bar[pos++] = '|';
        else bar[pos++] = ' ';
    }
    bar[pos] = '\0';

    printf("\r[%s] [", label ? label : "PROGRESS");
    poof_print(0xFE9900, "%s", bar);
    printf("] %d/%d jobs", (int)value, (int)max);
    if (norm >= 1.0f) printf("\n");
    fflush(stdout);
    return 0;
}

static void
poof_go_rebuild_urself_impl(int argc, char **argv, const char *source_file)
{
    (void)argc;
    const char *binary_file = argv[0];
    const char *sources[4];
    size_t n = 0;
    size_t i;
    static const char *hdr[] = { "poof.h", "Poof/poof.h", "godstack/Poof/poof.h" };

    sources[n++] = source_file;
    for (i = 0; i < 3; i++) {
        if (poof__get_mtime(hdr[i]))
            sources[n++] = hdr[i];
    }

    if (!poof_needs_rebuild(binary_file, sources, n)) {
        return;
    }

    char binary_old[4096];
    snprintf(binary_old, sizeof(binary_old), "%s.old", binary_file);

    poof_print(0xFF8800, "[POOF] Source changed. Rebuilding %s...\n", binary_file);

    poof_rm(binary_old);
    rename(binary_file, binary_old);

    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "gcc", "-o", binary_file, source_file);

    if (!poof_cmd_run(&cmd)) {
        poof_print(0xFF0000, "[POOF] Rebuild failed!\n");
        rename(binary_old, binary_file);
        exit(1);
    }
    poof_cmd_free(&cmd);

    poof_print(0x00FF00, "[POOF] Rebuild successful! Re-launching binary...\n");

    #if defined(_WIN32)
        // Windows restart
        int status = system(binary_file);
        exit(status);
    #else
        execv(binary_file, argv);
        perror("execv");
        exit(1);
    #endif
}

/*
------------------------------------------------------------------------------
MIT License
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
*/
