PODEF bool
is_power_of_two(uintptr x) {
	return (x & (x-1)) == 0;
}

PODEF uintptr
align_forward(uintptr ptr, usize align) 
{
    uintptr p, a, remainder;
    assert(is_power_of_two(align));

    p = ptr;
    a = (uintptr)align;
	remainder = p & (a-1);

    return (remainder != 0) ? p + a - remainder : p;
}

PODEF void
p_arena_create(Arena *a, void *backing_buffer, usize backing_buffer_length) 
{
	a->buf = (unsigned char *)backing_buffer;
	a->buf_len = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

PODEF void*
p_arena_alloc_align(Arena *a, usize s, usize align) 
{

	// Align 'curr_offset' forward to the specified alignment
	uintptr curr_ptr = (uintptr)a->buf + (uintptr)a->curr_offset;
	uintptr offset = align_forward(curr_ptr, align);
	offset -= (uintptr)a->buf; // Change to relative offset

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

PODEF void*
p_arena_alloc(Arena *a, usize s) {
    return p_arena_alloc_align(a, s, DEFAULT_ALIGNMENT);
}

PODEF void*
p_arena_resize_align(Arena *a, void *old_memory, usize old_size, usize new_size, usize align)
{
    assert(is_power_of_two(align));
    unsigned char *old_mem = (unsigned char *)old_memory;

    if (old_mem == NULL || old_size == 0) {
        return p_arena_alloc_align(a, new_size, align);
    } else if (a->buf <= old_mem && old_mem < a->buf+a->buf_len) {
        if (a->buf+a->prev_offset == old_mem) {
            a->curr_offset = a->prev_offset + new_size;
            if (new_size > old_size) {
                // Zero the new memory by default
                memset(&a->buf[a->curr_offset], 0, new_size-old_size);
            }
            return old_memory;
        } else {
            void *new_memory = p_arena_alloc_align(a, new_size, align);
            size_t copy_size = old_size < new_size ? old_size : new_size;
            memmove(new_memory, old_memory, copy_size);
            return new_memory;
        }

    } else {
        assert(0 && "Memory is out of bounds of the buffer in this arena");
        return NULL;
    }
}

PODEF void*
p_arena_resize(Arena *a, void *old_memory, usize old_size, usize new_size)
{
    return p_arena_resize_align(a, old_memory, old_size, new_size, DEFAULT_ALIGNMENT);
}

PODEF void
p_arena_free_all(Arena *a)
{
    a->curr_offset = 0;
    a->prev_offset = 0;
}

PODEF void
p_arena_destroy(Arena* a) 
{
    a->buf_len = 0;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

PODEF void
p_stack_create(Stack *s, void *backing_buffer, usize backing_buffer_length)
{
    s->buf = (unsigned char*) backing_buffer;
    s->buf_len = backing_buffer_length;
    s->offset = 0;
}

PODEF usize
calc_padding_with_header(uintptr ptr, uintptr alignment, usize header_size) 
{
	uintptr p, a, modulo, padding, needed_space;

	assert(is_power_of_two(alignment));

	p = ptr;
	a = alignment;
	modulo = p & (a-1); // (p % a) as it assumes alignment is a power of two

	padding = 0;
	needed_space = 0;

	if (modulo != 0) { // Same logic as 'align_forward'
		padding = a - modulo;
	}

	needed_space = (uintptr)header_size;

	if (padding < needed_space) {
		needed_space -= padding;

		if ((needed_space & (a-1)) != 0) {
			padding += a * (1+(needed_space/a));
		} else {
			padding += a * (needed_space/a);
		}
	}

	return (usize)padding;
}

PODEF void*
p_stack_alloc(Stack *s, usize len, usize alignment) 
{
	uintptr curr_addr, next_addr;
	usize padding;
	Stack_Allocation_Header *header;

	assert(is_power_of_two(alignment));

	if (alignment > 128) {
		// As the padding is 8 bits (1 byte), the largest alignment that can
		// be used is 128 bytes
		alignment = 128;
	}

	curr_addr = (uintptr)s->buf + (uintptr)s->offset;
	padding = calc_padding_with_header(curr_addr, (uintptr)alignment, sizeof(Stack_Allocation_Header));
	if (s->offset + padding + len > s->buf_len) {
		// Stack allocator is out of memory
		return NULL;
	}
	s->offset += padding;

	next_addr = curr_addr + (uintptr)padding;
	header = (Stack_Allocation_Header *)(next_addr - sizeof(Stack_Allocation_Header));
	header->padding = (u8)padding;

	s->offset += len;

	return memset((void *)next_addr, 0, len);
}

PODEF void
p_stack_free(Stack *s, void *ptr) 
{
	if (ptr != NULL) {
		uintptr start, end, curr_addr;
		Stack_Allocation_Header *header;
		usize prev_offset;

		start = (uintptr)s->buf;
		end = start + (uintptr)s->buf_len;
		curr_addr = (uintptr)ptr;

		if (!(start <= curr_addr && curr_addr < end)) {
			assert(0 && "Out of bounds memory address passed to stack allocator (free)");
			return;
		}

		if (curr_addr >= start+(uintptr)s->offset) {
			// Allow double frees
			return;
		}

		header = (Stack_Allocation_Header *)(curr_addr - sizeof(Stack_Allocation_Header));
		prev_offset = (usize)(curr_addr - (uintptr)header->padding - start);

		s->offset = prev_offset;
	}
}

PODEF void
p_stack_free_all(Stack* s)
{
    s->offset = 0;
}

PODEF void
p_stack_destroy(Stack* s)
{
    s->buf = 0;
    s->buf_len = 0;
    s->offset = 0;
}

PODEF void
p_pool_create(Pool *p, void *buf, usize buf_len, usize chunk_size, usize chunk_align) 
{
    /* align start */
    uintptr start = (uintptr) buf;
    uintptr aligned = align_forward(start, chunk_align);
    buf_len -= (aligned - start);

    /* align chunk usize */
    chunk_size = (usize) align_forward(chunk_size, chunk_align); 

	assert(chunk_size >= sizeof(Pool_Free_Node) && "Chunk usize is too small");
	assert(buf_len >= chunk_size && "Backing buffer length is smaller than the chunk size");
    
    p->buf = (unsigned char *) buf;
    p->pool_size = buf_len;
    p->chunk_size = chunk_size;
    p->head = NULL;

    p_pool_free_all(p);
}

PODEF void*
p_pool_alloc(Pool *p)
{
    Pool_Free_Node *node = p->head;

    assert(node != NULL && "Pool allocator has no free memory");
    
    p->head = p->head->next;

    return memset(node, 0 , p->chunk_size);
}

PODEF void 
p_pool_free(Pool *p, void *ptr) 
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

PODEF void 
p_pool_free_all(Pool *p) 
{
	usize chunk_count = p->pool_size / p->chunk_size;
	usize i;

	for (i = 0; i < chunk_count; i++) {
		void *ptr = &p->buf[i * p->chunk_size];
		Pool_Free_Node *node = (Pool_Free_Node *)ptr;

		// Push free node onto thte free list
		node->next = p->head;
		p->head = node;
	}
}

PODEF void 
p_pool_destroy(Pool *p) 
{
    p->buf = NULL;
    p->pool_size = 0;
    p->chunk_size = 0;
    p->head = NULL;
}

