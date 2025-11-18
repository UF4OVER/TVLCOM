/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : GLOBAL_CONFIG.h
 * @brief          :
 * @author         : UF4OVER
 * @date           : 2025/10/30
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 UF4.
 * All rights reserved.
 *
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32F407_LM5175_GLOBAL_CONFIG_H
#define STM32F407_LM5175_GLOBAL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* INFO ID  */
typedef enum
{
    INFO_VBUS = 0X01,
    INFO_IBUS = 0X03,
    INFO_PBUS = 0X05,
    INFO_BUS = 0X07,
    INFO_VOUT = 0X11,
    INFO_IOUT = 0X13,
    INFO_POUT = 0X15,
    INFO_OUT = 0X17,
    INFO_VSET = 0X09,
    INFO_ISET = 0X19,
} INFO_ID_t;

typedef enum
{
    SENSOR_FAN = 0X21,
    SENSOR_TPS = 0X22,
    SENSOR_INA = 0X23,
    SENSOR_TEMP = 0X24,
} SENSOR_ID_t;

typedef enum
{
    RAW_DAC1 = 0X31,
    RAW_DAC2 = 0X32,
    RAW_ADC = 0X33,
    RAW_PID1 = 0X34,
    RAW_PID2 = 0X35,
} RAW_ID_t;

typedef enum
{
    DEVICE_NAME = 0XDA,
    DEVICE_UID = 0XDB,
    DEVICE_REV = 0XDC,
    DEVICE_FLASH = 0XDE,
    DEVICE_ESPID = 0XDF,
    DEVICE_ESPFU = 0XFF,
} BOARD_ID_t;

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */
#define TLV_DEBUG_ENABLE 1  /* 1: enable protocol debug prints; 0: disable */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_GLOBAL_CONFIG_H
