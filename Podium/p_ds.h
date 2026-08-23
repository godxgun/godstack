#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#define ARRAY_GROW_FACTOR 2

typedef struct {
    u32 size;
    u32 capacity;
} DynamicArrayHeader;

#define DARRAY_INIT_CAPACITY 64
#define p_darray_push(array, var)                                              \
    do {                                                                         \
        if (array == NULL) {                                                       \
            DynamicArrayHeader *header =                                             \
            malloc(DARRAY_INIT_CAPACITY * sizeof *array + sizeof *header);     \
            header->size = 0;                                                        \
            header->capacity = DARRAY_INIT_CAPACITY;                                 \
            array = (void *)(header + 1);                                            \
        }                                                                          \
        DynamicArrayHeader *header = (DynamicArrayHeader *)(array) - 1;            \
        if (header->size >= header->capacity - 1) {                                \
            unsigned long new_capacity = header->capacity * 2;                       \
            DynamicArrayHeader *new_header =                                         \
            realloc(header, new_capacity * sizeof *array + sizeof *header);    \
            if (!new_header)                                                         \
            break;                                                                 \
            header = new_header;                                                     \
            array = (void *)(header + 1);                                            \
            header->capacity = new_capacity;                                         \
        }                                                                          \
        (array)[header->size++] = var;                                             \
    } while (0)

#define p_darray_len(array) ((array) ? ((DynamicArrayHeader *)(array) - 1)->size : 0)
#define p_darray_destroy(array)                                                \
{                                                                            \
    free((DynamicArrayHeader *)(array) - 1);                                 \
}

#define p_darray_shrink(array) \
    do { \
        DynamicArrayHeader *header = (array) ? ((DynamicArrayHeader *)(array) - 1) : 0;\
        if (header) {\
            if (header->capacity > 4) {\
                unsigned long new_capacity = (header->capacity >> 1) + (header->capacity & 1); \
                void *new_header = realloc(header, new_capacity * sizeof *array + sizeof *header); \
                if (!new_header) break; \
                header = new_header; \
                array = (void *)(header + 1); \
                header->capacity = new_capacity; \
            }\
        }\
    } while (0)
