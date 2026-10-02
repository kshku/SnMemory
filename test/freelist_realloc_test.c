#include <sncore/utils.h>
#include <snmemory/snmemory.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tests for sn_freelist_allocator_reallocate's in-place paths.
 *
 * reallocate can serve a request without moving the block, either by handing back
 * the tail of a block that is now too big, or by merging the block with the free
 * node that happens to sit immediately after it. The merge path used to be able to
 * carve a free node that reaches past the end of the block it was working on,
 * which silently hands out memory that another block is still using. */

#define TEST_ASSERT(x)                                                                                      \
    do {                                                                                                    \
        if (!(x)) {                                                                                         \
            fprintf(stderr, "ASSERT FAILED: %s in function %s(%s:%d)\n", #x, __func__, __FILE__, __LINE__); \
            abort();                                                                                        \
        }                                                                                                   \
    } while (0)

#define REGION_SIZE (64u * 1024u)

/* Aligned so the allocator's first node starts exactly at the buffer, which the
 * layout arithmetic below depends on. */
static union {
    uint8_t bytes[REGION_SIZE];
    uint64_t align;
} storage;

static uint8_t *region = storage.bytes;
static uintptr_t region_lo;
static uintptr_t region_hi;

/* The extent of the block a user pointer belongs to, recovered the way the
 * allocator itself recovers it: the byte in front of the pointer holds the
 * distance back to the node header. */
static void block_extent(const void *user_ptr, uintptr_t *lo, uintptr_t *hi) {
    uint64_t diff_to_node = sn_read_from_bytes((void *)((uint8_t *)user_ptr - 1), true);
    SnFreeNode *node = (SnFreeNode *)((uint8_t *)user_ptr - diff_to_node);
    *lo = (uintptr_t)node;
    *hi = (uintptr_t)node + sizeof(SnFreeNode) + node->size;
}

/* Every free node stays inside the region, keeps a sane size, the list stays in
 * address order, and no free node overlaps a block that is still handed out. The
 * last one is what a split that runs off the end of its node breaks. */
static void check_free_list(
    SnFreeListAllocator *alloc, const void *const *live, size_t live_count, const char *when) {
    SnFreeNode *previous = NULL;

    for (SnFreeNode *node = alloc->free_list; node; node = node->next) {
        uintptr_t lo = (uintptr_t)node;

        /* Checked before the extent is built, because a size that has underflowed
         * wraps the end address around and would slip past a range test. */
        if (lo < region_lo || node->size > region_hi - region_lo) {
            fprintf(stderr, "FAIL [%s]: free node at %p has size %llu, which does not fit the region\n",
                    when, (void *)node, (unsigned long long)node->size);
            abort();
        }

        uintptr_t hi = lo + sizeof(SnFreeNode) + node->size;

        if (hi > region_hi) {
            fprintf(stderr, "FAIL [%s]: free node [%p,%p) escapes region [%p,%p)\n", when,
                    (void *)lo, (void *)hi, (void *)region_lo, (void *)region_hi);
            abort();
        }
        if (node->size < sizeof(SnFreeNode)) {
            fprintf(stderr, "FAIL [%s]: free node at %p has size %llu\n", when, (void *)node,
                    (unsigned long long)node->size);
            abort();
        }
        if (previous && lo < (uintptr_t)previous) {
            fprintf(stderr, "FAIL [%s]: free list is not in address order at %p (after %p)\n", when,
                    (void *)node, (void *)previous);
            abort();
        }

        for (size_t i = 0; i < live_count; ++i) {
            if (!live[i]) continue;
            uintptr_t llo, lhi;
            block_extent(live[i], &llo, &lhi);
            if (lo < lhi && llo < hi) {
                fprintf(stderr, "FAIL [%s]: free node [%p,%p) overlaps live block [%p,%p)\n", when,
                        (void *)lo, (void *)hi, (void *)llo, (void *)lhi);
                abort();
            }
        }

        previous = node;
    }
}

/* Hand the rest of the region out, so that freeing a block above it cannot merge
 * with it. Once everything else is allocated there is a single node left, and
 * asking for size - 8 makes the allocator keep the node whole instead of splitting
 * off a remainder, which consumes the region exactly. */
static void consume_tail(SnFreeListAllocator *alloc) {
    SnFreeNode *tail = alloc->free_list;
    TEST_ASSERT(tail && tail->next == NULL);
    if (tail->size <= 8) return;

    TEST_ASSERT(sn_freelist_allocator_allocate(alloc, tail->size - 8, 8));
    TEST_ASSERT(alloc->free_list == NULL);
}

/* Build the layout the merge path needs: one live block, a large free node below
 * it and a 56 byte free node directly above it. The upper node only stays that
 * size because nothing free is left above it. */
static void setup_merge_case(
    SnFreeListAllocator *alloc, uint64_t block_size, void **block, void **ballast_out, void **upper_out) {
    TEST_ASSERT(sn_freelist_allocator_init(alloc, region, REGION_SIZE));

    void *ballast = sn_freelist_allocator_allocate(alloc, REGION_SIZE / 2, 8);
    TEST_ASSERT(ballast);

    void *block_ptr = sn_freelist_allocator_allocate(alloc, block_size, 8);
    TEST_ASSERT(block_ptr);
    memset(block_ptr, 0xA5, block_size);

    void *upper = sn_freelist_allocator_allocate(alloc, 48, 8);
    TEST_ASSERT(upper);

    consume_tail(alloc);
    TEST_ASSERT(alloc->free_list == NULL);

    sn_freelist_allocator_free(alloc, ballast); /* large free node below the block */
    sn_freelist_allocator_free(alloc, upper); /* 56 byte free node directly above it */

    const void *live[] = {block_ptr};
    check_free_list(alloc, live, 1, "setup");

    *block = block_ptr;
    *ballast_out = ballast;
    *upper_out = upper;
}

/* The reported failure. A 128 byte block sits between a large free node and a
 * 56 byte free node. Merging with the upper node yields a 208 byte node, which is
 * more than any request up to 207 bytes but not the 8 byte alignment reserve on
 * top of them, so requests in 201..207 must move the block rather than extend it
 * in place. */
static void test_extend_into_too_small_adjacent_node(void) {
    SnFreeListAllocator alloc;
    void *block, *ballast, *upper;
    setup_merge_case(&alloc, 128, &block, &ballast, &upper);
    (void)ballast;
    (void)upper;

    uint8_t *grown = (uint8_t *)sn_freelist_allocator_reallocate(&alloc, block, 204, 8);
    TEST_ASSERT(grown);

    for (size_t i = 0; i < 128; ++i) {
        if (grown[i] != 0xA5) {
            fprintf(stderr, "FAIL: byte %zu of the moved block was not preserved (0x%02X)\n", i, grown[i]);
            abort();
        }
    }

    const void *live[] = {grown};
    check_free_list(&alloc, live, 1, "after realloc");

    /* The enlarged block is genuinely usable end to end. */
    memset(grown, 0x5A, 204);
    for (size_t i = 0; i < 204; ++i) {
        if (grown[i] != 0x5A) {
            fprintf(stderr, "FAIL: byte %zu of the enlarged block did not hold its value\n", i);
            abort();
        }
    }

    sn_freelist_allocator_deinit(&alloc);
}

/* The whole neighbourhood of that case: every block size and every request size,
 * so the boundary between "the adjacent node is enough" and "the block has to
 * move" is crossed from both sides and no size in between goes unchecked. */
static void test_extend_boundary_across_sizes(void) {
    for (size_t original = 64; original <= 256; original += 8) {
        for (size_t requested = original + 1; requested <= original + 128; requested += 8) {
            SnFreeListAllocator alloc;
            void *block, *ballast, *upper;
            setup_merge_case(&alloc, original, &block, &ballast, &upper);

            const void *live[] = {block};
            check_free_list(&alloc, live, 1, "boundary before realloc");

            uint8_t *grown = (uint8_t *)sn_freelist_allocator_reallocate(&alloc, block, requested, 8);
            TEST_ASSERT(grown);
            for (size_t i = 0; i < original; ++i) {
                if (grown[i] != 0xA5) {
                    fprintf(stderr, "FAIL: original=%zu requested=%zu, byte %zu not preserved\n",
                            original, requested, i);
                    abort();
                }
            }

            const void *live_after[] = {grown};
            check_free_list(&alloc, live_after, 1, "boundary after realloc");

            sn_freelist_allocator_deinit(&alloc);
        }
    }
}

/* Several blocks sharing one free list, which is how the sparse set drives it:
 * three arrays growing at different times out of the same region. */
static void test_interleaved_growth_of_several_blocks(void) {
    SnFreeListAllocator alloc;
    TEST_ASSERT(sn_freelist_allocator_init(&alloc, region, REGION_SIZE));

    void *blocks[3];
    size_t sizes[3] = {24, 128, 64};
    const uint8_t fills[3] = {0x11, 0x22, 0x33};
    const size_t steps[8] = {40, 96, 200, 312, 456, 600, 900, 1200};

    for (int i = 0; i < 3; ++i) {
        blocks[i] = sn_freelist_allocator_allocate(&alloc, sizes[i], 8);
        TEST_ASSERT(blocks[i]);
        memset(blocks[i], fills[i], sizes[i]);
    }

    for (size_t step = 0; step < sizeof(steps) / sizeof(steps[0]); ++step) {
        for (int i = 0; i < 3; ++i) {
            /* A shrink releases the tail, so only the overlap is guaranteed. */
            const size_t preserved = sizes[i] < steps[step] ? sizes[i] : steps[step];

            void *moved = sn_freelist_allocator_reallocate(&alloc, blocks[i], steps[step], 8);
            TEST_ASSERT(moved);
            for (size_t b = 0; b < preserved; ++b) {
                if (((uint8_t *)moved)[b] != fills[i]) {
                    fprintf(stderr, "FAIL: step %zu block %d byte %zu not preserved\n", step, i, b);
                    abort();
                }
            }
            memset(moved, fills[i], steps[step]);
            blocks[i] = moved;
            sizes[i] = steps[step];
        }

        const void *live[] = {blocks[0], blocks[1], blocks[2]};
        check_free_list(&alloc, live, 3, "interleaved growth");
    }

    sn_freelist_allocator_deinit(&alloc);
}

int main(void) {
    region_lo = (uintptr_t)region;
    region_hi = (uintptr_t)region + REGION_SIZE;

    test_extend_into_too_small_adjacent_node();
    test_extend_boundary_across_sizes();
    test_interleaved_growth_of_several_blocks();

    puts("freelist_realloc_test: all checks passed");
    return 0;
}
