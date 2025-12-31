/**
 * @file hal_stm32.c
 * @brief STM32 HAL implementation skeleton for TVLCOM (portable stub).
 * @author UF4OVER
 * @date 2025-12-31
 *
 * This file is intentionally lightweight so it can compile without pulling in
 * vendor-specific headers on non-STM32 builds. When integrating on STM32,
 * enable TVLCOM_PLATFORM_STM32 and fill in the functions below using your
 * project stack (STM32Cube HAL / LL / FreeRTOS, etc.).
 */

#include "HAL/hal.h"

#if TVLCOM_PLATFORM_STM32

/*
 * Example mapping (Cube HAL):
 *  - tick_ms -> HAL_GetTick
 *  - sleep_ms -> HAL_Delay
 *  - mutex_* -> __disable_irq/__enable_irq or an RTOS mutex
 */

static uint32_t stm32_tick_ms(void)
{
    /* TODO: return HAL_GetTick(); */
    return 0u;
}

static void stm32_sleep_ms(uint32_t ms)
{
    (void)ms;
    /* TODO: HAL_Delay(ms); */
}

static const tvl_hal_vtable_t g_stm32_hal = {
    .tick_ms = stm32_tick_ms,
    .sleep_ms = stm32_sleep_ms,
    .mutex_create = NULL,
    .mutex_destroy = NULL,
    .mutex_lock = NULL,
    .mutex_unlock = NULL,
    .log = NULL,
};

const tvl_hal_vtable_t *TVL_HAL_Stm32(void)
{
    return &g_stm32_hal;
}

#endif /* TVLCOM_PLATFORM_STM32 */

