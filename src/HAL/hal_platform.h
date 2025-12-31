/**
 * @file hal_platform.h
 * @brief Platform selection macros for TVLCOM HAL.
 * @author UF4OVER
 * @date 2025-12-31
 *
 * This header centralizes platform detection and compile-time switches.
 * Upper layers should include this file (via hal.h) instead of using _WIN32/STM32
 * scattered across the codebase.
 */

#pragma once

/*
 * Platform selection:
 * - Define TVLCOM_PLATFORM_STM32 to build for STM32 (bare-metal/RTOS).
 * - Define TVLCOM_PLATFORM_WINDOWS to build for Windows.
 *
 * If neither is provided, we auto-detect Windows via _WIN32.
 */
#if !defined(TVLCOM_PLATFORM_STM32) && !defined(TVLCOM_PLATFORM_WINDOWS)
#  if defined(_WIN32)
#    define TVLCOM_PLATFORM_WINDOWS 1
#  else
#    define TVLCOM_PLATFORM_WINDOWS 0
#  endif
#  define TVLCOM_PLATFORM_STM32 0
#endif

#if (TVLCOM_PLATFORM_STM32 + TVLCOM_PLATFORM_WINDOWS) != 1
#  error "Select exactly one platform: TVLCOM_PLATFORM_STM32 or TVLCOM_PLATFORM_WINDOWS"
#endif

