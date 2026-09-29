# Changelog

## [0.3.3] - 2026-09-28

### Fixed
- sn_vm_get_page_size() narrowed the sysconf result with SN_MAX(0, ps), which
  turned the documented -1 error return into a page size of zero instead of
  reporting it. Every caller then passed a length of zero: mprotect with a length
  of zero succeeds and protects nothing, and munmap rejects it with EINVAL, so a
  released range could stay mapped. SN_ASSERT is compiled out in release, so it
  did not catch this either. The result is now checked before it is narrowed, and
  a failed query falls back to the smallest page size these platforms use

## [0.3.2] - 2026-09-28

### Fixed
- Export sn_freelist_allocator_increase_memory_size from the shared library.
  Every other out of line function here carries SN_MEMORY_API, so this one was
  the only symbol a shared build would not resolve for a caller

### Added
- Test growing a free-list allocator, both with free nodes left and with the
  free list exhausted, plus the no-op cases. The test links against the shared
  library, so a missing export fails the build rather than only a shared
  consumer

### Removed
- Two static forward declarations of sn_write_to_bytes and
  sn_read_from_bytes, which sncore declares inline and defines in its header

## [0.3.1] - 2026-09-28

### Fixed
- Fix sn_std_allocator realloc losing the payload when the block moved. It
  reallocates a raw block, re-aligns and returns the new address, but only
  copied anything when the new address happened to match the old one. Every
  block now starts with a header holding its size, and the bytes are moved from
  the old offset to the new one
- Fix sn_std_allocator realloc writing the new offset over the bytes it was
  about to copy. The offset goes in the byte below the payload, so a payload
  that shifts upwards lands on top of its own contents. The move happens first
  now
- Reject a size and alignment whose raw block would not fit, rather than
  wrapping around and handing back a block smaller than was asked for

## [0.3.0] - 2026-09-28

### Added
- Add sn_std_allocator, the standard library backed allocator, moved here
  from SnCore where it did not belong
- Add std_allocator.h to the snmemory.h umbrella header
- Build the tests against a shared library in CI, which is what catches a
  symbol that is missing an export macro

### Changed
- Build against SnCore v0.3.1, which fixed the variable length integer
  helpers. The freelist stores the distance to its node header in one of them,
  so that fix corrects the freelist too

### Known issues
- sn_std_allocator realloc does not honour a changed alignment when the block
  moves, so an alignment above 16 can return a pointer away from the preserved
  contents. Alignments of 16 and below, which is what alignof() yields, are
  unaffected.

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
