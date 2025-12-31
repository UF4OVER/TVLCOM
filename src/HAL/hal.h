/**
 * @file hal.h
 * @brief Hardware Abstraction Layer (HAL) interface for TVLCOM.
 * @author UF4OVER
 * @date 2025-12-31
 *
 * Contract:
 * - Provides a minimal, portable interface used by protocol layers.
 * - Avoids dynamic allocation and heavy platform headers in core modules.
 * - Concrete implementations live under src/HAL/<platform>/.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "HAL/hal_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** HAL status/error code. Keep negative for error, zero/non-negative for success. */
typedef int tvl_hal_status_t;

/** Optional logging callback used by HAL or protocol glue. */
typedef void (*tvl_hal_log_fn_t)(const char *fmt, ...);

/**
 * @brief Mutex/critical-section handle as an opaque pointer.
 *
 * On bare-metal you may provide a dummy lock that disables interrupts.
 * On Windows you may map to a CRITICAL_SECTION.
 */
typedef void* tvl_hal_mutex_t;

/** HAL vtable (all fields optional; NULL means "not supported"). */
typedef struct tvl_hal_vtable {
    /** @brief Millisecond tick (monotonic). */
    uint32_t (*tick_ms)(void);

    /** @brief Sleep/delay (best-effort). */
    void (*sleep_ms)(uint32_t ms);

    /** @brief Create/destroy a mutex/critical section. */
    tvl_hal_mutex_t (*mutex_create)(void);
    void (*mutex_destroy)(tvl_hal_mutex_t m);

    /** @brief Lock/unlock. Must be safe to call from multiple threads where applicable. */
    void (*mutex_lock)(tvl_hal_mutex_t m);
    void (*mutex_unlock)(tvl_hal_mutex_t m);

    /** @brief Optional logger (printf-like). */
    tvl_hal_log_fn_t log;
} tvl_hal_vtable_t;

/**
 * @brief Install the HAL implementation.
 * @note Call once during startup, before protocol init.
 */
void TVL_HAL_Set(const tvl_hal_vtable_t *vtable);

/** @brief Get the installed HAL implementation. Never returns NULL. */
const tvl_hal_vtable_t *TVL_HAL_Get(void);

#ifdef __cplusplus
}
#endif

