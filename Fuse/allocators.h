#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifndef DEFAULT_ALIGNMENT 
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

typedef struct Arena {
    unsigned char *buf;
    size_t buf_len;
    size_t curr_offset;
    size_t prev_offset;
} Arena;

typedef struct { uint8_t padding; } Stack_Allocation_Header;
typedef struct Stack {
    unsigned char *buf;
    size_t buf_len;
    size_t offset;
} Stack;

typedef struct Pool_Free_Node Pool_Free_Node;

typedef struct Pool {
    unsigned char *buf;
    size_t chunk_size;
    size_t pool_size;
    Pool_Free_Node *head;
} Pool;

struct Pool_Free_Node { struct Pool_Free_Node *next; };

void      al_arena_create(Arena *a, void *backing_buffer, size_t backing_buffer_length);
void*     al_arena_alloc_align(Arena *a, size_t s, size_t align);
void*     al_arena_alloc(Arena *a, size_t s);
void*     al_arena_resize_align(Arena *a, void *old_memory, size_t old_size, size_t new_size, size_t align);
void*     al_arena_resize(Arena *a, void *old_memory, size_t old_size, size_t new_size);
void      al_arena_free_all(Arena *a);
void      al_arena_destroy(Arena* a);

void      al_stack_create(Stack *s, void *backing_buffer, size_t backing_buffer_length);
size_t    al_calc_padding_with_header(uintptr_t ptr, uintptr_t alignment, size_t header_size);
void*     al_stack_alloc(Stack *s, size_t len, size_t alignment);
void      al_stack_free(Stack *s, void *ptr);
void      al_stack_free_all(Stack* s);
void      al_stack_destroy(Stack* s);

void      al_pool_create(Pool *p, void *buf, size_t buf_len, size_t chunk_size, size_t chunk_align);
void*     al_pool_alloc(Pool *p);
void      al_pool_free(Pool *p, void *ptr);
void      al_pool_free_all(Pool *p);
void      al_pool_destroy(Pool *p);


#endif
