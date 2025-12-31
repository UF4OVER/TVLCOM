/**
 * @file main.c
 * @brief Windows PC demo for TVLCOM TLV protocol over serial (UART).
 * @author UF4OVER
 * @date 2025-12-31
 *
 * This demo:
 * - Installs Windows HAL (mutex/tick/sleep).
 * - Opens a serial port (COMx) and registers a Transport sender.
 * - Initializes Receive module and registers TLV/CMD/ACK callbacks.
 * - Optionally runs a dedicated RX thread.
 *
 * Notes:
 * - When TLV_DEBUG_ENABLE==0 logs are compiled out.
 * - If serial port can't be opened, the program runs in "dry mode" (no TX).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>   /* rand/srand */
#include <time.h>     /* time */

#include "SERIAL.h"
#include "S_TRANSPORT_PROTOCOL.h"
#include "S_RECEIVE_PROTOCOL.h"
#include "S_TLV_PROTOCOL.h"

#include "GLOBAL_CONFIG.h"
#include "HAL/hal.h"
#include "HAL/windows/hal_windows.h"

#ifndef TLV_DEBUG_ENABLE
/*
 * Default to enabled for the PC demo so users can see RX/TLV prints out of box.
 * You can still override it globally in GLOBAL_CONFIG.h or via compiler defines.
 */
#define TLV_DEBUG_ENABLE 1
#endif

#if TLV_DEBUG_ENABLE
#  define TLV_LOG(...) do { printf(__VA_ARGS__); } while(0)
#else
#  define TLV_LOG(...) do { (void)0; } while(0)
#endif

/*
 * Optional: dump raw UART bytes before feeding the TLV parser.
 * Useful when you suspect framing/baud mismatch and TLV callbacks never fire.
 */
#ifndef TVLCOM_DEMO_DUMP_RX_HEX
#define TVLCOM_DEMO_DUMP_RX_HEX 0
#endif

/* ------------------------------- demo config ------------------------------- */

#ifndef TVLCOM_DEMO_PORT
#  define TVLCOM_DEMO_PORT "COM4"
#endif

#ifndef TVLCOM_DEMO_BAUD
#  define TVLCOM_DEMO_BAUD 115200u
#endif

#ifndef TVLCOM_DEMO_READ_TIMEOUT_MS
#  define TVLCOM_DEMO_READ_TIMEOUT_MS 50u
#endif

#ifndef TVLCOM_DEMO_IDLE_SLEEP_MS
#  define TVLCOM_DEMO_IDLE_SLEEP_MS 1u
#endif

#define ENABLE_PERIODIC_SENDER 0
#define ENABLE_RX_THREAD       1  /* 1: RX+parse in background thread */

/* ------------------------------- globals ---------------------------------- */

static serial_t *g_serial = NULL;
static volatile bool g_running = true;
#if ENABLE_PERIODIC_SENDER
static HANDLE g_sender_thread = NULL;
#endif
#if ENABLE_RX_THREAD
static HANDLE g_receiver_thread = NULL;
#endif

/* ------------------------------- helpers ---------------------------------- */

/**
 * @brief Console Ctrl+C handler.
 */
static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_running = false;
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * @brief Transport sender implementation for UART interface.
 */
static int uart_send_impl(const uint8_t *data, uint16_t len)
{
    if (!g_serial) return -1;
    ssize_t n = serial_write(g_serial, data, len);
    return (int)n;
}

/**
 * @brief Feed bytes from serial into the TLV parser.
 */
static void process_rx_bytes(tlv_parser_t *parser, const uint8_t *buf, ssize_t n)
{
    if (!parser || !buf || n <= 0) return;

#if TVLCOM_DEMO_DUMP_RX_HEX
    TLV_LOG("[UART][RX] %ld bytes: ", (long)n);
    for (ssize_t i = 0; i < n; ++i) {
        TLV_LOG("%02X ", (unsigned)buf[i]);
    }
    TLV_LOG("\n");
#endif

    for (ssize_t i = 0; i < n; ++i) {
        TLV_ProcessByte(parser, buf[i]);
    }
}

/* ------------------------------- callbacks -------------------------------- */

/**
 * @brief Example TLV handler that prints a 4-byte int32.
 */
static bool on_integer_tlv(const tlv_entry_t *e, tlv_interface_t iface)
{
    if (!e) return false;
    if (e->length == 4) {
        int32_t v = TLV_ExtractInt32Value(e);
        uint32_t uv = (uint32_t)v;
        TLV_LOG("[RX][IF%u] INT32 type=0x%02X len=4 val=%ld (u=%lu 0x%08lX)\n",
                (unsigned)iface, e->type, (long)v, (unsigned long)uv, (unsigned long)uv);
        return true;
    }
    return false;
}

/**
 * @brief Example TLV handler that prints an IEEE754 float32.
 */
static bool on_float32_tlv(const tlv_entry_t *e, tlv_interface_t iface)
{
    if (!e || !e->value) return false;
    if (e->length == 4) {
        uint32_t u = (uint32_t)e->value[0] |
                     ((uint32_t)e->value[1] << 8) |
                     ((uint32_t)e->value[2] << 16) |
                     ((uint32_t)e->value[3] << 24);
        union { uint32_t u; float f; } conv;
        conv.u = u;
        TLV_LOG("[RX][IF%u] F32  type=0x%02X val=%f\n", (unsigned)iface, e->type, (double)conv.f);
        return true;
    }
    return false;
}

/**
 * @brief Example TLV handler that prints a string when debug is enabled.
 */
static bool on_string_tlv(const tlv_entry_t *e, tlv_interface_t iface)
{
    if (!e) return false;

    /*
     * Important: previously the payload printing was compiled only when
     * TLV_DEBUG_ENABLE==1. That makes it look like "nothing is printed".
     * We now always print a safe preview (ASCII-only) when TLV_LOG is enabled.
     */
    TLV_LOG("[RX][IF%u] STR  type=0x%02X len=%u: ", (unsigned)iface, e->type, e->length);
#if TLV_DEBUG_ENABLE
    if (e->value) {
        /* Avoid huge/untrusted prints; keep it readable in console. */
        const uint8_t max_print = 64;
        uint8_t to_print = e->length;
        if (to_print > max_print) to_print = max_print;
        for (uint8_t i = 0; i < to_print; i++) {
            unsigned char c = (unsigned char)e->value[i];
            if (c >= 32 && c <= 126) {
                putchar((int)c);
            } else {
                putchar('.');
            }
        }
        if (to_print < e->length) {
            TLV_LOG("...");
        }
    }
    putchar('\n');
#endif
    return true;
}

/**
 * @brief Example TLV handler for scaled float (int32 / 10000).
 */
static bool on_scaled_tlv(const tlv_entry_t *e, tlv_interface_t iface)
{
    if (!e) return false;
    (void)iface;

    float val = TLV_ExtractFloatValue(e);
    TLV_LOG("[RX][IF%u] SCAL type=0x%02X val=%f\n", (unsigned)iface, e->type, (double)val);
    return true;
}

/**
 * @brief Example command handler.
 */
static bool on_cmd_ping(uint8_t cmd, tlv_interface_t iface)
{
    (void)cmd;
    (void)iface;
    TLV_LOG("[RX][IF%u] CMD  0x%02X\n", (unsigned)iface, cmd);
    return true;
}

/**
 * @brief ACK notification handler.
 */
static void on_ack(uint8_t orig_id, tlv_interface_t iface)
{
    (void)iface;
    TLV_LOG("[ACK] for frame 0x%02X on IF%u\n", orig_id, (unsigned)iface);
}

/**
 * @brief NACK notification handler.
 */
static void on_nack(uint8_t orig_id, tlv_interface_t iface)
{
    (void)iface;
    TLV_LOG("[NACK] for frame 0x%02X on IF%u\n", orig_id, (unsigned)iface);
}

/* ------------------------------- TX demos --------------------------------- */

/**
 * @brief Send a couple of demo frames.
 */
static void send_demo_frames(void)
{
    /* First: send a control cmd entry (demo) */
    tlv_entry_t nack_entry;
    TLV_CreateControlCmdEntry(0xFF, &nack_entry);
    uint8_t nack_frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, nack_frame_id, &nack_entry, 1);

    tlv_entry_t entries[6];
    TLV_CreateInt32Entry(0x40, 123456789, &entries[0]);
    const char *msg = "HELLO";
    TLV_CreateRawEntry(TLV_TYPE_STRING, (const uint8_t*)msg, (uint8_t)strlen(msg), &entries[1]);
    TLV_CreateFloat32Entry(0x41, 3.1415926f, &entries[2]);
    TLV_CreateVoltageEntry(12.3456f, &entries[3]);
    uint8_t raw[] = {0xDE,0xAD,0xBE,0xEF};
    TLV_CreateRawEntry(0x50, raw, (uint8_t)sizeof(raw), &entries[4]);
    TLV_CreateControlCmdEntry(0x02, &entries[5]);

    uint8_t frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, entries, 6);
}

static void send_voltage_once(float v)
{
    tlv_entry_t e;
    TLV_CreateVoltageEntry(v, &e);
    uint8_t frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, &e, 1);
}

/* ------------------------------- threads ---------------------------------- */

static DWORD WINAPI SenderThread(LPVOID lp)
{
    (void)lp;

    /*
     * Demo-only PRNG:
     * - This is not cryptographic.
     * - It is used only to generate "random looking" demo payloads.
     */
    srand((unsigned)time(NULL));

    /* Randomized sender emulating Python send_random_loop */
    while (g_running) {
        tlv_entry_t entries[5];
        uint8_t count = 0;

        /* choose 1-3 TLVs */
        uint8_t want = (uint8_t)(1u + (uint8_t)(rand() % 3));
        for (uint8_t i = 0; i < want; ++i) {
            int kind = rand() % 4; /* 0=str,1=int,2=cmd,3=custom */
            if (kind == 0) {
                char str[16];
                static const char alnum[] = "abcdefghijklmnopqrstuvwxyz0123456789";
                uint8_t slen = (uint8_t)(3u + (uint8_t)(rand() % 10));
                if (slen >= (uint8_t)sizeof(str)) slen = (uint8_t)(sizeof(str) - 1u);
                for (uint8_t k = 0; k < slen; ++k) str[k] = alnum[rand() % (int)(sizeof(alnum) - 1u)];
                str[slen] = '\0';
                TLV_CreateStringEntry(str, &entries[count++]);
            } else if (kind == 1) {
                int32_t v = (int32_t)rand();
                TLV_CreateInt32Entry(TLV_TYPE_INTEGER, v, &entries[count++]);
            } else if (kind == 2) {
                uint8_t cmd = (uint8_t)(1u + (uint8_t)(rand() % 2));
                TLV_CreateControlCmdEntry(cmd, &entries[count++]);
            } else {
                /* custom types 0x40,0x41,0x50 */
                static const uint8_t tsel_arr[] = {0x40,0x41,0x50};
                uint8_t tsel = tsel_arr[rand() % 3];
                if (tsel == 0x40) {
                    TLV_CreateInt32Entry(0x40, (int32_t)rand(), &entries[count++]);
                } else if (tsel == 0x41) {
                    TLV_CreateFloat32Entry(0x41, 3.14159f, &entries[count++]);
                } else {
                    uint8_t rawlen = (uint8_t)(1u + (uint8_t)(rand() % 8));
                    uint8_t rawbuf[8];
                    for (uint8_t r = 0; r < rawlen; ++r) rawbuf[r] = (uint8_t)(rand() & 0xFF);
                    TLV_CreateRawEntry(tsel, rawbuf, rawlen, &entries[count++]);
                }
            }
        }

        uint8_t frame_id = Transport_NextFrameId();
        (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, entries, count);
        Sleep(2000); /* interval */
    }
    return 0;
}

/**
 * @brief RX loop implementation shared by thread and non-thread mode.
 */
static void rx_loop(void)
{
    uint8_t buf[256];
    tlv_parser_t *parser = FloatReceive_GetUARTParser();

    while (g_running) {
        if (!g_serial) {
            Sleep(100);
            continue;
        }

        ssize_t n = serial_read(g_serial, buf, sizeof(buf), TVLCOM_DEMO_READ_TIMEOUT_MS);
        if (n > 0) {
            process_rx_bytes(parser, buf, n);
        } else {
            Sleep(TVLCOM_DEMO_IDLE_SLEEP_MS);
        }
    }
}

static DWORD WINAPI ReceiverThread(LPVOID lp)
{
    (void)lp;
    rx_loop();
    return 0;
}

int main(void)
{
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    /* Install platform HAL (enables optional mutex/time utilities in protocol layers). */
    TVL_HAL_Set(TVL_HAL_Windows());

    /* Make it obvious in release builds that logs are enabled/disabled. */
    TLV_LOG("[TVLCOM] Demo start. Port=%s Baud=%lu TLV_DEBUG_ENABLE=%d\n",
            TVLCOM_DEMO_PORT, (unsigned long)TVLCOM_DEMO_BAUD, (int)TLV_DEBUG_ENABLE);

    g_serial = serial_open(TVLCOM_DEMO_PORT, TVLCOM_DEMO_BAUD);
    if (!g_serial) {
        TLV_LOG("[WARN] %s 打开失败, dry 模式.\n", TVLCOM_DEMO_PORT);
    }

    Transport_RegisterSender(TLV_INTERFACE_UART, uart_send_impl);
    FloatReceive_Init(TLV_INTERFACE_UART);

    /* Register handlers */
    FloatReceive_RegisterTLVHandler(TLV_TYPE_INTEGER, on_integer_tlv);
    FloatReceive_RegisterTLVHandler(0x40, on_integer_tlv);
    FloatReceive_RegisterTLVHandler(0x41, on_float32_tlv);
    FloatReceive_RegisterTLVHandler(TLV_TYPE_STRING, on_string_tlv);
    FloatReceive_RegisterTLVHandler(INFO_VBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_IBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_PBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(SENSOR_TEMP, on_scaled_tlv);
    FloatReceive_RegisterCmdHandler(0x41, on_cmd_ping);
    FloatReceive_RegisterAckHandler(on_ack);
    FloatReceive_RegisterNackHandler(on_nack);

    /* Kick off an initial demo TX */
    send_demo_frames();
    (void)send_voltage_once; /* keep helper available for future demo actions */

#if ENABLE_PERIODIC_SENDER
    g_sender_thread = CreateThread(NULL, 0, SenderThread, NULL, 0, NULL);
#endif

#if ENABLE_RX_THREAD
    if (g_serial) {
        g_receiver_thread = CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);
    }

    while (g_running) {
        Sleep(100);
    }
#else
    rx_loop();
#endif

    g_running = false;

#if ENABLE_PERIODIC_SENDER
    if (g_sender_thread) {
        WaitForSingleObject(g_sender_thread, 2000);
        CloseHandle(g_sender_thread);
        g_sender_thread = NULL;
    }
#endif
#if ENABLE_RX_THREAD
    if (g_receiver_thread) {
        WaitForSingleObject(g_receiver_thread, 2000);
        CloseHandle(g_receiver_thread);
        g_receiver_thread = NULL;
    }
#endif

    if (g_serial) {
        serial_close(g_serial);
        g_serial = NULL;
    }
    return 0;
}
