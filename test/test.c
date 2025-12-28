#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <snmemory/defines.h>
#include <snmemory/snmemory.h>

#define TEST_ASSERT(x) do { \
    if (!(x)) { \
        fprintf(stderr, "ASSERT FAILED: %s in function %s(%s:%d)\n", #x, __func__, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)

#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * 1024ULL)

static uint64_t rand_range(uint64_t min, uint64_t max) {
    return min + (rand() % (max - min + 1));
}

static void fill_pattern(void *ptr, size_t size, uint8_t seed) {
    uint8_t *p = ptr;
    for (size_t i = 0; i < size; i++)
        p[i] = (uint8_t)(seed + i);
}

static void verify_pattern(void *ptr, size_t size, uint8_t seed) {
    uint8_t *p = ptr;
    for (size_t i = 0; i < size; i++)
        TEST_ASSERT(p[i] == (uint8_t)(seed + i));
}

static void test_linear_allocator(void) {
    uint8_t buffer[KB(4)];
    snLinearAllocator alloc;

    TEST_ASSERT(sn_linear_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *p1 = sn_linear_allocator_allocate(&alloc, 64, 8);
    void *p2 = sn_linear_allocator_allocate(&alloc, 128, 16);
    void *p3 = sn_linear_allocator_allocate(&alloc, 32, 4);

    TEST_ASSERT(p1 && p2 && p3);
    TEST_ASSERT(SN_IS_ALIGNED(p1, 8));
    TEST_ASSERT(SN_IS_ALIGNED(p2, 16));
    TEST_ASSERT(SN_IS_ALIGNED(p3, 4));

    uint64_t used = sn_linear_allocator_get_allocated_size(&alloc);
    TEST_ASSERT(used > 0);
    TEST_ASSERT(used <= sizeof(buffer));

    snMemoryMark mark = sn_linear_allocator_get_memory_mark(&alloc);

    void *p4 = sn_linear_allocator_allocate(&alloc, 256, 32);
    TEST_ASSERT(p4);

    sn_linear_allocator_free_to_memory_mark(&alloc, mark);
    TEST_ASSERT(sn_linear_allocator_get_allocated_size(&alloc) == used);

    sn_linear_allocator_reset(&alloc);
    TEST_ASSERT(sn_linear_allocator_get_allocated_size(&alloc) == 0);

    sn_linear_allocator_deinit(&alloc);
}

static void test_stack_allocator(void) {
    uint8_t buffer[KB(4)];
    snStackAllocator alloc;

    TEST_ASSERT(sn_stack_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *a = sn_stack_allocator_allocate(&alloc, 64, 8);
    void *b = sn_stack_allocator_allocate(&alloc, 128, 16);
    void *c = sn_stack_allocator_allocate(&alloc, 32, 4);

    TEST_ASSERT(a && b && c);
    TEST_ASSERT(SN_IS_ALIGNED(a, 8));
    TEST_ASSERT(SN_IS_ALIGNED(b, 16));
    TEST_ASSERT(SN_IS_ALIGNED(c, 4));

    sn_stack_allocator_free(&alloc, c);
    sn_stack_allocator_free(&alloc, b);
    sn_stack_allocator_free(&alloc, a);

    TEST_ASSERT(sn_stack_allocator_get_allocated_size(&alloc) == 0);

    sn_stack_allocator_reset(&alloc);
    TEST_ASSERT(alloc.top == alloc.mem);

    sn_stack_allocator_deinit(&alloc);
}

static void test_pool_allocator(void) {
    uint8_t buffer[KB(4)];
    snPoolAllocator alloc;

    TEST_ASSERT(
        sn_pool_allocator_init(&alloc, buffer, sizeof(buffer), 64, 8)
    );

    uint64_t total = sn_pool_allocator_get_block_count(&alloc);
    TEST_ASSERT(total > 0);

    void *blocks[128];
    uint64_t count = 0;

    while ((blocks[count] = sn_pool_allocator_allocate(&alloc))) {
        TEST_ASSERT(SN_IS_ALIGNED(blocks[count], 8));
        count++;
        TEST_ASSERT(count < 128);
    }

    TEST_ASSERT(count == total);
    TEST_ASSERT(sn_pool_allocator_get_free_count(&alloc) == 0);

    for (uint64_t i = 0; i < count; i++) {
        sn_pool_allocator_free(&alloc, blocks[i]);
    }

    TEST_ASSERT(sn_pool_allocator_get_free_count(&alloc) == total);

    sn_pool_allocator_deinit(&alloc);
}

static void test_frame_allocator(void) {
    uint8_t buffer[KB(8)];
    snFrameAllocator alloc;

    TEST_ASSERT(sn_frame_allocator_init(&alloc, buffer, sizeof(buffer)));

    sn_frame_allocator_begin(&alloc);

    void *a = sn_frame_allocator_allocate(&alloc, 128, 16);
    void *b = sn_frame_allocator_allocate(&alloc, 256, 32);

    TEST_ASSERT(a && b);
    TEST_ASSERT(SN_IS_ALIGNED(a, 16));
    TEST_ASSERT(SN_IS_ALIGNED(b, 32));

    uint64_t used = sn_frame_allocator_get_frame_usage(&alloc);
    TEST_ASSERT(used > 0);

    sn_frame_allocator_end(&alloc);

    TEST_ASSERT(sn_frame_allocator_get_frame_usage(&alloc) == 0);

    sn_frame_allocator_deinit(&alloc);
}

static void test_freelist_allocator_basic(void) {
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    TEST_ASSERT(sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *a = sn_freelist_allocator_allocate(&alloc, 128, 8);
    void *b = sn_freelist_allocator_allocate(&alloc, 256, 16);
    void *c = sn_freelist_allocator_allocate(&alloc, 64, 4);

    TEST_ASSERT(a && b && c);
    TEST_ASSERT(SN_IS_ALIGNED(a, 8));
    TEST_ASSERT(SN_IS_ALIGNED(b, 16));
    TEST_ASSERT(SN_IS_ALIGNED(c, 4));

    uint64_t free_before = sn_freelist_allocator_get_free_size(&alloc);

    sn_freelist_allocator_free(&alloc, b);
    sn_freelist_allocator_free(&alloc, a);
    sn_freelist_allocator_free(&alloc, c);

    uint64_t free_after = sn_freelist_allocator_get_free_size(&alloc);
    TEST_ASSERT(free_after >= free_before);

    sn_freelist_allocator_deinit(&alloc);
}

static void test_freelist_allocator_realloc(void) {
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    TEST_ASSERT(sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *p = sn_freelist_allocator_allocate(&alloc, 128, 8);
    TEST_ASSERT(p);

    memset(p, 0xAA, 128);

    p = sn_freelist_allocator_reallocate(&alloc, p, 256, 16);
    TEST_ASSERT(p);
    TEST_ASSERT(SN_IS_ALIGNED(p, 16));

    for (int i = 0; i < 128; i++) {
        TEST_ASSERT(((uint8_t *)p)[i] == 0xAA);
    }

    p = sn_freelist_allocator_reallocate(&alloc, p, 64, 8);
    TEST_ASSERT(p);

    sn_freelist_allocator_free(&alloc, p);
    sn_freelist_allocator_deinit(&alloc);
}

static void test_linear_allocator_exhaustion(void) {
    uint8_t buffer[1024];
    snLinearAllocator alloc;

    TEST_ASSERT(sn_linear_allocator_init(&alloc, buffer, sizeof(buffer)));

    uint64_t used = 0;
    while (true) {
        uint64_t size  = rand_range(1, 64);
        uint64_t align = 1ULL << rand_range(0, 5); // 1..32

        void *p = sn_linear_allocator_allocate(&alloc, size, align);
        if (!p) break;

        TEST_ASSERT(SN_IS_ALIGNED(p, align));
        used += size;
    }

    TEST_ASSERT(sn_linear_allocator_get_remaining_size(&alloc) < 64);
}

static void test_linear_allocator_marks(void) {
    uint8_t buffer[2048];
    snLinearAllocator alloc;

    sn_linear_allocator_init(&alloc, buffer, sizeof(buffer));

    snMemoryMark marks[32];
    int mark_count = 0;

    for (int i = 0; i < 32; i++) {
        marks[mark_count++] = sn_linear_allocator_get_memory_mark(&alloc);
        sn_linear_allocator_allocate(&alloc, 32, 8);
    }

    for (int i = mark_count - 1; i >= 0; i--) {
        sn_linear_allocator_free_to_memory_mark(&alloc, marks[i]);
    }

    TEST_ASSERT(sn_linear_allocator_get_allocated_size(&alloc) == 0);
}

static void test_stack_allocator_lifo(void) {
    uint8_t buffer[2048 + 16];
    snStackAllocator alloc;

    sn_stack_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[64];
    for (int i = 0; i < 64; i++) {
        ptrs[i] = sn_stack_allocator_allocate(&alloc, 16, 8);
        TEST_ASSERT(ptrs[i]);
    }

    for (int i = 63; i >= 0; i--) {
        sn_stack_allocator_free(&alloc, ptrs[i]);
    }

    TEST_ASSERT(sn_stack_allocator_get_allocated_size(&alloc) == 0);
}

static void test_stack_allocator_alignment_stress(void) {
    uint8_t buffer[8192];
    snStackAllocator alloc;

    TEST_ASSERT(sn_stack_allocator_init(&alloc, buffer, sizeof(buffer)));

    int success_count = 0;

    for (int i = 0; i < 200; i++) {
        uint64_t align = 1ULL << rand_range(0, 6); // 1..64
        uint64_t size  = rand_range(1, 64);

        void *p = sn_stack_allocator_allocate(&alloc, size, align);
        if (!p) {
            break; // expected when memory runs out
        }

        TEST_ASSERT(SN_IS_ALIGNED(p, align));
        success_count++;
    }

    TEST_ASSERT(success_count > 0);

    sn_stack_allocator_reset(&alloc);
    TEST_ASSERT(sn_stack_allocator_get_allocated_size(&alloc) == 0);

    sn_stack_allocator_deinit(&alloc);
}

static void test_pool_allocator_random_free(void) {
    uint8_t buffer[4096];
    snPoolAllocator alloc;

    sn_pool_allocator_init(&alloc, buffer, sizeof(buffer), 64, 8);

    uint64_t n = sn_pool_allocator_get_block_count(&alloc);
    void **ptrs = malloc(n * sizeof(void*));

    for (uint64_t i = 0; i < n; i++) {
        ptrs[i] = sn_pool_allocator_allocate(&alloc);
        TEST_ASSERT(ptrs[i]);
    }

    // Shuffle free order
    for (uint64_t i = 0; i < n; i++) {
        uint64_t j = rand_range(0, n - 1);
        void *tmp = ptrs[i];
        ptrs[i] = ptrs[j];
        ptrs[j] = tmp;
    }

    for (uint64_t i = 0; i < n; i++) {
        sn_pool_allocator_free(&alloc, ptrs[i]);
    }

    TEST_ASSERT(sn_pool_allocator_get_free_count(&alloc) == n);
    free(ptrs);
}

static void test_freelist_fragmentation(void) {
    uint8_t buffer[KB(48)];
    snFreeListAllocator alloc;

    TEST_ASSERT(sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *ptrs[256] = {0};
    int alloc_count = 0;

    for (int i = 0; i < 256; i++) {
        ptrs[i] = sn_freelist_allocator_allocate(
            &alloc,
            rand_range(8, 256),
            1ULL << rand_range(0, 5)
        );

        if (!ptrs[i])
            break;

        alloc_count++;
    }

    TEST_ASSERT(alloc_count > 0);

    // Free every other allocation
    for (int i = 0; i < alloc_count; i += 2) {
        sn_freelist_allocator_free(&alloc, ptrs[i]);
        ptrs[i] = NULL;
    }

    // Try to reuse fragmented space
    int reuse_count = 0;
    for (int i = 0; i < alloc_count / 2; i++) {
        void *p = sn_freelist_allocator_allocate(&alloc, 64, 8);
        if (!p)
            break;

        TEST_ASSERT(SN_IS_ALIGNED(p, 8));
        reuse_count++;
    }

    TEST_ASSERT(reuse_count > 0);

    sn_freelist_allocator_deinit(&alloc);
}

static void test_freelist_realloc_loop(void) {
    uint8_t buffer[KB(32)];
    snFreeListAllocator alloc;

    sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer));

    void *p = sn_freelist_allocator_allocate(&alloc, 32, 8);
    TEST_ASSERT(p);

    uint64_t min_size = 32;
    fill_pattern(p, min_size, 0x42);

    for (int i = 0; i < 100; i++) {
        uint64_t new_size = rand_range(16, 512);
        p = sn_freelist_allocator_reallocate(&alloc, p, new_size, 8);
        TEST_ASSERT(p);
        if (new_size < min_size) min_size = new_size; // only upto min_size guarantted to retain the content.
        verify_pattern(p, SN_MIN(min_size, new_size), 0x42);
    }

    sn_freelist_allocator_free(&alloc, p);
}

static void test_freelist_full_reuse(void) {
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[128];
    int count = 0;

    while ((ptrs[count] = sn_freelist_allocator_allocate(&alloc, 128, 8))) {
        count++;
        TEST_ASSERT(count < 128);
    }

    for (int i = 0; i < count; i++) {
        sn_freelist_allocator_free(&alloc, ptrs[i]);
    }

    uint64_t free_size = sn_freelist_allocator_get_free_size(&alloc);
    TEST_ASSERT(free_size >= KB(15)); // Almost entire buffer
}

int main(void) {
    int n = 100;

    printf("Running allocator tests %d times...\n\n", n);

    for (int run = 0; run < n; ++run) {
        srand((unsigned)time(NULL) + run);
        // srand(0xC0FFEE + run);

        /* Linear allocator */
        printf("Running test_linear_allocator...\n");
        test_linear_allocator();
        test_linear_allocator_exhaustion();
        test_linear_allocator_marks();
        printf("Linear allocator tests passed ✅\n\n");

        /* Stack allocator */
        printf("Running test_stack_allocator...\n");
        test_stack_allocator();
        test_stack_allocator_lifo();
        test_stack_allocator_alignment_stress();
        printf("Stack allocator tests passed ✅\n\n");

        /* Pool allocator */
        printf("Running test_pool_allocator...\n");
        test_pool_allocator();
        test_pool_allocator_random_free();
        printf("Pool allocator tests passed ✅\n\n");

        /* Frame allocator */
        printf("Running test_frame_allocator...\n");
        test_frame_allocator();
        printf("Frame allocator tests passed ✅\n\n");

        /* Free-list allocator */
        printf("Running test_freelist_allocator_basic...\n");
        test_freelist_allocator_basic();

        printf("Running test_freelist_allocator_realloc...\n");
        test_freelist_allocator_realloc();

        printf("Running test_freelist_fragmentation...\n");
        test_freelist_fragmentation();

        printf("Running test_freelist_realloc_loop...\n");
        test_freelist_realloc_loop();

        printf("Running test_freelist_full_reuse...\n");
        test_freelist_full_reuse();

        printf("Free-list allocator tests passed ✅\n\n");

    }

    printf("ALL ALLOCATOR TESTS PASSED %d times ✅✅✅\n", n);

    return 0;
}

