/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : S_TLV_PROTOCOL.h
 * @brief          : TLV framing/building/parsing + CRC16.
 * @author         : UF4OVER
 * @date           : 2025-12-31
 ******************************************************************************
 * @attention
 *
 * TVLCOM frame format (byte stream):
 *   [Header 2B: 0xF0 0x0F]
 *   [FrameID 1B]
 *   [DataLen 1B]          // length of TLV data segment
 *   [Data: TLV1+TLV2+...] // each TLV: [Type 1B][Len 1B][Value N]
 *   [CRC16 2B]            // CRC16-CCITT over (FrameID + DataLen + Data)
 *   [Tail 2B: 0xE0 0x0D]
 *
 * Endianness rules:
 * - CRC field is stored big-endian (high byte first).
 * - Integer payload helpers (e.g. TLV_CreateInt32Entry/TLV_ExtractInt32Value) are little-endian.
 *   If your peer uses a different byte order, define a project-wide rule and adjust helpers.
 *
 * Lifetime / ownership:
 * - TLV_ParseData() sets tlv_entry_t.value pointers that reference the caller-provided data buffer.
 * - In FloatReceive_FrameCallback(), these pointers reference the parser's internal buffer and are
 *   only valid during the callback. Copy out if you need to keep the data.
 *
 * Thread-safety:
 * - Parser instances (tlv_parser_t) are NOT thread-safe by default. Feed bytes from one context.
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
#include <stddef.h>
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
typedef void (*tlv_frame_callback_t)(uint8_t frame_id, const uint8_t *data, uint8_t length, tlv_interface_t interface);

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
#define TLV_TYPE_CONTROL_CMD 0x01
#define TLV_TYPE_INTEGER     0x02
#define TLV_TYPE_STRING      0x03
#define TLV_TYPE_ACK         0x08
#define TLV_TYPE_NACK        0x09

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
 * @brief Calculate CRC16-CCITT.
 *
 * Polynomial: 0x1021
 * Initial value: 0xFFFF
 *
 * @param data   Pointer to input bytes.
 * @param length Number of bytes.
 * @return CRC16 value.
 */
uint16_t TLV_CalculateCRC16(const uint8_t *data, uint16_t length);

/**
 * @brief Initialize a TLV parser instance.
 *
 * @param parser    Parser object to initialize.
 * @param interface Which interface this parser belongs to (UART/USB).
 * @param callback  Called when a full valid frame is decoded.
 * @note The parser keeps an internal receive buffer of size TLV_MAX_DATA_LENGTH.
 */
void TLV_InitParser(tlv_parser_t *parser,
                    tlv_interface_t interface,
                    tlv_frame_callback_t callback);

/**
 * @brief Set error callback for a parser.
 *
 * @param parser Parser instance.
 * @param err_cb Called on length/CRC errors.
 */
void TLV_SetErrorCallback(tlv_parser_t *parser, tlv_error_callback_t err_cb);

/**
 * @brief Feed one byte into the TLV parser state machine.
 *
 * Usage:
 * - Call this for each received byte (from UART RX ISR, DMA buffer walker, or PC read loop).
 * - On successful frame decode, the parser invokes the frame_callback.
 *
 * Error handling:
 * - On length overflow or CRC mismatch, the parser resets to header hunt state and (if set)
 *   invokes error_callback(frame_id, interface, error).
 *
 * @param parser Parser instance.
 * @param byte   Received byte.
 */
void TLV_ProcessByte(tlv_parser_t *parser, uint8_t byte);

/**
 * @brief Build a TLV frame from a list of TLV entries.
 *
 * @param frame_id       Frame ID chosen by sender (typically Transport_NextFrameId()).
 * @param tlv_entries    Array of TLV entries.
 * @param tlv_count      Number of entries in tlv_entries.
 * @param output_buffer  Output frame buffer.
 * @param output_size    Output frame size (bytes).
 *
 * @return true on success; false if the total TLV data length exceeds TLV_MAX_DATA_LENGTH.
 */
bool TLV_BuildFrame(uint8_t frame_id, const tlv_entry_t *tlv_entries, uint8_t tlv_count,
                    uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Build an ACK frame.
 *
 * Payload: a single byte holding the original frame id.
 *
 * @param frame_id      Original frame ID to acknowledge.
 * @param output_buffer Output buffer.
 * @param output_size   Output size.
 */
void TLV_BuildAckFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Build a NACK frame.
 *
 * Payload: a single byte holding the original frame id.
 *
 * @param frame_id      Original frame ID to NACK.
 * @param output_buffer Output buffer.
 * @param output_size   Output size.
 */
void TLV_BuildNackFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size);

/**
 * @brief Parse a TLV data segment into TLV entries.
 *
 * This parses only the TLV data segment (the bytes between DataLen and CRC), not the whole frame.
 *
 * @param data_buffer TLV data segment.
 * @param data_length Length of data segment.
 * @param tlv_entries Output array for parsed entries.
 * @param max_entries Capacity of tlv_entries.
 * @return Parsed entry count (0..max_entries).
 */
uint8_t TLV_ParseData(const uint8_t *data_buffer, uint8_t data_length,
                      tlv_entry_t *tlv_entries, uint8_t max_entries);

/**
 * @brief Create a control command TLV entry (TLV_TYPE_CONTROL_CMD).
 * @param command Control command byte.
 * @param entry   Output TLV entry.
 */
void TLV_CreateControlCmdEntry(uint8_t command, tlv_entry_t *entry);

/**
 * @brief Extract a scaled float value from a 4-byte TLV payload.
 *
 * The protocol uses an int32 scaled by 10000 (x10000).
 *
 * @param entry TLV entry.
 * @return value/10000.0f, or 0.0f if entry is invalid.
 */
float TLV_ExtractFloatValue(const tlv_entry_t *entry);

/**
 * @brief Extract int32 value from TLV payload (little-endian 4 bytes).
 * @param entry TLV entry.
 * @return int32 value, or 0 if entry is invalid.
 */
int32_t TLV_ExtractInt32Value(const tlv_entry_t *entry);

/* Convenience creators */

/** Create a raw TLV entry from a buffer (no copy at build time beyond the frame build). */
static inline void TLV_CreateRawEntry(uint8_t type, const uint8_t *buf, uint8_t len, tlv_entry_t *entry)
{
    entry->type = type;
    entry->length = len;
    entry->value = buf;
}

static inline void TLV_CreateInt32Entry(uint8_t type, int32_t value, tlv_entry_t *entry)
{
    entry->type = type;
    entry->length = 4;
    entry->inline_storage[0] = (uint8_t)((value >> 24) & 0xFF);
    entry->inline_storage[1] = (uint8_t)((value >> 16) & 0xFF);
    entry->inline_storage[2] = (uint8_t)((value >> 8) & 0xFF);
    entry->inline_storage[3] = (uint8_t)(value & 0xFF);
    entry->value = entry->inline_storage;
}

/** Create a float32 TLV entry (IEEE-754 binary32, little-endian). */
static inline void TLV_CreateFloat32Entry(uint8_t type, float fvalue, tlv_entry_t *entry)
{
    union { float f; uint32_t u; } u = { .f = fvalue };
    TLV_CreateInt32Entry(type, (int32_t)u.u, entry);
}


/** Create a UTF-8 string TLV entry (copies up to 255 bytes). */
static inline void TLV_CreateStringEntry(const char *str, tlv_entry_t *entry)
{
    size_t len = 0;
    while (str && str[len] && len < 255) len++;
    entry->type = TLV_TYPE_STRING;
    entry->length = (uint8_t)len;
    for (size_t i = 0; i < len && i < sizeof(entry->inline_storage); ++i) {
        entry->inline_storage[i] = (uint8_t)str[i];
    }
    entry->value = entry->inline_storage;
}

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
