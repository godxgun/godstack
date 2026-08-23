typedef struct Arena {
    unsigned char *buf;
    usize buf_len;
    usize curr_offset;
    usize prev_offset;
} Arena;

PODEF void      p_arena_create(Arena *a, void *backing_buffer, usize backing_buffer_length);
PODEF void*     p_arena_alloc_align(Arena *a, usize s, usize align);
PODEF void*     p_arena_alloc(Arena *a, usize s);
PODEF void*     p_arena_resize_align(Arena *a, void *old_memory, usize old_size, usize new_size, usize align);
PODEF void*     p_arena_resize(Arena *a, void *old_memory, usize old_size, usize new_size);
PODEF void      p_arena_free_all(Arena *a);
PODEF void      p_arena_destroy(Arena* a);

typedef struct { u8 padding; } Stack_Allocation_Header;
typedef struct Stack {
    unsigned char *buf;
    usize buf_len;
    usize offset;
} Stack;

PODEF void      p_stack_create(Stack *s, void *backing_buffer, usize backing_buffer_length);
PODEF usize     calc_padding_with_header(uintptr ptr, uintptr alignment, usize header_size);
PODEF void*     p_stack_alloc(Stack *s, usize len, usize alignment);
PODEF void      p_stack_free(Stack *s, void *ptr);
PODEF void      p_stack_free_all(Stack* s);
PODEF void      p_stack_destroy(Stack* s);

typedef struct Pool_Free_Node Pool_Free_Node;

typedef struct Pool {
    unsigned char *buf;
    usize chunk_size;
    usize pool_size;
    Pool_Free_Node *head;
} Pool;

struct Pool_Free_Node { struct Pool_Free_Node *next; };
PODEF void      p_pool_create(Pool *p, void *buf, usize buf_len, usize chunk_size, usize chunk_align);
PODEF void*     p_pool_alloc(Pool *p);
PODEF void      p_pool_free(Pool *p, void *ptr);
PODEF void      p_pool_free_all(Pool *p);
PODEF void      p_pool_destroy(Pool *p);
