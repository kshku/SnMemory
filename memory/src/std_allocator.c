#include "snmemory/std_allocator.h"

#include <sncore/defines.h>
#include <sncore/utils.h>
#include <stdlib.h>
#include <string.h>

/* Every block starts with a header holding the payload size. realloc is allowed
   to move the block, and when it does the payload has to be moved with it, so
   the allocator has to know how much was there. */
typedef struct SnStdAllocHeader {
    uint64_t size;
} SnStdAllocHeader;

/* A uint64_t needs at most 10 groups of 7 bits. The distance from the raw block
   to the payload is written as a variable length integer in the bytes just below
   the payload, and that distance is never smaller than the headroom, so the
   headroom has to leave room for the longest encoding plus the header. */
#define SN_STD_ALLOC_MAX_VARINT 10
#define SN_STD_ALLOC_HEADROOM (sizeof(SnStdAllocHeader) + SN_STD_ALLOC_MAX_VARINT)

/* The raw block is the header, the headroom, the payload, and enough slack for
   the alignment to push the payload out by up to align bytes. Returns false if
   that does not fit in a size_t, which is what malloc and realloc would do
   anyway, only less quietly. */
static bool raw_size(uint64_t size, uint64_t align, uint64_t *out) {
    if (align > UINT64_MAX - size - SN_STD_ALLOC_HEADROOM) return false;
    *out = size + align + SN_STD_ALLOC_HEADROOM;
    return true;
}

#define OFFSET_BYTE(ptr) ((void *)((uint8_t *)(ptr) - 1))
#define GET_OFFSET(ptr) (sn_read_from_bytes(OFFSET_BYTE(ptr), true))
#define SET_OFFSET(ptr, offset) (sn_write_to_bytes(OFFSET_BYTE(ptr), (offset), true))

static void *align_payload(void *raw, uint64_t align) {
    return (void *)SN_GET_NEXT_ALIGNED((uint8_t *)raw + SN_STD_ALLOC_HEADROOM, align);
}

static void *raw_to_payload(void *raw, uint64_t align) {
    void *payload = align_payload(raw, align);
    SET_OFFSET(payload, SN_PTR_DIFF(payload, raw));
    return payload;
}

static void *payload_to_raw(void *payload) {
    return (uint8_t *)payload - GET_OFFSET(payload);
}

static void *malloc_wrapper(void *data, uint64_t size, uint64_t align) {
    SN_UNUSED(data);

    uint64_t raw_size_bytes;
    if (!raw_size(size, align, &raw_size_bytes)) return NULL;

    void *raw = malloc(raw_size_bytes);
    if (!raw) return raw;

    ((SnStdAllocHeader *)raw)->size = size;
    return raw_to_payload(raw, align);
}

static void *realloc_wrapper(void *data, void *ptr, uint64_t new_size, uint64_t align) {
    SN_UNUSED(data);

    void *raw = payload_to_raw(ptr);
    uint64_t old_size = ((SnStdAllocHeader *)raw)->size;
    /* realloc keeps the contents at the start of the new block, so the old
       payload is still reachable there under the offset it had. */
    uint64_t old_offset = SN_PTR_DIFF(ptr, raw);

    uint64_t raw_size_bytes;
    if (!raw_size(new_size, align, &raw_size_bytes)) return NULL;

    raw = realloc(raw, raw_size_bytes);
    if (!raw) return raw;

    void *new_payload = align_payload(raw, align);
    /* The offset goes in the byte below the new payload, which can land inside
       the run that still has to be copied out of the old offset. Moving the
       bytes first is what keeps the two from overwriting each other. */
    if (new_payload != ptr)
        memmove(new_payload, (uint8_t *)raw + old_offset, SN_MIN(old_size, new_size));

    SET_OFFSET(new_payload, SN_PTR_DIFF(new_payload, raw));
    ((SnStdAllocHeader *)raw)->size = new_size;

    return new_payload;
}

static void free_wrapper(void *data, void *ptr) {
    SN_UNUSED(data);

    free(payload_to_raw(ptr));
}

SnMemoryAllocator sn_std_allocator = {
    .data = NULL,
    .alloc = malloc_wrapper,
    .realloc = realloc_wrapper,
    .free = free_wrapper,
};
