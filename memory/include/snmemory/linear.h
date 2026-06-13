#pragma once

#include <sncore/defines.h>
#include <sncore/types.h>

/**
 * @struct SnLinearAllocator
 * @brief Struct to store linear allocator context.
 *
 * @note None of the sn_linear_allocator* functions is thread-safe.
 */
typedef struct SnLinearAllocator {
    uint8_t *mem; /**< Pointer to memory */
    uint8_t *top; /**< Pointer to top of memory */
    uint64_t size; /**< Size of the memory */
} SnLinearAllocator;

/**
 * @brief Memory mark used to free upto the mark.
 */
typedef uint8_t *snMemoryMark;

/**
 * @brief Initialize a linear allocator.
 *
 * Initialize linear allocator for managing the given memory.
 *
 * @param alloc Pointer to the allocator context.
 * @param mem The memory to handle.
 * @param size Size of the memeory to handle.
 *
 * @return Returns true on success, false otherwise.
 */
SN_FORCE_INLINE bool sn_linear_allocator_init(SnLinearAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    *alloc = (SnLinearAllocator){
        .mem = (uint8_t *)mem,
        .top = (uint8_t *)mem,
        .size = size,
    };

    return true;
}

/**
 * @breif Deinitialize linear allocator.
 *
 * @note Doesn't deal with the memory passed to allocator.
 * User is responsible for handing the memory.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_linear_allocator_deinit(SnLinearAllocator *alloc) {
    if (!alloc) return;
    *alloc = (SnLinearAllocator){0};
}

/**
 * @brief Increase the size of memory managed by the allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param mem Pointer to the new memory (must be right next to current memory).
 * @param size Size of the new memory.
 */
SN_FORCE_INLINE void
    sn_linear_allocator_increase_memory_size(SnLinearAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc) return;
    SN_ASSERT(alloc->mem + alloc->size == mem);
    alloc->size += size;
}

/**
 * @brief Allocate from the linear allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param size Number of bytes to allocate.
 * @param align The alignment.
 *
 * @return Returns pointer to allocated memory or NULL no failure.
 */
SN_INLINE void *sn_linear_allocator_allocate(SnLinearAllocator *alloc, uint64_t size, uint64_t align) {
    if (!size || !align || !alloc) return NULL;

    uint8_t *aligned = (uint8_t *)SN_GET_ALIGNED(alloc->top, align);

    if (aligned + size > alloc->mem + alloc->size) return NULL;

    alloc->top = aligned;
    alloc->top += size;

    return aligned;
}

/**
 * @brief Clear all allocations from the linear allocator.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_linear_allocator_reset(SnLinearAllocator *alloc) {
    if (!alloc) return;
    alloc->top = alloc->mem;
}

/**
 * @brief Get the total allocated size.
 *
 * @param alloc Pointer to the allocator context.
 *
 * @return Returns size of memory that is not available for allocation.
 */
SN_FORCE_INLINE uint64_t sn_linear_allocator_get_allocated_size(SnLinearAllocator *alloc) {
    if (!alloc) return 0;
    return SN_PTR_DIFF(alloc->top, alloc->mem);
}

/**
 * @brief Get the size left for allocation.
 *
 * @param alloc Pointer to the allocator context.
 *
 * @return Returns size of memory left for allocation.
 *
 * @note entire size might not be used for allocation,
 * few bytes might be unused for maintaining alignment.
 */
SN_FORCE_INLINE uint64_t sn_linear_allocator_get_remaining_size(SnLinearAllocator *alloc) {
    if (!alloc) return 0;
    return SN_PTR_DIFF(alloc->mem + alloc->size, alloc->top);
}

/**
 * @brief Get the memory mark.
 *
 * Get the mark for freeing allocated memory up to mark.
 *
 * @param alloc Pointer to the allocator context.
 *
 * @return Returns @ref snMemoryMark.
 */
SN_FORCE_INLINE snMemoryMark sn_linear_allocator_get_memory_mark(SnLinearAllocator *alloc) {
    if (!alloc) return NULL;
    return alloc->top;
}

/**
 * @brief Free memory up to given memory mark.
 *
 * @param alloc Pointer to allocator context.
 * @param mark The mark
 */
SN_FORCE_INLINE void sn_linear_allocator_free_to_memory_mark(SnLinearAllocator *alloc, snMemoryMark mark) {
    if (!alloc || !mark) return;

    SN_ASSERT(((uint64_t)mark) >= ((uint64_t)alloc->mem));
    SN_ASSERT(((uint64_t)mark) <= ((uint64_t)(alloc->mem + alloc->size)));
    if (alloc->top > mark) alloc->top = mark;
}

/**
 * @brief Get the SnMemoryAllocator.
 *
 * @param alloc Pointer to linear allocator.
 */
SN_FORCE_INLINE SnMemoryAllocator sn_linear_allocator_get_allocator(SnLinearAllocator *alloc) {
    return (SnMemoryAllocator){
        .data = alloc,
        .alloc = (SnMemoryAllocateFn)sn_linear_allocator_allocate,
        .realloc = NULL,
        .free = NULL,
    };
}
