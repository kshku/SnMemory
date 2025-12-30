#include "snmemory/vm.h"

#if defined(SN_OS_LINUX) || defined(SN_OS_MAC)

#include <sys/mman.h>
#include <unistd.h>

void *sn_vm_reserve(uint32_t pages) {
    void *ptr = mmap(NULL, pages * sn_vm_get_page_size(),
            PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) return NULL;

    return ptr;
}

bool sn_vm_commit(void *ptr, uint32_t pages) {
    return mprotect(ptr, pages * sn_vm_get_page_size(), PROT_READ | PROT_WRITE) == 0;
}

bool sn_vm_decommit(void *ptr, uint32_t pages) {
    return mprotect(ptr, pages * sn_vm_get_page_size(), PROT_NONE) == 0;
}

bool sn_vm_release(void *ptr, uint32_t pages) {
    return munmap(ptr, pages * sn_vm_get_page_size()) == 0;
}

uint64_t sn_vm_get_page_size(void) {
    static uint64_t page_size = 0;
    if (page_size) return page_size;

    int64_t ps = sysconf(_SC_PAGE_SIZE);
    page_size = SN_MAX(0, ps);

    SN_ASSERT(page_size > 0);
    return page_size;
}

#endif
