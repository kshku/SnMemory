#pragma once

#include "snmemory/defines.h"

#include "snmemory/linear.h"

/**
 * @struct snFrameAllocator
 * @brief Frame-based memory allocator.
 *
 * Allocations made between begin() and end() calls
 * are automatically freed at the end of the frame.
 *
 * @note
 * - Backed by a linear allocator
 * - Not thread-safe
 * - Intended for per-frame temporary allocations
 */
typedef struct snFrameAllocator {
    snLinearAllocator arena; /**< Underlying linear allocator */
    snMemoryMark frame_mark; /**< Mark at frame begin */
} snFrameAllocator;

/**
 * @brief Initialize frame allocator.
 *
 * @param alloc Pointer to frame allocator
 * @param mem Memory buffer to manage
 * @param size Size of memory buffer
 *
 * @return true on success, false on failure
 */
SN_FORCE_INLINE bool sn_frame_allocator_init(snFrameAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !sn_linear_allocator_init(&alloc->arena, mem, size)) return false;

    alloc->frame_mark = NULL;
    return true;
}

/**
 * @brief Deinitialize frame allocator.
 *
 * @param alloc Pointer to frame allocator
 *
 * @note Does not free memory buffer
 */
SN_FORCE_INLINE void sn_frame_allocator_deinit(snFrameAllocator *alloc) {
    sn_linear_allocator_deinit(&alloc->arena);
    alloc->frame_mark = NULL;
}

/**
 * @brief Begin a new frame.
 *
 * @param alloc Pointer to frame allocator
 *
 * All allocations after this call belong to the frame.
 */
SN_FORCE_INLINE void sn_frame_allocator_begin(snFrameAllocator *alloc) {
    alloc->frame_mark = sn_linear_allocator_get_memory_mark(&alloc->arena);
}

/**
 * @brief End the current frame.
 *
 * @param alloc Pointer to frame allocator
 *
 * Frees all allocations made since begin().
 */
SN_FORCE_INLINE void sn_frame_allocator_end(snFrameAllocator *alloc) {
    SN_ASSERT(alloc->frame_mark != NULL);
    sn_linear_allocator_free_to_memory_mark(&alloc->arena, alloc->frame_mark);
    alloc->frame_mark = NULL;
}

/**
 * @brief Allocate memory for the current frame.
 *
 * @param alloc Pointer to frame allocator
 * @param size Number of bytes to allocate
 * @param align Alignment requirement
 *
 * @return Pointer to allocated memory or NULL on failure
 */
SN_FORCE_INLINE void *sn_frame_allocator_allocate(snFrameAllocator *alloc, uint64_t size, uint64_t align) {
    return sn_linear_allocator_allocate(&alloc->arena, size, align);
}

/**
 * @brief Get memory used in current frame.
 *
 * @param alloc Pointer to frame allocator
 */
SN_FORCE_INLINE uint64_t sn_frame_allocator_get_frame_usage(snFrameAllocator *alloc) {
    return sn_linear_allocator_get_allocated_size(&alloc->arena);
}

/**
 * @brief Get remaining memory in frame allocator.
 *
 * @param alloc Pointer to frame allocator
 */
SN_FORCE_INLINE uint64_t sn_frame_allocator_get_remaining_size(snFrameAllocator *alloc) {
    return sn_linear_allocator_get_remaining_size(&alloc->arena);
}

