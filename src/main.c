#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "SERIAL.h"
#include "S_TRANSPORT_PROTOCOL.h"
#include "S_RECEIVE_PROTOCOL.h"
#include "S_TLV_PROTOCOL.h"
#include "GLOBAL_CONFIG.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


static serial_t *g_serial = NULL;
static volatile bool g_running = true;
static HANDLE g_sender_thread = NULL;

static int uart_send_impl(const uint8_t *data, uint16_t len)
{
    if (!g_serial) return -1;
    ssize_t n = serial_write(g_serial, data, len);
    return (int)n;
}

static bool on_integer_tlv(const tlv_entry_t *entry, tlv_interface_t iface)
{
    if (entry->length == 4) {
        int32_t v = TLV_ExtractInt32Value(entry);
        printf("[RX][IF%u] INT32 type=0x%02X val=%ld\n", (unsigned)iface, entry->type, (long)v);
        return true;
    }
    return false;
}

static bool on_float32_tlv(const tlv_entry_t *entry, tlv_interface_t iface)
{
    if (entry->length == 4) {
        uint32_t u = (uint32_t)entry->value[0] |
                     ((uint32_t)entry->value[1] << 8) |
                     ((uint32_t)entry->value[2] << 16) |
                     ((uint32_t)entry->value[3] << 24);
        union { uint32_t u; float f; } conv = { .u = u };
        printf("[RX][IF%u] F32  type=0x%02X val=%f\n", (unsigned)iface, entry->type, (double)conv.f);
        return true;
    }
    return false;
}

static bool on_string_tlv(const tlv_entry_t *entry, tlv_interface_t iface)
{
    printf("[RX][IF%u] STR  type=0x%02X len=%u: ", (unsigned)iface, entry->type, entry->length);
    for (uint8_t i = 0; i < entry->length; i++) putchar((int)entry->value[i]);
    putchar('\n');
    return true;
}

static bool on_scaled_tlv(const tlv_entry_t *entry, tlv_interface_t iface)
{
    float val = TLV_ExtractFloatValue(entry);
    printf("[RX][IF%u] SCAL type=0x%02X val=%f\n", (unsigned)iface, entry->type, (double)val);
    return true;
}

static bool on_cmd_ping(uint8_t command, tlv_interface_t iface)
{
    printf("[RX][IF%u] CMD  0x%02X\n", (unsigned)iface, command);
    return true;
}
/* 发送一帧示例（保留） */
static void send_demo_frames(void)
{
    tlv_entry_t entries[6];

    /* 自定义 int32（type 0x40） */
    TLV_CreateInt32Entry(0x40, 123456789, &entries[0]);

    /* 字符串（type 0x30 预定义为 TLV_TYPE_STRING） */
    const char *msg = "HELLO";
    TLV_CreateRawEntry(TLV_TYPE_STRING, (const uint8_t*)msg, (uint8_t)strlen(msg), &entries[1]);

    /* float32（type 0x41） */
    TLV_CreateFloat32Entry(0x41, 3.1415926f, &entries[2]);

    /* 缩放电压（内部按 ×10000 封装，type=INFO_VBUS） */
    TLV_CreateVoltageEntry(12.3456f, &entries[3]);

    /* 原始二进制（type 0x50） */
    uint8_t raw[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    TLV_CreateRawEntry(0x50, raw, sizeof(raw), &entries[4]);

    /* 控制命令（type=TLV_TYPE_CONTROL_CMD，value[0]=0x01） */
    TLV_CreateControlCmdEntry(0x01, &entries[5]);

    uint8_t frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, entries, 6);
}

/* 连续按指定速率发送电压 TLV（每分钟 count_per_min 次） */
static void send_voltage_once(float voltage)
{
    tlv_entry_t entry;
    TLV_CreateVoltageEntry(voltage, &entry);
    uint8_t frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, &entry, 1);
}

/* 发送线程：默认每分钟 1000 次（间隔约 60ms），可在 g_running 置 false 时退出 */
static DWORD WINAPI SenderThread(LPVOID lpParam)
{
    const unsigned per_minute = 1000;
    /* 计算间隔（ms） */
    const unsigned interval_ms = (unsigned)((60.0f / (float)per_minute) * 1000.0f + 0.5f);
    (void)lpParam;

    /* 简单示例发送固定电压；可改为动态采样 */
    while (g_running) {
        /* 如果没有串口也能通过 Transport 模拟发送（dry run） */
        send_voltage_once(12.3456f);
        Sleep(interval_ms);
    }
    return 0;
}

int main(void)
{
    /* 打开串口（根据你的实际端口修改） */
    g_serial = serial_open("COM20", 115200);
    if (!g_serial) {
        printf("[WARN] Failed to open COM port. Running in dry mode.\n");
    }

    /* 注册 UART 发送函数 */
    Transport_RegisterSender(TLV_INTERFACE_UART, uart_send_impl);

    /* 初始化接收解析器 */
    FloatReceive_Init(TLV_INTERFACE_UART);

    /* 注册回调：类型与命令 */
    FloatReceive_RegisterTLVHandler(TLV_TYPE_INTEGER, on_integer_tlv); /* 若对端按 0x20 发 int32 */
    FloatReceive_RegisterTLVHandler(0x40, on_integer_tlv);             /* 我们自定义的 int32 类型 */
    FloatReceive_RegisterTLVHandler(0x41, on_float32_tlv);             /* 我们自定义的 float32 类型 */
    FloatReceive_RegisterTLVHandler(TLV_TYPE_STRING, on_string_tlv);
    /* 缩放值常用类型（可按需增减） */
    FloatReceive_RegisterTLVHandler(INFO_VBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_IBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(INFO_PBUS, on_scaled_tlv);
    FloatReceive_RegisterTLVHandler(SENSOR_TEMP, on_scaled_tlv);
    /* 控制命令 0x01 */
    FloatReceive_RegisterCmdHandler(0x01, on_cmd_ping);

    /* 发送一帧包含多种 TLV 的演示 */
    send_demo_frames();

    /* 启动发送线程（每分钟 1000 个电压 TLV） */
    g_sender_thread = CreateThread(NULL, 0, SenderThread, NULL, 0, NULL);
    if (!g_sender_thread) {
        printf("[WARN] Failed to create sender thread, sending will be in main thread.\n");
    }

    /* 简单接收循环（等待并解析串口返回数据） */
    if (g_serial) {
        uint8_t buf[256];
        /* 读取一段时间（约 5 秒：100 次 * 50ms 超时） */
        for (int iter = 0; iter < 100; iter++) {
            ssize_t n = serial_read(g_serial, buf, sizeof(buf), 50);
            if (n > 0) {
                tlv_parser_t *p = FloatReceive_GetUARTParser();
                for (ssize_t i = 0; i < n; i++) TLV_ProcessByte(p, buf[i]);
            }
        }
    } else {
        /* 即使串口未打开，也给出一段时间让发送线程跑（例如 5 秒） */
        Sleep(5000);
    }

    /* 请求发送线程停止并等待其退出 */
    g_running = false;
    if (g_sender_thread) {
        WaitForSingleObject(g_sender_thread, 2000);
        CloseHandle(g_sender_thread);
        g_sender_thread = NULL;
    }

    if (g_serial) serial_close(g_serial);
    return 0;
}