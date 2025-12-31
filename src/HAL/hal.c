/**
 * @file hal.c
 * @brief HAL vtable storage and safe defaults.
 * @author UF4OVER
 * @date 2025-12-31
 */

#include "HAL/hal.h"

static uint32_t default_tick_ms(void) { return 0u; }
static void default_sleep_ms(uint32_t ms) { (void)ms; }

static const tvl_hal_vtable_t g_default_hal = {
    .tick_ms = default_tick_ms,
    .sleep_ms = default_sleep_ms,
    .mutex_create = NULL,
    .mutex_destroy = NULL,
    .mutex_lock = NULL,
    .mutex_unlock = NULL,
    .log = NULL,
};

static const tvl_hal_vtable_t *g_hal = &g_default_hal;

void TVL_HAL_Set(const tvl_hal_vtable_t *vtable)
{
    g_hal = vtable ? vtable : &g_default_hal;
}

const tvl_hal_vtable_t *TVL_HAL_Get(void)
{
    return g_hal ? g_hal : &g_default_hal;
}

