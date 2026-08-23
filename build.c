#define POOF_IMPLEMENTATION
#include "Poof/poof.h"

int main(int argc, char **argv) {
    POOF_GO_REBUILD_URSELF(argc, argv);
    
    Poof_Cmd cmd = {0};
    poof_cmd_append(&cmd, "./Peak/build");
    poof_cmd_run(&cmd);
    return 0;
}
