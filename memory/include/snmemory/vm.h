#pragma once

#include "snmemory/defines.h"

#include "snmemory/api.h"

/**
 * @brief Reserve address space.
 *
 * @param pages Number of pages to allocate.
 *
 * @return Returns pointer to reserved address or NULL on failure.
 *
 * @note Address will be aligned to page.
 * @note Memory is not zeroed out.
 */
SN_API void *sn_vm_reserve(uint32_t pages);

/**
 * @brief Make the reserved region usable.
 *
 * @param ptr The address to commit from.
 * @param pages Pages to commit.
 *
 * @note @ref ptr must be page aligned.
 * @note Partial commit/decommit is allowed only at page granularity.
 *      Caller must track committed ranges.
 *
 * @return Returns true on success, false otherwise.
 */
SN_API bool sn_vm_commit(void *ptr, uint32_t pages);

/**
 * @brief Decommits the commited memory.
 *
 * @param ptr The address to decommit from.
 * @param pages Pages to commit.
 *
 * @note @ref ptr must be page aligned.
 * @note Partial commit/decommit is allowed only at page granularity.
 *      Caller must track committed ranges.
 *
 * @return Returns true on success, false otherwise.
 */
SN_API bool sn_vm_decommit(void *ptr, uint32_t pages);

/**
 * @brief Release reserved address space.
 *
 * @param ptr The reserved address space.
 * @param pages Number of pages.
 *
 * @note @ref ptr must be returned from @ref sn_vm_reserve.
 *
 * @return Returns true on succes, false otherwise.
 */
SN_API bool sn_vm_release(void *ptr, uint32_t pages);

/**
 * @brief Get the page size.
 *
 * @return Returns page size.
 */
SN_API uint64_t sn_vm_get_page_size(void);

#undef SN_API
