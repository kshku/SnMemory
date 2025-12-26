#pragma once

#include "snmemory/defines.h"

/**
 * @struct snPoolAllocator
 * @brief Fixed-size block memory allocator.
 *
 * Manages a user-provided memory buffer divided into equal-sized blocks.
 *
 * @note
 * - Block size must be >= sizeof(void *)
 * - None of the sn_pool_allocator* functions are thread-safe
 */
typedef struct snPoolAllocator {
    uint8_t *mem;        /**< Base memory pointer */
    uint64_t size;       /**< Total size of memory */

    uint64_t block_size; /**< Size of each block */
    uint64_t block_align;/**< Alignment of each block */

    void *free_list;     /**< Head of free block list */

    uint64_t block_count;/**< Total number of blocks */
    uint64_t free_count; /**< Number of free blocks */
} snPoolAllocator;

/**
 * @brief Initialize a pool allocator.
 *
 * @param alloc Pointer to allocator context
 * @param mem Memory buffer to manage
 * @param size Size of memory buffer
 * @param block_size Size of each block (should be >= sizeof(void *))
 * @param block_align Alignment of each block
 *
 * @return true on success, false on failure
 */
SN_INLINE bool sn_pool_allocator_init(snPoolAllocator *alloc, void *mem, uint64_t size, uint64_t block_size, uint64_t block_align) {
    if (!alloc || !mem || !size) return false;

    block_size = SN_GET_ALIGNED(block_size, block_align);

    if (block_size < sizeof(void *)) return false;

    *alloc = (snPoolAllocator){
        .mem = mem,
        .size = size,
        .block_size = block_size,
        .block_align = block_align,
        .free_list = (void *)SN_GET_ALIGNED(mem, block_align),
    };

    void *previous_block = NULL;
    void *freelist = alloc->free_list;
    while (((uint64_t)freelist) + block_size <= ((uint64_t)mem) + size) {
        void *next_block = (void *)(((uint64_t)freelist) + block_size);
        alloc->block_count++;
        *((void **)freelist) = next_block;
        previous_block = freelist;
        freelist = next_block;
    }

    if (alloc->block_count == 0) return false;

    // set the next pointer of last block to NULL
    *((void **)previous_block) = NULL;

    alloc->free_count = alloc->block_count;

    return true;
}

/**
 * @brief Deinitialize pool allocator.
 *
 * @note Does not free memory buffer
 */
SN_FORCE_INLINE void sn_pool_allocator_deinit(snPoolAllocator *alloc) {
    *alloc = (snPoolAllocator){0};
}

/**
 * @brief Allocate a block from the pool.
 *
 * @param alloc Pointer to allocator context
 *
 * @return Pointer to allocated block or NULL if exhausted
 */
SN_FORCE_INLINE void *sn_pool_allocator_allocate(snPoolAllocator *alloc) {
    void *ptr = alloc->free_list;

    if (alloc->free_list) {
        alloc->free_list = *((void **)alloc->free_list);
        alloc->free_count--;
    }

    return ptr;
}

/**
 * @brief Free a previously allocated block.
 *
 * @param alloc Pointer to allocator context
 * @param ptr Pointer to block to free
 *
 * @note
 * - ptr must be returned by this allocator
 * - ptr must not be freed twice
 */
SN_FORCE_INLINE void sn_pool_allocator_free(snPoolAllocator *alloc, void *ptr) {
    SN_ASSERT((uint8_t *)ptr >= alloc->mem);
    SN_ASSERT((uint8_t *)ptr < alloc->mem + alloc->size);
    SN_ASSERT(SN_IS_ALIGNED(ptr, alloc->block_align));

    *((void **)ptr) = alloc->free_list;
    alloc->free_list = ptr;
    alloc->free_count++;
}

/**
 * @brief Get total number of blocks.
 *
 * @param alloc Pointer to allocator context
 */
SN_FORCE_INLINE uint64_t sn_pool_allocator_get_block_count(snPoolAllocator *alloc) {
    return alloc->block_count;
}

/**
 * @brief Get number of free blocks.
 *
 * @param alloc Pointer to allocator context
 */
SN_FORCE_INLINE uint64_t sn_pool_allocator_get_free_count(snPoolAllocator *alloc) {
    return alloc->free_count;
}

/**
 * @brief Get number of allocated blocks.
 *
 * @param alloc Pointer to allocator context
 */
SN_FORCE_INLINE uint64_t sn_pool_allocator_get_used_count(snPoolAllocator *alloc) {
    return alloc->block_count - alloc->free_count;
}

