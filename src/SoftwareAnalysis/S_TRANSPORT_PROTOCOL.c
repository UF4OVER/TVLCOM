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

void Transport_RegisterSender(tlv_interface_t interface, transport_send_func_t fn)
{
    if (interface == TLV_INTERFACE_UART) {
        s_uart_sender = fn;
    } else if (interface == TLV_INTERFACE_USB) {
        s_usb_sender = fn;
    }
}

int Transport_Send(tlv_interface_t interface, const uint8_t *data, uint16_t len)
{
    transport_send_func_t fn = NULL;
    if (interface == TLV_INTERFACE_UART) {
        fn = s_uart_sender;
    } else if (interface == TLV_INTERFACE_USB) {
        fn = s_usb_sender;
    }
    if (fn == NULL) {
        return -1; /* not registered */
    }
    return fn(data, len);
}

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

uint8_t Transport_NextFrameId(void)
{
    /* 0 is a valid ID; allow wrap naturally */
    return ++s_frame_id_counter;
}

/* USER CODE END 1 */
