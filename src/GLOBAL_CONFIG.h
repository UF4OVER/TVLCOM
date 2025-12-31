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
#define TLV_DEBUG_ENABLE 0  /* 1: enable protocol debug prints; 0: disable */
    /* Info IDs */
#define INFO_VBUS   0xA1
#define INFO_IBUS   0xA3
#define INFO_PBUS   0xA5
#define INFO_BUS    0xA7
#define INFO_VOUT   0xB1
#define INFO_IOUT   0xB3
#define INFO_POUT   0xB5
#define INFO_OUT    0xB7

#define INFO_VSET   0xB9
#define INFO_ISET   0xA9

    /* Sensor IDs */
#define SENSOR_FAN  0x21
#define SENSOR_TPS  0x22
#define SENSOR_INA  0x23
#define SENSOR_TEMP 0x24

    /* Raw IDs */
#define RAW_DAC1    0x31
#define RAW_DAC2    0x32
#define RAW_ADC     0x33
#define RAW_PID1    0x34
#define RAW_PID2    0x35

    /* Board / Device IDs */
#define DEVICE_NAME   0xDA
#define DEVICE_UID    0xDB
#define DEVICE_REV    0xDC
#define DEVICE_FLASH  0xDE
#define DEVICE_ESPID  0xDF
#define DEVICE_ESPFU  0xFF



/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */
#define TLV_DEBUG_ENABLE 0  /* 1: enable protocol debug prints; 0: disable */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_GLOBAL_CONFIG_H
