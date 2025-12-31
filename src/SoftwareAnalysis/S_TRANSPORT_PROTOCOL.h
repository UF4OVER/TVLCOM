/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : S_TRANSPORT_PROTOCOL.h
 * @brief          : Transport layer abstraction for sending TLV frames over UART/USB.
 * @author         : UF4OVER
 * @date           : 2025-12-31
 ******************************************************************************
 * @attention
 *
 * This module decouples the protocol layer from the physical link.
 * Upper layers build TLV frames (S_TLV_PROTOCOL) and call Transport_Send/Transport_SendTLVs.
 * Applications must register a low-level sender (UART/USB/...) via Transport_RegisterSender().
 *
 * Thread-safety:
 * - Internally uses optional HAL mutex (see src/HAL/hal.h) when available.
 * - If no mutex is provided, functions are not thread-safe.
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

/* Function pointer for low-level TX send (e.g., UART/USB)
 *
 * Expected semantics:
 * - Return >=0 on success (typically bytes written).
 * - Return <0 on error.
 * - The buffer must be sent as-is; the caller already provides a complete frame.
 */
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

/**
 * @brief Register a low-level sender implementation for a given interface.
 *
 * @param interface TLV interface (UART/USB).
 * @param fn        Sender callback. Pass NULL to clear.
 */
void Transport_RegisterSender(tlv_interface_t interface, transport_send_func_t fn);

/**
 * @brief Send a raw byte buffer over the selected interface.
 *
 * @param interface TLV interface.
 * @param data      Buffer pointer.
 * @param len       Buffer length in bytes.
 * @return >=0 on success, <0 on error (or when sender is not registered).
 */
int Transport_Send(tlv_interface_t interface, const uint8_t *data, uint16_t len);

/**
 * @brief Build a frame from TLVs and send it.
 *
 * @param interface TLV interface.
 * @param frame_id  Frame ID (match for ACK/NACK). Use Transport_NextFrameId().
 * @param entries   TLV entries.
 * @param count     Number of entries.
 * @return true if frame was built and sent successfully.
 */
bool Transport_SendTLVs(tlv_interface_t interface, uint8_t frame_id,
                        const tlv_entry_t *entries, uint8_t count);

/**
 * @brief Allocate the next frame id.
 *
 * The counter is monotonic and wraps naturally at 0xFF back to 0x00.
 *
 * @return Next frame id.
 */
uint8_t Transport_NextFrameId(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_S_TRANSPORT_PROTOCOL_H
