#pragma once

#include <sncore/defines.h>

/**
 * @struct SnRignBufferAllocator
 */
typedef struct SnRingBufferAllocator {
    uint8_t *buffer;
    uint64_t size;
    uint64_t write_offset;
    uint64_t read_offset;
} SnRingBufferAllocator;

SN_FORCE_INLINE bool sn_ring_buffer_allocator_init(SnRingBufferAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    *alloc = (SnRingBufferAllocator){
        .buffer = (uint8_t *)mem,
        .size = size,
        .write_offset = 0,
        .read_offset = 0,
    };

    return true;
}

SN_FORCE_INLINE uint64_t sn_ring_buffer_allocator_free_size(SnRingBufferAllocator *alloc) {
    if (!alloc) return 0;

    if (alloc->read_offset > alloc->write_offset)
        return alloc->read_offset - alloc->write_offset - 1;
    return (alloc->size - alloc->write_offset) + alloc->read_offset - 1;
}

SN_FORCE_INLINE bool sn_ring_buffer_allocator_is_empty(SnRingBufferAllocator *alloc) {
    if (!alloc) return false;

    return alloc->write_offset == alloc->read_offset;
}

SN_INLINE void *sn_ring_buffer_allocator_allocate(SnRingBufferAllocator *alloc, uint64_t size, uint64_t align) {
    if (!alloc || !align || !size) return NULL;

    size += align;

    size_t free_size = sn_ring_buffer_allocator_free_size(alloc);

    if (free_size < size) return NULL;

    if ((alloc->write_offset >= alloc->read_offset && alloc->write_offset + size <= alloc->size)
        || (alloc->write_offset < alloc->read_offset && alloc->write_offset + size < alloc->read_offset)) {
        void *p = ((char *)alloc->buffer) + alloc->write_offset;
        void *aligned = (void *)SN_GET_ALIGNED(p, align);
        alloc->write_offset += size - align + SN_PTR_DIFF(aligned, p);
        return aligned;
    }

    // Wrapping logic
    if (alloc->write_offset >= alloc->read_offset && alloc->read_offset > size) {
        void *aligned = (void *)SN_GET_ALIGNED(alloc->buffer, align);
        alloc->write_offset = size - align + SN_PTR_DIFF(aligned, alloc->buffer);
        return aligned;
    }

    return NULL;
}

SN_FORCE_INLINE void sn_ring_buffer_allocator_advance_read(SnRingBufferAllocator *alloc, uint64_t size) {
    if (!alloc || !size) return;

    alloc->read_offset += size;
    if (alloc->read_offset >= alloc->size) alloc->read_offset -= alloc->size;
}

SN_FORCE_INLINE void *sn_ring_buffer_allocator_read_ptr(SnRingBufferAllocator *alloc) {
    if (!alloc) return NULL;
    return alloc->buffer + alloc->read_offset;
}

SN_FORCE_INLINE void sn_ring_buffer_allocator_reset(SnRingBufferAllocator *alloc) {
    if (!alloc) return;
    alloc->write_offset = 0;
    alloc->read_offset = 0;
}

