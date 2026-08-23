#pragma once

#include "fuse.h"
#include "fuse_internal.h"

static inline float
fuse__get_coef(FuseAnimation anim, float t)
{
    switch (anim) {
        case FUSE_ANIMATION_EASE_IN:  return t * t;
        case FUSE_ANIMATION_EASE_OUT: return t * (2.0f - t);
        case FUSE_ANIMATION_LINEAR:   
        default:                      return t;
    }
}

static inline float
fuse__ping_pong(float duration_sec, uint64_t time_ns)
{
    if (duration_sec <= 0.0f) return 1.0f;
    
    uint64_t duration_ns = (uint64_t)(duration_sec * 1000000000.0f);
    
    // modulo against DOUBLE the duration to handle reversing
    uint64_t loop_time_ns = time_ns % (duration_ns * 2);
    
    // in the second half REVERSE the clock direction
    if (loop_time_ns > duration_ns) {
        loop_time_ns = (duration_ns * 2) - loop_time_ns;
    }
    
    return (float)loop_time_ns / (float)duration_ns;   
}

extern float
fuse_tween_f(FuseAnimation anim, float f1, float f2, float duration, uint64_t time_ns)
{
    float t = fuse__ping_pong(duration, time_ns);
    float coef = fuse__get_coef(anim, t);
    return f1 + coef * (f2 - f1);
}

extern int
fuse_tween_i(FuseAnimation anim, int i1, int i2, float duration, uint64_t time_ns)
{
    float t = fuse__ping_pong(duration, time_ns);
    float coef = fuse__get_coef(anim, t);
    return (int)(i1 + coef * (i2 - i1));
}

extern uint32_t
fuse_tween_color(FuseAnimation anim, uint32_t c1, uint32_t c2, float duration, uint64_t time_ns)
{
    float t = fuse__ping_pong(duration, time_ns);
    float coef = fuse__get_coef(anim, t);

    int r1 = (c1 >> 24) & 0xFF, r2 = (c2 >> 24) & 0xFF;
    int g1 = (c1 >> 16) & 0xFF, g2 = (c2 >> 16) & 0xFF;
    int b1 = (c1 >> 8)  & 0xFF, b2 = (c2 >> 8)  & 0xFF;
    int a1 =  c1        & 0xFF, a2 =  c2        & 0xFF;

    uint32_t r = (uint32_t)(r1 + coef * (r2 - r1));
    uint32_t g = (uint32_t)(g1 + coef * (g2 - g1));
    uint32_t b = (uint32_t)(b1 + coef * (b2 - b1));
    uint32_t a = (uint32_t)(a1 + coef * (a2 - a1));

    return (r << 24) | (g << 16) | (b << 8) | a;
}
