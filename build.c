#define POOF_IMPLEMENTATION
#include "Poof/poof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
add_peak_config(Poof_CC *cc)
{
    poof_cmd_append(&cc->includes, "Peak");

#if defined(_WIN32)
    poof_cc_append_win32(cc, "-luser32", "-lgdi32");
    {
        const char *vulkan_sdk = getenv("VULKAN_SDK");
        if (vulkan_sdk) {
            char inc_buf[512], lib_buf[512];
            snprintf(inc_buf, sizeof(inc_buf), "-I%s/Include", vulkan_sdk);
            snprintf(lib_buf, sizeof(lib_buf), "-L%s/Lib", vulkan_sdk);
            poof_cc_append_win32(cc, strdup(inc_buf), strdup(lib_buf));
        }
    }
    poof_cc_append_win32(cc, "-lvulkan-1");
#else
    poof_cc_append_linux(cc, "-pthread", "-ldl", "-lvulkan");
#endif
}

static void
slangc_entry(Poof_Batch *batch, const char *src, const char *entry, const char *stage, const char *out)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "slangc", src, "-target", "spirv", "-entry", entry, "-stage", stage, "-o", out);
    poof_batch_append_cmd(batch, cmd);
}

static void
add_rend_demo(Poof_Batch *batch, const char *src, const char *out, uint32_t opt, const char *define)
{
    Poof_CC cc;
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = opt;
    cc.output = out;
    poof_cmd_append(&cc.inputs, src);
    poof_cmd_append(&cc.includes, ".", "Rend", "Peak");
    poof_cmd_append(&cc.defines, "PEAK_VULKAN");
#if !defined(_WIN32)
    poof_cmd_append(&cc.defines, "_POSIX_C_SOURCE=200809L");
#endif
    if (define) poof_cmd_append(&cc.defines, define);
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    add_peak_config(&cc);
    poof_batch_append_cc(batch, &cc);
}

static bool
build_peak_header(void)
{
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "python", "bundle_header.py", "Peak/src/peak_header.h");
    if (!poof_cmd_run(&cmd)) return false;
    return poof_copy_file("Peak/src/peak_header.h.out", "Peak/peak.h");
}

static bool
build_peak_demos(void)
{
    poof_mkdir("demos/multiplatform");

    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/multiplatform/demo";
    poof_cc_append(&cc.inputs, "demos/multiplatform/demo.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations", "-pthread");
    poof_cc_append(&cc.libs, "m", "pthread");
    if (!poof_cc_run(&cc)) return false;

    poof_cc_free(&cc);
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/multiplatform/demo_run";
    poof_cc_append(&cc.inputs, "demos/multiplatform/demo_run.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations", "-pthread");
    poof_cc_append(&cc.libs, "m", "pthread");
    if (!poof_cc_run(&cc)) return false;

    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "emcc", "demos/multiplatform/demo_run.c", "-o", "demos/multiplatform/demo.js",
        "-std=c99", "-Wall", "-Wno-deprecated-declarations",
        "-sALLOW_MEMORY_GROWTH=1", "-sENVIRONMENT=web");
    return poof_cmd_run(&cmd);
}

static bool
build_rend_demos(void)
{
    poof_mkdir("demos/compute");
    poof_mkdir("demos/teapot");

    Poof_Batch batch = {0};

    slangc_entry(&batch, "demos/compute/rend_compute.slang", "particleMain", "compute", "demos/compute/particle.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "sandMain", "compute", "demos/compute/sand.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "vertMain", "vertex", "demos/compute/vert.spv");
    slangc_entry(&batch, "demos/compute/rend_compute.slang", "fragMain", "fragment", "demos/compute/frag.spv");

    slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "vertMain", "vertex", "demos/teapot/basic.vert.spv");
    slangc_entry(&batch, "demos/teapot/shaders/basic.slang", "fragMain", "fragment", "demos/teapot/basic.frag.spv");
    slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "vertMain", "vertex", "demos/teapot/texture.vert.spv");
    slangc_entry(&batch, "demos/teapot/shaders/texture.slang", "fragMain", "fragment", "demos/teapot/texture.frag.spv");

    add_rend_demo(&batch, "demos/compute/rend_compute.c", "demos/compute/rend_compute_demo", POOF_O0, "REND_DEBUG");
    // add_rend_demo(&batch, "demos/compute/rend_compute.c", "demos/compute/rend_compute_fast", POOF_O3 | POOF_MSSE2, NULL);
    add_rend_demo(&batch, "demos/teapot/rend_teapot.c", "demos/teapot/rend_teapot", POOF_O0, "REND_DEBUG");

    return poof_batch_run(&batch, "Rend Demos");
}

int
main(int argc, char **argv)
{
    POOF_GO_REBUILD_URSELF(argc, argv);

    if (!build_peak_header()) return 1;
    if (!build_peak_demos()) return 1;
    if (!build_rend_demos()) return 1;
    return 0;
}
