/**
 * @file hal_windows.c
 * @brief Windows HAL implementation for TVLCOM.
 * @author UF4OVER
 * @date 2025-12-31
 */

#include "HAL/hal.h"

#if TVLCOM_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdlib.h>

static uint32_t win_tick_ms(void)
{
    return (uint32_t)GetTickCount();
}

static void win_sleep_ms(uint32_t ms)
{
    Sleep((DWORD)ms);
}

typedef struct {
    CRITICAL_SECTION cs;
} hal_win_mutex_t;

static tvl_hal_mutex_t win_mutex_create(void)
{
    hal_win_mutex_t *m = (hal_win_mutex_t *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    InitializeCriticalSection(&m->cs);
    return (tvl_hal_mutex_t)m;
}

static void win_mutex_destroy(tvl_hal_mutex_t m)
{
    if (!m) return;
    hal_win_mutex_t *mm = (hal_win_mutex_t *)m;
    DeleteCriticalSection(&mm->cs);
    free(mm);
}

static void win_mutex_lock(tvl_hal_mutex_t m)
{
    if (!m) return;
    EnterCriticalSection(&((hal_win_mutex_t *)m)->cs);
}

static void win_mutex_unlock(tvl_hal_mutex_t m)
{
    if (!m) return;
    LeaveCriticalSection(&((hal_win_mutex_t *)m)->cs);
}

static const tvl_hal_vtable_t g_windows_hal = {
    .tick_ms = win_tick_ms,
    .sleep_ms = win_sleep_ms,
    .mutex_create = win_mutex_create,
    .mutex_destroy = win_mutex_destroy,
    .mutex_lock = win_mutex_lock,
    .mutex_unlock = win_mutex_unlock,
    .log = NULL,
};

const tvl_hal_vtable_t *TVL_HAL_Windows(void)
{
    return &g_windows_hal;
}

#endif /* TVLCOM_PLATFORM_WINDOWS */

