#define POOF_IMPLEMENTATION
#include "../Poof/poof.h"

int main(int argc, char **argv) {
    POOF_GO_REBUILD_URSELF(argc, argv);
    
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "python", "../bundle_header.py", "src/peak_header.h");
    if (!poof_cmd_run(&cmd)) return 1;

    poof_copy_file("src/peak_header.h.out", "peak.h");

    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/demo";
    poof_cc_append(&cc.inputs, "demos/demo.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    poof_cc_append(&cc.libs, "m");
    if (!poof_cc_run(&cc)) return 1;

    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "demos/demo_run";
    poof_cc_append(&cc.inputs, "demos/demo_run.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    poof_cc_append(&cc.libs, "m");
    if (!poof_cc_run(&cc)) return 1;

    poof_cmd_clear(&cmd);
    poof_cmd_append(&cmd, "emcc", "demos/demo_run.c", "-o", "demos/demo.js",
        "-std=c99", "-Wall", "-Wno-deprecated-declarations",
        "-sALLOW_MEMORY_GROWTH=1", "-sENVIRONMENT=web");
    if (!poof_cmd_run(&cmd)) return 1;

    return 0;
}

