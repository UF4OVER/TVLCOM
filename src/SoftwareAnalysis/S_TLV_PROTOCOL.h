/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : S_TLV_PROTOCOL.h
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

#ifndef STM32F407_LM5175_S_TLV_PROTOCOL_H
#define STM32F407_LM5175_S_TLV_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include "stdint.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* Communication interface selector */
typedef enum {
    TLV_INTERFACE_UART = 0,
    TLV_INTERFACE_USB  = 1,
} tlv_interface_t;

/* Error codes for parser */
typedef enum {
    TLV_ERR_NONE = 0,
    TLV_ERR_LEN  = 1,
    TLV_ERR_CRC  = 2,
} tlv_error_t;

/* Callback type for a valid frame */
typedef void (*tlv_frame_callback_t)(uint8_t frame_id, uint8_t *data, uint8_t length, tlv_interface_t interface);

/* Callback type for parser error */
typedef void (*tlv_error_callback_t)(uint8_t frame_id, tlv_interface_t interface, tlv_error_t error);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* Frame header and tail constants */
#define TLV_FRAME_HEADER_0      0xF0
#define TLV_FRAME_HEADER_1      0x0F
#define TLV_FRAME_TAIL_0        0xE0
#define TLV_FRAME_TAIL_1        0x0D

/* Frame structure sizes */
#define TLV_HEADER_SIZE         2   /* 0xF0 0x0F */
#define TLV_FRAME_ID_SIZE       1   /* Frame ID */
#define TLV_DATA_LEN_SIZE       1   /* Data length */
#define TLV_CRC_SIZE            2   /* CRC16 */
#define TLV_TAIL_SIZE           2   /* 0xE0 0x0D */
#define TLV_OVERHEAD_SIZE       (TLV_HEADER_SIZE + TLV_FRAME_ID_SIZE + TLV_DATA_LEN_SIZE + TLV_CRC_SIZE + TLV_TAIL_SIZE)

/* Maximum frame sizes */
#define TLV_MAX_DATA_LENGTH     240 /* Maximum TLV data segment length */
#define TLV_MAX_FRAME_SIZE      (TLV_OVERHEAD_SIZE + TLV_MAX_DATA_LENGTH)

/* TLV Type definitions (generic utility types, user can define custom IDs) */
#define TLV_TYPE_CONTROL_CMD    0x10  /* 1 byte command */
#define TLV_TYPE_INTEGER        0x20  /* int32 */
#define TLV_TYPE_STRING         0x30  /* UTF-8 text */
#define TLV_TYPE_ACK            0x06  /* ACK response */
#define TLV_TYPE_NACK           0x15  /* NACK response */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* Parser states */
typedef enum {
    TLV_STATE_HEADER_0 = 0,
    TLV_STATE_HEADER_1,
    TLV_STATE_FRAME_ID,
    TLV_STATE_DATA_LEN,
    TLV_STATE_DATA,
    TLV_STATE_CRC_LOW,
    TLV_STATE_CRC_HIGH,
    TLV_STATE_TAIL_0,
    TLV_STATE_TAIL_1
} tlv_parser_state_t;

/* TLV entry structure: supports inline storage for small values and pointer for large */
typedef struct {
    uint8_t type;           /* TLV type (user-defined ID) */
    uint8_t length;         /* TLV value length */
    const uint8_t *value;   /* Pointer to value bytes */
    uint8_t inline_storage[32]; /* Optional inline storage for small values (created by helpers) */
} tlv_entry_t;

/* Frame parser context */
typedef struct {
    tlv_parser_state_t state;
    uint8_t frame_id;
    uint8_t data_length;
    uint8_t data_buffer[TLV_MAX_DATA_LENGTH];
    uint16_t data_index;
    uint16_t crc_received;
    uint16_t crc_calculated;
    tlv_interface_t interface;              /* Which interface this parser is bound to */
    tlv_frame_callback_t frame_callback;    /* Called on valid frame */
    tlv_error_callback_t error_callback;    /* Called on parser errors */
} tlv_parser_t;


/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
/* USER CODE BEGIN EFP */

/**
 * @brief Calculate CRC16-CCITT
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return CRC16 value
 */
uint16_t TLV_CalculateCRC16(const uint8_t *data, uint16_t length);

/**
 * @brief Initialize TLV parser
 * @param parser Pointer to parser structure
 * @param interface Which interface this parser reads from
 * @param callback Callback function for received frames
 */
void TLV_InitParser(tlv_parser_t *parser,
                    tlv_interface_t interface,
                    tlv_frame_callback_t callback);

/**
 * @brief Set error callback for parser
 */
void TLV_SetErrorCallback(tlv_parser_t *parser, tlv_error_callback_t err_cb);

/**
 * @brief Process a single byte through the parser
 * @param parser Pointer to parser structure
 * @param byte Received byte
 */
void TLV_ProcessByte(tlv_parser_t *parser, uint8_t byte);

/**
 * @brief Build a TLV frame with multiple TLV entries
 * @param frame_id Frame ID for matching ACK
 * @param tlv_entries Array of TLV entries
 * @param tlv_count Number of TLV entries
 * @param output_buffer Output buffer for frame
 * @param output_size Pointer to store output frame size
 * @return true if successful, false if buffer overflow
 */
bool TLV_BuildFrame(uint8_t frame_id, const tlv_entry_t *tlv_entries, uint8_t tlv_count,
                    uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Build an ACK frame
 * @param frame_id Frame ID to acknowledge
 * @param output_buffer Output buffer for ACK frame
 * @param output_size Pointer to store output frame size
 */
void TLV_BuildAckFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Build a NACK frame
 * @param frame_id Frame ID to negative acknowledge
 * @param output_buffer Output buffer for NACK frame
 * @param output_size Pointer to store output frame size
 */
void TLV_BuildNackFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Parse TLV data segment into individual TLV entries
 * @param data_buffer TLV data segment
 * @param data_length Length of data segment
 * @param tlv_entries Output array for parsed TLV entries
 * @param max_entries Maximum number of entries to parse
 * @return Number of TLV entries parsed
 */
uint8_t TLV_ParseData(const uint8_t *data_buffer, uint8_t data_length,
                      tlv_entry_t *tlv_entries, uint8_t max_entries);

/* Convenience creators */

/** Create a raw TLV entry from a buffer (no copy at build time beyond the frame build). */
static inline void TLV_CreateRawEntry(uint8_t type, const uint8_t *buf, uint8_t len, tlv_entry_t *entry)
{
    entry->type = type;
    entry->length = len;
    entry->value = buf;
}

/** Create an int32 TLV entry (little-endian). */
static inline void TLV_CreateInt32Entry(uint8_t type, int32_t value, tlv_entry_t *entry)
{
    entry->type = type;
    entry->length = 4;
    entry->inline_storage[0] = (uint8_t)(value & 0xFF);
    entry->inline_storage[1] = (uint8_t)((value >> 8) & 0xFF);
    entry->inline_storage[2] = (uint8_t)((value >> 16) & 0xFF);
    entry->inline_storage[3] = (uint8_t)((value >> 24) & 0xFF);
    entry->value = entry->inline_storage;
}

/** Create a float32 TLV entry (IEEE-754 binary32, little-endian). */
static inline void TLV_CreateFloat32Entry(uint8_t type, float fvalue, tlv_entry_t *entry)
{
    union { float f; uint32_t u; } u = { .f = fvalue };
    TLV_CreateInt32Entry(type, (int32_t)u.u, entry);
}

/** Create a control command TLV entry (1 byte). */
void TLV_CreateControlCmdEntry(uint8_t command, tlv_entry_t *entry);

/** Extract float from a TLV entry that encodes int32 scaled by ×10000. */
float TLV_ExtractFloatValue(const tlv_entry_t *entry);

/** Extract int32 value from TLV (little-endian, 4 bytes). */
int32_t TLV_ExtractInt32Value(const tlv_entry_t *entry);

/** Scaled-value helpers (×10000) */
void TLV_CreateVoltageEntry(float voltage, tlv_entry_t *entry);
void TLV_CreateCurrentEntry(float current, tlv_entry_t *entry);
void TLV_CreatePowerEntry(float power, tlv_entry_t *entry);
void TLV_CreateTemperatureEntry(float temperature, tlv_entry_t *entry);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif // STM32F407_LM5175_S_TLV_PROTOCOL_H
