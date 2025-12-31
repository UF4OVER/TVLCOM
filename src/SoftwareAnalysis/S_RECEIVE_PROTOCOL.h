/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : S_RECEIVE_PROTOCOL.h
 * @brief          : Receive-side TLV frame dispatch + ACK/NACK policy.
 * @author         : UF4OVER
 * @date           : 2025-12-31
 ******************************************************************************
 * @attention
 *
 * Responsibilities:
 * - Owns TLV parsers (UART/USB) and wires parser->frame_callback.
 * - Parses TLV data segment into entries.
 * - Dispatches TLVs to registered type handlers / control cmd handlers.
 * - Applies ACK/NACK policy:
 *   - If all non-ACK/NACK TLVs are handled successfully => send ACK for received frame_id.
 *   - Otherwise => send NACK.
 *   - If the received frame contains only ACK/NACK TLVs, it will NOT respond (prevents storms).
 *
 * Lifetime rules:
 * - tlv_entry_t.value points into an internal parser buffer; copy out if you need persistence.
 *
 * Thread-safety:
 * - This module uses optional HAL mutex to protect handler tables when available.
 *
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/
//
// Created by 33974 on 2025/10/30.
//

#ifndef STM32F407_LM5175_S_RECEIVE_PROTOCOL_H
#define STM32F407_LM5175_S_RECEIVE_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "S_TLV_PROTOCOL.h"
#include "stdint.h"
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

typedef bool (*tlv_type_handler_t)(const tlv_entry_t *entry, tlv_interface_t interface);
typedef bool (*cmd_handler_t)(uint8_t command, tlv_interface_t interface);
/* ACK/NACK notification (value carries original frame id) */
typedef void (*ack_notify_t)(uint8_t original_frame_id, tlv_interface_t interface);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

#define SYNC0 0xF0
#define SYNC1 0x0F
#define CMD_TYPE_CONTROL 0x00  // 0x00: Control command
#define CMD_TYPE_ACK     0xFF  // 0xFF: Acknowledge (legacy, use TLV_TYPE_ACK)

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

typedef tlv_interface_t comm_interface_t;
#define COMM_INTERFACE_UART TLV_INTERFACE_UART
#define COMM_INTERFACE_USB  TLV_INTERFACE_USB

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
 * @brief Initialize receiver/parsers for a given interface.
 *
 * @param interface Communication interface (UART or USB).
 */
void FloatReceive_Init(tlv_interface_t interface);

/**
 * @brief Get the parser instance bound to UART interface.
 * @return Parser pointer (singleton storage).
 */
tlv_parser_t* FloatReceive_GetUARTParser(void);

/**
 * @brief Get the parser instance bound to USB interface.
 * @return Parser pointer (singleton storage).
 */
tlv_parser_t* FloatReceive_GetUSBParser(void);

/**
 * @brief Send ACK for a received frame.
 * @param frame_id  Received frame id to acknowledge.
 * @param interface Interface to send via.
 */
void FloatReceive_SendAck(uint8_t frame_id, tlv_interface_t interface);

/**
 * @brief Send NACK for a received frame.
 * @param frame_id  Received frame id to negative-ack.
 * @param interface Interface to send via.
 */
void FloatReceive_SendNack(uint8_t frame_id, tlv_interface_t interface);

/**
 * @brief TLV frame callback (wired into TLV parser).
 *
 * @param frame_id  Received frame id.
 * @param data      Pointer to TLV data segment.
 * @param length    Length of TLV data segment.
 * @param interface Interface this frame came from.
 */
void FloatReceive_FrameCallback(uint8_t frame_id, const uint8_t *data, uint8_t length, tlv_interface_t interface);

/**
 * @brief Parser error callback.
 *
 * Default behavior: immediately sends a NACK for the frame.
 *
 * @param frame_id  Current parser frame id (best-effort; may be partial).
 * @param interface Parser interface.
 * @param error     Error type.
 */
void FloatReceive_ErrorCallback(uint8_t frame_id, tlv_interface_t interface, tlv_error_t error);

/**
 * @brief Register a TLV type handler.
 *
 * Handler contract:
 * - Return true if TLV is handled successfully.
 * - Return false if TLV is unknown or failed (will trigger NACK for the frame).
 */
void FloatReceive_RegisterTLVHandler(uint8_t type, tlv_type_handler_t handler);

/**
 * @brief Register a control command handler.
 *
 * Control commands are carried inside TLV_TYPE_CONTROL_CMD where value[0] is command id.
 */
void FloatReceive_RegisterCmdHandler(uint8_t command, cmd_handler_t handler);

/**
 * @brief Register ACK notification handler.
 *
 * When a received frame contains only ACK TLV(s), this handler is notified with original frame id.
 */
void FloatReceive_RegisterAckHandler(ack_notify_t handler);

/**
 * @brief Register NACK notification handler.
 *
 * When a received frame contains only NACK TLV(s), this handler is notified with original frame id.
 */
void FloatReceive_RegisterNackHandler(ack_notify_t handler);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_S_RECEIVE_PROTOCOL_H
