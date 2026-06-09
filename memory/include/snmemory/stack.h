#pragma once

#include "snmemory/defines.h"

typedef struct SnStackAllocatorFooter {
    uint8_t *previous_top;
    uint64_t align_diff;
} SnStackAllocatorFooter;

/**
 * @struct SnStackAllocator
 * @brief Struct to store stack allocator context.
 *
 * @note None of the sn_stack_allocator* functions are thread-safe.
 */
typedef struct SnStackAllocator {
    uint8_t *mem; /**< Pointer to memory */
    uint8_t *top; /**< Pointer to top of memory */
    uint64_t size; /**< Size of the memory */
} SnStackAllocator;

/**
 * @brief Initialize a stack allocator.
 *
 * Initialize stack allocator for managing the given memory.
 *
 * @param alloc Pointer to the allocator context.
 * @param mem The memory to handle.
 * @param size Size of the memeory to handle.
 *
 * @return Returns true on success, false otherwise.
 */
SN_FORCE_INLINE bool sn_stack_allocator_init(SnStackAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    *alloc = (SnStackAllocator){.mem = (uint8_t *)mem, .top = (uint8_t *)mem, .size = size};

    return true;
}

/**
 * @breif Deinitialize stack allocator.
 *
 * @note Doesn't deal with the memory passed to allocator.
 * User is responsible for handing the memory.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_stack_allocator_deinit(SnStackAllocator *alloc) {
    if (!alloc) return;
    *alloc = (SnStackAllocator){0};
}

/**
 * @brief Increase the size of memory managed by the allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param mem Pointer to the new memory (must be right next to current memory).
 * @param size Size of the new memory.
 */
SN_FORCE_INLINE void sn_stack_allocator_increase_memory_size(SnStackAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc) return;
    SN_ASSERT(alloc->mem + alloc->size == mem);
    alloc->size += size;
}

/**
 * @brief Allocate from the stack allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param size Number of bytes to allocate.
 * @param align The alignment.
 *
 * @return Returns pointer to allocated memory or NULL no failure.
 */
SN_INLINE void *sn_stack_allocator_allocate(SnStackAllocator *alloc, uint64_t size, uint64_t align) {
    if (!size || !align || !alloc) return NULL;

    uint8_t *aligned = (uint8_t *)SN_GET_ALIGNED(alloc->top, align);

    if (aligned + size + sizeof(SnStackAllocatorFooter) + alignof(SnStackAllocatorFooter)
        > alloc->mem + alloc->size)
        return NULL;

    SnStackAllocatorFooter *footer = SN_GET_ALIGNED_PTR(aligned + size, SnStackAllocatorFooter);
    footer->previous_top = alloc->top;
    footer->align_diff = SN_PTR_DIFF(aligned, alloc->top);

    alloc->top = (uint8_t *)(footer + 1);

    return aligned;
}

/**
 * @brief Free the allocated memory from stack allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param ptr The pointer to free.
 */
SN_INLINE void sn_stack_allocator_free(SnStackAllocator *alloc, void *ptr) {
    if (!ptr || !alloc) return;

    SnStackAllocatorFooter *footer = ((SnStackAllocatorFooter *)alloc->top) - 1;

    SN_ASSERT(footer->previous_top == (uint8_t *)(((uint64_t)ptr) - footer->align_diff));

    alloc->top = footer->previous_top;
}

/**
 * @brief Clear all allocations from the stack allocator.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_stack_allocator_reset(SnStackAllocator *alloc) {
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
SN_FORCE_INLINE uint64_t sn_stack_allocator_get_allocated_size(SnStackAllocator *alloc) {
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
 * few bytes might be unused for maintaining alignment and headers.
 */
SN_FORCE_INLINE uint64_t sn_stack_allocator_get_remaining_size(SnStackAllocator *alloc) {
    if (!alloc) return 0;
    return SN_PTR_DIFF(alloc->mem + alloc->size, alloc->top);
}

