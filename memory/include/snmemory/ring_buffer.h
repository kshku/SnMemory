#pragma once

#include <sncore/defines.h>

#include <stdbool.h>

typedef struct SnRingBuffer {
    uint8_t  *buffer;
    uint64_t  size;
    uint64_t  write_offset;
    uint64_t  read_offset;
} SnRingBuffer;

SN_INLINE void sn_ring_buffer_init(SnRingBuffer *rb, void *memory, uint64_t size) {
    rb->buffer = (uint8_t *)memory;
    rb->size = size;
    rb->write_offset = 0;
    rb->read_offset = 0;
}

SN_INLINE uint64_t sn_ring_buffer_free_size(SnRingBuffer *rb) {
    uint64_t read = rb->read_offset;
    uint64_t write = rb->write_offset;
    if (read > write) {
        return read - write - 1;
    }
    return (rb->size - write) + read - 1;
}

SN_INLINE bool sn_ring_buffer_is_empty(SnRingBuffer *rb) {
    return rb->write_offset == rb->read_offset;
}

SN_INLINE void *sn_ring_buffer_allocate(SnRingBuffer *rb, uint64_t size, uint64_t alignment) {
    uint64_t read = rb->read_offset;
    uint64_t write = rb->write_offset;

    void *unaligned = rb->buffer + write;
    void *aligned_ptr = SN_GET_ALIGNED(unaligned, alignment);

    uint64_t aligned = (uint8_t *)aligned_ptr - rb->buffer;
    uint64_t padded_size = size + (aligned - write);

    if (sn_ring_buffer_free_size(rb) < padded_size) return NULL;

    if ((write >= read && write + padded_size <= rb->size)
        || (write < read && write + padded_size < read)) {
        rb->write_offset = write + padded_size;
        return aligned_ptr;
    }

    if (write >= read && read > padded_size) {
        void *begin_aligned = SN_GET_ALIGNED(rb->buffer, alignment);
        uint64_t begin_offset = (uint8_t *)begin_aligned - rb->buffer;
        rb->write_offset = begin_offset + size;
        return begin_aligned;
    }

    return NULL;
}

SN_INLINE void sn_ring_buffer_advance_read(SnRingBuffer *rb, uint64_t size) {
    uint64_t new_read = rb->read_offset + size;
    if (new_read >= rb->size) {
        new_read -= rb->size;
    }
    rb->read_offset = new_read;
}

SN_INLINE void *sn_ring_buffer_read_ptr(SnRingBuffer *rb) {
    return rb->buffer + rb->read_offset;
}

SN_INLINE void sn_ring_buffer_reset(SnRingBuffer *rb) {
    rb->write_offset = 0;
    rb->read_offset = 0;
}
