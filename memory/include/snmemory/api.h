#pragma once

#include <sncore/api_common.h>

#if defined(SN_MEMORY_STATIC)
    #define SN_MEMORY_API
#elif defined(SN_EXPORT)
    #define SN_MEMORY_API SN_API_HELPER_EXPORT
#else
    #define SN_MEMORY_API SN_API_HELPER_IMPORT
#endif
