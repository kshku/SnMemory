#pragma once

#include <sncore/defines.h>

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(SN_DEBUG)
    #define SN_ASSERT(x) assert(x)
#else
    #define SN_ASSERT(x) SN_UNUSED(x)
#endif

#define SN_SHOULD_NOT_REACH_HERE (SN_ASSERT(false))
