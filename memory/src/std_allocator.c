#include "snmemory/std_allocator.h"

#include <sncore/defines.h>
#include <sncore/utils.h>
#include <stdlib.h>

/* The alignment shift is stashed in the byte just below the aligned address, so
   the block handed to malloc has to leave room for it plus the worst case
   alignment padding. */
#define ALIGN_BYTE(ptr) ((void *)((uint64_t)(ptr) - 1))
#define GET_ALIGN_SHIFT(ptr) (sn_read_from_bytes(ALIGN_BYTE(ptr), true))
#define SET_ALIGN_SHIFT(ptr, shift) (sn_write_to_bytes(ALIGN_BYTE(ptr), (shift), true))

static void *malloc_wrapper(void *data, uint64_t size, uint64_t align) {
    SN_UNUSED(data);
    SN_UNUSED(align);
    void *ptr = malloc(size + align);
    if (!ptr) return ptr;

    void *new_ptr = (void *)SN_GET_NEXT_ALIGNED(ptr, align);
    SET_ALIGN_SHIFT(new_ptr, SN_PTR_DIFF(new_ptr, ptr));
    return new_ptr;
}

static void *realloc_wrapper(void *data, void *ptr, uint64_t new_size, uint64_t align) {
    SN_UNUSED(data);

    uint8_t *p = (uint8_t *)ptr;
    uint64_t align_shift = GET_ALIGN_SHIFT(ptr);
    p -= align_shift;
    void *new_p = realloc(p, new_size + align);
    if (!new_p) return new_p;
    void *new_ptr = (void *)SN_GET_NEXT_ALIGNED(new_p, align);
    SET_ALIGN_SHIFT(new_ptr, SN_PTR_DIFF(new_ptr, new_p));
    return new_ptr;
}

static void free_wrapper(void *data, void *ptr) {
    SN_UNUSED(data);

    uint8_t *p = (uint8_t *)ptr;
    uint64_t align_shift = GET_ALIGN_SHIFT(ptr);
    p -= align_shift;
    free(p);
}

SnMemoryAllocator sn_std_allocator = {
    .data = NULL,
    .alloc = malloc_wrapper,
    .realloc = realloc_wrapper,
    .free = free_wrapper,
};
