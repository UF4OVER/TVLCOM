/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : SERIAL.h
  * @brief          : 
  * @author         : UF4OVER
  * @date           : 2025/11/14
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 UF4.
  * All rights reserved.
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

  /* 打开串口：portname 例如 Windows 上 `COM3` 或 `\\\\.\\COM10`，POSIX 上 `/dev/ttyS0` 或 `/dev/ttyUSB0` */
  /* baud 如 115200。成功返回 serial_t*，失败返回 NULL。 */
  serial_t *serial_open(const char *portname, unsigned int baud);

  /* 写入，返回写入字节数，出错返回 -1 */
  ssize_t serial_write(serial_t *s, const void *buf, size_t len);

  /* 读取，timeout_ms 为超时时间（毫秒）。返回读到的字节数；超时返回 0；出错返回 -1 */
  ssize_t serial_read(serial_t *s, void *buf, size_t len, unsigned int timeout_ms);

  /* 关闭串口并释放结构 */
  void serial_close(serial_t *s);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif //SERIAL_H
