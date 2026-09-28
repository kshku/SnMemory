#pragma once

#include "snmemory/api.h"

#include <sncore/types.h>

/**
 * @brief Standard library backed allocator.
 *
 * A ready made SnMemoryAllocator over the C standard library malloc family.
 * Every allocation honours the requested alignment, and a failed allocation
 * is reported by returning NULL.
 *
 * @note
 * - This is an optional convenience for applications that bring no allocator
 *   of their own. No allocator in SnMemory uses it, they all operate on
 *   memory the caller provides.
 * - The allocator holds no state, so it is safe to share between threads.
 * - @warning realloc does not yet honour a changed alignment when the block
 *   moves. Requesting an alignment above 16 can return a pointer that is not
 *   where the previous contents were preserved. Alignments of 16 and below,
 *   which is what alignof() yields, are unaffected.
 */
extern SN_MEMORY_API SnMemoryAllocator sn_std_allocator;
