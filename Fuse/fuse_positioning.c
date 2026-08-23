#pragma once
#include "fuse_internal.h"

extern float
fuse_percent_x(float p)
{
    return p * f__canvas->width / 100;
}

extern float
fuse_percent_y(float p)
{
    return p * f__canvas->height / 100;
}
