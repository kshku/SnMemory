#pragma once

#include "snmemory/defines.h"

typedef struct snStackAllocatorFooter {
    uint8_t *previous_top;
    uint64_t align_diff;
} snStackAllocatorFooter;

/**
 * @struct snStackAllocator
 * @brief Struct to store stack allocator context.
 *
 * @note None of the sn_stack_allocator* functions is thread-safe.
 */
typedef struct snStackAllocator {
    uint8_t *mem; /**< Pointer to memory */
    uint8_t *top; /**< Pointer to top of memory */
    uint64_t size; /**< Size of the memory */
} snStackAllocator;

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
SN_FORCE_INLINE bool sn_stack_allocator_init(snStackAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    *alloc = (snStackAllocator){
        .mem = (uint8_t *)mem,
        .top = (uint8_t *)mem,
        .size = size
    };

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
SN_FORCE_INLINE void sn_stack_allocator_deinit(snStackAllocator *alloc) {
    *alloc = (snStackAllocator){0};
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
SN_INLINE void *sn_stack_allocator_allocate(snStackAllocator *alloc, uint64_t size, uint64_t align) {
    uint8_t *aligned = (uint8_t *)SN_GET_ALIGNED(alloc->top, align);

    if (aligned + size + sizeof(snStackAllocatorFooter) + alignof(snStackAllocatorFooter) > alloc->mem + alloc->size) return NULL;

    snStackAllocatorFooter *footer = SN_GET_ALIGNED_PTR(aligned + size, snStackAllocatorFooter);
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
SN_INLINE void sn_stack_allocator_free(snStackAllocator *alloc, void *ptr) {
    snStackAllocatorFooter *footer = ((snStackAllocatorFooter *)alloc->top) - 1;

    SN_ASSERT(footer->previous_top == (uint8_t *)(((uint64_t)ptr) - footer->align_diff));

    alloc->top = footer->previous_top;
}

/**
 * @brief Clear all allocations from the stack allocator.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_stack_allocator_reset(snStackAllocator *alloc) {
    alloc->top = alloc->mem;
}

/**
 * @brief Get the total allocated size.
 *
 * @param alloc Pointer to the allocator context.
 *
 * @return Returns size of memory that is not available for allocation.
 */
SN_FORCE_INLINE uint64_t sn_stack_allocator_get_allocated_size(snStackAllocator *alloc) {
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
SN_FORCE_INLINE uint64_t sn_stack_allocator_get_remaining_size(snStackAllocator *alloc) {
    return SN_PTR_DIFF(alloc->mem + alloc->size, alloc->top);
}

