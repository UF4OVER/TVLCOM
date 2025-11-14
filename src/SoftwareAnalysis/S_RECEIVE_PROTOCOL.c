/**
 ******************************************************************************
 * @file           : S_RECEIVE_PROTOCOL.c
 * @brief          :
 * @author         : UF4OVER
 * @date           : 2025/10/30
 ******************************************************************************
 * @attention
 *
 *
 * Copyright (c) 2025 UF4.
 * All rights reserved.
 *
 ******************************************************************************
 */


/* Includes ------------------------------------------------------------------*/
#include "S_RECEIVE_PROTOCOL.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "S_TRANSPORT_PROTOCOL.h"
/* USER CODE END Includes */

/* Forward declarations for local use */
/* Transport_Send is declared in S_TRANSPORT_PROTOCOL.h */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MAX_TLV_TYPE_HANDLERS 32
#define MAX_CMD_HANDLERS      32

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

static tlv_parser_t uart_parser;
static tlv_parser_t usb_parser;

static tlv_type_handler_t tlv_type_handlers[MAX_TLV_TYPE_HANDLERS];
static uint8_t tlv_type_ids[MAX_TLV_TYPE_HANDLERS];
static uint8_t tlv_type_handler_count = 0;

static cmd_handler_t cmd_handlers[MAX_CMD_HANDLERS];
static uint8_t cmd_ids[MAX_CMD_HANDLERS];
static uint8_t cmd_handler_count = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static bool dispatch_tlv_entries(uint8_t frame_id, tlv_entry_t *entries, uint8_t count, tlv_interface_t interface);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Initialize TLV receiver
 * @param interface Communication interface type (UART or USB)
 */
void FloatReceive_Init(tlv_interface_t interface)
{
    if (interface == TLV_INTERFACE_UART) {
        TLV_InitParser(&uart_parser, TLV_INTERFACE_UART, FloatReceive_FrameCallback);
        TLV_SetErrorCallback(&uart_parser, FloatReceive_ErrorCallback);
    } else if (interface == TLV_INTERFACE_USB) {
        TLV_InitParser(&usb_parser, TLV_INTERFACE_USB, FloatReceive_FrameCallback);
        TLV_SetErrorCallback(&usb_parser, FloatReceive_ErrorCallback);
    }
}

/**
 * @brief Get UART parser
 */
tlv_parser_t* FloatReceive_GetUARTParser(void)
{
    return &uart_parser;
}

/**
 * @brief Get USB parser
 */
tlv_parser_t* FloatReceive_GetUSBParser(void)
{
    return &usb_parser;
}

/**
 * @brief Send ACK frame
 */
void FloatReceive_SendAck(uint8_t frame_id, tlv_interface_t interface)
{
    uint8_t ack_frame[20];
    uint16_t ack_size;

    TLV_BuildAckFrame(frame_id, ack_frame, &ack_size);
    Transport_Send(interface, ack_frame, ack_size);
}

/**
 * @brief Send NACK frame
 */
void FloatReceive_SendNack(uint8_t frame_id, tlv_interface_t interface)
{
    uint8_t nack_frame[20];
    uint16_t nack_size;

    TLV_BuildNackFrame(frame_id, nack_frame, &nack_size);
    Transport_Send(interface, nack_frame, nack_size);
}

/**
 * @brief TLV帧回调——接收到有效帧时调用
 */
void FloatReceive_FrameCallback(uint8_t frame_id, uint8_t *data, uint8_t length, tlv_interface_t interface)
{
    /* Parse TLV entries from data segment */
    tlv_entry_t tlv_entries[16];
    uint8_t tlv_count = TLV_ParseData(data, length, tlv_entries, 16);

    /* Do not respond to pure ACK/NACK frames to avoid loops */
    bool has_non_ack = false;
    uint8_t i;
    for (i = 0; i < tlv_count; i++) {
        if (tlv_entries[i].type != TLV_TYPE_ACK && tlv_entries[i].type != TLV_TYPE_NACK) {
            has_non_ack = true;
            break;
        }
    }
    if (!has_non_ack) {
        return;
    }

    bool ok = dispatch_tlv_entries(frame_id, tlv_entries, tlv_count, interface);

    /* Send ACK on success, NACK otherwise */
    if (ok) {
        FloatReceive_SendAck(frame_id, interface);
    } else {
        FloatReceive_SendNack(frame_id, interface);
    }
}

void FloatReceive_ErrorCallback(uint8_t frame_id, tlv_interface_t interface, tlv_error_t error)
{
    /* On parser error, immediately NACK */
    FloatReceive_SendNack(frame_id, interface);
}

void FloatReceive_RegisterTLVHandler(uint8_t type, tlv_type_handler_t handler)
{
    uint8_t i;
    for (i = 0; i < tlv_type_handler_count; i++) {
        if (tlv_type_ids[i] == type) {
            tlv_type_handlers[i] = handler;
            return;
        }
    }
    if (tlv_type_handler_count < MAX_TLV_TYPE_HANDLERS) {
        tlv_type_ids[tlv_type_handler_count] = type;
        tlv_type_handlers[tlv_type_handler_count] = handler;
        tlv_type_handler_count = (uint8_t)(tlv_type_handler_count + 1);
    }
}

void FloatReceive_RegisterCmdHandler(uint8_t command, cmd_handler_t handler)
{
    uint8_t i;
    for (i = 0; i < cmd_handler_count; i++) {
        if (cmd_ids[i] == command) {
            cmd_handlers[i] = handler;
            return;
        }
    }
    if (cmd_handler_count < MAX_CMD_HANDLERS) {
        cmd_ids[cmd_handler_count] = command;
        cmd_handlers[cmd_handler_count] = handler;
        cmd_handler_count = (uint8_t)(cmd_handler_count + 1);
    }
}

static bool handle_control_cmd(const tlv_entry_t *entry, tlv_interface_t interface)
{
    if (entry->length < 1 || entry->value == NULL) return false;
    uint8_t cmd = entry->value[0];
    uint8_t i;
    for (i = 0; i < cmd_handler_count; i++) {
        if (cmd_ids[i] == cmd && cmd_handlers[i]) {
            return cmd_handlers[i](cmd, interface);
        }
    }
    return false; /* no handler */
}

static bool dispatch_tlv_entries(uint8_t frame_id, tlv_entry_t *entries, uint8_t count, tlv_interface_t interface)
{
    bool all_ok = true;
    uint8_t i;
    for (i = 0; i < count; i++) {
        const tlv_entry_t *e = &entries[i];

        if (e->type == TLV_TYPE_ACK || e->type == TLV_TYPE_NACK) {
            /* treat as handled, but outer logic avoids responding */
            continue;
        }

        if (e->type == TLV_TYPE_CONTROL_CMD) {
            bool ok = handle_control_cmd(e, interface);
            all_ok = all_ok && ok;
            continue;
        }

        /* Try custom type handler first */
        bool handled = false;
        uint8_t j;
        for (j = 0; j < tlv_type_handler_count; j++) {
            if (tlv_type_ids[j] == e->type && tlv_type_handlers[j]) {
                handled = tlv_type_handlers[j](e, interface);
                break;
            }
        }
        if (!handled) {
            all_ok = false; /* unknown or failed */
        }
    }
    return all_ok;
}

/* USER CODE END 0 */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
