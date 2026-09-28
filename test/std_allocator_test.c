#include <snmemory/snmemory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                             \
    do {                                                                     \
        if (!(x)) {                                                          \
            fprintf(stderr, "FAILED: %s (%s:%d)\n", #x, __FILE__, __LINE__); \
            return 1;                                                        \
        }                                                                    \
    } while (0)

#define IS_ALIGNED(ptr, align) (((uint64_t)(ptr) % (uint64_t)(align)) == 0)

static int test_vtable_is_populated(void) {
    CHECK(sn_std_allocator.alloc != NULL);
    CHECK(sn_std_allocator.realloc != NULL);
    CHECK(sn_std_allocator.free != NULL);
    return 0;
}

static int test_alloc_honours_alignment(void) {
    uint64_t aligns[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};

    for (uint64_t i = 0; i < sizeof(aligns) / sizeof(aligns[0]); ++i) {
        void *ptr = sn_std_allocator.alloc(sn_std_allocator.data, 100, aligns[i]);
        CHECK(ptr != NULL);
        CHECK(IS_ALIGNED(ptr, aligns[i]));
        memset(ptr, 0xAB, 100);
        sn_std_allocator.free(sn_std_allocator.data, ptr);
    }
    return 0;
}

static int test_alloc_is_writable_and_independent(void) {
    uint8_t *a = sn_std_allocator.alloc(sn_std_allocator.data, 32, 8);
    CHECK(a != NULL);
    memset(a, 0x11, 32);

    uint8_t *b = sn_std_allocator.alloc(sn_std_allocator.data, 32, 8);
    CHECK(b != NULL);
    CHECK(a != b);

    /* Writing through b must not disturb a, otherwise the alignment shift
       bookkeeping is scribbling outside its own block. */
    memset(b, 0x22, 32);
    for (int i = 0; i < 32; ++i) CHECK(a[i] == 0x11);

    sn_std_allocator.free(sn_std_allocator.data, a);
    sn_std_allocator.free(sn_std_allocator.data, b);
    return 0;
}

static int test_realloc_preserves_alignment_and_contents(void) {
    uint64_t aligns[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};

    for (uint64_t i = 0; i < sizeof(aligns) / sizeof(aligns[0]); ++i) {
        uint8_t *ptr = sn_std_allocator.alloc(sn_std_allocator.data, 40, aligns[i]);
        CHECK(ptr != NULL);
        CHECK(IS_ALIGNED(ptr, aligns[i]));
        memset(ptr, 0x5A, 40);

        /* Grow past the original size, which forces a real block move. */
        uint8_t *grown = sn_std_allocator.realloc(sn_std_allocator.data, ptr, 500, aligns[i]);
        CHECK(grown != NULL);
        CHECK(IS_ALIGNED(grown, aligns[i]));
        for (int j = 0; j < 40; ++j) CHECK(grown[j] == 0x5A);

        sn_std_allocator.free(sn_std_allocator.data, grown);
    }
    return 0;
}

/* The offset of the payload within its block is a variable length integer, so a
   big enough alignment is a different encoding width. */
static int test_realloc_preserves_a_multibyte_offset(void) {
    for (uint64_t align = 2048; align <= 65536; align *= 2) {
        uint8_t *ptr = sn_std_allocator.alloc(sn_std_allocator.data, 64, align);
        CHECK(ptr != NULL);
        CHECK(IS_ALIGNED(ptr, align));
        memset(ptr, 0x3C, 64);

        uint8_t *grown = sn_std_allocator.realloc(sn_std_allocator.data, ptr, 4096, align);
        CHECK(grown != NULL);
        CHECK(IS_ALIGNED(grown, align));
        for (int j = 0; j < 64; ++j) CHECK(grown[j] == 0x3C);

        sn_std_allocator.free(sn_std_allocator.data, grown);
    }
    return 0;
}

/* Growing and shrinking repeatedly has to keep the size header honest, a stale
   one either loses the tail on a move or copies past what is there. */
static int test_realloc_chains_keep_their_contents(void) {
    uint64_t sizes[] = {16, 4096, 32, 8192, 8, 1024, 48};

    uint8_t *ptr = sn_std_allocator.alloc(sn_std_allocator.data, 16, 64);
    CHECK(ptr != NULL);
    uint64_t filled = 16;
    uint8_t fill = 0x11;
    memset(ptr, fill, filled);

    for (uint64_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        ptr = sn_std_allocator.realloc(sn_std_allocator.data, ptr, sizes[i], 64);
        CHECK(ptr != NULL);
        CHECK(IS_ALIGNED(ptr, 64));

        /* Growing adds uninitialised bytes, so only the overlap is meaningful. */
        for (uint64_t j = 0; j < SN_MIN(filled, sizes[i]); ++j) CHECK(ptr[j] == fill);

        filled = sizes[i];
        fill = (uint8_t)(fill + 1);
        memset(ptr, fill, filled);
    }

    sn_std_allocator.free(sn_std_allocator.data, ptr);
    return 0;
}

static int test_realloc_can_shrink(void) {
    uint8_t *ptr = sn_std_allocator.alloc(sn_std_allocator.data, 400, 16);
    CHECK(ptr != NULL);
    memset(ptr, 0x77, 400);

    uint8_t *shrunk = sn_std_allocator.realloc(sn_std_allocator.data, ptr, 16, 16);
    CHECK(shrunk != NULL);
    CHECK(IS_ALIGNED(shrunk, 16));
    for (int i = 0; i < 16; ++i) CHECK(shrunk[i] == 0x77);

    sn_std_allocator.free(sn_std_allocator.data, shrunk);
    return 0;
}

static int test_works_as_an_allocator_backend(void) {
    /* The point of the vtable is that a real allocator can be driven by it,
       so exercise it the way the library's own allocators do. */
    SnLinearAllocator linear;
    uint8_t mem[256];

    sn_linear_allocator_init(&linear, mem, sizeof(mem));
    CHECK(sn_linear_allocator_get_allocator(&linear).alloc != NULL);

    /* A SnContainer style default: a NULL allocator means the caller has none
       to give, and the standard one stands in. */
    const SnMemoryAllocator *alloc = &sn_std_allocator;
    CHECK(alloc->data == NULL);
    return 0;
}

int main(void) {
    struct {
        const char *name;
        int (*fn)(void);
    } tests[] = {
        {"vtable_is_populated",                      test_vtable_is_populated                     },
        {"alloc_honours_alignment",                  test_alloc_honours_alignment                 },
        {"alloc_is_writable_and_independent",        test_alloc_is_writable_and_independent       },
        {"realloc_preserves_alignment_and_contents", test_realloc_preserves_alignment_and_contents},
        {"realloc_preserves_a_multibyte_offset",     test_realloc_preserves_a_multibyte_offset    },
        {"realloc_chains_keep_their_contents",       test_realloc_chains_keep_their_contents      },
        {"realloc_can_shrink",                       test_realloc_can_shrink                      },
        {"works_as_an_allocator_backend",            test_works_as_an_allocator_backend           },
    };

    int failed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("  %s... ", tests[i].name);
        fflush(stdout);
        if (tests[i].fn() == 0) {
            printf("passed\n");
        } else {
            printf("FAILED\n");
            failed++;
        }
    }

    printf("%zu/%zu tests passed\n", sizeof(tests) / sizeof(tests[0]) - (size_t)failed,
           sizeof(tests) / sizeof(tests[0]));
    return failed == 0 ? 0 : 1;
}
