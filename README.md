# SnMemory

Memory allocator library written in C.

SnMemory does **not** depend on malloc/free internally — all allocators
operate on user-provided memory buffers. Also includes a ring buffer utility
and virtual memory abstraction.

## Allocators

| Allocator | Description |
|-----------|-------------|
| Linear / Arena | Fast bump allocator with memory mark support |
| Stack | LIFO allocator |
| Pool | Fixed-size block allocator |
| Frame | Stack-like with frame boundaries (no nesting) |
| Free-list | General-purpose with reallocation support (slower) |

## Ring Buffer

Fixed-size ring buffer with allocation, free-space query, and read-pointer
advancement. Write-side overflow wraps around; the buffer tracks read and
write offsets to distinguish empty from full.

## Usage

```c
#include <snmemory/snmemory.h>
#include <stdio.h>

int main(void) {
    uint8_t buffer[1024];

    // Linear allocator
    SnLinearAllocator linear;
    sn_linear_allocator_init(&linear, buffer, sizeof(buffer));

    int *numbers = sn_linear_allocator_allocate(&linear, 10 * sizeof(int), alignof(int));
    if (numbers) {
        for (int i = 0; i < 10; ++i) numbers[i] = i;
        printf("Allocated 10 ints from linear allocator\n");
    }

    // Ring buffer
    SnRingBufferAllocator ring;
    sn_ring_buffer_allocator_init(&ring, buffer, sizeof(buffer));

    void *slot = sn_ring_buffer_allocator_allocate(&ring, 64, 8);
    if (slot) printf("Allocated 64 bytes from ring buffer\n");

    return 0;
}
```

## Adding to your project

```cmake
include(FetchContent)
FetchContent_Declare(snmemory
    GIT_REPOSITORY https://github.com/kshku/SnMemory.git
    GIT_TAG <tag>  # e.g., v0.1.0
)
FetchContent_MakeAvailable(snmemory)

target_link_libraries(myapp PRIVATE snmemory)
```

## Build

```sh
cmake -B build
cmake --build build
```

| Option | Default | Description |
|--------|---------|-------------|
| `SN_MEMORY_BUILD_SHARED` | `OFF` | Build as shared library |
| `SN_MEMORY_BUILD_TEST` | `OFF` | Build tests |

## Notes

- None of the allocators are thread-safe; external synchronization is assumed.
- Alignment must be a non-zero power of two; otherwise behavior is undefined.
- Linear allocator supports memory marks.
- Frame allocator does not support nesting.
- Free-list allocator supports reallocation and is slower than the other allocators.

## Dependencies

- **SnCore** — fetched automatically via FetchContent
