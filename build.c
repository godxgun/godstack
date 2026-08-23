#define POOF_IMPLEMENTATION
#include "../Poof/poof.h"

void Peak() {
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "python", "bundle_header.py", "src/peak_header.h");
    poof_cmd_run(&cmd);

    poof_copy_file("Peak/src/peak_header.h.out", "Peak/peak.h");

    Poof_CC cc = {0};
    poof_cc_init(&cc, POOF_CC_GCC | POOF_CC_CLANG, POOF_TARGET_HOST);
    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "Peak/demos/demo";
    poof_cc_append(&cc.inputs, "Peak/demos/demo.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    poof_cc_append(&cc.libs, "m");
    poof_cc_run(&cc);

    cc.debug_mode = true;
    cc.optimization = POOF_O0;
    cc.output = "Peak/demos/demo_main";
    poof_cc_append(&cc.inputs, "Peak/demos/demo_replace_main.c");
    poof_cc_append(&cc.extra_flags, "-std=c99", "-Wall", "-Wno-deprecated-declarations");
    poof_cc_append(&cc.libs, "m");
    poof_cc_run(&cc);
}

int main(int argc, char **argv) {
    POOF_GO_REBUILD_URSELF(argc, argv);
    Peak();
}
