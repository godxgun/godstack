#ifndef FUSE_BACKEND_H
#define FUSE_BACKEND_H

typedef enum {
    FUSE_CMD_CLIP = 0,
    FUSE_CMD_RECT,
    FUSE_CMD_LINE,
    FUSE_CMD_COUNT
} FuseCmdType;

typedef struct { 
    float x, y, w, h; 
} FuseClip;

typedef struct { 
    float x, y, w, h; 
    uint32_t color; 
} FuseRect;

typedef struct { 
    float x1, y1, x2, y2; 
    float thickness; 
    uint32_t color; 
} FuseLine;

typedef struct {
    union {
        FuseClip clip;
        FuseRect rect;
        FuseLine line;
    };
    uint8_t type;
} FuseCmd;


static const char* fuse_cmd_name[] = {
    [FUSE_CMD_RECT] = "RECT",
    [FUSE_CMD_CLIP] = "CLIP",
    [FUSE_CMD_LINE] = "LINE",
};

#endif
