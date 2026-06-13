# Changelog

## [0.2.0] - 2026-06-12

## Added
- Added sn_\*_allocator_get_allocator function which returns SnMemoryAllocator

## Removed
- SnAllocator - SnCore defines SnMemoryAllocator


## [0.1.0] - 2026-06-11

- First release. See [0.0.0] section in CHANGELOG.md for full changelog.

## [0.0.0] - 2025-12-26

### Added
- Linear (arena) allocator — O(1) allocate, bulk reset
- Stack allocator — LIFO allocation with mark/release
- Pool allocator — fixed-size element pool
- Frame allocator — nested frame regions
- Free-list allocator — general-purpose reuse
- Queue allocator — FIFO allocation pattern
- Ring buffer utility (`SnRingBuffer`)
- Virtual memory abstraction (`sn_vm_alloc`, `sn_vm_free`, `sn_vm_commit`)
- Address hint support for VM allocation
- No malloc/free internally (uses OS-level virtual memory)
- SnCore dependency for platform detection and assertion support
- Comprehensive test suite (100× iterations, all allocators verified)
- CI workflows (Linux, macOS, Windows, formatting)
