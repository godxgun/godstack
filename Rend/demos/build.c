#define POOF_IMPLEMENTATION
#include "poof.h"

void add_podium_config(Poof_CC *cc) {
    poof_cmd_append(&cc->includes, "Podium");

#if defined(PODIUM_WIN32)
    poof_cc_append_win32(cc, "-DPODIUM_WIN32", "-luser32", "-lgdi32");
    const char *vulkan_sdk = getenv("VULKAN_SDK");
    if (vulkan_sdk) {
        char inc_buf[512], lib_buf[512];
        snprintf(inc_buf, sizeof(inc_buf), "-I%s/Include", vulkan_sdk);
        snprintf(lib_buf, sizeof(lib_buf), "-L%s/Lib", vulkan_sdk);
        poof_cc_append_win32(cc, strdup(inc_buf), strdup(lib_buf));
    }
    poof_cc_append_win32(cc, "-lvulkan-1");
#else
    poof_cc_append_linux(cc, "-lX11", "-D_REENTRANT", "-lpulse", "-lpulse-simple", "-pthread", "-lvulkan");
#endif
}

int main(int argc, char **argv) {
    POOF_GO_REBUILD_URSELF(argc, argv);

    Poof_Batch batch = {0};

    {
        Poof_Cmd cmd = {0};
        poof_cmd_append(&cmd, "slangc", "rend_compute.slang", "-target", "spirv", "-entry", "particleMain", "-stage", "compute", "-o", "compute/particle.spv");
        poof_batch_append_cmd(&batch, cmd);
    }

    {
        Poof_Cmd cmd = {0};
        poof_cmd_append(&cmd, "slangc", "rend_compute.slang", "-target", "spirv", "-entry", "sandMain", "-stage", "compute", "-o", "compute/sand.spv");
        poof_batch_append_cmd(&batch, cmd);
    }

    {
        Poof_Cmd cmd = {0};
        poof_cmd_append(&cmd, "slangc", "rend_compute.slang", "-target", "spirv", "-entry", "vertMain", "-stage", "vertex", "-o", "compute/vert.spv");
        poof_batch_append_cmd(&batch, cmd);
    }

    {
        Poof_Cmd cmd = {0};
        poof_cmd_append(&cmd, "slangc", "rend_compute.slang", "-target", "spirv", "-entry", "fragMain", "-stage", "fragment", "-o", "compute/frag.spv");
        poof_batch_append_cmd(&batch, cmd);
    }

    // Debug build
    {
        Poof_CC cc;
        poof_cc_init(&cc, POOF_CC_GCC, POOF_TARGET_HOST);
        cc.debug_mode = true;
        cc.optimization = POOF_O0;
        cc.output = "compute/rend_compute_demo";
        poof_cmd_append(&cc.inputs, "rend_compute.c");
        poof_cmd_append(&cc.includes, "..", ".");
        poof_cmd_append(&cc.defines, "REND_DEBUG");
        poof_cmd_append(&cc.libs, "m");
        poof_cmd_append(&cc.extra_flags, "-ggdb");
        add_podium_config(&cc);

        poof_batch_append_cc(&batch, &cc);
    }

    // Debug build
    {
        Poof_CC cc;
        poof_cc_init(&cc, POOF_CC_GCC, POOF_TARGET_HOST);
        cc.debug_mode = true;
        cc.optimization = POOF_O3 | POOF_MSSE2;
        cc.output = "compute/rend_compute_fast";
        poof_cmd_append(&cc.inputs, "rend_compute.c");
        poof_cmd_append(&cc.includes, "..");
        poof_cmd_append(&cc.libs, "m");
        add_podium_config(&cc);

        poof_batch_append_cc(&batch, &cc);
    }

    poof_batch_run(&batch, "Rend Demos");
    return 0;
}
