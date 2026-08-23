#ifndef FUSE_INTERNAL_H
#define FUSE_INTERNAL_H

#include "fuse.h"
#include "fuse_backend.h"

#define FUSE_TODO \
    do { \
        fprintf(stderr, "FUSE TODO: %s() in %s:%d\n", __func__, __FILE__, __LINE__); \
        abort(); \
    } while(0)


typedef struct {
    void *data;
    size_t idk;
} FuseHash;

struct fuse_canvas_t {
    void *memory; // memory pointer to be slit into separate arenas
    Arena arena; // arena the manages all our memory and will be split into smaller arenas

    FuseCmd *cmdbuffer_ptr;

    Arena element_data_arena;
    FuseHash element_data; // persistent element data

    size_t cmdbuffer_head;
    size_t cmdbuffer_size;

    size_t hot;
    size_t active;

    size_t current_frame;
    size_t current_id;

    float width;
    float height;

    float pointer_x;
    float pointer_y;

    uint64_t current_time_ns;

    int  pointer_state;

    bool debug;
};

struct fuse_font_t {
    int x;
};

struct fuse_style_t {
    int x;
};
struct fuse_window_t {
    int x;
};

static FuseCanvas f__canvas;

#endif
