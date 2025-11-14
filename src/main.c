#include "SERIAL.h"
#include "S_TRANSPORT_PROTOCOL.h"
#include "S_RECEIVE_PROTOCOL.h"
#include "S_TLV_PROTOCOL.h"
#include "GLOBAL_CONFIG.h"
#include <stdio.h>
#include <string.h>

static serial_t *g_serial = NULL;

static int uart_send_impl(const uint8_t *data, uint16_t len)
{
    if (!g_serial) return -1;
    ssize_t n = serial_write(g_serial, data, len);
    return (int)n;
}

/* Example handlers */
static bool on_integer_tlv(const tlv_entry_t *entry, tlv_interface_t interface)
{
    if (entry->length == 4) {
        int32_t v = TLV_ExtractInt32Value(entry);
        printf("[RX][IF%u] INT32 type=0x%02X val=%ld\n", (unsigned)interface, entry->type, (long)v);
        return true;
    }
    return false;
}

static bool on_float32_tlv(const tlv_entry_t *entry, tlv_interface_t interface)
{
    if (entry->length == 4) {
        uint32_t u = (uint32_t)entry->value[0] |
                     ((uint32_t)entry->value[1] << 8) |
                     ((uint32_t)entry->value[2] << 16) |
                     ((uint32_t)entry->value[3] << 24);
        union { uint32_t u; float f; } conv = { .u = u };
        printf("[RX][IF%u] F32  type=0x%02X val=%f\n", (unsigned)interface, entry->type, (double)conv.f);
        return true;
    }
    return false;
}

static bool on_string_tlv(const tlv_entry_t *entry, tlv_interface_t interface)
{
    /* Print up to length as string (not null-terminated) */
    printf("[RX][IF%u] STR  type=0x%02X len=%u: ", (unsigned)interface, entry->type, entry->length);
    for (uint8_t i = 0; i < entry->length; i++) putchar((int)entry->value[i]);
    putchar('\n');
    return true;
}

static bool on_scaled_tlv(const tlv_entry_t *entry, tlv_interface_t interface)
{
    /* 解析 ×10000 缩放的 int32 为 float */
    float val = TLV_ExtractFloatValue(entry);
    printf("[RX][IF%u] SCAL type=0x%02X val=%f\n", (unsigned)interface, entry->type, (double)val);
    return true;
}

static bool on_cmd_ping(uint8_t command, tlv_interface_t interface)
{
    printf("[RX][IF%u] CMD  0x%02X\n", (unsigned)interface, command);
    return true; /* handled */
}

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

int main(void)
{
    /* 打开串口（根据你的实际端口修改） */
    g_serial = serial_open("COM19", 115200);
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
    }
    if (g_serial) serial_close(g_serial);
    return 0;
}
