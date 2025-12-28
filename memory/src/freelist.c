#include "snmemory/freelist.h"

#include <string.h>

#define SPLITTING_THRESHOLD (sizeof(snFreeNode) + SN_FREELIST_SPLITTING_THRESHOLD)

#define NODE_END(node) (((uint8_t *)((node) + 1)) + (node)->size)
#define PADDING_BYTE(ptr) ((void *)(((uint8_t *)ptr) - 1))

static snFreeNode *first_fit(snFreeNode *freenode, uint64_t size, snFreeNode **previous_freenode);

static void write_to_bytes(void *bytes, uint64_t value, bool reverse);

static uint64_t read_from_bytes(void *bytes, bool reverse);

static void try_merge(snFreeNode *previous_node, snFreeNode *node);

static snFreeNode *get_previous_free_node(snFreeNode *freelist, snFreeNode *node);

static void split_node_if_possible(snFreeNode *node, uint64_t allocated_size);

void *sn_freelist_allocator_allocate(snFreeListAllocator *alloc, uint64_t size, uint64_t align) {
    if (!alloc->free_list) return NULL;

    size = SN_GET_ALIGNED(size, align);
    size += align;

    snFreeNode *previous_freenode = NULL;
    snFreeNode *node = first_fit(alloc->free_list, size, &previous_freenode);
    if (!node) return NULL;

    // Get next aligned number (ensures we have padding before user pointer)
    void *aligned = (void *)SN_GET_NEXT_ALIGNED(node + 1, align);
    write_to_bytes(PADDING_BYTE(aligned), SN_PTR_DIFF(aligned, node), true);

    split_node_if_possible(node, size);

    if (previous_freenode) previous_freenode->next = node->next;
    else alloc->free_list = node->next;

    return aligned;
}

void sn_freelist_allocator_free(snFreeListAllocator *alloc, void *ptr) {
    if (!ptr) return;

    uint64_t diff_to_node = read_from_bytes(PADDING_BYTE(ptr), true);
    snFreeNode *node = (snFreeNode *)(((uint8_t *)ptr) - diff_to_node);

    snFreeNode *previous_freenode = get_previous_free_node(alloc->free_list, node);

    if (!previous_freenode) {
        node->next = alloc->free_list;
        previous_freenode = alloc->free_list = node;
        node = node->next;
    } else {
        node->next = previous_freenode->next;
        previous_freenode->next = node;
    }

    try_merge(previous_freenode, node);
}

void *sn_freelist_allocator_reallocate(snFreeListAllocator *alloc, void *ptr, uint64_t new_size, uint64_t align) {
    if (!ptr || !new_size) return NULL;

    uint64_t diff_to_node = read_from_bytes(PADDING_BYTE(ptr), true);
    snFreeNode *node = (snFreeNode *)(((uint8_t *)ptr) - diff_to_node);

    uint64_t current_size = SN_PTR_DIFF(NODE_END(node), ptr);

    if (!SN_IS_ALIGNED(ptr, align)) goto alloc_copy_free;

    snFreeNode *previous_freenode = NULL;
    snFreeNode *freenode = alloc->free_list;
    while (freenode) {
        if (freenode > node) break;
        previous_freenode = freenode;
        freenode = freenode->next;
    }

    if (current_size >= new_size) {
        // Fake this node as freenode and make it point to next free node
        node->next = previous_freenode->next;

        split_node_if_possible(node, new_size + align);

        // try to merge that new freenode with next node
        if (node->next) try_merge(node->next, node->next->next);

        // Whether it was split or not, this works
        if (previous_freenode) previous_freenode->next = node->next;
        else alloc->free_list = node->next;

        return ptr;
    }

    // Try to extend
    if (freenode ==(snFreeNode *)NODE_END(node)) { // We have a freenode right next to this node
        // Merge both nodes
        node->size += sizeof(snFreeNode) + freenode->size;

        // Fake this node as freenode and make it point to next freenode
        node->next = freenode->next;

        split_node_if_possible(node, new_size + align);

        // Remove that merged node from freelist
        if (previous_freenode) previous_freenode->next = node->next;
        else alloc->free_list = node->next;

        return ptr;
    }

alloc_copy_free:;
    void *new_ptr = sn_freelist_allocator_allocate(alloc, new_size, align);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, SN_MIN(new_size, current_size));

    sn_freelist_allocator_free(alloc, ptr);
    
    return new_ptr;
}

static snFreeNode *first_fit(snFreeNode *freenode, uint64_t size, snFreeNode **previous_freenode) {
    snFreeNode *previous = NULL;

    while (freenode) {
        if (freenode->size >= size) {
            if (previous_freenode) *previous_freenode = previous;
            return freenode;
        }

        previous = freenode;
        freenode = freenode->next;
    }

    return NULL;
}

static void write_to_bytes(void *bytes, uint64_t value, bool reverse) {
    // value is never 0, and we will always have enough bytes to write the value.
    uint8_t *p = (uint8_t *)bytes;
    uint8_t inc = reverse ? -1 : 1;

    while (value) {
        *p = (value % SN_BIT_FLAG(7));
        value >>= 7;
        SN_BIT_SET(*p, 7);
        p += inc;
    }

    p -= inc;
    SN_BIT_CLEAR(*p, 7);
}

static uint64_t read_from_bytes(void *bytes, bool reverse) {
    uint8_t *p = (uint8_t *)bytes;
    uint8_t inc = reverse ? -1 : 1;
    uint64_t value = 0, i = 0;

    while (SN_BIT_CHECK(*p, 7)) {
        value |= (uint64_t)(SN_BIT_CLEARED_VALUE(*p, 7)) << i;
        i += 7;
        p += inc;
    }

    value |= (uint64_t)*p  << i;

    return value;
}

static snFreeNode *get_previous_free_node(snFreeNode *freelist, snFreeNode *node) {
    snFreeNode *previous_node = NULL;
    while (freelist) {
        if (freelist > node) break;
        previous_node = freelist;
        freelist = freelist->next;
    }

    return previous_node;
}

static void try_merge(snFreeNode *previous_node, snFreeNode *node) {
    // Previous node is never NULL, node can be NULL
    if (NODE_END(previous_node) == ((uint8_t *)node)) {
        previous_node->size += sizeof(snFreeNode) + node->size;
        previous_node->next = node->next;

        try_merge(previous_node, previous_node->next);
    } else if (node && NODE_END(node) == (uint8_t *)node->next) {
        node->size += sizeof(snFreeNode) + node->next->size;
        node->next = node->next->next;
    }
}

static void split_node_if_possible(snFreeNode *node, uint64_t allocated_size) {
    if (node->size - allocated_size < SPLITTING_THRESHOLD) return; // Not enough space to split

    snFreeNode *new_node = SN_GET_ALIGNED_PTR(((uint8_t *)(node + 1)) + allocated_size, snFreeNode);

    *new_node = (snFreeNode){
        .next = node->next,
        .size = SN_PTR_DIFF(NODE_END(node), new_node + 1)
    };

    *node = (snFreeNode){
        .next = new_node,
        .size = SN_PTR_DIFF(new_node, node + 1)
    };
}

