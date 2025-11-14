/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : S_TRANSPORT_PROTOCOL.h
 * @brief          :
 * @author         : UF4OVER
 * @date           : 2025/10/30
 ******************************************************************************
 * @attention
 *
 * Frame Structure:
 * [Frame Header 2B: 0xFF 0xFF]
 * [Frame ID 1B]
 * [Data Length 1B]
 * [Data Segment N bytes: TLV1 + TLV2 + ...]
 * [CRC16 2B]
 * [Frame Tail 2B: 0xED 0xED]
 *
 * Copyright (c) 2025 UF4.
 * All rights reserved.
 *
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
//
// Created by 33974 on 2025/10/30.
//

#ifndef STM32F407_LM5175_S_TRANSPORT_PROTOCOL_H
#define STM32F407_LM5175_S_TRANSPORT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
/* USER CODE BEGIN Includes */

#include "S_TLV_PROTOCOL.h"

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* Function pointer for low-level TX send (e.g., UART/USB) */
typedef int (*transport_send_func_t)(const uint8_t *data, uint16_t len);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/* Register actual send function for an interface */
void Transport_RegisterSender(tlv_interface_t interface, transport_send_func_t fn);

/* Send raw buffer via selected interface */
int Transport_Send(tlv_interface_t interface, const uint8_t *data, uint16_t len);

/* Build and send a TLV frame (helper) */
bool Transport_SendTLVs(tlv_interface_t interface, uint8_t frame_id,
                        const tlv_entry_t *entries, uint8_t count);

/* Allocate next frame id (monotonic, wraps at 0xFF) */
uint8_t Transport_NextFrameId(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_S_TRANSPORT_PROTOCOL_H
