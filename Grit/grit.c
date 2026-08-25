#include "grit.h"

uintptr_t
grit_internal_align_forward(uintptr_t ptr, size_t align)
{
    uintptr_t a, rem;
    GASSERT(align != 0 && (align & (align - 1)) == 0, "align must be power of two");
    a = (uintptr_t)align;
    rem = ptr & (a - 1);
    if (rem)
        ptr += a - rem;
    return ptr;
}

float
grit_internal_sqrtf(float x)
{
    float y;
    int i;
    if (x <= 0.0f)
        return 0.0f;
    y = x;
    for (i = 0; i < 4; ++i)
        y = 0.5f * (y + x / y);
    return y;
}

void
grit_arena_init(GritArena *a, void *buf, size_t len)
{
    GASSERT(a, "arena is NULL");
    if (!a)
        return;
    a->buf = buf;
    a->buf_len = len;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

void *
grit_arena_alloc_align(GritArena *a, size_t size, size_t align)
{
    uintptr_t curr, offset;
    void *ptr;
    GASSERT(a && a->buf, "arena is not initialized");
    if (!a || !a->buf)
        return NULL;
    curr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
    offset = grit_internal_align_forward(curr, align);
    offset -= (uintptr_t)a->buf;
    if (offset + size > a->buf_len)
        return NULL;
    ptr = &a->buf[offset];
    a->prev_offset = offset;
    a->curr_offset = offset + size;
    memset(ptr, 0, size);
    return ptr;
}

void *
grit_arena_alloc(GritArena *a, size_t size)
{
    return grit_arena_alloc_align(a, size, GRIT_DEFAULT_ALIGN);
}

void *
grit_arena_resize_align(GritArena *a, void *old, size_t old_size, size_t new_size, size_t align)
{
    unsigned char *old_mem;
    void *p;
    size_t copy;
    GASSERT(a && a->buf, "arena is not initialized");
    if (!a || !a->buf)
        return NULL;
    old_mem = old;
    if (!old_mem || old_size == 0)
        return grit_arena_alloc_align(a, new_size, align);
    if (!(a->buf <= old_mem && old_mem < a->buf + a->buf_len)) {
        GASSERT(0, "memory is outside this arena");
        return NULL;
    }
    if (a->buf + a->prev_offset == old_mem) {
        if (a->prev_offset + new_size > a->buf_len)
            return NULL;
        a->curr_offset = a->prev_offset + new_size;
        if (new_size > old_size)
            memset(&a->buf[a->prev_offset + old_size], 0, new_size - old_size);
        return old;
    }
    p = grit_arena_alloc_align(a, new_size, align);
    if (!p)
        return NULL;
    copy = old_size < new_size ? old_size : new_size;
    memmove(p, old, copy);
    return p;
}

void *
grit_arena_resize(GritArena *a, void *old, size_t old_size, size_t new_size)
{
    return grit_arena_resize_align(a, old, old_size, new_size, GRIT_DEFAULT_ALIGN);
}

void
grit_arena_free_all(GritArena *a)
{
    GASSERT(a, "arena is NULL");
    if (!a)
        return;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

void
grit_arena_destroy(GritArena *a)
{
    GASSERT(a, "arena is NULL");
    if (!a)
        return;
    a->buf = NULL;
    a->buf_len = 0;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

size_t
grit_internal_padding_with_header(uintptr_t ptr, uintptr_t alignment, size_t header_size)
{
    uintptr_t modulo, padding, needed;
    GASSERT((alignment & (alignment - 1)) == 0, "align must be power of two");
    modulo = ptr & (alignment - 1);
    padding = 0;
    needed = (uintptr_t)header_size;
    if (modulo != 0)
        padding = alignment - modulo;
    if (padding < needed) {
        needed -= padding;
        if ((needed & (alignment - 1)) != 0)
            padding += alignment * (1 + (needed / alignment));
        else
            padding += alignment * (needed / alignment);
    }
    return (size_t)padding;
}

void
grit_stack_init(GritStack *s, void *buf, size_t len)
{
    GASSERT(s, "stack is NULL");
    if (!s)
        return;
    s->buf = buf;
    s->buf_len = len;
    s->offset = 0;
}

void *
grit_stack_alloc(GritStack *s, size_t len, size_t alignment)
{
    uintptr_t curr, next;
    size_t padding;
    GritStackHeader *header;
    GASSERT(s && s->buf, "stack is not initialized");
    if (!s || !s->buf)
        return NULL;
    if (alignment > 128)
        alignment = 128;
    curr = (uintptr_t)s->buf + (uintptr_t)s->offset;
    padding = grit_internal_padding_with_header(curr, (uintptr_t)alignment, sizeof (GritStackHeader));
    if (s->offset + padding + len > s->buf_len)
        return NULL;
    s->offset += padding;
    next = curr + (uintptr_t)padding;
    header = (GritStackHeader *)(next - sizeof (GritStackHeader));
    header->padding = (uint8_t)padding;
    s->offset += len;
    return memset((void *)next, 0, len);
}

void
grit_stack_free(GritStack *s, void *ptr)
{
    uintptr_t start, end, curr;
    GritStackHeader *header;
    size_t prev;
    GASSERT(s && s->buf, "stack is not initialized");
    if (!s || !s->buf || !ptr)
        return;
    start = (uintptr_t)s->buf;
    end = start + (uintptr_t)s->buf_len;
    curr = (uintptr_t)ptr;
    if (!(start <= curr && curr < end)) {
        GASSERT(0, "pointer is outside this stack");
        return;
    }
    if (curr >= start + (uintptr_t)s->offset)
        return;
    header = (GritStackHeader *)(curr - sizeof (GritStackHeader));
    prev = (size_t)(curr - (uintptr_t)header->padding - start);
    s->offset = prev;
}

void
grit_stack_free_all(GritStack *s)
{
    GASSERT(s, "stack is NULL");
    if (!s)
        return;
    s->offset = 0;
}

void
grit_stack_destroy(GritStack *s)
{
    GASSERT(s, "stack is NULL");
    if (!s)
        return;
    s->buf = NULL;
    s->buf_len = 0;
    s->offset = 0;
}

void
grit_pool_init(GritPool *p, void *buf, size_t buf_len, size_t chunk_size, size_t chunk_align)
{
    uintptr_t start, aligned;
    GASSERT(p && buf, "pool is not initialized");
    if (!p || !buf)
        return;
    start = (uintptr_t)buf;
    aligned = grit_internal_align_forward(start, chunk_align);
    buf_len -= (size_t)(aligned - start);
    chunk_size = (size_t)grit_internal_align_forward((uintptr_t)chunk_size, chunk_align);
    GASSERT(chunk_size >= sizeof (GritPoolNode), "chunk size is too small");
    GASSERT(buf_len >= chunk_size, "backing buffer is smaller than one chunk");
    p->buf = (unsigned char *)aligned;
    p->pool_size = buf_len;
    p->chunk_size = chunk_size;
    p->head = NULL;
    grit_pool_free_all(p);
}

void *
grit_pool_alloc(GritPool *p)
{
    GritPoolNode *node;
    GASSERT(p, "pool is NULL");
    if (!p)
        return NULL;
    node = p->head;
    if (!node) {
        GASSERT(0, "pool is empty");
        return NULL;
    }
    p->head = node->next;
    return memset(node, 0, p->chunk_size);
}

void
grit_pool_free(GritPool *p, void *ptr)
{
    GritPoolNode *node;
    void *start, *end;
    GASSERT(p && p->buf, "pool is not initialized");
    if (!p || !p->buf || !ptr)
        return;
    start = p->buf;
    end = &p->buf[p->pool_size];
    if (!(start <= ptr && ptr < end)) {
        GASSERT(0, "pointer is outside this pool");
        return;
    }
    node = ptr;
    node->next = p->head;
    p->head = node;
}

void
grit_pool_free_all(GritPool *p)
{
    size_t count, i;
    GASSERT(p && p->buf && p->chunk_size, "pool is not initialized");
    if (!p || !p->buf || !p->chunk_size)
        return;
    count = p->pool_size / p->chunk_size;
    p->head = NULL;
    for (i = 0; i < count; ++i) {
        GritPoolNode *node;
        node = (GritPoolNode *)&p->buf[i * p->chunk_size];
        node->next = p->head;
        p->head = node;
    }
}

void
grit_pool_destroy(GritPool *p)
{
    GASSERT(p, "pool is NULL");
    if (!p)
        return;
    p->buf = NULL;
    p->pool_size = 0;
    p->chunk_size = 0;
    p->head = NULL;
}

GritDArray
grit_darray_create(size_t init_capacity, size_t elem_size)
{
    GritDArray a;
    GASSERT(elem_size != 0, "elem_size is 0");
    a.data = NULL;
    a.len = 0;
    a.cap = 0;
    a.elem_size = elem_size;
    if (elem_size == 0)
        return a;
    if (init_capacity == 0)
        return a;
    a.data = calloc(init_capacity, elem_size);
    if (!a.data)
        return a;
    a.cap = init_capacity;
    return a;
}

void
grit_darray_destroy(GritDArray *a)
{
    GASSERT(a, "darray is NULL");
    if (!a)
        return;
    free(a->data);
    a->data = NULL;
    a->len = 0;
    a->cap = 0;
}

int
grit_internal_darray_grow(GritDArray *a, size_t need)
{
    size_t cap;
    void *p;
    if (need <= a->cap)
        return 1;
    cap = a->cap ? a->cap : 1;
    while (cap < need)
        cap *= 2;
    p = realloc(a->data, cap * a->elem_size);
    if (!p)
        return 0;
    if (cap > a->cap)
        memset((unsigned char *)p + a->cap * a->elem_size, 0, (cap - a->cap) * a->elem_size);
    a->data = p;
    a->cap = cap;
    return 1;
}

void
grit_darray_push(GritDArray *a, const void *elem)
{
    GASSERT(a && a->elem_size, "darray is not initialized");
    GASSERT(elem, "elem is NULL");
    if (!a || !a->elem_size || !elem)
        return;
    if (!grit_internal_darray_grow(a, a->len + 1))
        return;
    memcpy((unsigned char *)a->data + a->len * a->elem_size, elem, a->elem_size);
    a->len++;
}

void *
grit_darray_get(GritDArray *a, size_t i)
{
    GASSERT(a && a->data, "darray is not initialized");
    GASSERT(i < a->len, "darray index out of range");
    if (!a || !a->data || i >= a->len)
        return NULL;
    return (unsigned char *)a->data + i * a->elem_size;
}

void
grit_darray_pop(GritDArray *a)
{
    GASSERT(a && a->len > 0, "darray is empty");
    if (!a || a->len == 0)
        return;
    a->len--;
}

size_t
grit_internal_next_pow2(size_t n)
{
    size_t p;
    p = 1;
    while (p < n)
        p *= 2;
    return p;
}

uint64_t
grit_internal_hash(const void *key, size_t n)
{
    /* NOTE(vasco): splitmix/murmur mix, word-at-a-time. Fast, not cryptographic. */
    const unsigned char *p;
    uint64_t h;
    p = key;
    h = (uint64_t)n * 0x9E3779B97F4A7C15ull;
    while (n >= 8) {
        uint64_t w;
        memcpy(&w, p, 8);
        h ^= w;
        h *= 0xBF58476D1CE4E5B9ull;
        p += 8;
        n -= 8;
    }
    if (n >= 4) {
        uint32_t w;
        memcpy(&w, p, 4);
        h ^= w;
        h *= 0x94D049BB133111EBull;
        p += 4;
        n -= 4;
    }
    while (n) {
        h ^= *p++;
        h *= 0xBF58476D1CE4E5B9ull;
        --n;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDull;
    h ^= h >> 33;
    return h;
}

GritHashMap
grit_hashmap_create(size_t init_capacity, size_t key_size, size_t val_size)
{
    GritHashMap m;
    GASSERT(key_size != 0 && val_size != 0, "key_size or val_size is 0");
    m.keys = NULL;
    m.vals = NULL;
    m.state = NULL;
    m.len = 0;
    m.cap = 0;
    m.tombed = 0;
    m.key_size = key_size;
    m.val_size = val_size;
    if (key_size == 0 || val_size == 0 || init_capacity == 0)
        return m;
    init_capacity = grit_internal_next_pow2(init_capacity);
    m.keys = calloc(init_capacity, key_size);
    m.vals = calloc(init_capacity, val_size);
    m.state = calloc(init_capacity, 1);
    if (!m.keys || !m.vals || !m.state) {
        free(m.keys);
        free(m.vals);
        free(m.state);
        m.keys = NULL;
        m.vals = NULL;
        m.state = NULL;
        return m;
    }
    m.cap = init_capacity;
    return m;
}

void
grit_hashmap_destroy(GritHashMap *m)
{
    GASSERT(m, "hashmap is NULL");
    if (!m)
        return;
    free(m->keys);
    free(m->vals);
    free(m->state);
    m->keys = NULL;
    m->vals = NULL;
    m->state = NULL;
    m->len = 0;
    m->cap = 0;
    m->tombed = 0;
}

int
grit_internal_hashmap_rehash(GritHashMap *m, size_t new_cap)
{
    void *old_keys, *old_vals;
    uint8_t *old_state;
    size_t old_cap, i;
    old_keys = m->keys;
    old_vals = m->vals;
    old_state = m->state;
    old_cap = m->cap;
    m->keys = calloc(new_cap, m->key_size);
    m->vals = calloc(new_cap, m->val_size);
    m->state = calloc(new_cap, 1);
    if (!m->keys || !m->vals || !m->state) {
        free(m->keys);
        free(m->vals);
        free(m->state);
        m->keys = old_keys;
        m->vals = old_vals;
        m->state = old_state;
        return 0;
    }
    m->cap = new_cap;
    m->len = 0;
    m->tombed = 0;
    for (i = 0; i < old_cap; ++i) {
        if (old_state[i] == 1)
            grit_hashmap_put(m, (unsigned char *)old_keys + i * m->key_size, (unsigned char *)old_vals + i * m->val_size);
    }
    free(old_keys);
    free(old_vals);
    free(old_state);
    return 1;
}

size_t
grit_internal_hashmap_lookup(GritHashMap *m, const void *key, size_t *tomb)
{
    uint64_t hash;
    size_t mask, i, start;
    if (tomb)
        *tomb = (size_t)-1;
    if (!m->cap)
        return (size_t)-1;
    hash = grit_internal_hash(key, m->key_size);
    mask = m->cap - 1;
    i = (size_t)hash & mask;
    start = i;
    for (;;) {
        if (m->state[i] == 0)
            return i;
        if (m->state[i] == 2) {
            if (tomb && *tomb == (size_t)-1)
                *tomb = i;
        } else if (memcmp((unsigned char *)m->keys + i * m->key_size, key, m->key_size) == 0) {
            return i;
        }
        i = (i + 1) & mask;
        if (i == start)
            return (size_t)-1;
    }
}

void
grit_hashmap_put(GritHashMap *m, const void *key, const void *val)
{
    size_t slot, tomb;
    GASSERT(m && m->key_size && m->val_size, "hashmap is not initialized");
    GASSERT(key && val, "key or val is NULL");
    if (!m || !m->key_size || !m->val_size || !key || !val)
        return;
    if (!m->cap) {
        if (!grit_internal_hashmap_rehash(m, 8))
            return;
    } else if ((m->len + m->tombed + 1) * 4 > m->cap * 3) {
        if (!grit_internal_hashmap_rehash(m, m->cap * 2))
            return;
    }
    slot = grit_internal_hashmap_lookup(m, key, &tomb);
    if (slot == (size_t)-1)
        return;
    if (m->state[slot] == 1) {
        memcpy((unsigned char *)m->vals + slot * m->val_size, val, m->val_size);
        return;
    }
    if (m->state[slot] == 0 && tomb != (size_t)-1)
        slot = tomb;
    if (m->state[slot] == 2)
        m->tombed--;
    memcpy((unsigned char *)m->keys + slot * m->key_size, key, m->key_size);
    memcpy((unsigned char *)m->vals + slot * m->val_size, val, m->val_size);
    m->state[slot] = 1;
    m->len++;
}

void *
grit_hashmap_get(GritHashMap *m, const void *key)
{
    size_t slot;
    GASSERT(m, "hashmap is NULL");
    GASSERT(key, "key is NULL");
    if (!m || !m->cap || !key)
        return NULL;
    slot = grit_internal_hashmap_lookup(m, key, NULL);
    if (slot == (size_t)-1 || m->state[slot] != 1)
        return NULL;
    return (unsigned char *)m->vals + slot * m->val_size;
}

void
grit_hashmap_del(GritHashMap *m, const void *key)
{
    size_t slot;
    GASSERT(m, "hashmap is NULL");
    GASSERT(key, "key is NULL");
    if (!m || !m->cap || !key)
        return;
    slot = grit_internal_hashmap_lookup(m, key, NULL);
    if (slot == (size_t)-1 || m->state[slot] != 1)
        return;
    m->state[slot] = 2;
    m->len--;
    m->tombed++;
}

void
grit_ceil(float *x)
{
    int base;
    base = (int)*x;
    *x = (*x > (float)base) ? (float)(base + 1) : (float)base;
}

void
grit_sinf(float *x)
{
    float y, B, C, P;
    while (*x > GRIT_PI)
        *x -= GRIT_PI2;
    while (*x < -GRIT_PI)
        *x += GRIT_PI2;
    B = 4.0f / GRIT_PI;
    C = -4.0f / GRIT_PI_POW2;
    y = B * *x + C * *x * (*x < 0 ? -*x : *x);
    P = 0.225f;
    *x = P * (y * (y < 0 ? -y : y) - y) + y;
}

void
grit_cosf(float *x)
{
    *x += GRIT_PI_HALF;
    grit_sinf(x);
}

void
grit_atan2f(float *y, float x)
{
    float ay, ax, a, r;
    ay = *y < 0.0f ? -*y : *y;
    ax = x < 0.0f ? -x : x;
    if (ay == 0.0f && ax == 0.0f) {
        *y = 0.0f;
        return;
    }
    a = ay < ax ? ay / ax : ax / ay;
    r = ((-0.0464964749f * a + 0.15931422f) * a - 0.327622764f) * a * a + a;
    if (ay > ax)
        r = GRIT_PI_HALF - r;
    if (x < 0.0f)
        r = GRIT_PI - r;
    if (*y < 0.0f)
        r = -r;
    *y = r;
}

void
grit_lerpf(float *a, float b, float t)
{
    *a += t * (b - *a);
}

void
grit_vec2f(float *v, float x, float y)
{
    v[0] = x;
    v[1] = y;
}

void
grit_vec2f_add(float *a, const float *b)
{
    a[0] += b[0];
    a[1] += b[1];
}

void
grit_vec2f_sub(float *a, const float *b)
{
    a[0] -= b[0];
    a[1] -= b[1];
}

void
grit_vec2f_mult(float *a, float s)
{
    a[0] *= s;
    a[1] *= s;
}

void
grit_vec2f_dot(float *out, const float *a, const float *b)
{
    *out = a[0] * b[0] + a[1] * b[1];
}

void
grit_vec2f_cross(float *out, const float *a, const float *b)
{
    *out = a[0] * b[1] - a[1] * b[0];
}

void
grit_vec2f_len(float *out, const float *a)
{
    *out = grit_internal_sqrtf(a[0] * a[0] + a[1] * a[1]);
}

void
grit_vec2f_norm(float *a)
{
    float len;
    grit_vec2f_len(&len, a);
    if (len == 0.0f)
        return;
    grit_vec2f_mult(a, 1.0f / len);
}

void
grit_vec2f_lerp(float *a, const float *b, float t)
{
    a[0] += t * (b[0] - a[0]);
    a[1] += t * (b[1] - a[1]);
}

void
grit_vec3f(float *v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

void
grit_vec3f_add(float *a, const float *b)
{
    a[0] += b[0];
    a[1] += b[1];
    a[2] += b[2];
}

void
grit_vec3f_sub(float *a, const float *b)
{
    a[0] -= b[0];
    a[1] -= b[1];
    a[2] -= b[2];
}

void
grit_vec3f_mult(float *a, float s)
{
    a[0] *= s;
    a[1] *= s;
    a[2] *= s;
}

void
grit_vec3f_dot(float *out, const float *a, const float *b)
{
    *out = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void
grit_vec3f_cross(float *a, const float *b)
{
    float x, y, z;
    x = a[1] * b[2] - a[2] * b[1];
    y = a[2] * b[0] - a[0] * b[2];
    z = a[0] * b[1] - a[1] * b[0];
    a[0] = x;
    a[1] = y;
    a[2] = z;
}

void
grit_vec3f_len(float *out, const float *a)
{
    *out = grit_internal_sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

void
grit_vec3f_norm(float *a)
{
    float len;
    grit_vec3f_len(&len, a);
    if (len == 0.0f)
        return;
    grit_vec3f_mult(a, 1.0f / len);
}

void
grit_vec3f_lerp(float *a, const float *b, float t)
{
    a[0] += t * (b[0] - a[0]);
    a[1] += t * (b[1] - a[1]);
    a[2] += t * (b[2] - a[2]);
}

void
grit_mat4_identity(float *m)
{
    int i;
    for (i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void
grit_mat4_mul(float *a, const float *b)
{
    float res[16];
    int col, row, k;
    for (col = 0; col < 4; ++col) {
        for (row = 0; row < 4; ++row) {
            float sum;
            sum = 0.0f;
            for (k = 0; k < 4; ++k)
                sum += a[k * 4 + row] * b[col * 4 + k];
            res[col * 4 + row] = sum;
        }
    }
    for (k = 0; k < 16; k++)
        a[k] = res[k];
}

void
grit_mat4_translate(float *m, const float *v)
{
    grit_mat4_identity(m);
    m[12] = v[0];
    m[13] = v[1];
    m[14] = v[2];
}

void
grit_mat4_translate_by(float *m, const float *v)
{
    m[12] = m[0] * v[0] + m[4] * v[1] + m[8] * v[2] + m[12];
    m[13] = m[1] * v[0] + m[5] * v[1] + m[9] * v[2] + m[13];
    m[14] = m[2] * v[0] + m[6] * v[1] + m[10] * v[2] + m[14];
    m[15] = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15];
}

void
grit_mat4_rotate_x(float *m, float rad)
{
    float c, s;
    c = rad;
    s = rad;
    grit_cosf(&c);
    grit_sinf(&s);
    grit_mat4_identity(m);
    m[5] = c;
    m[6] = s;
    m[9] = -s;
    m[10] = c;
}

void
grit_mat4_rotate_x_by(float *m, float rad)
{
    float r[16];
    grit_mat4_rotate_x(r, rad);
    grit_mat4_mul(m, r);
}

void
grit_mat4_rotate_y(float *m, float rad)
{
    float c, s;
    c = rad;
    s = rad;
    grit_cosf(&c);
    grit_sinf(&s);
    grit_mat4_identity(m);
    m[0] = c;
    m[2] = -s;
    m[8] = s;
    m[10] = c;
}

void
grit_mat4_rotate_y_by(float *m, float rad)
{
    float r[16];
    grit_mat4_rotate_y(r, rad);
    grit_mat4_mul(m, r);
}

void
grit_mat4_rotate_z(float *m, float rad)
{
    float c, s;
    c = rad;
    s = rad;
    grit_cosf(&c);
    grit_sinf(&s);
    grit_mat4_identity(m);
    m[0] = c;
    m[1] = s;
    m[4] = -s;
    m[5] = c;
}

void
grit_mat4_perspective(float *m, float fov_rad, float aspect, float near_z, float far_z)
{
    float half, c, s, tan_half_fov;
    int i;
    half = fov_rad * 0.5f;
    c = half;
    s = half;
    grit_sinf(&s);
    grit_cosf(&c);
    tan_half_fov = s / c;
    for (i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = 1.0f / (aspect * tan_half_fov);
    m[5] = 1.0f / tan_half_fov;
    m[10] = far_z / (near_z - far_z);
    m[11] = -1.0f;
    m[14] = (near_z * far_z) / (near_z - far_z);
}

void
grit_mat4_ortho(float *m, float left, float right, float bottom, float top, float near_z, float far_z)
{
    int i;
    for (i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[0] = 2.0f / (right - left);
    m[5] = 2.0f / (top - bottom);
    m[10] = -2.0f / (far_z - near_z);
    m[12] = -(right + left) / (right - left);
    m[13] = -(top + bottom) / (top - bottom);
    m[14] = -(far_z + near_z) / (far_z - near_z);
    m[15] = 1.0f;
}

void
grit_mat4_lookat(float *m, const float *eye, const float *center, const float *up)
{
    float f[3], s[3], u[3];
    float ds, du, df;
    f[0] = center[0] - eye[0];
    f[1] = center[1] - eye[1];
    f[2] = center[2] - eye[2];
    grit_vec3f_norm(f);
    s[0] = f[0];
    s[1] = f[1];
    s[2] = f[2];
    grit_vec3f_cross(s, up);
    grit_vec3f_norm(s);
    u[0] = s[0];
    u[1] = s[1];
    u[2] = s[2];
    grit_vec3f_cross(u, f);
    grit_vec3f_dot(&ds, s, eye);
    grit_vec3f_dot(&du, u, eye);
    grit_vec3f_dot(&df, f, eye);
    m[0] = s[0];
    m[1] = s[1];
    m[2] = s[2];
    m[3] = 0.0f;
    m[4] = u[0];
    m[5] = u[1];
    m[6] = u[2];
    m[7] = 0.0f;
    m[8] = -f[0];
    m[9] = -f[1];
    m[10] = -f[2];
    m[11] = 0.0f;
    m[12] = -ds;
    m[13] = -du;
    m[14] = df;
    m[15] = 1.0f;
}

void
grit_rng_seed(GritRng *r, uint64_t seed)
{
    if (seed == 0)
        seed = 0x9E3779B97F4A7C15ull;
    r->state = seed;
}

uint64_t
grit_rng_u64(GritRng *r)
{
    uint64_t x;
    x = r->state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->state = x;
    return x * 0x2545F4914F6CDD1Dull;
}

uint32_t
grit_rng_u32(GritRng *r)
{
    return (uint32_t)(grit_rng_u64(r) >> 32);
}

void
grit_rng_f32(GritRng *r, float *out)
{
    *out = (float)(grit_rng_u32(r) >> 8) * (1.0f / 16777216.0f);
}

void
grit_draw_begin(GritDraw *d, uint32_t *px, int w, int h)
{
    d->px = px;
    d->w = w;
    d->h = h;
    d->cx = 0;
    d->cy = 0;
    d->cw = w;
    d->ch = h;
}

void
grit_draw_clip(GritDraw *d, int x, int y, int w, int h)
{
    d->cx = x;
    d->cy = y;
    d->cw = w;
    d->ch = h;
}

void
grit_draw_clip_reset(GritDraw *d)
{
    d->cx = 0;
    d->cy = 0;
    d->cw = d->w;
    d->ch = d->h;
}

void
grit_draw_clear(GritDraw *d, uint32_t color)
{
    int i, n;
    n = d->w * d->h;
    for (i = 0; i < n; ++i)
        d->px[i] = color;
}

void
grit_draw_pixel(GritDraw *d, int x, int y, uint32_t color)
{
    if (x < d->cx || x >= d->cx + d->cw || y < d->cy || y >= d->cy + d->ch)
        return;
    if (x < 0 || x >= d->w || y < 0 || y >= d->h)
        return;
    d->px[y * d->w + x] = color;
}

uint32_t
grit_internal_draw_lerp(uint32_t c0, uint32_t c1, float t)
{
    uint32_t a0, a1, r0, r1, g0, g1, b0, b1;
    uint32_t a, r, g, b;
    if (t <= 0.0f)
        return c0;
    if (t >= 1.0f)
        return c1;
    a0 = (c0 >> 24) & 0xFF;
    a1 = (c1 >> 24) & 0xFF;
    r0 = (c0 >> 16) & 0xFF;
    r1 = (c1 >> 16) & 0xFF;
    g0 = (c0 >> 8) & 0xFF;
    g1 = (c1 >> 8) & 0xFF;
    b0 = c0 & 0xFF;
    b1 = c1 & 0xFF;
    a = (uint32_t)(a0 + t * ((float)a1 - (float)a0));
    r = (uint32_t)(r0 + t * ((float)r1 - (float)r0));
    g = (uint32_t)(g0 + t * ((float)g1 - (float)g0));
    b = (uint32_t)(b0 + t * ((float)b1 - (float)b0));
    return (a << 24) | (r << 16) | (g << 8) | b;
}

float
grit_internal_draw_cross(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void
grit_draw_blit(GritDraw *d, int x, int y, const uint32_t *src, int sw, int sh)
{
    int row, ty, x0, x1;
    for (row = 0; row < sh; ++row) {
        ty = y + row;
        if (ty < d->cy || ty >= d->cy + d->ch || ty < 0 || ty >= d->h)
            continue;
        x0 = GRIT_MAX(x, d->cx);
        x1 = GRIT_MIN(x + sw, d->cx + d->cw);
        if (x0 >= x1)
            continue;
        memcpy(&d->px[ty * d->w + x0], &src[row * sw + (x0 - x)], (size_t)(x1 - x0) * sizeof(uint32_t));
    }
}

void
grit_draw_rect(GritDraw *d, int x, int y, int w, int h, uint32_t color)
{
    int x0, y0, x1, y1;
    int py, px;
    x0 = GRIT_MAX(x, d->cx);
    y0 = GRIT_MAX(y, d->cy);
    x1 = GRIT_MIN(x + w, d->cx + d->cw);
    y1 = GRIT_MIN(y + h, d->cy + d->ch);
    for (py = y0; py < y1; ++py) {
        if (py < 0 || py >= d->h)
            continue;
        for (px = x0; px < x1; ++px) {
            if (px < 0 || px >= d->w)
                continue;
            d->px[py * d->w + px] = color;
        }
    }
}

void
grit_draw_rect_gradient(GritDraw *d, int x, int y, int w, int h, uint32_t c0, uint32_t c1, uint32_t c2, uint32_t c3)
{
    int ry, rx;
    float ty, tx;
    uint32_t left, right;
    for (ry = 0; ry < h; ++ry) {
        ty = (h > 1) ? (float)ry / (float)(h - 1) : 0.0f;
        left = grit_internal_draw_lerp(c0, c3, ty);
        right = grit_internal_draw_lerp(c1, c2, ty);
        for (rx = 0; rx < w; ++rx) {
            tx = (w > 1) ? (float)rx / (float)(w - 1) : 0.0f;
            grit_draw_pixel(d, x + rx, y + ry, grit_internal_draw_lerp(left, right, tx));
        }
    }
}

void
grit_draw_tri(GritDraw *d, int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c0, uint32_t c1, uint32_t c2)
{
    int min_x, max_x, min_y, max_y;
    int solid;
    int x, y;
    float area;
    float w0, w1, w2;
    uint32_t c;
    min_x = GRIT_MIN(x0, GRIT_MIN(x1, x2));
    max_x = GRIT_MAX(x0, GRIT_MAX(x1, x2));
    min_y = GRIT_MIN(y0, GRIT_MIN(y1, y2));
    max_y = GRIT_MAX(y0, GRIT_MAX(y1, y2));
    area = grit_internal_draw_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x2, (float)y2);
    if (area == 0.0f)
        return;
    solid = (c0 == c1 && c1 == c2);
    for (y = min_y; y <= max_y; ++y) {
        for (x = min_x; x <= max_x; ++x) {
            w0 = grit_internal_draw_cross((float)x1, (float)y1, (float)x2, (float)y2, (float)x, (float)y);
            w1 = grit_internal_draw_cross((float)x2, (float)y2, (float)x0, (float)y0, (float)x, (float)y);
            w2 = grit_internal_draw_cross((float)x0, (float)y0, (float)x1, (float)y1, (float)x, (float)y);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                c = c0;
                if (!solid) {
                    w0 /= area;
                    w1 /= area;
                    w2 /= area;
                    c = grit_internal_draw_lerp(c0, c1, w1 / (w0 + w1 + 1e-6f));
                    c = grit_internal_draw_lerp(c, c2, w2);
                }
                grit_draw_pixel(d, x, y, c);
            }
        }
    }
}

void
grit_draw_line(GritDraw *d, int x0, int y0, int x1, int y1, int width, uint32_t c0, uint32_t c1)
{
    float total;
    int grad;
    int dx, sx, dy, sy, err;
    int ox0, oy0, hw;
    int ox, oy, e2;
    uint32_t c;
    total = 0.0f;
    grad = (c0 != c1);
    if (grad)
        total = grit_internal_sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)));
    dx = abs(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -abs(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;
    ox0 = x0;
    oy0 = y0;
    hw = width / 2;
    for (;;) {
        c = c0;
        if (grad) {
            float cd;
            cd = grit_internal_sqrtf((float)((x0 - ox0) * (x0 - ox0) + (y0 - oy0) * (y0 - oy0)));
            c = grit_internal_draw_lerp(c0, c1, total > 0.0f ? cd / total : 0.0f);
        }
        for (ox = -hw; ox <= hw; ++ox)
            for (oy = -hw; oy <= hw; ++oy)
                grit_draw_pixel(d, x0 + ox, y0 + oy, c);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void
grit_draw_circle(GritDraw *d, int cx, int cy, int radius, uint32_t color)
{
    int x, y, r2;
    if (radius < 0)
        return;
    r2 = radius * radius;
    for (y = -radius; y <= radius; ++y) {
        for (x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= r2)
                grit_draw_pixel(d, cx + x, cy + y, color);
        }
    }
}
