/**
 ******************************************************************************
 * @file           : S_TLV_PROTOCOL.c
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

/* Includes ------------------------------------------------------------------*/
#include "S_TLV_PROTOCOL.h"
/* USER CODE BEGIN Includes */

#include <memory.h>
#include <stddef.h>
#include "GLOBAL_CONFIG.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Calculate CRC16-CCITT (polynomial 0x1021, initial value 0xFFFF)
 */
uint16_t TLV_CalculateCRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Initialize TLV parser
 */
void TLV_InitParser(tlv_parser_t *parser,
                    tlv_interface_t interface,
                    tlv_frame_callback_t callback)
{
    memset(parser, 0, sizeof(tlv_parser_t));
    parser->state = TLV_STATE_HEADER_0;
    parser->interface = interface;
    parser->frame_callback = callback;
}

void TLV_SetErrorCallback(tlv_parser_t *parser, tlv_error_callback_t err_cb)
{
    if (parser) {
        parser->error_callback = err_cb;
    }
}

/**
 * @brief Process a single byte through the parser state machine
 */
void TLV_ProcessByte(tlv_parser_t *parser, uint8_t byte)
{
    switch (parser->state) {
    case TLV_STATE_HEADER_0:
        if (byte == TLV_FRAME_HEADER_0) {
            parser->state = TLV_STATE_HEADER_1;
            parser->data_index = 0;
        }
        break;

    case TLV_STATE_HEADER_1:
        if (byte == TLV_FRAME_HEADER_1) {
            parser->state = TLV_STATE_FRAME_ID;
        } else {
            parser->state = TLV_STATE_HEADER_0;
        }
        break;

    case TLV_STATE_FRAME_ID:
        parser->frame_id = byte;
        parser->state = TLV_STATE_DATA_LEN;
        break;

    case TLV_STATE_DATA_LEN:
        parser->data_length = byte;
        if (parser->data_length > TLV_MAX_DATA_LENGTH) {
            /* Invalid length, reset parser and report error */
            if (parser->error_callback) {
                parser->error_callback(parser->frame_id, parser->interface, TLV_ERR_LEN);
            }
            parser->state = TLV_STATE_HEADER_0;
            parser->data_index = 0;
        } else if (parser->data_length == 0) {
            /* No data, go to CRC */
            parser->state = TLV_STATE_CRC_LOW;
        } else {
            parser->state = TLV_STATE_DATA;
        }
        break;

    case TLV_STATE_DATA:
        if (parser->data_index < parser->data_length) {
            parser->data_buffer[parser->data_index++] = byte;
            if (parser->data_index >= parser->data_length) {
                parser->state = TLV_STATE_CRC_LOW;
            }
        } else {
            /* Overflow safety */
            if (parser->error_callback) {
                parser->error_callback(parser->frame_id, parser->interface, TLV_ERR_LEN);
            }
            parser->state = TLV_STATE_HEADER_0;
            parser->data_index = 0;
        }
        break;

    case TLV_STATE_CRC_LOW:
        parser->crc_received = byte;
        parser->state = TLV_STATE_CRC_HIGH;
        break;

    case TLV_STATE_CRC_HIGH:
        parser->crc_received |= ((uint16_t)byte << 8);
        parser->state = TLV_STATE_TAIL_0;
        break;

    case TLV_STATE_TAIL_0:
        if (byte == TLV_FRAME_TAIL_0) {
            parser->state = TLV_STATE_TAIL_1;
        } else {
            parser->state = TLV_STATE_HEADER_0;
        }
        break;

    case TLV_STATE_TAIL_1:
        if (byte == TLV_FRAME_TAIL_1) {
            /* Frame complete, verify CRC */
            uint8_t crc_buffer[2 + TLV_MAX_DATA_LENGTH];
            crc_buffer[0] = parser->frame_id;
            crc_buffer[1] = parser->data_length;
            memcpy(&crc_buffer[2], parser->data_buffer, parser->data_length);

            parser->crc_calculated = TLV_CalculateCRC16(crc_buffer, (uint16_t)(2 + parser->data_length));

            if (parser->crc_calculated == parser->crc_received) {
                /* CRC valid, invoke callback */
                if (parser->frame_callback != NULL) {
                    parser->frame_callback(parser->frame_id, parser->data_buffer,
                                           parser->data_length, parser->interface);
                }
            } else {
                /* CRC invalid: report error */
                if (parser->error_callback) {
                    parser->error_callback(parser->frame_id, parser->interface, TLV_ERR_CRC);
                }
            }
        }
        /* Reset parser regardless */
        parser->state = TLV_STATE_HEADER_0;
        parser->data_index = 0;
        break;

    default:
        parser->state = TLV_STATE_HEADER_0;
        parser->data_index = 0;
        break;
    }
}

/**
 * @brief Build a TLV frame with multiple TLV entries
 */
bool TLV_BuildFrame(uint8_t frame_id, const tlv_entry_t *tlv_entries, uint8_t tlv_count,
                    uint8_t *output_buffer, uint16_t *output_size)
{
    uint16_t idx = 0;
    uint16_t data_length = 0;

    /* Calculate total data length */
    for (uint8_t i = 0; i < tlv_count; i++) {
        data_length += (uint16_t)(2 + tlv_entries[i].length); /* Type + Length + Value */
    }

    /* Check if frame will fit */
    if (data_length > TLV_MAX_DATA_LENGTH) {
        return false;
    }

    /* Frame Header */
    output_buffer[idx++] = TLV_FRAME_HEADER_0;
    output_buffer[idx++] = TLV_FRAME_HEADER_1;

    /* Frame ID */
    output_buffer[idx++] = frame_id;

    /* Data Length */
    output_buffer[idx++] = (uint8_t)data_length;

    /* TLV Data Segment */
    for (uint8_t i = 0; i < tlv_count; i++) {
        output_buffer[idx++] = tlv_entries[i].type;
        output_buffer[idx++] = tlv_entries[i].length;
        if (tlv_entries[i].length) {
            const uint8_t *src = tlv_entries[i].value ? tlv_entries[i].value : tlv_entries[i].inline_storage;
            memcpy(&output_buffer[idx], src, tlv_entries[i].length);
            idx += tlv_entries[i].length;
        }
    }

    /* Calculate CRC over Frame ID + Data Length + Data Segment */
    uint16_t crc = TLV_CalculateCRC16(&output_buffer[2], (uint16_t)(2 + data_length));
    output_buffer[idx++] = (uint8_t)(crc & 0xFF);        /* CRC low byte */
    output_buffer[idx++] = (uint8_t)((crc >> 8) & 0xFF); /* CRC high byte */

    /* Frame Tail */
    output_buffer[idx++] = TLV_FRAME_TAIL_0;
    output_buffer[idx++] = TLV_FRAME_TAIL_1;

    *output_size = idx;
    return true;
}

/**
 * @brief Build an ACK frame
 */
void TLV_BuildAckFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size)
{
    tlv_entry_t ack_entry;
    ack_entry.type = TLV_TYPE_ACK;
    ack_entry.length = 1;
    ack_entry.inline_storage[0] = 0x06; /* ACK byte */
    ack_entry.value = ack_entry.inline_storage;

    TLV_BuildFrame(frame_id, &ack_entry, 1, output_buffer, output_size);
}

/**
 * @brief Build a NACK frame
 */
void TLV_BuildNackFrame(uint8_t frame_id, uint8_t *output_buffer, uint16_t *output_size)
{
    tlv_entry_t nack_entry;
    nack_entry.type = TLV_TYPE_NACK;
    nack_entry.length = 1;
    nack_entry.inline_storage[0] = 0x15; /* NACK byte */
    nack_entry.value = nack_entry.inline_storage;

    TLV_BuildFrame(frame_id, &nack_entry, 1, output_buffer, output_size);
}

/**
 * @brief Parse TLV data segment into individual TLV entries
 */
uint8_t TLV_ParseData(const uint8_t *data_buffer, uint8_t data_length,
                      tlv_entry_t *tlv_entries, uint8_t max_entries)
{
    uint8_t count = 0;
    uint16_t idx = 0;

    while (idx < data_length && count < max_entries) {
        /* Check if we have at least Type and Length */
        if (idx + 2 > data_length) {
            break;
        }

        uint8_t type = data_buffer[idx++];
        uint8_t len  = data_buffer[idx++];

        /* Check if we have enough data for the value */
        if ((uint16_t)idx + len > data_length) {
            break;
        }

        /* Fill entry referencing into the data buffer (no copy) */
        tlv_entries[count].type = type;
        tlv_entries[count].length = len;
        tlv_entries[count].value = &data_buffer[idx];

        idx += len;
        count++;
    }

    return count;
}

/**
 * @brief Create a control command TLV entry
 */
void TLV_CreateControlCmdEntry(uint8_t command, tlv_entry_t *entry)
{
    entry->type = TLV_TYPE_CONTROL_CMD;
    entry->length = 1;
    entry->inline_storage[0] = command;
    entry->value = entry->inline_storage;
}

/**
 * @brief Extract float value from int32 TLV (scaled ×10000)
 */
float TLV_ExtractFloatValue(const tlv_entry_t *entry)
{
    if (entry->length != 4 || !entry->value) {
        return 0.0f;
    }

    int32_t scaled_value = (int32_t)(entry->value[0] |
                                     ((uint32_t)entry->value[1] << 8) |
                                     ((uint32_t)entry->value[2] << 16) |
                                     ((uint32_t)entry->value[3] << 24));

    return (float)scaled_value / 10000.0f;
}

/**
 * @brief Extract int32 value from TLV
 */
int32_t TLV_ExtractInt32Value(const tlv_entry_t *entry)
{
    if (entry->length != 4 || !entry->value) {
        return 0;
    }

    return (int32_t)(entry->value[0] |
                     ((uint32_t)entry->value[1] << 8) |
                     ((uint32_t)entry->value[2] << 16) |
                     ((uint32_t)entry->value[3] << 24));
}

/* Scaled creators (×10000) */
static inline int32_t TLV_ScaleFloat(float v) { return (int32_t)(v * 10000.0f); }

void TLV_CreateVoltageEntry(float voltage, tlv_entry_t *entry)
{
    TLV_CreateInt32Entry(INFO_VBUS, TLV_ScaleFloat(voltage), entry);
}

void TLV_CreateCurrentEntry(float current, tlv_entry_t *entry)
{
    TLV_CreateInt32Entry(INFO_IBUS, TLV_ScaleFloat(current), entry);
}

void TLV_CreatePowerEntry(float power, tlv_entry_t *entry)
{
    TLV_CreateInt32Entry(INFO_PBUS, TLV_ScaleFloat(power), entry);
}

void TLV_CreateTemperatureEntry(float temperature, tlv_entry_t *entry)
{
    TLV_CreateInt32Entry(SENSOR_TEMP, TLV_ScaleFloat(temperature), entry);
}

/* USER CODE END 0 */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
