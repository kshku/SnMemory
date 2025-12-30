#include "snmemory/vm.h"

#if defined(SN_OS_WINDOWS)

#include <windows.h>

void *sn_vm_reserve(uint32_t pages) {
    return VirtualAlloc(NULL, pages * sn_vm_get_page_size(), MEM_RESERVE, PAGE_NOACCESS);
}

bool sn_vm_commit(void *ptr, uint32_t pages) {
    return VirtualAlloc(ptr, pages * sn_vm_get_page_size(), MEM_COMMIT, PAGE_READWRITE) != NULL;
}

bool sn_vm_decommit(void *ptr, uint32_t pages) {
    return VirtualFree(ptr, pages * sn_vm_get_page_size(), MEM_DECOMMIT);
}

bool sn_vm_release(void *ptr, uint32_t pages) {
    return VirtualFree(ptr, 0, MEM_RELEASE);
}

uint64_t sn_vm_get_page_size(void) {
    static uint64_t page_size = 0;
    if (page_size) return page_size;

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    page_size = (uint64_t)system_info.dwPageSize;

    SN_ASSERT(page_size > 0);
    return page_size;
}

#endif
