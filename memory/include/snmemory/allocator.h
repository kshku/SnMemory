#pragma once

#include <sncore/types.h>

typedef struct SnAllocator {
    SnMemoryAllocateFn     allocate;
    SnMemoryReallocateFn   reallocate;
    SnMemoryFreeFn         free;
    void                   *user_data;
} SnAllocator;
