#include <snmemory/snmemory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ASSERT(x)                                                                                      \
    do {                                                                                                    \
        if (!(x)) {                                                                                         \
            fprintf(stderr, "ASSERT FAILED: %s in function %s(%s:%d)\n", #x, __func__, __FILE__, __LINE__); \
            abort();                                                                                        \
        }                                                                                                   \
    } while (0)

#define KB(x) ((x) * 1024ULL)

/* One block cut in two, so the halves are adjacent. Growing the allocator
   requires the new memory to start exactly where the old one ended, which two
   separate allocations cannot promise. */
static uint8_t *cut_block(size_t first_size, size_t second_size, uint8_t **first, uint8_t **second) {
    uint8_t *block = (uint8_t *)malloc(first_size + second_size);
    if (!block) return NULL;
    *first = block;
    *second = block + first_size;
    return block;
}

static bool within(uint8_t *ptr, uint8_t *base, size_t size) {
    return ptr >= base && ptr < base + size;
}

/* Growing into memory added to an allocator that still has a free node, which
   is the path that has to find the tail node and grow it. */
static void test_grow_with_free_nodes_left(void) {
    uint8_t *first, *second;
    uint8_t *block = cut_block(KB(1), KB(4), &first, &second);
    TEST_ASSERT(block);

    SnFreeListAllocator alloc;
    TEST_ASSERT(sn_freelist_allocator_init(&alloc, first, KB(1)));

    void *p = sn_freelist_allocator_allocate(&alloc, 64, 8);
    TEST_ASSERT(p);
    sn_freelist_allocator_free(&alloc, p);

    uint64_t total_before = sn_freelist_allocator_get_total_size(&alloc);
    uint64_t free_before = sn_freelist_allocator_get_free_size(&alloc);
    TEST_ASSERT(total_before == KB(1));
    TEST_ASSERT(free_before > 0);

    sn_freelist_allocator_increase_memory_size(&alloc, second, KB(4));

    TEST_ASSERT(sn_freelist_allocator_get_total_size(&alloc) == KB(1) + KB(4));
    /* The added block has to become usable, so the free total grows by most of
       the second half rather than by the bookkeeping node alone. */
    TEST_ASSERT(sn_freelist_allocator_get_free_size(&alloc) > free_before + KB(2));

    /* Growing folds the added memory into the existing tail node, so this
       allocation may be served from anywhere in the managed region. What
       matters is that a block bigger than the original half is satisfiable at
       all, which it was not before the grow. */
    void *big = sn_freelist_allocator_allocate(&alloc, KB(4), 8);
    TEST_ASSERT(big);
    TEST_ASSERT(within((uint8_t *)big, first, KB(1) + KB(4)));
    memset(big, 0x5A, KB(4));
    sn_freelist_allocator_free(&alloc, big);

    /* And the memory is reusable, so the new block really did fold into the
       free list rather than being described and then leaked. */
    void *again = sn_freelist_allocator_allocate(&alloc, KB(4), 8);
    TEST_ASSERT(again);
    TEST_ASSERT(within((uint8_t *)again, first, KB(1) + KB(4)));
    sn_freelist_allocator_free(&alloc, again);

    sn_freelist_allocator_deinit(&alloc);
    free(block);
}

/* Growing an allocator that has been fully consumed, so the free list is empty
   and the added block has to become the free list itself. */
static void test_grow_when_exhausted(void) {
    uint8_t *first, *second;
    uint8_t *block = cut_block(KB(4), KB(4), &first, &second);
    TEST_ASSERT(block);

    SnFreeListAllocator alloc;
    TEST_ASSERT(sn_freelist_allocator_init(&alloc, first, KB(4)));

    void *held[16];
    for (int i = 0; i < 16; i++) {
        held[i] = sn_freelist_allocator_allocate(&alloc, KB(0.25) - 64, 8);
        TEST_ASSERT(held[i]);
    }

    uint64_t free_before = sn_freelist_allocator_get_free_size(&alloc);
    /* Nearly all of the first half is handed out, so this is a small residue
       rather than a free list that still spans the block. */
    TEST_ASSERT(free_before < KB(1));

    sn_freelist_allocator_increase_memory_size(&alloc, second, KB(4));

    TEST_ASSERT(sn_freelist_allocator_get_total_size(&alloc) == KB(8));
    TEST_ASSERT(sn_freelist_allocator_get_free_size(&alloc) > free_before);

    /* The original blocks must have survived intact, the allocator may not
       have moved or reclaimed them while growing. */
    for (int i = 0; i < 16; i++) memset(held[i], (uint8_t)(i + 1), KB(0.25) - 64);
    for (int i = 0; i < 16; i++) {
        uint8_t *q = (uint8_t *)held[i];
        for (size_t j = 0; j < KB(0.25) - 64; j++) TEST_ASSERT(q[j] == (uint8_t)(i + 1));
    }

    /* The allocator has only ever seen the first half, so a block this large
       is only satisfiable because of the memory that was added. */
    void *big = sn_freelist_allocator_allocate(&alloc, KB(3), 8);
    TEST_ASSERT(big);
    TEST_ASSERT(within((uint8_t *)big, first, KB(4) + KB(4)));
    memset(big, 0x3C, KB(3));
    sn_freelist_allocator_free(&alloc, big);

    /* Freeing it must return the space rather than lose it, otherwise the
       account grows on every cycle. */
    uint64_t free_now = sn_freelist_allocator_get_free_size(&alloc);
    void *again = sn_freelist_allocator_allocate(&alloc, KB(3), 8);
    TEST_ASSERT(again);
    TEST_ASSERT(within((uint8_t *)again, first, KB(4) + KB(4)));
    sn_freelist_allocator_free(&alloc, again);
    TEST_ASSERT(sn_freelist_allocator_get_free_size(&alloc) == free_now);

    for (int i = 0; i < 16; i++) sn_freelist_allocator_free(&alloc, held[i]);

    sn_freelist_allocator_deinit(&alloc);
    free(block);
}

/* A block too small to describe, or no allocator at all, is a no-op rather than
   a crash or a corrupted free list. */
static void test_grow_ignores_a_block_too_small(void) {
    uint8_t *first, *second;
    uint8_t *block = cut_block(KB(1), KB(1), &first, &second);
    TEST_ASSERT(block);

    SnFreeListAllocator alloc;
    TEST_ASSERT(sn_freelist_allocator_init(&alloc, first, KB(1)));
    uint64_t total_before = sn_freelist_allocator_get_total_size(&alloc);
    uint64_t free_before = sn_freelist_allocator_get_free_size(&alloc);

    sn_freelist_allocator_increase_memory_size(&alloc, second, 1);
    TEST_ASSERT(sn_freelist_allocator_get_total_size(&alloc) == total_before);
    TEST_ASSERT(sn_freelist_allocator_get_free_size(&alloc) == free_before);

    sn_freelist_allocator_increase_memory_size(&alloc, second, 0);
    TEST_ASSERT(sn_freelist_allocator_get_total_size(&alloc) == total_before);

    /* A null allocator is a no-op rather than a crash. */
    sn_freelist_allocator_increase_memory_size(NULL, second, KB(1));

    /* The allocator is still usable after being asked to grow by nothing. */
    void *p = sn_freelist_allocator_allocate(&alloc, 32, 8);
    TEST_ASSERT(p);
    sn_freelist_allocator_free(&alloc, p);

    sn_freelist_allocator_deinit(&alloc);
    free(block);
}

int main(void) {
    printf("Running test_grow_with_free_nodes_left...\n");
    test_grow_with_free_nodes_left();

    printf("Running test_grow_when_exhausted...\n");
    test_grow_when_exhausted();

    printf("Running test_grow_ignores_a_block_too_small...\n");
    test_grow_ignores_a_block_too_small();

    printf("Free-list grow tests passed\n\n");
    return 0;
}
