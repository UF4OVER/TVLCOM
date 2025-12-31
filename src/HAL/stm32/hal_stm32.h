/**
 * @file hal_stm32.h
 * @brief Helper to obtain the STM32 HAL vtable.
 * @author UF4OVER
 * @date 2025-12-31
 */

#pragma once

#include "HAL/hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @return HAL vtable for STM32 build. */
const tvl_hal_vtable_t *TVL_HAL_Stm32(void);

#ifdef __cplusplus
}
#endif

