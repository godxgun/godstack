/* fuse.c - immediate-mode UI command buffer
 * 0.0.1 - @vasco - canvas, screen stack, button, slider, class layout
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fuse.h"

#if defined(FUSE_DEBUG)
#define FASSERT_N(_1, _2, N, ...) N
#define FASSERT(...) FASSERT_N(__VA_ARGS__, FASSERT2, FASSERT1)(__VA_ARGS__)
#define FASSERT1(a) assert(a)
#define FASSERT2(a, s) assert((a) && (s))
#else
#define FASSERT(...) ((void)0)
#endif

#define FUSE_MEM_ALIGN 8
#define FUSE_CMDS_PER_EL 4
#define FUSE_EDGE_NONE 0
#define FUSE_EDGE_PRESSED 1
#define FUSE_EDGE_RELEASED 2
#define FUSE_EL_DIV 1
#define FUSE_EL_BUTTON 2
#define FUSE_EL_SLIDER 4

typedef struct FuseScreen {
    uint32_t element;
} FuseScreen;

typedef struct FuseElement {
    uint32_t id;
    float x, y, w, h;
    uint32_t color;
    uint32_t color_alt;
    float nob_pos;
    const FuseClass *cls;
    uint32_t first_child;
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t child_count;
    uint8_t flags;
} FuseElement;

typedef struct FuseHashItem {
    uint32_t id, generation;
    float x, y, w, h;
} FuseHashItem;

typedef struct FuseMem {
    size_t total;
    size_t hash_off;
    size_t hash_cap;
    size_t elements_off;
    size_t screens_off;
    size_t cmds_off;
    size_t cmd_cap;
} FuseMem;

struct fuse_canvas_t {
    size_t max_elements;
    size_t hash_cap, cmd_cap;
    FuseElement *elements;
    uint32_t element_count, cmd_count;
    FuseScreen *screens;
    uint32_t screen_count;
    FuseCmd *cmds;
    FuseHashItem *hash;
    uint32_t generation, capture_id, pending_id;
    float width, height, pointer_x, pointer_y;
    int pointer_state, pointer_edge;
    FuseError error;
};

static size_t fuse_internal_align(size_t n);
static size_t fuse_internal_hash_cap(size_t n);
static void fuse_internal_mem(FuseMem *m, size_t n);
static size_t fuse_internal_max_elements(size_t bufsize);
static void fuse_internal_hash_keep_last(FuseCanvas c);
static FuseHashItem *fuse_internal_hash_slot(FuseCanvas c, uint32_t id, int vacant_ok);
static uint32_t fuse_internal_hash_str(const char *s);
static uint32_t fuse_internal_hash_mix(uint32_t a, uint32_t b);
static int fuse_internal_ok(FuseCanvas c);
static void fuse_internal_fail(FuseCanvas c, FuseError err);
static void fuse_internal_reset_frame(FuseCanvas c, int bump_gen);
static uint32_t fuse_internal_take_id(FuseCanvas c, uint32_t parent_id, uint32_t sibling);
static uint32_t fuse_internal_add_element(FuseCanvas c, uint32_t parent, uint32_t id, float x, float y, float w, float h, uint8_t flags);
static uint32_t fuse_internal_open_el(FuseCanvas c, float x, float y, float w, float h, uint8_t flags);
static float fuse_internal_percent(FuseCanvas c, float p, int yaxis);
static FuseHashItem *fuse_internal_last_box(FuseCanvas c, uint32_t id);
static int fuse_internal_last_hit(FuseCanvas c, uint32_t id);
static float fuse_internal_clamp01(float t);
static float fuse_internal_axis_size(uint8_t sizing, float declared, float cls_value, float inner);
static float fuse_internal_clamp_axis(float v, float mn, float mx);
static void fuse_internal_layout_class(FuseCanvas c, FuseElement *el);
static void fuse_internal_layout(FuseCanvas c, uint32_t index);
static int fuse_internal_rect_visible(FuseCanvas c, float x, float y, float w, float h);
static FuseCmd *fuse_internal_emit(FuseCanvas c, uint8_t type, uint32_t id);
static void fuse_internal_emit_rect(FuseCanvas c, uint32_t id, float x, float y, float w, float h, uint32_t color);
static void fuse_internal_emit_clip(FuseCanvas c, uint8_t type, float x, float y, float w, float h);
static void fuse_internal_emit_tree(FuseCanvas c, uint32_t index, float ox, float oy);
static void fuse_internal_emit_children(FuseCanvas c, FuseElement *el, float ox, float oy);

static size_t
fuse_internal_align(size_t n)
{
    return (n + (FUSE_MEM_ALIGN - 1)) & ~(size_t)(FUSE_MEM_ALIGN - 1);
}

static size_t
fuse_internal_hash_cap(size_t n)
{
    size_t cap;
    cap = 8;
    if (n > ((size_t)-1) / 2)
        n = ((size_t)-1) / 2;
    while (cap < n * 2)
        cap *= 2;
    return cap;
}

static void
fuse_internal_mem(FuseMem *m, size_t n)
{
    size_t off;
    FASSERT(m);
    m->hash_cap = fuse_internal_hash_cap(n);
    m->cmd_cap = n * FUSE_CMDS_PER_EL;
    off = fuse_internal_align(sizeof (struct fuse_canvas_t));
    m->hash_off = off;
    off = fuse_internal_align(off + m->hash_cap * sizeof (FuseHashItem));
    m->elements_off = off;
    off = fuse_internal_align(off + n * sizeof (FuseElement));
    m->screens_off = off;
    off = fuse_internal_align(off + n * sizeof (FuseScreen));
    m->cmds_off = off;
    off = fuse_internal_align(off + m->cmd_cap * sizeof (FuseCmd));
    m->total = off;
}

static size_t
fuse_internal_max_elements(size_t bufsize)
{
    size_t lo, hi, mid, best;
    FuseMem m;
    if (bufsize < fuse_canvas_memory(1))
        return 0;
    lo = 1;
    hi = bufsize / 32;
    if (hi < 1)
        hi = 1;
    best = 0;
    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;
        fuse_internal_mem(&m, mid);
        if (m.total <= bufsize) {
            best = mid;
            lo = mid + 1;
        } else {
            if (mid == 0)
                break;
            hi = mid - 1;
        }
    }
    return best;
}

static void
fuse_internal_hash_keep_last(FuseCanvas c)
{
    FuseHashItem *scratch, *slot;
    size_t i, nkeep, keep_gen;
    nkeep = 0;
    keep_gen = c->generation - 1;
    scratch = (FuseHashItem *)c->cmds;
    for (i = 0; i < c->hash_cap; i++) {
        if (c->hash[i].id != 0 && c->hash[i].generation == keep_gen) {
            scratch[nkeep] = c->hash[i];
            nkeep++;
        }
    }
    memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
    for (i = 0; i < nkeep; i++) {
        slot = fuse_internal_hash_slot(c, scratch[i].id, 1);
        if (slot)
            *slot = scratch[i];
    }
}

static FuseHashItem *
fuse_internal_hash_slot(FuseCanvas c, uint32_t id, int vacant_ok)
{
    size_t i, mask, start;
    if (id == 0 || !c->hash)
        return NULL;
    mask = c->hash_cap - 1;
    start = id & mask;
    i = start;
    for (;;) {
        if (c->hash[i].id == id)
            return &c->hash[i];
        if (c->hash[i].id == 0)
            return vacant_ok ? &c->hash[i] : NULL;
        i = (i + 1) & mask;
        if (i == start)
            return NULL;
    }
}

static uint32_t
fuse_internal_hash_str(const char *s)
{
    uint32_t h;
    h = 2166136261u;
    if (!s)
        return 1;
    while (*s) {
        h ^= (uint8_t)*s;
        h *= 16777619u;
        s++;
    }
    if (h == 0)
        h = 1;
    return h;
}

static uint32_t
fuse_internal_hash_mix(uint32_t a, uint32_t b)
{
    uint32_t h;
    h = 2166136261u;
    h ^= a;
    h *= 16777619u;
    h ^= b;
    h *= 16777619u;
    if (h == 0)
        h = 1;
    return h;
}

static int
fuse_internal_ok(FuseCanvas c)
{
    return c && c->error == FUSE_ERR_OK;
}

static void
fuse_internal_fail(FuseCanvas c, FuseError err)
{
    if (c && c->error == FUSE_ERR_OK)
        c->error = err;
}

static void
fuse_internal_reset_frame(FuseCanvas c, int bump_gen)
{
    if (bump_gen) {
        c->generation++;
        if (c->generation == 0) {
            memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
            c->generation = 1;
        } else {
            fuse_internal_hash_keep_last(c);
        }
    }
    c->element_count = 1;
    c->cmd_count = 0;
    c->screen_count = 1;
    c->pending_id = 0;
    c->error = FUSE_ERR_OK;
    memset(&c->elements[0], 0, sizeof c->elements[0]);
    c->elements[0].w = c->width;
    c->elements[0].h = c->height;
    c->screens[0].element = 0;
}

static uint32_t
fuse_internal_take_id(FuseCanvas c, uint32_t parent_id, uint32_t sibling)
{
    uint32_t id, i;
    id = c->pending_id;
    c->pending_id = 0;
    if (id == 0)
        id = fuse_internal_hash_mix(parent_id, sibling);
    for (i = 0; i < c->element_count; i++) {
        if (c->elements[i].id == id) {
            fuse_internal_fail(c, FUSE_ERR_DUPLICATE_ID);
            return 0;
        }
    }
    return id;
}

static uint32_t
fuse_internal_add_element(FuseCanvas c, uint32_t parent, uint32_t id, float x, float y, float w, float h, uint8_t flags)
{
    FuseElement *el, *p;
    uint32_t index;
    if (c->element_count >= c->max_elements) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return 0;
    }
    index = c->element_count;
    c->element_count++;
    el = &c->elements[index];
    memset(el, 0, sizeof *el);
    el->id = id;
    el->x = x;
    el->y = y;
    el->w = w;
    el->h = h;
    el->flags = flags;
    p = &c->elements[parent];
    if (p->last_child != 0) {
        c->elements[p->last_child].next_sibling = index;
    } else {
        p->first_child = index;
    }
    p->last_child = index;
    p->child_count++;
    return index;
}

static uint32_t
fuse_internal_open_el(FuseCanvas c, float x, float y, float w, float h, uint8_t flags)
{
    uint32_t parent_el, sibling, id;
    parent_el = c->screens[c->screen_count - 1].element;
    sibling = c->elements[parent_el].child_count;
    id = fuse_internal_take_id(c, c->elements[parent_el].id, sibling);
    if (!fuse_internal_ok(c))
        return 0;
    return fuse_internal_add_element(c, parent_el, id, x, y, w, h, flags);
}

static FuseHashItem *
fuse_internal_last_box(FuseCanvas c, uint32_t id)
{
    FuseHashItem *item;
    item = fuse_internal_hash_slot(c, id, 0);
    if (!item || item->generation + 1 != c->generation)
        return NULL;
    return item;
}

static int
fuse_internal_last_hit(FuseCanvas c, uint32_t id)
{
    FuseHashItem *item;
    item = fuse_internal_last_box(c, id);
    if (!item)
        return 0;
    if (c->pointer_x < item->x || c->pointer_y < item->y)
        return 0;
    if (c->pointer_x > item->x + item->w || c->pointer_y > item->y + item->h)
        return 0;
    return 1;
}

static float
fuse_internal_clamp01(float t)
{
    if (t < 0.0f)
        return 0.0f;
    if (t > 1.0f)
        return 1.0f;
    return t;
}

static float
fuse_internal_axis_size(uint8_t sizing, float declared, float cls_value, float inner)
{
    switch (sizing) {
    case FUSE_SIZING_FIXED:
        return cls_value;
    case FUSE_SIZING_PERCENT:
        return cls_value * inner;
    case FUSE_SIZING_GROW:
        return 0.0f;
    case FUSE_SIZING_FIT: /* FALLTHROUGH */
    default:
        return declared;
    }
}

static float
fuse_internal_clamp_axis(float v, float mn, float mx)
{
    if (v < mn)
        v = mn;
    if (mx > 0.0f && v > mx)
        v = mx;
    return v;
}

static void
fuse_internal_layout_class(FuseCanvas c, FuseElement *el)
{
    const FuseClass *cls;
    uint32_t n, grow_n, idx;
    float inner[2], cls_v[2], mn[2], mx[2];
    float gap, extra, cursor, leftover, grow, sum_main;
    uint8_t sizing[2], align_main, align_cross;
    int main_ax, cross_ax;
    FuseElement *ch;
    cls = el->cls;
    n = el->child_count;
    if (!cls || n == 0)
        return;
    inner[0] = el->w - cls->pad_l - cls->pad_r;
    inner[1] = el->h - cls->pad_t - cls->pad_b;
    if (inner[0] < 0.0f)
        inner[0] = 0.0f;
    if (inner[1] < 0.0f)
        inner[1] = 0.0f;
    main_ax = cls->direction != FUSE_DIRECTION_COLUMN ? 0 : 1;
    cross_ax = 1 - main_ax;
    sizing[0] = cls->width_sizing;
    sizing[1] = cls->height_sizing;
    cls_v[0] = cls->width;
    cls_v[1] = cls->height;
    mn[0] = cls->min_width;
    mn[1] = cls->min_height;
    mx[0] = cls->max_width;
    mx[1] = cls->max_height;
    align_main = main_ax == 0 ? cls->align_x : cls->align_y;
    align_cross = main_ax == 0 ? cls->align_y : cls->align_x;
    gap = cls->gap;
    sum_main = 0.0f;
    grow_n = 0;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        float sz[2];
        ch = &c->elements[idx];
        sz[0] = fuse_internal_clamp_axis(
            fuse_internal_axis_size(sizing[0], ch->w, cls_v[0], inner[0]),
            mn[0], mx[0]);
        sz[1] = fuse_internal_clamp_axis(
            fuse_internal_axis_size(sizing[1], ch->h, cls_v[1], inner[1]),
            mn[1], mx[1]);
        ch->w = sz[0];
        ch->h = sz[1];
        if (sizing[main_ax] == FUSE_SIZING_GROW)
            grow_n++;
        else
            sum_main += sz[main_ax];
    }
    leftover = inner[main_ax] - sum_main - gap * (float)(n - 1);
    grow = 0.0f;
    if (grow_n > 0) {
        grow = leftover / (float)grow_n;
        if (grow < 0.0f)
            grow = 0.0f;
        leftover = 0.0f;
    }
    extra = leftover < 0.0f ? 0.0f : leftover;
    cursor = main_ax == 0 ? cls->pad_l : cls->pad_t;
    if (align_main == FUSE_ALIGN_CENTER)
        cursor += extra * 0.5f;
    else if (align_main == FUSE_ALIGN_END)
        cursor += extra;
    for (idx = el->first_child; idx != 0; idx = c->elements[idx].next_sibling) {
        float sz[2], pos[2], cross;
        ch = &c->elements[idx];
        sz[0] = ch->w;
        sz[1] = ch->h;
        if (sizing[main_ax] == FUSE_SIZING_GROW)
            sz[main_ax] = fuse_internal_clamp_axis(grow, mn[main_ax], mx[main_ax]);
        if (sizing[cross_ax] == FUSE_SIZING_GROW)
            sz[cross_ax] = fuse_internal_clamp_axis(inner[cross_ax], mn[cross_ax], mx[cross_ax]);
        cross = main_ax == 0 ? cls->pad_t : cls->pad_l;
        if (align_cross == FUSE_ALIGN_CENTER)
            cross += (inner[cross_ax] - sz[cross_ax]) * 0.5f;
        else if (align_cross == FUSE_ALIGN_END)
            cross += inner[cross_ax] - sz[cross_ax];
        pos[main_ax] = cursor;
        pos[cross_ax] = cross;
        ch->x = pos[0];
        ch->y = pos[1];
        ch->w = sz[0];
        ch->h = sz[1];
        cursor += sz[main_ax] + gap;
    }
}

static void
fuse_internal_layout(FuseCanvas c, uint32_t index)
{
    FuseElement *el;
    uint32_t child;
    el = &c->elements[index];
    if (el->cls)
        fuse_internal_layout_class(c, el);
    for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
        fuse_internal_layout(c, child);
}

static int
fuse_internal_rect_visible(FuseCanvas c, float x, float y, float w, float h)
{
    if (w <= 0.0f || h <= 0.0f)
        return 0;
    if (x + w <= 0.0f || y + h <= 0.0f)
        return 0;
    if (x >= c->width || y >= c->height)
        return 0;
    return 1;
}

static FuseCmd *
fuse_internal_emit(FuseCanvas c, uint8_t type, uint32_t id)
{
    FuseCmd *cmd;
    if (c->cmd_count >= c->cmd_cap) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return NULL;
    }
    cmd = &c->cmds[c->cmd_count];
    c->cmd_count++;
    memset(cmd, 0, sizeof *cmd);
    cmd->type = type;
    cmd->id = id;
    cmd->z = 0;
    return cmd;
}

static void
fuse_internal_emit_rect(FuseCanvas c, uint32_t id, float x, float y, float w, float h, uint32_t color)
{
    FuseCmd *cmd;
    if (!fuse_internal_rect_visible(c, x, y, w, h))
        return;
    cmd = fuse_internal_emit(c, FUSE_CMD_RECT, id);
    if (!cmd)
        return;
    cmd->rect.x = x;
    cmd->rect.y = y;
    cmd->rect.w = w;
    cmd->rect.h = h;
    cmd->rect.color = color;
}

static void
fuse_internal_emit_clip(FuseCanvas c, uint8_t type, float x, float y, float w, float h)
{
    FuseCmd *cmd;
    cmd = fuse_internal_emit(c, type, 0);
    if (!cmd)
        return;
    cmd->clip.x = x;
    cmd->clip.y = y;
    cmd->clip.w = w;
    cmd->clip.h = h;
}

static void
fuse_internal_emit_children(FuseCanvas c, FuseElement *el, float ox, float oy)
{
    uint32_t child;
    for (child = el->first_child; child != 0; child = c->elements[child].next_sibling)
        fuse_internal_emit_tree(c, child, ox, oy);
}

static void
fuse_internal_emit_tree(FuseCanvas c, uint32_t index, float ox, float oy)
{
    FuseElement *el;
    FuseHashItem *item;
    float cx, cy, nob_w, nob_x;
    el = &c->elements[index];
    cx = ox + el->x;
    cy = oy + el->y;
    if (el->id != 0) {
        item = fuse_internal_hash_slot(c, el->id, 1);
        if (item) {
            item->id = el->id;
            item->generation = c->generation;
            item->x = cx;
            item->y = cy;
            item->w = el->w;
            item->h = el->h;
        } else {
            fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
            return;
        }
    }
    if (index == 0) {
        fuse_internal_emit_children(c, el, cx, cy);
        return;
    }
    if (el->flags & FUSE_EL_DIV) {
        if (el->color != 0)
            fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_START, cx, cy, el->w, el->h);
        fuse_internal_emit_children(c, el, cx, cy);
        fuse_internal_emit_clip(c, FUSE_CMD_CLIP_END, cx, cy, el->w, el->h);
        return;
    }
    if (el->flags & FUSE_EL_BUTTON) {
        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        return;
    }
    if (el->flags & FUSE_EL_SLIDER) {
        nob_w = el->h;
        if (nob_w > el->w)
            nob_w = el->w;
        nob_x = cx + el->nob_pos * (el->w - nob_w);
        fuse_internal_emit_rect(c, el->id, cx, cy, el->w, el->h, el->color);
        fuse_internal_emit_rect(c, el->id, nob_x, cy, nob_w, el->h, el->color_alt);
    }
}

size_t
fuse_canvas_memory(size_t max_elements)
{
    FuseMem m;
    if (max_elements == 0)
        return 0;
    fuse_internal_mem(&m, max_elements);
    return m.total;
}

FuseCanvas
fuse_canvas_create(void *buf, size_t bufsize)
{
    FuseMem m;
    FuseCanvas c;
    unsigned char *bytes;
    size_t n;
    if (!buf)
        return NULL;
    n = fuse_internal_max_elements(bufsize);
    if (n == 0)
        return NULL;
    fuse_internal_mem(&m, n);
    bytes = buf;
    memset(bytes, 0, sizeof (struct fuse_canvas_t));
    c = (FuseCanvas)bytes;
    c->max_elements = n;
    c->hash_cap = m.hash_cap;
    c->cmd_cap = m.cmd_cap;
    c->hash = (FuseHashItem *)(bytes + m.hash_off);
    c->elements = (FuseElement *)(bytes + m.elements_off);
    c->screens = (FuseScreen *)(bytes + m.screens_off);
    c->cmds = (FuseCmd *)(bytes + m.cmds_off);
    memset(c->hash, 0, c->hash_cap * sizeof *c->hash);
    fuse_internal_reset_frame(c, 0);
    return c;
}

FuseError
fuse_canvas_error(FuseCanvas c)
{
    if (!c)
        return FUSE_ERR_BUF_TOO_SMALL;
    return c->error;
}

void
fuse_canvas_clear(FuseCanvas c)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    fuse_internal_reset_frame(c, 1);
}

void
fuse_canvas_resize(FuseCanvas c, float w, float h)
{
    FASSERT(c, "null canvas");
    if (!c)
        return;
    c->width = w;
    c->height = h;
    if (c->element_count > 0) {
        c->elements[0].w = w;
        c->elements[0].h = h;
    }
}

void
fuse_canvas_pointer(FuseCanvas c, int pointer_state, float x, float y)
{
    int prev;
    FASSERT(c, "null canvas");
    if (!c)
        return;
    prev = c->pointer_state;
    c->pointer_x = x;
    c->pointer_y = y;
    c->pointer_edge = FUSE_EDGE_NONE;
    if (pointer_state == FUSE_POINTER_PRESSED && prev != FUSE_POINTER_PRESSED)
        c->pointer_edge = FUSE_EDGE_PRESSED;
    else if (pointer_state == FUSE_POINTER_RELEASED && prev == FUSE_POINTER_PRESSED)
        c->pointer_edge = FUSE_EDGE_RELEASED;
    c->pointer_state = pointer_state;
}

FuseCmd *
fuse_canvas_draw(FuseCanvas c, size_t *cmd_count)
{
    FASSERT(c, "null canvas");
    FASSERT(cmd_count, "null cmd_count");
    if (cmd_count)
        *cmd_count = 0;
    if (!fuse_internal_ok(c))
        return NULL;
    if (c->screen_count != 1) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return NULL;
    }
    fuse_internal_layout(c, 0);
    c->cmd_count = 0;
    fuse_internal_emit_tree(c, 0, 0.0f, 0.0f);
    if (!fuse_internal_ok(c))
        return NULL;
    if (cmd_count)
        *cmd_count = c->cmd_count;
    return c->cmds;
}

void
fuse_div_begin(FuseCanvas c, float x, float y, float w, float h, const FuseClass *cls)
{
    uint32_t index;
    FASSERT(c, "null canvas");
    FASSERT(w > 0.0f && h > 0.0f, "div w,h must be > 0");
    if (!fuse_internal_ok(c))
        return;
    if (w <= 0.0f || h <= 0.0f)
        return;
    if (c->screen_count >= c->max_elements) {
        fuse_internal_fail(c, FUSE_ERR_OVERFLOW);
        return;
    }
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_DIV);
    if (!fuse_internal_ok(c))
        return;
    c->elements[index].cls = cls;
    c->elements[index].color = cls ? cls->color : 0;
    c->screens[c->screen_count].element = index;
    c->screen_count++;
}

void
fuse_div_end(FuseCanvas c)
{
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return;
    if (c->screen_count <= 1) {
        fuse_internal_fail(c, FUSE_ERR_UNBALANCED);
        return;
    }
    c->screen_count--;
}

void
fuse_id(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null id name");
    if (!fuse_internal_ok(c))
        return;
    c->pending_id = fuse_internal_hash_str(name);
}

void
fuse_idi(FuseCanvas c, const char *name, int index)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null id name");
    if (!fuse_internal_ok(c))
        return;
    c->pending_id = fuse_internal_hash_mix(fuse_internal_hash_str(name), (uint32_t)index);
}

bool
fuse_element_is_hovered(FuseCanvas c, const char *name)
{
    FASSERT(c, "null canvas");
    FASSERT(name, "null hover name");
    if (!fuse_internal_ok(c) || !name)
        return false;
    return fuse_internal_last_hit(c, fuse_internal_hash_str(name)) ? true : false;
}

bool
fuse_button(FuseCanvas c, float x, float y, float w, float h, uint32_t selected, uint32_t hovered)
{
    uint32_t index, id;
    int hit;
    FASSERT(c, "null canvas");
    if (!fuse_internal_ok(c))
        return false;
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_BUTTON);
    if (!fuse_internal_ok(c))
        return false;
    id = c->elements[index].id;
    hit = fuse_internal_last_hit(c, id);
    c->elements[index].color = hit ? hovered : selected;
    return hit && c->pointer_edge == FUSE_EDGE_RELEASED && c->capture_id == 0;
}

void
fuse_slider(FuseCanvas c, float x, float y, float w, float h, uint32_t track, uint32_t nob, float *nob_pos)
{
    uint32_t index, id;
    int hit;
    FuseHashItem *item;
    FASSERT(c, "null canvas");
    FASSERT(nob_pos, "null nob_pos");
    if (!fuse_internal_ok(c) || !nob_pos)
        return;
    index = fuse_internal_open_el(c, x, y, w, h, FUSE_EL_SLIDER);
    if (!fuse_internal_ok(c))
        return;
    id = c->elements[index].id;
    hit = fuse_internal_last_hit(c, id);
    if (c->capture_id == id) {
        if (c->pointer_state != FUSE_POINTER_PRESSED)
            c->capture_id = 0;
    } else if (c->capture_id == 0 && hit && c->pointer_edge == FUSE_EDGE_PRESSED) {
        c->capture_id = id;
    }
    if (c->capture_id == id) {
        item = fuse_internal_last_box(c, id);
        if (item && item->w > 0.0f)
            *nob_pos = fuse_internal_clamp01((c->pointer_x - item->x) / item->w);
    }
    *nob_pos = fuse_internal_clamp01(*nob_pos);
    c->elements[index].color = track;
    c->elements[index].color_alt = nob;
    c->elements[index].nob_pos = *nob_pos;
}

static float
fuse_internal_percent(FuseCanvas c, float p, int yaxis)
{
    FuseElement *el;
    if (!c || c->screen_count == 0)
        return 0.0f;
    el = &c->elements[c->screens[c->screen_count - 1].element];
    return p * (yaxis ? el->h : el->w) / 100.0f;
}

float
fuse_percent_x(FuseCanvas c, float p)
{
    FASSERT(c, "null canvas");
    return fuse_internal_percent(c, p, 0);
}

float
fuse_percent_y(FuseCanvas c, float p)
{
    FASSERT(c, "null canvas");
    return fuse_internal_percent(c, p, 1);
}
