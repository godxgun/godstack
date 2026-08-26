#include "Poof/poof.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
add_peak_config(Poof_CC *cc)
{
    poof_cmd_append(&cc->includes, "Peak");

#if defined(_WIN32)
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
    poof_cc_append_linux(cc, "-lvulkan");
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
    poof_cmd_append(&cc.includes, ".", "Rend", "Peak", "Fuse", "Grit");
    poof_cmd_append(&cc.defines, "PEAK_VULKAN");
    if (define) poof_cmd_append(&cc.defines, define);
    poof_cmd_append(&cc.libs, "m");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    add_peak_config(&cc);
    poof_batch_append_cc(batch, &cc);
}

static bool
build_peak_native(const char *src, const char *out)
{
    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = out;
    poof_cmd_append(&cc.inputs, src);
    poof_cmd_append(&cc.includes, "Peak");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    return poof_cc_run(&cc);
}

static bool
build_peak_demos(void)
{
    poof_mkdir("demos/multiplatform");
    if (!build_peak_native("demos/multiplatform/demo.c", "demos/multiplatform/demo")) return false;
    if (!build_peak_native("demos/multiplatform/demo_run.c", "demos/multiplatform/demo_run")) return false;

    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "emcc", "demos/multiplatform/demo_run.c", "-o", "demos/multiplatform/demo.js",
        "-IPeak", "-std=c99", "-Wall", "-Wno-deprecated-declarations",
        "-sALLOW_MEMORY_GROWTH=1", "-sENVIRONMENT=web");
    return poof_cmd_run(&cmd);
}

static bool
build_fuse_test(void)
{
    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "Fuse/fuse_test";
    poof_cmd_append(&cc.inputs, "Fuse/fuse_test.c");
    poof_cmd_append(&cc.includes, "Fuse");
    poof_cmd_append(&cc.defines, "FUSE_DEBUG");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_grit_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/grit");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/grit/demo";
    poof_cmd_append(&cc.inputs, "demos/grit/demo.c");
    poof_cmd_append(&cc.includes, ".", "Grit");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_cool_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/doc-generator");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/doc-generator/doc_generator";
    poof_cmd_append(&cc.inputs, "demos/doc-generator/doc_generator.c");
    poof_cmd_append(&cc.includes, ".", "Cool");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_term_demo(void)
{
    Poof_CC cc = {0};
    poof_mkdir("demos/term");
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/term/demo";
    poof_cmd_append(&cc.inputs, "demos/term/demo.c");
    poof_cmd_append(&cc.includes, ".", "Term");
    poof_cmd_append(&cc.extra_flags, "-std=c99", "-Wall", "-Werror");
    return poof_cc_run(&cc);
}

static bool
build_rend_demos(void)
{
    poof_mkdir("demos/compute");
    poof_mkdir("demos/teapot");
    poof_mkdir("demos/snake");
    poof_mkdir("demos/dashboard");

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
    add_rend_demo(&batch, "demos/teapot/rend_teapot.c", "demos/teapot/rend_teapot", POOF_O0, "REND_DEBUG");

    slangc_entry(&batch, "demos/snake/shaders/snake.slang", "vertMain", "vertex", "demos/snake/snake.vert.spv");
    slangc_entry(&batch, "demos/snake/shaders/snake.slang", "fragMain", "fragment", "demos/snake/snake.frag.spv");
    add_rend_demo(&batch, "demos/snake/snake.c", "demos/snake/snake", POOF_O0, "REND_DEBUG");

    slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "vertMain", "vertex", "demos/dashboard/fuse_ui.vert.spv");
    slangc_entry(&batch, "demos/dashboard/fuse_ui.slang", "fragMain", "fragment", "demos/dashboard/fuse_ui.frag.spv");
    add_rend_demo(&batch, "demos/dashboard/dashboard.c", "demos/dashboard/dashboard", POOF_O0, "REND_DEBUG");

    return poof_batch_run(&batch, "Rend Demos");
}

static bool
run_one(Poof_Cmd *cmd)
{
    bool ok;
    ok = poof_cmd_run(cmd);
    poof_cmd_free(cmd);
    return ok;
}

static bool
run_tests(void)
{
    Poof_Cmd cmd;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./Fuse/fuse_test");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/grit/demo");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/term/demo");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "sh", "-c", "./demos/doc-generator/doc_generator Cool/cool.h >/dev/null");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/compute/rend_compute_demo", "--headless");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/teapot/rend_teapot", "--headless");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/snake/snake", "--headless");
    if (!run_one(&cmd)) return false;

    cmd = (Poof_Cmd){0};
    poof_cmd_append(&cmd, "./demos/dashboard/dashboard", "--headless");
    if (!run_one(&cmd)) return false;

    return true;
}

int
main(int argc, char **argv)
{
    int test;
    int i;

    POOF_GO_REBUILD_URSELF(argc, argv);

    test = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "test") == 0)
            test = 1;
    }

    if (!build_peak_demos()) return 1;
    if (!build_fuse_test()) return 1;
    if (!build_grit_demo()) return 1;
    if (!build_term_demo()) return 1;
    if (!build_cool_demo()) return 1;
    if (!build_rend_demos()) return 1;
    if (test && !run_tests()) return 1;
    return 0;
}
