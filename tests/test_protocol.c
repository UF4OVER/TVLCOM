/**
 * @file test_protocol.c
 * @brief Minimal unit tests for TLV/Receive/Transport layers (no real serial needed).
 * @author UF4OVER
 * @date 2025-12-31
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "HAL/hal.h"
#include "S_TLV_PROTOCOL.h"
#include "S_TRANSPORT_PROTOCOL.h"
#include "S_RECEIVE_PROTOCOL.h"

/* --------------------------- tiny test macros --------------------------- */

#define TEST_ASSERT(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while(0)

#define TEST_RUN(fn) do { \
    int rc = (fn)(); \
    if (rc != 0) return rc; \
    fprintf(stdout, "[PASS] %s\n", #fn); \
} while(0)

/* --------------------------- mock transport --------------------------- */

typedef struct {
    uint8_t buf[2048];
    uint16_t len;
} capture_t;

static capture_t g_tx;

static int mock_send(const uint8_t *data, uint16_t len)
{
    if (!data) return -1;
    if ((uint32_t)g_tx.len + len > sizeof(g_tx.buf)) return -2;
    memcpy(&g_tx.buf[g_tx.len], data, len);
    g_tx.len = (uint16_t)(g_tx.len + len);
    return (int)len;
}

static void capture_reset(void)
{
    memset(&g_tx, 0, sizeof(g_tx));
}

static bool capture_contains_tlv_type(uint8_t type)
{
    /* naive scan for type byte in captured frames; safe enough for small tests */
    for (uint16_t i = 0; i < g_tx.len; ++i) {
        if (g_tx.buf[i] == type) return true;
    }
    return false;
}

static void feed_bytes_to_uart_parser(const uint8_t *data, uint16_t len)
{
    tlv_parser_t *p = FloatReceive_GetUARTParser();
    for (uint16_t i = 0; i < len; ++i) {
        TLV_ProcessByte(p, data[i]);
    }
}

/* --------------------------- handlers --------------------------- */

static bool g_seen_custom = false;

static bool on_custom_ok(const tlv_entry_t *e, tlv_interface_t iface)
{
    (void)iface;
    g_seen_custom = true;
    return (e && e->length == 1 && e->value && e->value[0] == 0xAA);
}

/* --------------------------- tests --------------------------- */

static int test_auto_ack_when_all_handlers_ok(void)
{
    TVL_HAL_Set(NULL); /* tests don't need mutex */

    capture_reset();
    Transport_RegisterSender(TLV_INTERFACE_UART, mock_send);
    FloatReceive_Init(TLV_INTERFACE_UART);

    g_seen_custom = false;
    FloatReceive_RegisterTLVHandler(0x55, on_custom_ok);

    tlv_entry_t e;
    uint8_t v = 0xAA;
    TLV_CreateRawEntry(0x55, &v, 1, &e);

    uint8_t frame_id = 0x10;
    uint8_t frame[TLV_MAX_FRAME_SIZE];
    uint16_t frame_len = 0;
    TEST_ASSERT(TLV_BuildFrame(frame_id, &e, 1, frame, &frame_len));

    feed_bytes_to_uart_parser(frame, frame_len);

    TEST_ASSERT(g_seen_custom);
    TEST_ASSERT(capture_contains_tlv_type(TLV_TYPE_ACK));
    TEST_ASSERT(!capture_contains_tlv_type(TLV_TYPE_NACK));
    return 0;
}

static int test_auto_nack_when_unknown_type(void)
{
    TVL_HAL_Set(NULL);

    capture_reset();
    Transport_RegisterSender(TLV_INTERFACE_UART, mock_send);
    FloatReceive_Init(TLV_INTERFACE_UART);

    tlv_entry_t e;
    uint8_t v = 0x01;
    TLV_CreateRawEntry(0x77, &v, 1, &e); /* not registered */

    uint8_t frame_id = 0x11;
    uint8_t frame[TLV_MAX_FRAME_SIZE];
    uint16_t frame_len = 0;
    TEST_ASSERT(TLV_BuildFrame(frame_id, &e, 1, frame, &frame_len));

    feed_bytes_to_uart_parser(frame, frame_len);

    TEST_ASSERT(capture_contains_tlv_type(TLV_TYPE_NACK));
    return 0;
}

static int test_no_ack_storm_on_received_ack(void)
{
    TVL_HAL_Set(NULL);

    capture_reset();
    Transport_RegisterSender(TLV_INTERFACE_UART, mock_send);
    FloatReceive_Init(TLV_INTERFACE_UART);

    /* Build an ACK frame and feed it into receiver; receiver must NOT reply with another ACK/NACK */
    uint8_t ack_frame[TLV_MAX_FRAME_SIZE];
    uint16_t ack_len = 0;
    TLV_BuildAckFrame(0x22, ack_frame, &ack_len);

    feed_bytes_to_uart_parser(ack_frame, ack_len);

    TEST_ASSERT(g_tx.len == 0);
    return 0;
}

int main(void)
{
    TEST_RUN(test_auto_ack_when_all_handlers_ok);
    TEST_RUN(test_auto_nack_when_unknown_type);
    TEST_RUN(test_no_ack_storm_on_received_ack);

    fprintf(stdout, "All tests passed.\n");
    return 0;
}

