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
        __asm__ volatile("int $3"); \
    } \
} while (0)

#define KB(x) ((x) * 1024ULL)
#define MB(x) ((x) * 1024ULL * 1024ULL)

/*
    Small slack is allowed because:

    - headers consume bytes
    - alignment introduces padding gaps
    - freelist fragmentation may create unusable regions
#define ACCOUNTING_SLACK 256
*/

static void check_accounting(uint64_t allocated, uint64_t remaining, uint64_t total)
{
    TEST_ASSERT(allocated <= total);
    TEST_ASSERT(remaining <= total);

    TEST_ASSERT(allocated + remaining <= total);

    uint64_t slack = total - (allocated + remaining);
    TEST_ASSERT(slack == 0);

    // TEST_ASSERT(slack < ACCOUNTING_SLACK);
}

static uint64_t rand_range(uint64_t min, uint64_t max)
{
    return min + (rand() % (max - min + 1));
}

static void fill_pattern(void *ptr, size_t size, uint8_t seed)
{
    uint8_t *p = ptr;

    for (size_t i = 0; i < size; i++)
        p[i] = (uint8_t)(seed + i);
}

static void verify_pattern(void *ptr, size_t size, uint8_t seed)
{
    uint8_t *p = ptr;

    for (size_t i = 0; i < size; i++)
        TEST_ASSERT(p[i] == (uint8_t)(seed + i));
}

static void check_linear(snLinearAllocator *a, uint64_t total)
{
    check_accounting(
        sn_linear_allocator_get_allocated_size(a),
        sn_linear_allocator_get_remaining_size(a),
        total
    );
}

static void check_stack(snStackAllocator *a, uint64_t total)
{
    check_accounting(
        sn_stack_allocator_get_allocated_size(a),
        sn_stack_allocator_get_remaining_size(a),
        total
    );
}

static void check_queue(snQueueAllocator *a, uint64_t total)
{
    check_accounting(
        sn_queue_allocator_get_allocated_size(a),
        sn_queue_allocator_get_remaining_size(a),
        total
    );
}

static void check_freelist(snFreeListAllocator *a, uint64_t total)
{
    uint64_t free_size = sn_freelist_allocator_get_free_size(a);

    check_accounting(
        total - free_size,
        free_size,
        total
    );
}

static void check_frame(snFrameAllocator *a, uint64_t total)
{
    uint64_t used = sn_frame_allocator_get_frame_usage(a);

    check_accounting(
        used,
        total - used,
        total
    );
}

static void check_pool(snPoolAllocator *a)
{
    uint64_t total = sn_pool_allocator_get_block_count(a);
    uint64_t free  = sn_pool_allocator_get_free_count(a);

    TEST_ASSERT(free <= total);
}

/* ------------------------------------------------------------- */
/* Linear allocator */

static void test_linear_allocator(void)
{
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

    check_linear(&alloc, sizeof(buffer));

    snMemoryMark mark = sn_linear_allocator_get_memory_mark(&alloc);

    void *p4 = sn_linear_allocator_allocate(&alloc, 256, 32);

    TEST_ASSERT(p4);

    check_linear(&alloc, sizeof(buffer));

    sn_linear_allocator_free_to_memory_mark(&alloc, mark);

    check_linear(&alloc, sizeof(buffer));

    sn_linear_allocator_reset(&alloc);

    check_linear(&alloc, sizeof(buffer));

    sn_linear_allocator_deinit(&alloc);
}

static void test_linear_allocator_exhaustion(void)
{
    uint8_t buffer[1024];
    snLinearAllocator alloc;

    TEST_ASSERT(sn_linear_allocator_init(&alloc, buffer, sizeof(buffer)));

    while (true)
    {
        uint64_t size  = rand_range(1, 64);
        uint64_t align = 1ULL << rand_range(0, 5);

        void *p = sn_linear_allocator_allocate(&alloc, size, align);

        if (!p)
            break;

        TEST_ASSERT(SN_IS_ALIGNED(p, align));

        check_linear(&alloc, sizeof(buffer));
    }

    TEST_ASSERT(sn_linear_allocator_get_remaining_size(&alloc) < (64 + 32));
}

static void test_linear_allocator_marks(void)
{
    uint8_t buffer[2048];
    snLinearAllocator alloc;

    sn_linear_allocator_init(&alloc, buffer, sizeof(buffer));

    snMemoryMark marks[32];

    for (int i = 0; i < 32; i++)
    {
        marks[i] = sn_linear_allocator_get_memory_mark(&alloc);

        sn_linear_allocator_allocate(&alloc, 32, 8);

        check_linear(&alloc, sizeof(buffer));
    }

    for (int i = 31; i >= 0; i--)
    {
        sn_linear_allocator_free_to_memory_mark(&alloc, marks[i]);

        check_linear(&alloc, sizeof(buffer));
    }
}

/* ------------------------------------------------------------- */
/* Stack allocator */

static void test_stack_allocator(void)
{
    uint8_t buffer[KB(4)];
    snStackAllocator alloc;

    TEST_ASSERT(sn_stack_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *a = sn_stack_allocator_allocate(&alloc, 64, 8);
    void *b = sn_stack_allocator_allocate(&alloc, 128, 16);
    void *c = sn_stack_allocator_allocate(&alloc, 32, 4);

    TEST_ASSERT(a && b && c);

    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_free(&alloc, c);
    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_free(&alloc, b);
    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_free(&alloc, a);

    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_reset(&alloc);

    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_deinit(&alloc);
}

static void test_stack_allocator_lifo(void)
{
    uint8_t buffer[2048 + 16];
    snStackAllocator alloc;

    sn_stack_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[64];

    for (int i = 0; i < 64; i++)
    {
        ptrs[i] = sn_stack_allocator_allocate(&alloc, 16, 8);

        TEST_ASSERT(ptrs[i]);

        check_stack(&alloc, sizeof(buffer));
    }

    for (int i = 63; i >= 0; i--)
    {
        sn_stack_allocator_free(&alloc, ptrs[i]);

        check_stack(&alloc, sizeof(buffer));
    }
}

static void test_stack_allocator_alignment_stress(void)
{
    uint8_t buffer[8192];
    snStackAllocator alloc;

    TEST_ASSERT(sn_stack_allocator_init(&alloc, buffer, sizeof(buffer)));

    for (int i = 0; i < 200; i++)
    {
        uint64_t align = 1ULL << rand_range(0, 6);
        uint64_t size  = rand_range(1, 64);

        void *p = sn_stack_allocator_allocate(&alloc, size, align);

        if (!p)
            break;

        TEST_ASSERT(SN_IS_ALIGNED(p, align));

        check_stack(&alloc, sizeof(buffer));
    }

    sn_stack_allocator_reset(&alloc);

    check_stack(&alloc, sizeof(buffer));

    sn_stack_allocator_deinit(&alloc);
}

/* ------------------------------------------------------------- */
/* Pool allocator */

static void test_pool_allocator(void)
{
    uint8_t buffer[KB(4)];
    snPoolAllocator alloc;

    TEST_ASSERT(
        sn_pool_allocator_init(&alloc, buffer, sizeof(buffer), 64, 8)
    );

    check_pool(&alloc);

    uint64_t total = sn_pool_allocator_get_block_count(&alloc);

    void *blocks[128];

    uint64_t count = 0;

    while ((blocks[count] = sn_pool_allocator_allocate(&alloc)))
    {
        count++;

        check_pool(&alloc);
    }

    TEST_ASSERT(count == total);

    for (uint64_t i = 0; i < count; i++)
    {
        sn_pool_allocator_free(&alloc, blocks[i]);

        check_pool(&alloc);
    }
}

static void test_pool_allocator_random_free(void)
{
    uint8_t buffer[4096];
    snPoolAllocator alloc;

    sn_pool_allocator_init(&alloc, buffer, sizeof(buffer), 64, 8);

    uint64_t n = sn_pool_allocator_get_block_count(&alloc);

    void **ptrs = malloc(n * sizeof(void*));

    for (uint64_t i = 0; i < n; i++)
    {
        ptrs[i] = sn_pool_allocator_allocate(&alloc);

        check_pool(&alloc);
    }

    for (uint64_t i = 0; i < n; i++)
    {
        uint64_t j = rand_range(0, n - 1);

        void *tmp = ptrs[i];

        ptrs[i] = ptrs[j];

        ptrs[j] = tmp;
    }

    for (uint64_t i = 0; i < n; i++)
    {
        sn_pool_allocator_free(&alloc, ptrs[i]);

        check_pool(&alloc);
    }

    free(ptrs);
}

/* ------------------------------------------------------------- */
/* Frame allocator */

static void test_frame_allocator(void)
{
    uint8_t buffer[KB(8)];
    snFrameAllocator alloc;

    TEST_ASSERT(sn_frame_allocator_init(&alloc, buffer, sizeof(buffer)));

    sn_frame_allocator_begin(&alloc);

    void *a = sn_frame_allocator_allocate(&alloc, 128, 16);
    void *b = sn_frame_allocator_allocate(&alloc, 256, 32);

    TEST_ASSERT(a && b);

    check_frame(&alloc, sizeof(buffer));

    sn_frame_allocator_end(&alloc);

    TEST_ASSERT(sn_frame_allocator_get_frame_usage(&alloc) == 0);
}

/* ------------------------------------------------------------- */
/* Free list allocator */

static void test_freelist_allocator_basic(void)
{
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    TEST_ASSERT(sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer)));

    void *a = sn_freelist_allocator_allocate(&alloc, 128, 8);
    void *b = sn_freelist_allocator_allocate(&alloc, 256, 16);
    void *c = sn_freelist_allocator_allocate(&alloc, 64, 4);

    TEST_ASSERT(a && b && c);

    check_freelist(&alloc, sizeof(buffer));

    sn_freelist_allocator_free(&alloc, b);
    sn_freelist_allocator_free(&alloc, a);
    sn_freelist_allocator_free(&alloc, c);

    check_freelist(&alloc, sizeof(buffer));
}

static void test_freelist_allocator_realloc(void)
{
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer));

    void *p = sn_freelist_allocator_allocate(&alloc, 128, 8);

    fill_pattern(p, 128, 0xAA);

    p = sn_freelist_allocator_reallocate(&alloc, p, 256, 16);

    check_freelist(&alloc, sizeof(buffer));

    p = sn_freelist_allocator_reallocate(&alloc, p, 64, 8);

    check_freelist(&alloc, sizeof(buffer));

    sn_freelist_allocator_free(&alloc, p);

    check_freelist(&alloc, sizeof(buffer));
}

static void test_freelist_fragmentation(void)
{
    uint8_t buffer[KB(48)];
    snFreeListAllocator alloc;

    sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[256];

    int count = 0;

    for (int i = 0; i < 256; i++)
    {
        ptrs[i] = sn_freelist_allocator_allocate(
            &alloc,
            rand_range(8, 256),
            1ULL << rand_range(0, 5)
        );

        if (!ptrs[i])
            break;

        count++;

        check_freelist(&alloc, sizeof(buffer));
    }

    for (int i = 0; i < count; i += 2)
    {
        sn_freelist_allocator_free(&alloc, ptrs[i]);

        check_freelist(&alloc, sizeof(buffer));
    }
}

static void test_freelist_full_reuse(void)
{
    uint8_t buffer[KB(16)];
    snFreeListAllocator alloc;

    sn_freelist_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[128];

    int count = 0;

    while ((ptrs[count] =
        sn_freelist_allocator_allocate(&alloc, 128, 8)))
    {
        count++;

        check_freelist(&alloc, sizeof(buffer));
    }

    for (int i = 0; i < count; i++)
    {
        sn_freelist_allocator_free(&alloc, ptrs[i]);

        check_freelist(&alloc, sizeof(buffer));
    }
}

/* ------------------------------------------------------------- */
/* Queue allocator */

static void test_queue_allocator_basic(void)
{
    uint8_t buffer[KB(4)];
    snQueueAllocator alloc;

    sn_queue_allocator_init(&alloc, buffer, sizeof(buffer));

    void *a = sn_queue_allocator_allocate(&alloc, 64, 8);
    void *b = sn_queue_allocator_allocate(&alloc, 128, 16);
    void *c = sn_queue_allocator_allocate(&alloc, 32, 4);

    TEST_ASSERT(a && b && c);

    check_queue(&alloc, sizeof(buffer));

    sn_queue_allocator_free(&alloc, a);
    check_queue(&alloc, sizeof(buffer));

    sn_queue_allocator_free(&alloc, b);
    check_queue(&alloc, sizeof(buffer));

    sn_queue_allocator_free(&alloc, c);

    check_queue(&alloc, sizeof(buffer));
}

static void test_queue_allocator_wrap(void)
{
    uint8_t buffer[512];
    snQueueAllocator alloc;

    sn_queue_allocator_init(&alloc, buffer, sizeof(buffer));

    void *a = sn_queue_allocator_allocate(&alloc, 128, 8);
    void *b = sn_queue_allocator_allocate(&alloc, 128, 8);
    void *c = sn_queue_allocator_allocate(&alloc, 128, 8);

    check_queue(&alloc, sizeof(buffer));

    sn_queue_allocator_free(&alloc, a);
    sn_queue_allocator_free(&alloc, b);

    check_queue(&alloc, sizeof(buffer));

    void *d = sn_queue_allocator_allocate(&alloc, 128, 8);

    TEST_ASSERT(d);

    check_queue(&alloc, sizeof(buffer));

    sn_queue_allocator_free(&alloc, c);
    sn_queue_allocator_free(&alloc, d);

    check_queue(&alloc, sizeof(buffer));
}

#define TRACK_CAP 256

static void test_queue_allocator_random_stress(void)
{
    uint8_t buffer[KB(8)];
    snQueueAllocator alloc;

    sn_queue_allocator_init(&alloc, buffer, sizeof(buffer));

    void *ptrs[TRACK_CAP];
    uint64_t sizes[TRACK_CAP];
    uint8_t seeds[TRACK_CAP];

    uint32_t head = 0;
    uint32_t tail = 0;
    uint32_t count = 0;
    uint8_t next_seed = 1;

    for (int i = 0; i < 1000; i++)
    {
        bool do_alloc =
            (count == 0) ||
            (rand_range(0,1) == 0 && count < TRACK_CAP);

        if (do_alloc)
        {
            uint64_t size  = rand_range(8,128);
            uint64_t align = 1ULL << rand_range(0,5);

            void *p = sn_queue_allocator_allocate(&alloc, size, align);

            if (p)
            {
                TEST_ASSERT(SN_IS_ALIGNED(p, align));

                fill_pattern(p, size, next_seed);

                ptrs[head]  = p;
                sizes[head] = size;
                seeds[head] = next_seed;

                next_seed++;

                head = (head + 1) % TRACK_CAP;
                count++;
            }
        }
        else
        {
            verify_pattern(ptrs[tail], sizes[tail], seeds[tail]);

            sn_queue_allocator_free(&alloc, ptrs[tail]);

            tail = (tail + 1) % TRACK_CAP;
            count--;
        }
    }

    while (count > 0)
    {
        verify_pattern(ptrs[tail], sizes[tail], seeds[tail]);

        sn_queue_allocator_free(&alloc, ptrs[tail]);

        tail = (tail + 1) % TRACK_CAP;
        count--;
    }

    TEST_ASSERT(
        sn_queue_allocator_get_allocated_size(&alloc) == 0
    );
}

/* ------------------------------------------------------------- */
/* VM */

static void test_vm_basic(void)
{
    uint64_t page_size = sn_vm_get_page_size();

    TEST_ASSERT(page_size > 0);

    void *ptr = sn_vm_reserve(4);

    TEST_ASSERT(ptr);

    TEST_ASSERT(sn_vm_commit(ptr, 4));

    memset(ptr, 0xAB, page_size * 4);

    TEST_ASSERT(sn_vm_decommit(ptr, 4));

    TEST_ASSERT(sn_vm_release(ptr, 4));
}

/* ------------------------------------------------------------- */

int main(void)
{
    int n = 100;

    printf("Running allocator tests %d times...\n\n", n);

    for (int run = 0; run < n; ++run)
    {
        srand((unsigned)time(NULL) + run);

        test_linear_allocator();
        test_linear_allocator_exhaustion();
        test_linear_allocator_marks();

        test_stack_allocator();
        test_stack_allocator_lifo();
        test_stack_allocator_alignment_stress();

        test_pool_allocator();
        test_pool_allocator_random_free();

        test_frame_allocator();

        test_freelist_allocator_basic();
        test_freelist_allocator_realloc();
        test_freelist_fragmentation();
        test_freelist_full_reuse();

        test_queue_allocator_basic();
        test_queue_allocator_wrap();
        test_queue_allocator_random_stress();

        test_vm_basic();
    }

    printf("\nALL TESTS PASSED %d times ✅\n", n);

    return 0;
}
