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
            __asm__ volatile("int $3");                                                                     \
        }                                                                                                   \
    } while (0)

#define KB(x) ((x) * 1024ULL)

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(name)            \
    do {                          \
        tests_run++;              \
        printf("  %s...", #name); \
        fflush(stdout);           \
        name();                   \
        printf(" passed\n");      \
        tests_passed++;           \
    } while (0)

static void test_init(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    TEST_ASSERT(rb.buffer == mem);
    TEST_ASSERT(rb.size == 64);
    TEST_ASSERT(rb.write_offset == 0);
    TEST_ASSERT(rb.read_offset == 0);
}

static void test_is_empty(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));
    TEST_ASSERT(sn_ring_buffer_allocator_is_empty(&rb));

    sn_ring_buffer_allocator_advance_read(&rb, 0);
    TEST_ASSERT(sn_ring_buffer_allocator_is_empty(&rb));

    sn_ring_buffer_allocator_reset(&rb);
    TEST_ASSERT(sn_ring_buffer_allocator_is_empty(&rb));
}

static void test_free_size(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));
    TEST_ASSERT(sn_ring_buffer_allocator_free_size(&rb) == 63);
}

static void test_read_ptr(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));
    TEST_ASSERT(sn_ring_buffer_allocator_read_ptr(&rb) == mem);
}

static void test_reset(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_advance_read(&rb, 8);
    TEST_ASSERT(rb.read_offset == 8);

    sn_ring_buffer_allocator_reset(&rb);
    TEST_ASSERT(rb.write_offset == 0);
    TEST_ASSERT(rb.read_offset == 0);
    TEST_ASSERT(sn_ring_buffer_allocator_is_empty(&rb));
    TEST_ASSERT(sn_ring_buffer_allocator_free_size(&rb) == 63);
}

static void test_alloc_simple(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    void *p = sn_ring_buffer_allocator_allocate(&rb, 16, 1);
    TEST_ASSERT(p != NULL);
    TEST_ASSERT(p == mem);
    TEST_ASSERT(rb.write_offset == 16);
    TEST_ASSERT(!sn_ring_buffer_allocator_is_empty(&rb));
    TEST_ASSERT(sn_ring_buffer_allocator_free_size(&rb) == 47);
}

static void test_alloc_multiple(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    void *p1 = sn_ring_buffer_allocator_allocate(&rb, 16, 1);
    TEST_ASSERT(p1 == mem);

    void *p2 = sn_ring_buffer_allocator_allocate(&rb, 16, 1);
    TEST_ASSERT(p2 == mem + 16);

    void *p3 = sn_ring_buffer_allocator_allocate(&rb, 16, 1);
    TEST_ASSERT(p3 == mem + 32);

    TEST_ASSERT(rb.write_offset == 48);
}

static void test_alloc_alignment(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_allocate(&rb, 5, 1);
    TEST_ASSERT(rb.write_offset == 5);

    void *p = sn_ring_buffer_allocator_allocate(&rb, 8, 8);
    TEST_ASSERT(p != NULL);
    TEST_ASSERT(((uint64_t)p % 8) == 0);
    TEST_ASSERT(p == mem + 8);
}

static void test_write_and_read_back(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    void *p = sn_ring_buffer_allocator_allocate(&rb, 16, 1);
    TEST_ASSERT(p != NULL);

    memcpy(p, "Hello Ring Buffer!", 18);

    void *read_ptr = sn_ring_buffer_allocator_read_ptr(&rb);
    TEST_ASSERT(read_ptr == mem);
    TEST_ASSERT(memcmp(read_ptr, "Hello Ring Buffer!", 18) == 0);

    sn_ring_buffer_allocator_advance_read(&rb, 16);
    TEST_ASSERT(rb.read_offset == 16);
    TEST_ASSERT(sn_ring_buffer_allocator_is_empty(&rb));
}

static void test_alloc_after_advance(void) {
    uint8_t mem[32];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_allocate(&rb, 10, 1);
    TEST_ASSERT(rb.write_offset == 10);

    sn_ring_buffer_allocator_advance_read(&rb, 10);
    TEST_ASSERT(rb.read_offset == 10);

    void *p = sn_ring_buffer_allocator_allocate(&rb, 20, 1);
    TEST_ASSERT(p != NULL);
    TEST_ASSERT(p == mem + 10);
    TEST_ASSERT(rb.write_offset == 30);
}

static void test_alloc_wrap(void) {
    uint8_t mem[32];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_allocate(&rb, 10, 1);
    sn_ring_buffer_allocator_advance_read(&rb, 10);

    sn_ring_buffer_allocator_allocate(&rb, 10, 1);
    sn_ring_buffer_allocator_advance_read(&rb, 10);

    void *p = sn_ring_buffer_allocator_allocate(&rb, 15, 1);
    TEST_ASSERT(p != NULL);
    TEST_ASSERT(p == mem);
    TEST_ASSERT(rb.write_offset == 15);
}

static void test_alloc_after_read_wrap(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_allocate(&rb, 20, 1);
    sn_ring_buffer_allocator_allocate(&rb, 20, 1);
    sn_ring_buffer_allocator_advance_read(&rb, 20);

    void *p = sn_ring_buffer_allocator_allocate(&rb, 20, 1);
    TEST_ASSERT(p != NULL);
    TEST_ASSERT(p == mem + 40);
}

static void test_alloc_full(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    void *p1 = sn_ring_buffer_allocator_allocate(&rb, 62, 1);
    TEST_ASSERT(p1 != NULL);

    void *p2 = sn_ring_buffer_allocator_allocate(&rb, 1, 1);
    TEST_ASSERT(p2 == NULL);
}

static void test_advance_read_wrap(void) {
    uint8_t mem[64];
    SnRingBufferAllocator rb;

    sn_ring_buffer_allocator_init(&rb, mem, sizeof(mem));

    sn_ring_buffer_allocator_advance_read(&rb, 50);
    TEST_ASSERT(rb.read_offset == 50);

    sn_ring_buffer_allocator_advance_read(&rb, 20);
    TEST_ASSERT(rb.read_offset == 6);
}

int main(void) {
    printf("SnRingBufferAllocator tests:\n");

    RUN_TEST(test_init);
    RUN_TEST(test_is_empty);
    RUN_TEST(test_free_size);
    RUN_TEST(test_read_ptr);
    RUN_TEST(test_reset);
    RUN_TEST(test_alloc_simple);
    RUN_TEST(test_alloc_multiple);
    RUN_TEST(test_alloc_alignment);
    RUN_TEST(test_write_and_read_back);
    RUN_TEST(test_alloc_after_advance);
    RUN_TEST(test_alloc_wrap);
    RUN_TEST(test_alloc_after_read_wrap);
    RUN_TEST(test_alloc_full);
    RUN_TEST(test_advance_read_wrap);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
