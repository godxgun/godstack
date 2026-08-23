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

#define POOF_MAJOR 0
#define POOF_MINOR 1
#define POOF_PATCH 0

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

#ifdef POOF_IMPLEMENTATION
#undef POOF_IMPLEMENTATION

#if defined(_WIN32)
#define POOF_DEV_NULL "nul 2>&1"
#else
#define POOF_DEV_NULL "/dev/null 2>&1"
#endif

static bool
poof__probe_cmd(const char *cmd) 
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s > " POOF_DEV_NULL, cmd);
    return (system(buffer) == 0);
}

extern uint8_t
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

extern void
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


extern void
poof_cmd_clear(Poof_Cmd *cmd)
{
    /* clear count while keeping capacity */
    cmd->count = 0;
}

extern void
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

extern bool
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

extern void
poof_cc_init(Poof_CC *cc, uint8_t compiler, uint8_t target_platform)
{
    memset(cc, 0, sizeof(Poof_CC));
    cc->compiler = compiler;
    cc->target_platform = target_platform;
    cc->optimization = POOF_O0;
    cc->debug_mode = true;
}

extern void
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

extern void
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

    // Input files
    for (size_t i = 0; i < cc->inputs.count; ++i) {
        poof_cmd_append(&cmd, cc->inputs.items[i]);
    }

    // Output target
    if (cc->output) {
        char out_path[512];
        if (target_platform == POOF_TARGET_WIN32) {
            if (strstr(cc->output, ".exe")) {
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
            snprintf(buf, sizeof(buf), "/Fe:%s", out_path);
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
    const Poof_Cmd *plat_cmd = NULL;
    if (target_platform == POOF_TARGET_WIN32) plat_cmd = &cc->win32_flags;
    else if (target_platform == POOF_TARGET_LINUX) plat_cmd = &cc->linux_flags;
    else if (target_platform == POOF_TARGET_MACOS) plat_cmd = &cc->macos_flags;

    if (plat_cmd) {
        for (size_t i = 0; i < plat_cmd->count; ++i) {
            poof_cmd_append(&cmd, plat_cmd->items[i]);
        }
    }

    return cmd;
}

extern void
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

extern void
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

extern bool
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

extern bool
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

extern bool
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


extern bool
poof_mkdir(const char *path)
{
#if defined(_WIN32)
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

extern bool
poof_touch(const char *path)
{
    FILE *f = fopen(path, "a");
    if (!f) return false;
    fclose(f);
    return true;
}

extern bool
poof_rm(const char *path)
{
    return remove(path) == 0;
}

extern bool
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

extern bool
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

extern bool
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

extern int
poof_print(uint32_t col, const char *text, ...)
{
    uint8_t r = (col >> 16) & 0xFF;
    uint8_t g = (col >> 8) & 0xFF;
    uint8_t b = col & 0xFF;

    printf("\033[38;2;%d;%d;%dm", r, g, b);

    va_list args;
    va_start(args, text);
    int res = vprintf(text, args);
    va_end(args);

    printf("\033[0m");
    fflush(stdout);
    return res;
}

extern int
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

extern void
poof_go_rebuild_urself_impl(int argc, char **argv, const char *source_file)
{
    const char *binary_file = argv[0];
    const char *sources[] = { source_file, "Poof/poof.h" };

    if (!poof_needs_rebuild(binary_file, sources, 2)) {
        return;
    }

    char binary_old[4096];
    snprintf(binary_old, sizeof(binary_old), "%s.old", binary_file);

    poof_print(0xFF8800, "[POOF] Source changed. Rebuilding %s...\n", binary_file);

    poof_rm(binary_old);
    rename(binary_file, binary_old);

    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "gcc", "-o", binary_file, source_file, "-IPodium", "-IPoof");

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
#endif 

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
