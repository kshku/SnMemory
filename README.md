# SnMemory

Memory allocator library written in C.

SnMemory does **not** depend on malloc/free internally — all allocators
operate on user-provided memory buffers.

## Allocators
1. Linear / Arena allocator
2. Stack allocator
3. Pool allocator
4. Frame allocator
5. Free-list allocator

## Notes
- None of the allocators are thread-safe; external synchronization is assumed.
- Alignment must be a non-zero power of two; otherwise behavior is undefined.
- Linear allocator supports memory marks.
- Frame allocator does not support nesting.
- Free-list allocator supports reallocation and is slower than the other allocators.

