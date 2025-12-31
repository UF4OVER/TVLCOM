/**
 ******************************************************************************
 * @file           : S_TRANSPORT_PROTOCOL.c
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

/* Includes ------------------------------------------------------------------*/
#include "S_TRANSPORT_PROTOCOL.h"

/* USER CODE BEGIN Includes */

#include "S_TLV_PROTOCOL.h"
#include <string.h>
#include "HAL/hal.h"

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

static transport_send_func_t s_uart_sender = NULL;
static transport_send_func_t s_usb_sender  = NULL;
static uint8_t s_frame_id_counter = 0;

/* Optional lock to protect shared state in multi-thread / ISR + main scenarios */
static tvl_hal_mutex_t s_transport_lock = NULL;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

/* Exported functions --------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/**
 * @brief Register a low-level sender for an interface.
 *
 * This is typically called once during startup.
 * If HAL provides mutex_create/lock/unlock, registration is protected.
 */
void Transport_RegisterSender(tlv_interface_t interface, transport_send_func_t fn)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (!s_transport_lock && hal && hal->mutex_create) {
        /* Best-effort, avoid dynamic allocation in MCU builds by leaving mutex_* NULL */
        s_transport_lock = hal->mutex_create();
    }
    if (s_transport_lock && hal && hal->mutex_lock) hal->mutex_lock(s_transport_lock);

    if (interface == TLV_INTERFACE_UART) {
        s_uart_sender = fn;
    } else if (interface == TLV_INTERFACE_USB) {
        s_usb_sender = fn;
    }

    if (s_transport_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_transport_lock);
}

/**
 * @brief Send raw bytes to the interface.
 *
 * @return <0 when sender is not registered or sender reports an error.
 */
int Transport_Send(tlv_interface_t interface, const uint8_t *data, uint16_t len)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_transport_lock && hal && hal->mutex_lock) hal->mutex_lock(s_transport_lock);

    transport_send_func_t fn = NULL;
    if (interface == TLV_INTERFACE_UART) {
        fn = s_uart_sender;
    } else if (interface == TLV_INTERFACE_USB) {
        fn = s_usb_sender;
    }

    if (s_transport_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_transport_lock);

    if (fn == NULL) {
        return -1; /* not registered */
    }
    return fn(data, len);
}

/**
 * @brief Build a TLV frame into a stack buffer and send it.
 *
 * Failure cases:
 * - TLV_BuildFrame() fails if total TLV payload > TLV_MAX_DATA_LENGTH.
 * - Transport_Send() fails if sender not registered.
 */
bool Transport_SendTLVs(tlv_interface_t interface, uint8_t frame_id,
                        const tlv_entry_t *entries, uint8_t count)
{
    uint8_t buffer[TLV_MAX_FRAME_SIZE];
    uint16_t size = 0;
    if (!TLV_BuildFrame(frame_id, entries, count, buffer, &size)) {
        return false;
    }
    return Transport_Send(interface, buffer, size) >= 0;
}

/**
 * @brief Allocate a frame id for outgoing frames.
 *
 * @note Wraps naturally (uint8_t overflow).
 */
uint8_t Transport_NextFrameId(void)
{
    const tvl_hal_vtable_t *hal = TVL_HAL_Get();
    if (s_transport_lock && hal && hal->mutex_lock) hal->mutex_lock(s_transport_lock);
    /* 0 is a valid ID; allow wrap naturally */
    uint8_t id = (uint8_t)(++s_frame_id_counter);
    if (s_transport_lock && hal && hal->mutex_unlock) hal->mutex_unlock(s_transport_lock);
    return id;
}

/* USER CODE END 1 */
