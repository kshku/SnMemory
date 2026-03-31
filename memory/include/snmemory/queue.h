#pragma once

#include "snmemory/defines.h"

typedef struct snQueueAllocatorHeader {
    uint8_t *next;
    uint64_t align_diff;
} snQueueAllocatorHeader;

/**
 * @struct snQueueAllocator
 * @brief Struct to store the queue allocator context.
 *
 * @note None of the sn_queue_allocator* functions are thread-safe.
 */
typedef struct snQueueAllocator {
    uint8_t *mem; /**< Pointer to memory */
    uint64_t size; /**< Size of memory */
    uint8_t *head; /**< Pointer to head */
    uint8_t *tail; /**< Pointer to tail */
} snQueueAllocator;

/**
 * @brief Initialize a queue allocator.
 *
 * Initialize queue allocator for managing the given memory.
 *
 * @param alloc Pointer to the allocator context.
 * @param mem The memory to handle.
 * @param size Size of memory to handle.
 *
 * @return Returns true on success, false otherwise.
 */
SN_FORCE_INLINE bool sn_queue_allocator_init(snQueueAllocator *alloc, void *mem, uint64_t size) {
    if (!alloc || !mem || !size) return false;

    if (size < sizeof(snQueueAllocatorHeader) * 2) return false;

    *alloc = (snQueueAllocator){
        .mem = (uint8_t *)mem,
        .head = NULL,
        .tail = NULL,
        .size = size
    };

    return true;
}

/**
 * @brief Deinitialize queue allocator.
 *
 * @note Doesn't deal with the memory passed to allocator.
 * User is responsible for handling the memory.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_queue_allocator_deinit(snQueueAllocator *alloc) {
    if (!alloc) return;
    *alloc = (snQueueAllocator){0};
}

/**
 * @brief Allocate from the queue allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param size Number of bytes to allocate.
 * @param align The alignment.
 *
 * @return Returns pointer to allocated memory or NULL no failure.
 */
SN_INLINE void *sn_queue_allocator_allocate(snQueueAllocator *alloc, uint64_t size, uint64_t align) {
    if (!size || !align || !alloc) return NULL;

    snQueueAllocatorHeader *head_header = (snQueueAllocatorHeader *)alloc->head;
    uint8_t *mem_end = alloc->mem + alloc->size;
    
    uint8_t *raw_head = alloc->head ? head_header->next : alloc->mem;
    snQueueAllocatorHeader *new_head = SN_GET_ALIGNED_PTR(raw_head, snQueueAllocatorHeader);

    void *aligned = (void *)SN_GET_ALIGNED(new_head + 1, align);

    uint64_t align_diff = SN_PTR_DIFF(aligned, new_head + 1);
    uint64_t required_size = size + sizeof(snQueueAllocatorHeader) + align_diff;

    uint64_t free_size = 0;
    if ((uint8_t *)new_head < mem_end) {
        if (!alloc->tail || alloc->tail < (uint8_t *)new_head) free_size = SN_PTR_DIFF(mem_end, new_head);
        else if (((uint8_t *)new_head) < alloc->tail) free_size = SN_PTR_DIFF(alloc->tail, new_head);
    }

    // Wrapping
    if (free_size < required_size && alloc->tail && alloc->tail < (uint8_t *)new_head) {
        required_size -= align_diff;

        new_head = SN_GET_ALIGNED_PTR(alloc->mem, snQueueAllocatorHeader);
        aligned = (void *)SN_GET_ALIGNED(new_head + 1, align);
        align_diff = SN_PTR_DIFF(aligned, new_head + 1);

        required_size += align_diff;

        free_size = 0;
        if (((uint8_t *)new_head) < alloc->tail) free_size = SN_PTR_DIFF(alloc->tail, new_head);
    }

    if (free_size < required_size) return NULL;

    if (alloc->head) {
        head_header->next = (uint8_t *)new_head;
        alloc->head = (uint8_t *)new_head;
    } else {
        alloc->head = (uint8_t *)new_head;
        alloc->tail = (uint8_t *)new_head;
    }

    *new_head = (snQueueAllocatorHeader){
        .next = ((uint8_t *)aligned) + size,
        .align_diff = align_diff
    };

    if ((uint8_t *)new_head < alloc->tail) SN_ASSERT(new_head->next <= alloc->tail);

    return aligned;
}

/**
 * @brief Free the allocated memory from queue allocator.
 *
 * @param alloc Pointer to the allocator context.
 * @param ptr The pointer to free.
 */
SN_INLINE void sn_queue_allocator_free(snQueueAllocator *alloc, void *ptr) {
    if (!ptr || !alloc) return;

    snQueueAllocatorHeader *header = (snQueueAllocatorHeader *)alloc->tail;
    SN_ASSERT(ptr == (void *)(((uint8_t *)(header + 1)) + header->align_diff));

    if (alloc->tail == alloc->head) alloc->tail = alloc->head = NULL;
    else alloc->tail = header->next;
}

/**
 * @brief Clear all allocations from the queue allocator.
 *
 * @param alloc Pointer to the allocator context.
 */
SN_FORCE_INLINE void sn_queue_allocator_reset(snQueueAllocator *alloc) {
    if (!alloc) return;
    alloc->head = alloc->tail = NULL;
}

/**
 * @brief Get the total allocated size.
 *
 * @param alloc Pointer to the allocator context.
 *
 * @return Returns size of memory that is not available for allocation.
 */
SN_FORCE_INLINE uint64_t sn_queue_allocator_get_allocated_size(snQueueAllocator *alloc) {
    if (!alloc || alloc->tail == NULL) return 0;

    uint64_t size = 0;
    if (alloc->head < alloc->tail) {
        size += SN_PTR_DIFF(alloc->mem + alloc->size, alloc->tail);
        size += SN_PTR_DIFF(alloc->head, alloc->mem);
    } else {
        size += SN_PTR_DIFF(alloc->head, alloc->tail);
    }

    snQueueAllocatorHeader *head_header = (snQueueAllocatorHeader *)alloc->head;
    size += SN_PTR_DIFF(head_header->next, alloc->head);

    return size;
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
SN_FORCE_INLINE uint64_t sn_queue_allocator_get_remaining_size(snQueueAllocator *alloc) {
    if (!alloc) return 0;

    if (alloc->tail == NULL) return alloc->size;

    uint64_t size = 0;
    if (alloc->tail <= alloc->head) {
        size += SN_PTR_DIFF(alloc->mem + alloc->size, alloc->head);
        size += SN_PTR_DIFF(alloc->tail, alloc->mem);
    } else {
        size += SN_PTR_DIFF(alloc->tail, alloc->head);
    }

    snQueueAllocatorHeader *head_header = (snQueueAllocatorHeader *)alloc->head;
    size -= SN_PTR_DIFF(head_header->next, alloc->head);

    return size;
}


