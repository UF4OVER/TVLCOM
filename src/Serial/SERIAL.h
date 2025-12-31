/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : SERIAL.h
  * @brief          : Cross-platform serial port abstraction (Windows/POSIX).
  * @author         : UF4OVER
  * @date           : 2025-12-31
  ******************************************************************************
  * @attention
  *
  * This module provides a tiny serial API:
  *   - serial_open/serial_close
  *   - serial_read with timeout
  *   - serial_write
  *
  * It is used by the PC demo (src/main.c). On MCU targets you typically won't
  * use this module; instead, register a sender to Transport layer.
  *
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
//
// Created by 33974 on 2025/11/14.
//

#ifndef SERIAL_H
#define SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
/* USER CODE BEGIN Includes */
#include <stddef.h>

/* Provide ssize_t without pulling in windows.h in headers */
#ifndef _SSIZE_T_DEFINED
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct serial_t serial_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
 * @brief Open a serial port.
 *
 * Windows examples:
 * - "COM3"
 * - "\\\\.\\COM10"
 *
 * POSIX examples:
 * - "/dev/ttyS0"
 * - "/dev/ttyUSB0"
 *
 * @param portname Port name string.
 * @param baud     Baudrate, e.g. 115200.
 * @return serial_t* on success, NULL on failure.
 */
serial_t *serial_open(const char *portname, unsigned int baud);

/**
 * @brief Write bytes to the serial port.
 * @param s   Serial handle.
 * @param buf Data buffer.
 * @param len Number of bytes.
 * @return Bytes written, or -1 on error.
 */
ssize_t serial_write(serial_t *s, const void *buf, size_t len);

/**
 * @brief Read bytes from the serial port.
 * @param s          Serial handle.
 * @param buf        Output buffer.
 * @param len        Max bytes to read.
 * @param timeout_ms Timeout in milliseconds. 0 means "block" on some platforms.
 * @return >0 bytes read, 0 on timeout, -1 on error.
 */
ssize_t serial_read(serial_t *s, void *buf, size_t len, unsigned int timeout_ms);

/**
 * @brief Close a serial port and free internal resources.
 * @param s Serial handle.
 */
void serial_close(serial_t *s);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif //SERIAL_H
