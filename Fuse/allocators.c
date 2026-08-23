#pragma once

#include "allocators.h"

static inline bool is_power_of_two(uintptr_t x) {
	return (x & (x-1)) == 0;
}

static inline uintptr_t align_forward(uintptr_t ptr, size_t align) {
    uintptr_t p, a, remainder;
    assert(is_power_of_two(align));

    p = ptr;
    a = (uintptr_t)align;
	remainder = p & (a-1);

    return (remainder != 0) ? p + a - remainder : p;
}


void
al_arena_create(Arena *a, void *backing_buffer, size_t backing_buffer_length) 
{
	a->buf = (unsigned char *)backing_buffer;
	a->buf_len = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

void*
al_arena_alloc_align(Arena *a, size_t s, size_t align) 
{

	// Align 'curr_offset' forward to the specified alignment
	uintptr_t curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
	uintptr_t offset = align_forward(curr_ptr, align);
	offset -= (uintptr_t)a->buf; // Change to relative offset

	// Check to see if the backing memory has space left
	if (offset+s <= a->buf_len) {
		void *ptr = &a->buf[offset];
		a->prev_offset = offset;
		a->curr_offset = offset+s;

		memset(ptr, 0, s);
		return ptr;
	}

    return NULL;
}

void*
al_arena_alloc(Arena *a, size_t s) {
    return al_arena_alloc_align(a, s, DEFAULT_ALIGNMENT);
}

void*
al_arena_resize_align(Arena *a, void *old_memory, size_t old_size, size_t new_size, size_t align)
{
    assert(is_power_of_two(align));
    unsigned char *old_mem = (unsigned char *)old_memory;

    if (old_mem == NULL || old_size == 0) {
        return al_arena_alloc_align(a, new_size, align);
    } else if (a->buf <= old_mem && old_mem < a->buf+a->buf_len) {
        if (a->buf+a->prev_offset == old_mem) {
            a->curr_offset = a->prev_offset + new_size;
            if (new_size > old_size) {
                // Zero the new memory by default
                memset(&a->buf[a->curr_offset], 0, new_size-old_size);
            }
            return old_memory;
        } else {
            void *new_memory = al_arena_alloc_align(a, new_size, align);
            size_t copy_size = old_size < new_size ? old_size : new_size;
            memmove(new_memory, old_memory, copy_size);
            return new_memory;
        }

    } else {
        assert(0 && "Memory is out of bounds of the buffer in this arena");
        return NULL;
    }
}

void*
al_arena_resize(Arena *a, void *old_memory, size_t old_size, size_t new_size)
{
    return al_arena_resize_align(a, old_memory, old_size, new_size, DEFAULT_ALIGNMENT);
}

void
al_arena_free_all(Arena *a)
{
    a->curr_offset = 0;
    a->prev_offset = 0;
}

void
al_arena_destroy(Arena* a) 
{
    a->buf_len = 0;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

void
al_stack_create(Stack *s, void *backing_buffer, size_t backing_buffer_length)
{
    s->buf = (unsigned char*) backing_buffer;
    s->buf_len = backing_buffer_length;
    s->offset = 0;
}

size_t
al_calc_padding_with_header(uintptr_t ptr, uintptr_t alignment, size_t header_size) 
{
	uintptr_t p, a, modulo, padding, needed_space;

	assert(is_power_of_two(alignment));

	p = ptr;
	a = alignment;
	modulo = p & (a-1); // (p % a) as it assumes alignment is a power of two

	padding = 0;
	needed_space = 0;

	if (modulo != 0) { // Same logic as 'align_forward'
		padding = a - modulo;
	}

	needed_space = (uintptr_t)header_size;

	if (padding < needed_space) {
		needed_space -= padding;

		if ((needed_space & (a-1)) != 0) {
			padding += a * (1+(needed_space/a));
		} else {
			padding += a * (needed_space/a);
		}
	}

	return (size_t)padding;
}

void*
al_stack_alloc(Stack *s, size_t len, size_t alignment) 
{
	uintptr_t curr_addr, next_addr;
	size_t padding;
	Stack_Allocation_Header *header;

	assert(is_power_of_two(alignment));

	if (alignment > 128) {
		// As the padding is 8 bits (1 byte), the largest alignment that can
		// be used is 128 bytes
		alignment = 128;
	}

	curr_addr = (uintptr_t)s->buf + (uintptr_t)s->offset;
	padding = al_calc_padding_with_header(curr_addr, (uintptr_t)alignment, sizeof(Stack_Allocation_Header));
	if (s->offset + padding + len > s->buf_len) {
		// Stack allocator is out of memory
		return NULL;
	}
	s->offset += padding;

	next_addr = curr_addr + (uintptr_t)padding;
	header = (Stack_Allocation_Header *)(next_addr - sizeof(Stack_Allocation_Header));
	header->padding = (uint8_t)padding;

	s->offset += len;

	return memset((void *)next_addr, 0, len);
}

void
al_stack_free(Stack *s, void *ptr) 
{
	if (ptr != NULL) {
		uintptr_t start, end, curr_addr;
		Stack_Allocation_Header *header;
		size_t prev_offset;

		start = (uintptr_t)s->buf;
		end = start + (uintptr_t)s->buf_len;
		curr_addr = (uintptr_t)ptr;

		if (!(start <= curr_addr && curr_addr < end)) {
			assert(0 && "Out of bounds memory address passed to stack allocator (free)");
			return;
		}

		if (curr_addr >= start+(uintptr_t)s->offset) {
			// Allow double frees
			return;
		}

		header = (Stack_Allocation_Header *)(curr_addr - sizeof(Stack_Allocation_Header));
		prev_offset = (size_t)(curr_addr - (uintptr_t)header->padding - start);

		s->offset = prev_offset;
	}
}

void
al_stack_free_all(Stack* s)
{
    s->offset = 0;
}

void
al_stack_destroy(Stack* s)
{
    s->buf = 0;
    s->buf_len = 0;
    s->offset = 0;
}

void
al_pool_create(Pool *p, void *buf, size_t buf_len, size_t chunk_size, size_t chunk_align) 
{
    /* align start */
    uintptr_t start = (uintptr_t) buf;
    uintptr_t aligned = align_forward(start, chunk_align);
    buf_len -= (aligned - start);

    /* align chunk size_t */
    chunk_size = (size_t) align_forward(chunk_size, chunk_align); 

	assert(chunk_size >= sizeof(Pool_Free_Node) && "Chunk size_t is too small");
	assert(buf_len >= chunk_size && "Backing buffer length is smaller than the chunk size");
    
    p->buf = (unsigned char *) buf;
    p->pool_size = buf_len;
    p->chunk_size = chunk_size;
    p->head = NULL;

    al_pool_free_all(p);
}

void*
al_pool_alloc(Pool *p)
{
    Pool_Free_Node *node = p->head;

    assert(node != NULL && "Pool allocator has no free memory");
    
    p->head = p->head->next;

    return memset(node, 0 , p->chunk_size);
}

void 
al_pool_free(Pool *p, void *ptr) 
{
    Pool_Free_Node *node;

	void *start = p->buf;
	void *end = &p->buf[p->pool_size];

	if (ptr == NULL) {
		return;
	}

	if (!(start <= ptr && ptr < end)) {
		assert(0 && "Memory is out of bounds of the buffer in this pool");
		return;
	}

	node = (Pool_Free_Node *)ptr;
    node->next = p->head;
	p->head = node;
}

void 
al_pool_free_all(Pool *p) 
{
	size_t chunk_count = p->pool_size / p->chunk_size;
	size_t i;

	for (i = 0; i < chunk_count; i++) {
		void *ptr = &p->buf[i * p->chunk_size];
		Pool_Free_Node *node = (Pool_Free_Node *)ptr;

		// Push free node onto thte free list
		node->next = p->head;
		p->head = node;
	}
}

void 
al_pool_destroy(Pool *p) 
{
    p->buf = NULL;
    p->pool_size = 0;
    p->chunk_size = 0;
    p->head = NULL;
}

