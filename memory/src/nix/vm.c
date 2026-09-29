#include "snmemory/vm.h"

#if defined(SN_OS_LINUX) || defined(SN_OS_MAC)

    #include <sys/mman.h>
    #include <unistd.h>

void *sn_vm_reserve(void *address, uint32_t pages) {
    void *ptr = mmap(address, pages * sn_vm_get_page_size(), PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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

    /* sysconf returns -1 on error, so the result has to be checked before it is
     * narrowed. Clamping with SN_MAX(0, ps) turned the error into a page size of
     * zero, which then made every caller pass a length of zero: mprotect with a
     * length of zero succeeds and protects nothing, and munmap rejects it with
     * EINVAL, so a released range could stay mapped. SN_ASSERT is compiled out in
     * release, so it would not have caught that either.
     *
     * A page size of zero cannot be served, so fall back to the smallest value
     * every platform this file builds for uses. That keeps the failure local to
     * here and leaves the callers reporting an honest result. */
    long ps = sysconf(_SC_PAGE_SIZE);
    page_size = (ps > 0) ? (uint64_t)ps : (uint64_t)4096;

    return page_size;
}

#endif
