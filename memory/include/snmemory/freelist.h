#pragma once

#include "snmemory/defines.h"

#include "snmemory/api.h"

typedef struct snFreeNode {
    uint64_t size;
    struct snFreeNode *next;
} snFreeNode;

#ifndef SN_FREELIST_SPLITTING_THRESHOLD
    #define SN_FREELIST_SPLITTING_THRESHOLD (2 * alignof(max_align_t))
#endif

/**
 * @struct snFreeListAllocator
 * @brief General-purpose free-list allocator.
 *
 * Manages variable-sized allocations from a user-provided memory buffer.
 *
 * @note
 * - Not thread-safe
 * - No OS allocations
 */
typedef struct snFreeListAllocator {
    uint8_t *mem;      /**< Base memory pointer */
    uint64_t size;     /**< Total size of managed memory */

    snFreeNode *free_list;   /**< Head of free block list */
} snFreeListAllocator;

/**
 * @brief Initialize free-list allocator.
 *
 * @param alloc Pointer to allocator context
 * @param mem Memory buffer to manage
 * @param size Size of memory buffer
 *
 * @return true on success, false on failure
 */
SN_INLINE bool sn_freelist_allocator_init(snFreeListAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    // Too small buffer
    if (size < sizeof(snFreeNode) + SN_FREELIST_SPLITTING_THRESHOLD) return false;

    *alloc = (snFreeListAllocator){
        .mem = mem,
        .size = size,
        .free_list = SN_GET_ALIGNED_PTR(mem, snFreeNode),
    };

    *alloc->free_list = (snFreeNode) {
        .size = SN_PTR_DIFF(((uint8_t *)mem) + size, alloc->free_list + 1),
        .next = NULL
    };

    return true;
}

/**
 * @brief Deinitialize free-list allocator.
 *
 * @param alloc Pointer to allocator context
 *
 * @note Does not free memory buffer
 */
SN_FORCE_INLINE void sn_freelist_allocator_deinit(snFreeListAllocator *alloc) {
    *alloc = (snFreeListAllocator){0};
}

/**
 * @brief Allocate memory from free-list allocator.
 *
 * @param alloc Pointer to allocator context
 * @param size Number of bytes to allocate
 * @param align Alignment requirement
 *
 * @return Pointer to allocated memory or NULL on failure
 */
SN_API void *sn_freelist_allocator_allocate(snFreeListAllocator *alloc, uint64_t size, uint64_t align);

/**
 * @brief Free memory allocated by free-list allocator.
 *
 * @param alloc Pointer to allocator context
 * @param ptr Pointer to memory to free
 *
 * @note
 * - ptr must be returned by this allocator
 * - ptr must not be freed twice
 */
SN_API void sn_freelist_allocator_free(snFreeListAllocator *alloc, void *ptr);

/**
 * @brief Reallocate memory allocated by free-list allocator.
 *
 * @param alloc Pointer to allocator context.
 * @param ptr Pointer to memory to reallocate.
 * @param new_size The new size.
 * @param align The alignment.
 *
 * @note
 * - ptr must be returned by this allocator
 */
SN_API void *sn_freelist_allocator_reallocate(snFreeListAllocator *alloc, void *ptr, uint64_t new_size, uint64_t align);

/**
 * @brief Get total managed memory size.
 *
 * @param alloc Pointer to allocator context
 */
SN_FORCE_INLINE uint64_t sn_freelist_allocator_get_total_size(snFreeListAllocator *alloc) {
    return alloc->size;
}

/**
 * @brief Get total free memory size.
 *
 * @param alloc Pointer to allocator context
 *
 * @note May include fragmentation
 */
SN_INLINE uint64_t sn_freelist_allocator_get_free_size(snFreeListAllocator *alloc) {
    uint64_t size = 0;

    snFreeNode *node = alloc->free_list;
    while (node) {
        size += node->size;
        node = node->next;
    }

    return size;
}

#undef SN_API
