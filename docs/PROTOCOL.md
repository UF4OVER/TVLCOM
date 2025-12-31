# TVLCOM 详细文档：协议、移植与调试

> 本文档是对 `README.md` 的补充，包含更完整的协议细节、收发流程、移植指南、调试方法与常见问题。

## 目录
- [1. 协议设计目标](#1-协议设计目标)
- [2. 协议帧格式](#2-协议帧格式)
- [3. CRC16 计算规则](#3-crc16-计算规则)
- [4. TLV 编码规则](#4-tlv-编码规则)
- [5. ACK/NACK 机制](#5-acknack-机制)
- [6. 发送侧流程（TX）](#6-发送侧流程tx)
- [7. 接收侧流程（RX）](#7-接收侧流程rx)
- [8. 回调与分发模型](#8-回调与分发模型)
- [9. 平台移植指南（HAL 层）](#9-平台移植指南hal-层)
- [10. 调试与抓包建议](#10-调试与抓包建议)
- [11. 常见问题（FAQ）](#11-常见问题faq)

---

## 1. 协议设计目标
TVLCOM 面向 **UART/USB CDC** 这类“字节流”链路：
- 不保证分包对齐（一次 read 可能拿到半帧 / 多帧）
- 可能丢字节/插入噪声
- 需要明确的帧边界与校验

因此协议采用：
- 固定头尾（快速重同步）
- `DataLen`（快速跳过）
- `CRC16`（检测误码）
- TLV（类型可扩展、强自描述）

---

## 2. 协议帧格式
整体帧格式如下（字节序：**小端** 用于多字节 Value；帧字段本身按顺序写入）：

```
[Header 2B]  : 0xF0 0x0F
[FrameID 1B] : 0x00..0xFF
[DataLen 1B] : TLV 数据段总长度 N (0..TLV_MAX_DATA_LENGTH)
[Data N B]   : TLV1 + TLV2 + ...
[CRC16 2B]   : 对 FrameID + DataLen + Data 做 CRC16-CCITT
[Tail 2B]    : 0xE0 0x0D
```

### 2.1 FrameID
- 用于把 ACK/NACK 与某次发送关联起来。
- 推荐使用 `Transport_NextFrameId()` 自动递增（回环到 0）。

### 2.2 DataLen
- 表示后续 TLV 数据段总长度。
- 接收端会用它做边界判断，防止越界。

---

## 3. CRC16 计算规则
- 算法：CRC16-CCITT
- 多项式：0x1021
- 初值：0xFFFF
- 输入：从 `FrameID` 开始到 `Data` 结束（**不含 Header/Tail**，不含 CRC 字段本身）

> 若你要与外部设备互联，最容易出错的就是 CRC 的“覆盖范围”和“初值”，建议双方写一个相同的测试向量对齐。

---

## 4. TLV 编码规则
每个 TLV：

```
[Type 1B][Len 1B][Value Len bytes]
```

- `Type`：业务类型或控制类型
- `Len`：Value 的字节数
- `Value`：原始数据

### 4.1 常用类型（以源码定义为准）
- `0x01` 控制命令（`Len=1`，`Value[0]=cmd`）
- `0x02` int32（`Len=4`，小端）
- `0x03` string（`Len=字符串长度`，不强制 \0 结尾）
- `0x08` ACK（通常 `Len=1`，携带被确认的 FrameID）
- `0x09` NACK（通常 `Len=1`，携带被拒绝的 FrameID）

### 4.2 字节序
- int32/float 等多字节 value：**小端**。
- 字符串：UTF-8，长度以 `Len` 为准。

---

## 5. ACK/NACK 机制
接收分发层通常遵循：
- 如果一帧里的 TLV 都成功处理（有 handler 且返回成功） ⇒ 回 ACK
- 如果存在未知类型、handler 返回失败、解析失败等 ⇒ 回 NACK

### 5.1 防 ACK 风暴
当收到的帧本身是 ACK 或 NACK 时：
- **不要再回复 ACK**
- 否则双方会互相确认形成“ACK 风暴”。

这就是你在日志里看到：
- 收到 `[TLV type=0x09]` 后打印 `[NACK] for frame ...`
- 但不会再因这个 NACK 回 NACK。

---

## 6. 发送侧流程（TX）
典型调用链（以实现为准）：
1. 上层准备 `tlv_entry_t entries[]`
2. `frame_id = Transport_NextFrameId()`
3. `Transport_SendTLVs(ifc, frame_id, entries, count)`
4. 内部调用 `TLV_BuildFrame()` 组帧、计算 CRC
5. 通过 `Transport_Send()` 调用已注册的 sender 写到底层串口/USB

注意点：
- `TLV_BuildFrame()` 可能因为长度超限返回失败
- sender 未注册会导致发送失败

---

## 7. 接收侧流程（RX）
关键点：**按字节喂入解析器**。

1. 底层收到字节（中断/DMA/串口 read）
2. 逐字节调用 `TLV_ProcessByte(parser, byte)`
3. 解析器内部完成：找头、累加长度、校验 CRC、找尾
4. 当一帧完整时回调 `FloatReceive_FrameCallback()`
5. `TLV_ParseData()` 把 Data 区拆成 `entries[]`
6. 分发到 type/cmd handler
7. 根据处理结果自动 ACK/NACK

---

## 8. 回调与分发模型
通常有两层：
- TLV Type handler：处理某个 `type`，例如 int32 / raw / string
- Cmd handler：当 `type=CMD` 时进一步根据 cmd 分发

### 8.1 生命周期与拷贝
常见实现会让 `tlv_entry_t.value` 指向解析器内部缓存。
- **只在当前回调期间有效**
- 若要异步处理或保存，必须复制出一份。

---

## 9. 平台移植指南（HAL 层）
目标：上层协议代码不改，替换底层 IO。

### 9.1 你需要实现/对接的点
1) **发送**：实现 `transport_send_func_t` 并注册
- STM32：用 `HAL_UART_Transmit()` / DMA
- RTOS：注意线程安全与互斥

2) **接收**：在合适的地方逐字节喂解析器
- UART IRQ 单字节接收
- DMA 环形缓冲 + IDLE 中断批量取出

3) **时间/日志（可选）**：用于调试打印

### 9.2 HAL 设计建议
- 在 `src/HAL/hal_platform.h` 做统一抽象
- 平台区分用 `#if defined(TVLCOM_PLATFORM_STM32)` / `TVLCOM_PLATFORM_WINDOWS`

---

## 10. 调试与抓包建议
### 10.1 打开调试日志
- 开启 `TLV_DEBUG_ENABLE=1` 可以打印：
  - 每帧原始字节
  - 每条 TLV 的 type/len/value
  - ACK/NACK 决策

### 10.2 抓包
- Windows：串口调试助手/逻辑分析仪/USB 抓包工具
- MCU：建议在发送前后打印 frame_id、长度、CRC

### 10.3 常见现象
- `written=0`：通常是串口写失败（端口断开/句柄无效/未打开）
- 收到 NACK：对端无法解析、长度超限、或 handler 未注册导致“未处理”

---

## 11. 常见问题（FAQ）
### Q1：为什么我能看到帧 bytes，但上层没打印“业务数据”？
A：协议栈能解析帧不代表你的业务 handler 已注册。
- 如果只开启了 debug，可能只打印了解析过程。
- 需要注册对应 type/cmd 的 handler 并在 handler 中打印/处理。

### Q2：这是库的问题吗？影响大吗？
A：多数情况下不是“库坏了”，而是 **ACK/NACK 机制与 handler 注册策略** 的预期行为。
- 影响：不会影响解析正确性，但会影响你是否能看到“业务层打印”。
- 解决：补齐 handler、或在 FrameCallback 里统一 dump。

### Q3：怎么验证 CRC 与对端一致？
A：固定一帧内容，双方计算 CRC 并对比；或者打印 CRC 输入区间的 hex dump。

