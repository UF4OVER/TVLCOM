// c
// 文件: `src/main.c`
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "SERIAL.h"
#include "S_TRANSPORT_PROTOCOL.h"
#include "S_RECEIVE_PROTOCOL.h"
#include "S_TLV_PROTOCOL.h"
#include "GLOBAL_CONFIG.h"

#define ENABLE_PERIODIC_SENDER 0

static serial_t *g_serial = NULL;
static volatile bool g_running = true;
static HANDLE g_sender_thread = NULL;

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
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

static int uart_send_impl(const uint8_t *data, uint16_t len) {
    if (!g_serial) return -1;
    ssize_t n = serial_write(g_serial, data, len);
    return (int)n;
}

/* 回调示例 */
static bool on_integer_tlv(const tlv_entry_t *e, tlv_interface_t iface) {
    if (e->length == 4) {
        int32_t v = TLV_ExtractInt32Value(e);
        printf("[RX][IF%u] INT32 type=0x%02X val=%ld\n", (unsigned)iface, e->type, (long)v);
        return true;
    }
    return false;
}
static bool on_float32_tlv(const tlv_entry_t *e, tlv_interface_t iface) {
    if (e->length == 4) {
        uint32_t u = (uint32_t)e->value[0] |
                     ((uint32_t)e->value[1] << 8) |
                     ((uint32_t)e->value[2] << 16) |
                     ((uint32_t)e->value[3] << 24);
        union { uint32_t u; float f; } conv = { .u = u };
        printf("[RX][IF%u] F32  type=0x%02X val=%f\n", (unsigned)iface, e->type, (double)conv.f);
        return true;
    }
    return false;
}
static bool on_string_tlv(const tlv_entry_t *e, tlv_interface_t iface) {
    printf("[RX][IF%u] STR  type=0x%02X len=%u: ", (unsigned)iface, e->type, e->length);
    for (uint8_t i = 0; i < e->length; i++) putchar((int)e->value[i]);
    putchar('\n');
    return true;
}
static bool on_scaled_tlv(const tlv_entry_t *e, tlv_interface_t iface) {
    float val = TLV_ExtractFloatValue(e);
    printf("[RX][IF%u] SCAL type=0x%02X val=%f\n", (unsigned)iface, e->type, (double)val);
    return true;
}
static bool on_cmd_ping(uint8_t cmd, tlv_interface_t iface) {
    printf("[RX][IF%u] CMD  0x%02X\n", (unsigned)iface, cmd);
    return true;
}

/* 演示帧 */
static void send_demo_frames(void) {
    tlv_entry_t entries[6];
    TLV_CreateInt32Entry(0x40, 123456789, &entries[0]);
    const char *msg = "HELLO";
    TLV_CreateRawEntry(TLV_TYPE_STRING, (const uint8_t*)msg, (uint8_t)strlen(msg), &entries[1]);
    TLV_CreateFloat32Entry(0x41, 3.1415926f, &entries[2]);
    TLV_CreateVoltageEntry(12.3456f, &entries[3]);
    uint8_t raw[] = {0xDE,0xAD,0xBE,0xEF};
    TLV_CreateRawEntry(0x50, raw, sizeof(raw), &entries[4]);
    TLV_CreateControlCmdEntry(0x02, &entries[5]);
    uint8_t frame_id = Transport_NextFrameId();
    Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, entries, 6);
}

static void send_voltage_once(float v) {
    tlv_entry_t e;
    TLV_CreateVoltageEntry(v, &e);
    uint8_t frame_id = Transport_NextFrameId();
    Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, &e, 1);
}

static void on_ack(uint8_t orig_id, tlv_interface_t iface) {
    printf("[ACK] for frame 0x%02X on IF%u\n", orig_id, (unsigned)iface);
}
static void on_nack(uint8_t orig_id, tlv_interface_t iface) {
    printf("[NACK] for frame 0x%02X on IF%u\n", orig_id, (unsigned)iface);
}

static DWORD WINAPI SenderThread(LPVOID lp) {
    (void)lp;
    /* Randomized sender emulating Python send_random_loop */
    while (g_running) {
        tlv_entry_t entries[5];
        uint8_t count = 0;
        /* choose 1-3 TLVs */
        uint8_t want = (uint8_t)(1 + (rand() % 3));
        for (uint8_t i = 0; i < want; ++i) {
            int kind = rand() % 4; /* 0=str,1=int,2=cmd,3=custom */
            if (kind == 0) {
                char str[16];
                static const char alnum[] = "abcdefghijklmnopqrstuvwxyz0123456789";
                uint8_t slen = (uint8_t)(3 + rand() % 10);
                for (uint8_t k = 0; k < slen; ++k) str[k] = alnum[rand() % (sizeof(alnum)-1)];
                str[slen] = '\0';
                TLV_CreateStringEntry(str, &entries[count++]);
            } else if (kind == 1) {
                int32_t v = (int32_t)rand();
                TLV_CreateInt32Entry(TLV_TYPE_INTEGER, v, &entries[count++]);
            } else if (kind == 2) {
                uint8_t cmd = (uint8_t)(1 + (rand() % 2));
                TLV_CreateControlCmdEntry(cmd, &entries[count++]);
            } else {
                /* custom types 0x40,0x41,0x50 */
                uint8_t tsel_arr[] = {0x40,0x41,0x50};
                uint8_t tsel = tsel_arr[rand() % 3];
                if (tsel == 0x40) {
                    TLV_CreateInt32Entry(0x40, (int32_t)rand(), &entries[count++]);
                } else if (tsel == 0x41) {
                    TLV_CreateFloat32Entry(0x41, 3.14159f, &entries[count++]);
                } else {
                    uint8_t rawlen = (uint8_t)(1 + rand() % 8);
                    uint8_t rawbuf[8];
                    for (uint8_t r = 0; r < rawlen; ++r) rawbuf[r] = (uint8_t)(rand() & 0xFF);
                    TLV_CreateRawEntry(tsel, rawbuf, rawlen, &entries[count++]);
                }
            }
        }
        uint8_t frame_id = Transport_NextFrameId();
        Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, entries, count);
        Sleep(2000); /* interval */
    }
    return 0;
}

/* 持续接收与解析 */
static void receive_loop(void) {
    uint8_t buf[256];
    tlv_parser_t *parser = FloatReceive_GetUARTParser();
    while (g_running) {
        ssize_t n = serial_read(g_serial, buf, sizeof(buf), 50); // 50ms 轮询
        if (n > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                TLV_ProcessByte(parser, buf[i]);
            }
        } else {
            Sleep(1); // 空闲减负
        }
    }
}

int main(void) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    g_serial = serial_open("COM4", 115200);
    if (!g_serial) {
        printf("[WARN] COM4 打开失败, dry 模式.\n");
    }

    Transport_RegisterSender(TLV_INTERFACE_UART, uart_send_impl);
    FloatReceive_Init(TLV_INTERFACE_UART);

    FloatReceive_RegisterTLVHandler(TLV_TYPE_INTEGER, on_integer_tlv);
    FloatReceive_RegisterTLVHandler(0x40, on_integer_tlv);
    FloatReceive_RegisterTLVHandler(0x41, on_float32_tlv);
    FloatReceive_RegisterTLVHandler(TLV_TYPE_STRING, on_string_tlv);
    FloatReceive_RegisterTLVHandler(INFO_VBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_IBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_PBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(SENSOR_TEMP, on_scaled_tlv);
    FloatReceive_RegisterCmdHandler(0x01, on_cmd_ping);
    FloatReceive_RegisterAckHandler(on_ack);
    FloatReceive_RegisterNackHandler(on_nack);

    /* 启动先发送一次 */
    send_demo_frames();

#if ENABLE_PERIODIC_SENDER
    g_sender_thread = CreateThread(NULL, 0, SenderThread, NULL, 0, NULL);
#endif

    if (g_serial) receive_loop();
    else while (g_running) Sleep(100);

    g_running = false;

#if ENABLE_PERIODIC_SENDER
    if (g_sender_thread) {
        WaitForSingleObject(g_sender_thread, 2000);
        CloseHandle(g_sender_thread);
    }
#endif
    if (g_serial) serial_close(g_serial);
    return 0;
}