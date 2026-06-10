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

    uint64_t aligned = (uint64_t)SN_GET_ALIGNED(rb->buffer + write, alignment);
    if (aligned > write) {
        aligned = (uint64_t)(rb->buffer + write) + alignment - 1;
        aligned = (aligned & ~(alignment - 1)) - (uint64_t)rb->buffer;
    } else {
        aligned = write;
    }
    uint64_t new_write = aligned + size;

    bool wraps = false;
    if (new_write > rb->size) {
        wraps = true;
        new_write = size;
        aligned = 0;
    }

    if (wraps) {
        if (read <= write) {
            if (read <= new_write) return NULL;
        }
    } else {
        if (read > write) {
            if (aligned < read && new_write > read) return NULL;
        } else if (read <= aligned && new_write >= read) {
            wraps = true;
            new_write = size;
            aligned = 0;
            if (read <= new_write) return NULL;
        }
    }

    void *ptr = rb->buffer + aligned;
    rb->write_offset = new_write;
    return ptr;
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
