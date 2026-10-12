+++
title = 'Grit symbols'
description = 'Types, macros, and functions declared by Grit.h.'
weight = 70
[params]
  id = 'symbols/grit'
  product = 'Grit'
  category = 'Symbols'
  tags = ['API', 'symbols', 'C']
  access = 'free'
  symbols = true
  audience = 'Core'
+++

# Grit symbols

For setup and API workflows, read the [Grit guide](/docs/grit/). This vault lists individual declarations; it is not a tutorial.

<!-- BEGIN GENERATED API -->

## API reference

Generated from `Grit.h` by `tools/generate_docs.py`. Edit the header comments, not this section.

Declarations from all platform branches are listed; this is not a preprocessed API.

### Types

#### GritArena {#gritarena}

Source: `Grit.h:80`

```c
typedef struct GritArena {
    unsigned char *buf;
    size_t buf_len;
    size_t curr_offset;
    size_t prev_offset;
} GritArena;
```

#### GritStackHeader {#gritstackheader}

Source: `Grit.h:87`

```c
typedef struct GritStackHeader {
    uint8_t padding;
} GritStackHeader;
```

#### GritStack {#gritstack}

Source: `Grit.h:91`

```c
typedef struct GritStack {
    unsigned char *buf;
    size_t buf_len;
    size_t offset;
} GritStack;
```

#### GritPoolNode {#gritpoolnode}

Source: `Grit.h:97`

```c
typedef struct GritPoolNode GritPoolNode;
```

#### GritPool {#gritpool}

Source: `Grit.h:99`

```c
typedef struct GritPool {
    unsigned char *buf;
    size_t chunk_size;
    size_t pool_size;
    GritPoolNode *head;
} GritPool;
```

#### GritPoolNode {#gritpoolnode-1}

Source: `Grit.h:106`

```c
struct GritPoolNode {
    struct GritPoolNode *next;
};
```

#### GritDArray {#gritdarray}

Source: `Grit.h:110`

```c
typedef struct GritDArray {
    void *data;
    size_t len;
    size_t cap;
    size_t elem_size;
} GritDArray;
```

#### GritHashMap {#grithashmap}

Source: `Grit.h:117`

```c
typedef struct GritHashMap {
    void *keys;
    void *vals;
    uint8_t *state;
    size_t len;
    size_t cap;
    size_t tombed;
    size_t key_size;
    size_t val_size;
} GritHashMap;
```

#### GritDraw {#gritdraw}

Source: `Grit.h:128`

```c
typedef struct GritDraw {
    uint32_t *px;
    int w, h;
    int cx, cy, cw, ch;
} GritDraw;
```

#### GritRng {#gritrng}

Source: `Grit.h:134`

```c
typedef struct GritRng {
    uint64_t state;
} GritRng;
```

### Macros

#### GRIT_MIN {#grit_min}

Source: `Grit.h:77`

```c
#define GRIT_MIN(a, b) ((a) < (b) ? (a) : (b))
```

#### GRIT_MAX {#grit_max}

Source: `Grit.h:78`

```c
#define GRIT_MAX(a, b) ((a) > (b) ? (a) : (b))
```

### Functions

#### grit_arena_init {#grit_arena_init}

Source: `Grit.h:138`

```c
void grit_arena_init(GritArena *a, void *buf, size_t len);
```

#### grit_arena_alloc_align {#grit_arena_alloc_align}

Source: `Grit.h:139`

```c
void *grit_arena_alloc_align(GritArena *a, size_t size, size_t align);
```

#### grit_arena_alloc {#grit_arena_alloc}

Source: `Grit.h:140`

```c
void *grit_arena_alloc(GritArena *a, size_t size);
```

#### grit_arena_resize_align {#grit_arena_resize_align}

Source: `Grit.h:141`

```c
void *grit_arena_resize_align(GritArena *a, void *old, size_t old_size, size_t new_size, size_t align);
```

#### grit_arena_resize {#grit_arena_resize}

Source: `Grit.h:142`

```c
void *grit_arena_resize(GritArena *a, void *old, size_t old_size, size_t new_size);
```

#### grit_arena_free_all {#grit_arena_free_all}

Source: `Grit.h:143`

```c
void grit_arena_free_all(GritArena *a);
```

#### grit_arena_destroy {#grit_arena_destroy}

Source: `Grit.h:144`

```c
void grit_arena_destroy(GritArena *a);
```

#### grit_stack_init {#grit_stack_init}

Source: `Grit.h:146`

```c
void grit_stack_init(GritStack *s, void *buf, size_t len);
```

#### grit_stack_alloc {#grit_stack_alloc}

Source: `Grit.h:147`

```c
void *grit_stack_alloc(GritStack *s, size_t len, size_t alignment);
```

#### grit_stack_free {#grit_stack_free}

Source: `Grit.h:148`

```c
void grit_stack_free(GritStack *s, void *ptr);
```

#### grit_stack_free_all {#grit_stack_free_all}

Source: `Grit.h:149`

```c
void grit_stack_free_all(GritStack *s);
```

#### grit_stack_destroy {#grit_stack_destroy}

Source: `Grit.h:150`

```c
void grit_stack_destroy(GritStack *s);
```

#### grit_pool_init {#grit_pool_init}

Source: `Grit.h:152`

```c
void grit_pool_init(GritPool *p, void *buf, size_t buf_len, size_t chunk_size, size_t chunk_align);
```

#### grit_pool_alloc {#grit_pool_alloc}

Source: `Grit.h:153`

```c
void *grit_pool_alloc(GritPool *p);
```

#### grit_pool_free {#grit_pool_free}

Source: `Grit.h:154`

```c
void grit_pool_free(GritPool *p, void *ptr);
```

#### grit_pool_free_all {#grit_pool_free_all}

Source: `Grit.h:155`

```c
void grit_pool_free_all(GritPool *p);
```

#### grit_pool_destroy {#grit_pool_destroy}

Source: `Grit.h:156`

```c
void grit_pool_destroy(GritPool *p);
```

#### grit_darray_create {#grit_darray_create}

Source: `Grit.h:158`

```c
GritDArray grit_darray_create(size_t init_capacity, size_t elem_size);
```

#### grit_darray_destroy {#grit_darray_destroy}

Source: `Grit.h:159`

```c
void grit_darray_destroy(GritDArray *a);
```

#### grit_darray_push {#grit_darray_push}

Source: `Grit.h:160`

```c
void grit_darray_push(GritDArray *a, const void *elem);
```

#### grit_darray_get {#grit_darray_get}

Source: `Grit.h:161`

```c
void *grit_darray_get(GritDArray *a, size_t i);
```

#### grit_darray_pop {#grit_darray_pop}

Source: `Grit.h:162`

```c
void grit_darray_pop(GritDArray *a);
```

#### grit_hashmap_create {#grit_hashmap_create}

Source: `Grit.h:164`

```c
GritHashMap grit_hashmap_create(size_t init_capacity, size_t key_size, size_t val_size);
```

#### grit_hashmap_destroy {#grit_hashmap_destroy}

Source: `Grit.h:165`

```c
void grit_hashmap_destroy(GritHashMap *m);
```

#### grit_hashmap_put {#grit_hashmap_put}

Source: `Grit.h:166`

```c
void grit_hashmap_put(GritHashMap *m, const void *key, const void *val);
```

#### grit_hashmap_get {#grit_hashmap_get}

Source: `Grit.h:167`

```c
void *grit_hashmap_get(GritHashMap *m, const void *key);
```

#### grit_hashmap_del {#grit_hashmap_del}

Source: `Grit.h:168`

```c
void grit_hashmap_del(GritHashMap *m, const void *key);
```

#### grit_ceil {#grit_ceil}

Source: `Grit.h:170`

```c
void grit_ceil(float *x);
```

#### grit_sinf {#grit_sinf}

Source: `Grit.h:171`

```c
void grit_sinf(float *x);
```

#### grit_cosf {#grit_cosf}

Source: `Grit.h:172`

```c
void grit_cosf(float *x);
```

#### grit_atan2f {#grit_atan2f}

Source: `Grit.h:173`

```c
void grit_atan2f(float *y, float x);
```

#### grit_lerpf {#grit_lerpf}

Source: `Grit.h:174`

```c
void grit_lerpf(float *a, float b, float t);
```

#### grit_vec2f {#grit_vec2f}

Source: `Grit.h:176`

```c
void grit_vec2f(float *v, float x, float y);
```

#### grit_vec2f_add {#grit_vec2f_add}

Source: `Grit.h:177`

```c
void grit_vec2f_add(float *a, const float *b);
```

#### grit_vec2f_sub {#grit_vec2f_sub}

Source: `Grit.h:178`

```c
void grit_vec2f_sub(float *a, const float *b);
```

#### grit_vec2f_mult {#grit_vec2f_mult}

Source: `Grit.h:179`

```c
void grit_vec2f_mult(float *a, float s);
```

#### grit_vec2f_dot {#grit_vec2f_dot}

Source: `Grit.h:180`

```c
void grit_vec2f_dot(float *out, const float *a, const float *b);
```

#### grit_vec2f_cross {#grit_vec2f_cross}

Source: `Grit.h:181`

```c
void grit_vec2f_cross(float *out, const float *a, const float *b);
```

#### grit_vec2f_len {#grit_vec2f_len}

Source: `Grit.h:182`

```c
void grit_vec2f_len(float *out, const float *a);
```

#### grit_vec2f_norm {#grit_vec2f_norm}

Source: `Grit.h:183`

```c
void grit_vec2f_norm(float *a);
```

#### grit_vec2f_lerp {#grit_vec2f_lerp}

Source: `Grit.h:184`

```c
void grit_vec2f_lerp(float *a, const float *b, float t);
```

#### grit_vec3f {#grit_vec3f}

Source: `Grit.h:186`

```c
void grit_vec3f(float *v, float x, float y, float z);
```

#### grit_vec3f_add {#grit_vec3f_add}

Source: `Grit.h:187`

```c
void grit_vec3f_add(float *a, const float *b);
```

#### grit_vec3f_sub {#grit_vec3f_sub}

Source: `Grit.h:188`

```c
void grit_vec3f_sub(float *a, const float *b);
```

#### grit_vec3f_mult {#grit_vec3f_mult}

Source: `Grit.h:189`

```c
void grit_vec3f_mult(float *a, float s);
```

#### grit_vec3f_dot {#grit_vec3f_dot}

Source: `Grit.h:190`

```c
void grit_vec3f_dot(float *out, const float *a, const float *b);
```

#### grit_vec3f_cross {#grit_vec3f_cross}

Source: `Grit.h:191`

```c
void grit_vec3f_cross(float *a, const float *b);
```

#### grit_vec3f_len {#grit_vec3f_len}

Source: `Grit.h:192`

```c
void grit_vec3f_len(float *out, const float *a);
```

#### grit_vec3f_norm {#grit_vec3f_norm}

Source: `Grit.h:193`

```c
void grit_vec3f_norm(float *a);
```

#### grit_vec3f_lerp {#grit_vec3f_lerp}

Source: `Grit.h:194`

```c
void grit_vec3f_lerp(float *a, const float *b, float t);
```

#### grit_mat4_identity {#grit_mat4_identity}

Source: `Grit.h:196`

```c
void grit_mat4_identity(float *m);
```

#### grit_mat4_mul {#grit_mat4_mul}

Source: `Grit.h:197`

```c
void grit_mat4_mul(float *a, const float *b);
```

#### grit_mat4_translate {#grit_mat4_translate}

Source: `Grit.h:198`

```c
void grit_mat4_translate(float *m, const float *v);
```

#### grit_mat4_translate_by {#grit_mat4_translate_by}

Source: `Grit.h:199`

```c
void grit_mat4_translate_by(float *m, const float *v);
```

#### grit_mat4_rotate_x {#grit_mat4_rotate_x}

Source: `Grit.h:200`

```c
void grit_mat4_rotate_x(float *m, float rad);
```

#### grit_mat4_rotate_x_by {#grit_mat4_rotate_x_by}

Source: `Grit.h:201`

```c
void grit_mat4_rotate_x_by(float *m, float rad);
```

#### grit_mat4_rotate_y {#grit_mat4_rotate_y}

Source: `Grit.h:202`

```c
void grit_mat4_rotate_y(float *m, float rad);
```

#### grit_mat4_rotate_y_by {#grit_mat4_rotate_y_by}

Source: `Grit.h:203`

```c
void grit_mat4_rotate_y_by(float *m, float rad);
```

#### grit_mat4_rotate_z {#grit_mat4_rotate_z}

Source: `Grit.h:204`

```c
void grit_mat4_rotate_z(float *m, float rad);
```

#### grit_mat4_perspective {#grit_mat4_perspective}

Source: `Grit.h:205`

```c
void grit_mat4_perspective(float *m, float fov_rad, float aspect, float near_z, float far_z);
```

#### grit_mat4_ortho {#grit_mat4_ortho}

Source: `Grit.h:206`

```c
void grit_mat4_ortho(float *m, float left, float right, float bottom, float top, float near_z, float far_z);
```

#### grit_mat4_lookat {#grit_mat4_lookat}

Source: `Grit.h:207`

```c
void grit_mat4_lookat(float *m, const float *eye, const float *center, const float *up);
```

#### grit_rng_seed {#grit_rng_seed}

Source: `Grit.h:209`

```c
void grit_rng_seed(GritRng *r, uint64_t seed);
```

#### grit_rng_u64 {#grit_rng_u64}

Source: `Grit.h:210`

```c
uint64_t grit_rng_u64(GritRng *r);
```

#### grit_rng_u32 {#grit_rng_u32}

Source: `Grit.h:211`

```c
uint32_t grit_rng_u32(GritRng *r);
```

#### grit_rng_f32 {#grit_rng_f32}

Source: `Grit.h:212`

```c
void grit_rng_f32(GritRng *r, float *out);
```

#### grit_draw_begin {#grit_draw_begin}

Source: `Grit.h:214`

```c
void grit_draw_begin(GritDraw *d, uint32_t *px, int w, int h);
```

#### grit_draw_clip {#grit_draw_clip}

Source: `Grit.h:215`

```c
void grit_draw_clip(GritDraw *d, int x, int y, int w, int h);
```

#### grit_draw_clip_reset {#grit_draw_clip_reset}

Source: `Grit.h:216`

```c
void grit_draw_clip_reset(GritDraw *d);
```

#### grit_draw_clear {#grit_draw_clear}

Source: `Grit.h:217`

```c
void grit_draw_clear(GritDraw *d, uint32_t color);
```

#### grit_draw_pixel {#grit_draw_pixel}

Source: `Grit.h:218`

```c
void grit_draw_pixel(GritDraw *d, int x, int y, uint32_t color);
```

#### grit_draw_blit {#grit_draw_blit}

Source: `Grit.h:219`

```c
void grit_draw_blit(GritDraw *d, int x, int y, const uint32_t *src, int sw, int sh);
```

#### grit_draw_rect {#grit_draw_rect}

Source: `Grit.h:220`

```c
void grit_draw_rect(GritDraw *d, int x, int y, int w, int h, uint32_t color);
```

#### grit_draw_rect_gradient {#grit_draw_rect_gradient}

Source: `Grit.h:221`

```c
void grit_draw_rect_gradient(GritDraw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3);
```

#### grit_draw_tri {#grit_draw_tri}

Source: `Grit.h:222`

```c
void grit_draw_tri(GritDraw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2);
```

#### grit_draw_line {#grit_draw_line}

Source: `Grit.h:223`

```c
void grit_draw_line(GritDraw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1);
```

#### grit_draw_circle {#grit_draw_circle}

Source: `Grit.h:224`

```c
void grit_draw_circle(GritDraw *d, int cx, int cy, int radius, uint32_t color);
```

<!-- END GENERATED API -->
