/**
 * @file hal_windows.h
 * @brief Helper to obtain the Windows HAL vtable.
 * @author UF4OVER
 * @date 2025-12-31
 */

#pragma once

#include "HAL/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @return HAL vtable for Windows build. */
const tvl_hal_vtable_t *TVL_HAL_Windows(void);

#ifdef __cplusplus
}
#endif

