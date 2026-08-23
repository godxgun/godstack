#define FUSE_DEBUG
#include "fuse.h"
#include "fuse.c"

#include "allocators.c"
#include <stdio.h>

float nob = 0.5;
int main() {

    FuseCanvas canvas = fuse_canvas_create(1024, NULL); /* lowk 1kb enough? */

    fuse_canvas_clear(canvas, 400, 400, FUSE_POINTER_RELEASED, 50, 50);

    if (fuse_button(0, 0, 100, 100, 0xFF00FF, 0xFF0000)) {
        printf("pressed\n");
    }

    fuse_slider(0, 0, 100, 100, 0xFF00FF, 0xFF0000, &nob);

    fuse__cmdbuffer_print();

    fuse_canvas_destroy(canvas);
    return 0;
}
