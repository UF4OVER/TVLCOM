# TVLCOM TLV 双向传输协议使用文档（含 STM32 指南）

本仓库提供一个轻量的 TLV（Type-Length-Value）双向数据传输协议实现，支持：
- 自定义类型、变长数据（整数、浮点、字符串、原始二进制）
- CRC16 校验，长度检查
- 收到有效帧回调分发
- 严格对等的 ACK/NACK（成功回 ACK，失败回 NACK），避免 ACK 风暴
- 可在多种接口上使用（UART/USB 等），通过传输层适配

目录结构（核心模块）
- `src/SoftwareAnalysis/S_TLV_PROTOCOL.[h/c]` 协议核心：帧格式、解析、CRC、TLV 构造、解析等
- `src/SoftwareAnalysis/S_RECEIVE_PROTOCOL.[h/c]` 接收分发层：注册回调、自动 ACK/NACK、错误处理
- `src/SoftwareAnalysis/S_TRANSPORT_PROTOCOL.[h/c]` 传输层：注册底层发送函数、统一发帧接口
- `src/Serial/` PC 端串口简单实现（STM32 上无需此目录）
- `src/main.c` 示例（PC 端 demo，可参考其使用方式）


## 1. 帧格式

```
[Header 2B: 0xF0 0x0F]
[Frame ID 1B]
[DataLen 1B]  // 后续 TLV 数据段总长度（不含 CRC/Tail）
[Data: TLV1 + TLV2 + ...]
[CRC16 2B]    // 对 FrameID + DataLen + Data 计算（CCITT 0x1021, init 0xFFFF）
[Tail 2B: 0xE0 0x0D]
```

TLV 单元格式：`[Type 1B][Length 1B][Value N bytes]`
- 协议不限制 Type 的语义，可自定义。
- 预定义了若干通用 Type：
  - `TLV_TYPE_CONTROL_CMD (0x10)` 控制命令，Value[0] 为命令字节
  - `TLV_TYPE_INTEGER (0x20)` 32 位整型（小端）
  - `TLV_TYPE_STRING  (0x30)` UTF-8 文本（不含结尾 0）
  - `TLV_TYPE_ACK     (0x06)` ACK
  - `TLV_TYPE_NACK    (0x15)` NACK

数据段最大长度默认 `TLV_MAX_DATA_LENGTH = 240`（可在 `S_TLV_PROTOCOL.h` 调整）。


## 2. 快速上手（通用）

1) 注册底层发送函数（按接口类型）
- 在你的平台初始化时：
```
Transport_RegisterSender(TLV_INTERFACE_UART, my_uart_send_func);
// 如果有 USB：Transport_RegisterSender(TLV_INTERFACE_USB, my_usb_send_func);
```
- 发送函数原型：`int (*transport_send_func_t)(const uint8_t *data, uint16_t len)`，返回发送字节数或 -1。

2) 初始化接收解析器，并注册回调
```
FloatReceive_Init(TLV_INTERFACE_UART);
FloatReceive_RegisterTLVHandler(TLV_TYPE_STRING, my_string_handler);
FloatReceive_RegisterCmdHandler(0x01 /*命令值*/, my_cmd_handler);
```
- TLV 类型回调原型：`bool (*tlv_type_handler_t)(const tlv_entry_t *entry, tlv_interface_t interface)`
- 命令回调原型：`bool (*cmd_handler_t)(uint8_t command, tlv_interface_t interface)`

3) 喂入接收到的字节
- 将串口/USB 收到的每个字节（或一段缓冲）逐个交给：`TLV_ProcessByte(tlv_parser_t *parser, uint8_t ch)`
- UART 解析器可通过 `FloatReceive_GetUARTParser()` 取到。

4) 发送数据帧
```
// 组装 TLV
tlv_entry_t tlvs[2];
const char *msg = "HELLO";
TLV_CreateRawEntry(TLV_TYPE_STRING, (const uint8_t*)msg, (uint8_t)strlen(msg), &tlvs[0]);
TLV_CreateControlCmdEntry(0x01 /*命令*/, &tlvs[1]);

// 帧 ID 可用工具函数分配
uint8_t frame_id = Transport_NextFrameId();
Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, tlvs, 2);
```

5) ACK/NACK 行为
- 当解析到一个完整有效的帧后，`S_RECEIVE_PROTOCOL` 会分发给回调：
  - 所有 TLV 都被成功处理 → 自动发送 `ACK(frame_id)`
  - 解析错误（CRC/长度）或处理失败 → 自动发送 `NACK(frame_id)`
- 收到仅包含 ACK/NACK 的帧不会再回包，避免 ACK 风暴。

注意：`TLV_ParseData` 解析出的 `tlv_entry_t.value` 指向解析器内部缓存，不要跨回调保存；若需长期保存请自行拷贝。


## 3. API 速览

核心计算
- `uint16_t TLV_CalculateCRC16(const uint8_t *data, uint16_t length)`

解析/构建
- `void TLV_InitParser(tlv_parser_t *p, tlv_interface_t ifc, tlv_frame_callback_t cb)`
- `void TLV_SetErrorCallback(tlv_parser_t *p, tlv_error_callback_t err_cb)`
- `void TLV_ProcessByte(tlv_parser_t *p, uint8_t ch)`
- `bool TLV_BuildFrame(uint8_t frame_id, const tlv_entry_t *entries, uint8_t count, uint8_t *out, uint16_t *out_size)`
- `uint8_t TLV_ParseData(const uint8_t *buf, uint8_t len, tlv_entry_t *out, uint8_t max)`

传输层
- `void Transport_RegisterSender(tlv_interface_t ifc, transport_send_func_t fn)`
- `int Transport_Send(tlv_interface_t ifc, const uint8_t *data, uint16_t len)`
- `bool Transport_SendTLVs(tlv_interface_t ifc, uint8_t frame_id, const tlv_entry_t *entries, uint8_t count)`
- `uint8_t Transport_NextFrameId(void)`

接收分发层
- `void FloatReceive_Init(tlv_interface_t ifc)`
- `tlv_parser_t* FloatReceive_GetUARTParser(void)` / `FloatReceive_GetUSBParser(void)`
- `void FloatReceive_RegisterTLVHandler(uint8_t type, tlv_type_handler_t handler)`
- `void FloatReceive_RegisterCmdHandler(uint8_t cmd, cmd_handler_t handler)`

TLV 便利构造
- `void TLV_CreateRawEntry(uint8_t type, const uint8_t *buf, uint8_t len, tlv_entry_t *e)`
- `void TLV_CreateInt32Entry(uint8_t type, int32_t value, tlv_entry_t *e)`（小端）
- `void TLV_CreateFloat32Entry(uint8_t type, float value, tlv_entry_t *e)`（IEEE-754 小端）
- `void TLV_CreateControlCmdEntry(uint8_t cmd, tlv_entry_t *e)`
- 以及 ×10000 缩放的四个示例：`TLV_CreateVoltageEntry / CurrentEntry / PowerEntry / TemperatureEntry`


## 4. STM32 集成指南（重点）

以下示例以 HAL 为例；LL 或裸寄存器思路一致。

### 4.1 实现底层发送函数并注册

在你的工程中（例如 `app_tlv.c`）实现 UART 发送封装：
```c
#include "S_TRANSPORT_PROTOCOL.h"
#include "usart.h"   // 你的 HAL UART 句柄

static int stm32_uart_send_impl(const uint8_t *data, uint16_t len)
{
    // 阻塞发送（可改为 DMA 发送）
    if (HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 100) == HAL_OK) {
        return (int)len;
    }
    return -1;
}

void TLV_PortInit(void)
{
    Transport_RegisterSender(TLV_INTERFACE_UART, stm32_uart_send_impl);
    FloatReceive_Init(TLV_INTERFACE_UART);
}
```
- 若使用 DMA 发送，建议在发送繁忙时返回 -1 或排队。

### 4.2 喂入接收数据（字节流）

常见两种接收方式：
- 中断逐字节：在 `HAL_UART_RxCpltCallback` 里将字节 `TLV_ProcessByte(FloatReceive_GetUARTParser(), ch);`
- DMA+IDLE：一次 DMA 收到一段，IDLE 中断触发时对本段缓冲逐字节调用 `TLV_ProcessByte`。

示例（DMA + IDLE）：
```c
// 假设有环形/线性 DMA 缓冲 rx_buf, 本次有效长度 rx_len
extern uint8_t rx_buf[];
extern size_t  rx_len;

void UART_IDLE_ReceiveHandler(void)
{
    tlv_parser_t *p = FloatReceive_GetUARTParser();
    for (size_t i = 0; i < rx_len; i++) {
        TLV_ProcessByte(p, rx_buf[i]);
    }
    // 重新启动 DMA / 复位索引
}
```
注意：
- 解析器不是线程安全对象；确保同一接口的数据从单一上下文推进（或加锁/消息队列）。
- 回调里避免重负荷操作；必要时仅设标志，延后处理。

### 4.3 发送一帧数据
```c
void TLV_SendDemo(void)
{
    tlv_entry_t tlvs[2];
    const char *msg = "STM32 HELLO";
    TLV_CreateRawEntry(TLV_TYPE_STRING, (const uint8_t*)msg, (uint8_t)strlen(msg), &tlvs[0]);
    TLV_CreateControlCmdEntry(0x01, &tlvs[1]);

    uint8_t frame_id = Transport_NextFrameId();
    (void)Transport_SendTLVs(TLV_INTERFACE_UART, frame_id, tlvs, 2);
}
```

### 4.4 注册回调（类型/命令）
```c
static bool on_str(const tlv_entry_t *e, tlv_interface_t ifc)
{
    // e->value 指向内部缓冲，仅当前回调有效
    // 做应用处理或复制
    return true; // 处理成功
}

static bool on_cmd(uint8_t cmd, tlv_interface_t ifc)
{
    switch (cmd) {
        case 0x01: /* do something */ return true;
        default: return false; // 未处理
    }
}

void TLV_AppInit(void)
{
    FloatReceive_RegisterTLVHandler(TLV_TYPE_STRING, on_str);
    FloatReceive_RegisterCmdHandler(0x01, on_cmd);
}
```
> 分发规则：所有 TLV 都被“成功处理”才会自动回 `ACK`，否则回 `NACK`。

### 4.5 资源与配置
- 最大帧数据段长度 `TLV_MAX_DATA_LENGTH` 默认为 240，可按带宽与内存需求调整。
- 若 MCU 不便使用浮点，可用 `TLV_CreateInt32Entry` 或本仓库提供的 ×10000 缩放函数。
- 端序：本实现按小端打包 int32/float；若对端不同端序，请约定统一格式。


## 5. 常见问题（FAQ）

1) 为什么会出现 ACK 风暴？
- 协议已避免：若收到的帧仅包含 ACK/NACK，将不会再回包。

2) 为什么我收不到 ACK？
- 检查是否正确 `Transport_RegisterSender()`；若底层发送失败，ACK 自然发不出去。
- 检查回调是否全部返回 `true`；有任何未处理/失败会导致 NACK。

3) TLV_ParseData 的 `value` 指针可否长期保存？
- 不可以。那是解析器内部缓冲的指针；若要跨回调使用，请立即拷贝。

4) 如何做超时与重发？
- 这是高层策略，可在发送后等待对端同 frame_id 的 ACK，超时则重发。可扩展传输层加入发送队列与定时器。

5) 可以扩展更多 TLV 类型吗？
- 可以，随意定义 `type` 值，并注册类型回调处理即可。


## 6. 在 PC 上尝试运行（可选）

本仓库已提供一个 PC demo（`src/main.c`），使用 CMake 构建。

- Windows（MinGW）下命令示例：
```cmd
cmake -S . -B cmake-build-debug-mingw -G "MinGW Makefiles"
cmake --build cmake-build-debug-mingw --config Debug --target TVLCOM
```
- 运行前请在 `src/main.c` 将串口号改为你的端口（>COM9 用 `\\.\\COM10` 格式）。

> 注意：PC 端串口模块仅用于 demo。STM32 上无需 `src/Serial`；你应当按 4.1 节注册自己的 HAL 发送函数。


## 7. 变更与扩展建议
- 如需区分 NACK 原因，可在 NACK 帧中再附带一个原因码 TLV（自定义 type）。
- 可引入发送窗口与 ACK 等待的重发机制（可靠性增强）。
- 可在 TLV 数据中加入时间戳与序列号，方便应用层追踪。


---
有任何接入问题或需要我补充 STM32 示例工程/代码片段，请提出具体外设与需求（串口号、是否 DMA、是否 RTOS），我可以直接按你的项目结构补齐代码。
