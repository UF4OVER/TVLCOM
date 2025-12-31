/**
 ******************************************************************************
 * @file           : S_RECEIVE_PROTOCOL.c
 * @brief          : Receive-side TLV dispatch implementation (handlers + ACK/NACK).
 * @author         : UF4OVER
 * @date           : 2025-12-31
 ******************************************************************************
 * @attention
 *
 * See S_RECEIVE_PROTOCOL.h for policy and callback contracts.
 *
 ******************************************************************************
 */


/* Includes ------------------------------------------------------------------*/
#include "S_RECEIVE_PROTOCOL.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "S_TRANSPORT_PROTOCOL.h"
#include "HAL/hal.h"
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

static ack_notify_t s_ack_handler = NULL;
static ack_notify_t s_nack_handler = NULL;

/* Optional lock to protect handler registry in multi-thread / ISR contexts */
static tvl_hal_mutex_t s_receive_lock = NULL;

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
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (!s_receive_lock && hal && hal->mutex_create) {
        s_receive_lock = hal->mutex_create();
    }

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
 * @brief Parser error callback.
 *
 * Current policy: any parser error results in immediate NACK.
 * The 'error' parameter can be used for diagnostics/logging.
 */
void FloatReceive_ErrorCallback(uint8_t frame_id, tlv_interface_t interface, tlv_error_t error)
{
    (void)error;
    /* On parser error, immediately NACK */
    FloatReceive_SendNack(frame_id, interface);
}

/**
 * @brief TLV帧回调——接收到有效帧时调用
 */
void FloatReceive_FrameCallback(uint8_t frame_id, const uint8_t *data, uint8_t length, tlv_interface_t interface)
{
    tlv_entry_t tlv_entries[16];
    uint8_t tlv_count = TLV_ParseData(data, length, tlv_entries, 16);

    bool has_non_ack = false;
    bool all_ack_or_nack = true;
    for (uint8_t i = 0; i < tlv_count; i++) {
        uint8_t t = tlv_entries[i].type;
        if (t != TLV_TYPE_ACK && t != TLV_TYPE_NACK) {
            has_non_ack = true;
        }
        if (t != TLV_TYPE_ACK && t != TLV_TYPE_NACK) {
            all_ack_or_nack = false;
        }
    }
    if (tlv_count == 0) return;

    if (all_ack_or_nack && !has_non_ack) {
        /* Notify upper layer but do not respond */
        for (uint8_t i = 0; i < tlv_count; ++i) {
            const tlv_entry_t *e = &tlv_entries[i];
            if (e->length >= 1) {
                uint8_t original_id = e->value[0];
                if (e->type == TLV_TYPE_ACK && s_ack_handler) s_ack_handler(original_id, interface);
                else if (e->type == TLV_TYPE_NACK && s_nack_handler) s_nack_handler(original_id, interface);
            }
        }
        return;
    }

    bool ok = dispatch_tlv_entries(frame_id, tlv_entries, tlv_count, interface);
    if (ok) {
        FloatReceive_SendAck(frame_id, interface);
    } else {
        FloatReceive_SendNack(frame_id, interface);
    }
}

void FloatReceive_RegisterTLVHandler(uint8_t type, tlv_type_handler_t handler)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);

    uint8_t i;
    for (i = 0; i < tlv_type_handler_count; i++) {
        if (tlv_type_ids[i] == type) {
            tlv_type_handlers[i] = handler;
            if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
            return;
        }
    }
    if (tlv_type_handler_count < MAX_TLV_TYPE_HANDLERS) {
        tlv_type_ids[tlv_type_handler_count] = type;
        tlv_type_handlers[tlv_type_handler_count] = handler;
        tlv_type_handler_count = (uint8_t)(tlv_type_handler_count + 1);
    }

    if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
}

void FloatReceive_RegisterCmdHandler(uint8_t command, cmd_handler_t handler)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);

    uint8_t i;
    for (i = 0; i < cmd_handler_count; i++) {
        if (cmd_ids[i] == command) {
            cmd_handlers[i] = handler;
            if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
            return;
        }
    }
    if (cmd_handler_count < MAX_CMD_HANDLERS) {
        cmd_ids[cmd_handler_count] = command;
        cmd_handlers[cmd_handler_count] = handler;
        cmd_handler_count = (uint8_t)(cmd_handler_count + 1);
    }

    if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
}

void FloatReceive_RegisterAckHandler(ack_notify_t handler)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);
    s_ack_handler = handler;
    if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
}

void FloatReceive_RegisterNackHandler(ack_notify_t handler)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);
    s_nack_handler = handler;
    if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
}

static bool handle_control_cmd(const tlv_entry_t *entry, tlv_interface_t interface)
{
    if (entry->length < 1 || entry->value == NULL) return false;
    uint8_t cmd = entry->value[0];

    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);

    uint8_t i;
    for (i = 0; i < cmd_handler_count; i++) {
        if (cmd_ids[i] == cmd && cmd_handlers[i]) {
            cmd_handler_t fn = cmd_handlers[i];
            if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
            return fn(cmd, interface);
        }
    }

    if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
    return false; /* no handler */
}

static bool dispatch_tlv_entries(uint8_t frame_id, tlv_entry_t *entries, uint8_t count, tlv_interface_t interface)
{
    (void)frame_id;
    bool all_ok = true;

    const tvl_hal_vtable_t *hal = TVL_HAL_Get();

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

        if (s_receive_lock && hal && hal->mutex_lock) hal->mutex_lock(s_receive_lock);
        uint8_t j;
        for (j = 0; j < tlv_type_handler_count; j++) {
            if (tlv_type_ids[j] == e->type && tlv_type_handlers[j]) {
                tlv_type_handler_t fn = tlv_type_handlers[j];
                if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);
                handled = fn(e, interface);
                goto done_one;
            }
        }
        if (s_receive_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_receive_lock);

done_one:
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
