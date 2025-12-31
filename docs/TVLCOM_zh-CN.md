# TVLCOM 文档（中文）

## 项目概述

TVLCOM 是一个轻量的通信层：使用**固定帧结构**与 **TLV（Type-Length-Value）** 负载编码。
它面向串口/字节流场景（PC ↔ MCU 等），同时传输层可插拔（不仅限于串口）。

本项目提供：

- 帧构建与解析（Header + FrameID + TLV 数据 + CRC16 + Tail）
- TLV 构造/解析工具
- 字节流接收解析状态机（逐字节喂入）
- 回调分发器（Dispatcher），支持 ACK/NACK 自动回包策略
- 可扩展的传输层抽象（Transport）和注册/工厂机制

## 模块说明

- `PYTVLCOM.COM`
  - 帧/TLV 常量（`HEADER`, `TAIL`, `FrameDefine`）
  - CRC16-CCITT（`crc16_ccitt`）
  - TLV 构造函数（`create_*_entry`）
  - 帧构造函数（`build_frame`）
  - 分发器（`Dispatcher`）：根据 TLV 类型/命令回调处理，并按策略回 ACK/NACK

- `PYTVLCOM.RE`
  - 字节流解析状态机（`TLVParser`）
  - TLV 解析工具（`parse_tlvs`, `TLVEntry`）

- `PYTVLCOM.TR`
  - 传输层抽象（`Transport`）
  - 传输层注册/工厂（`register_transport`, `create_transport`）
  - 串口传输实现（`SerialTransport`，基于 `pyserial`）

## 协议定义

### 帧格式（Frame）

帧结构如下：

- Header：2 字节，固定 `0xF0 0x0F`
- FrameId：1 字节
- DataLen：1 字节（TLV 负载总长度）
- Data：N 字节（多个 TLV 直接拼接）
- CRC16：2 字节，大端序（big-endian），计算范围为 `FrameId + DataLen + Data`
- Tail：2 字节，固定 `0xE0 0x0D`

### TLV 格式

TLV 编码格式：

`[Type:1][Len:1][Value:Len]`

其中：

- Type：1 字节，表示数据类型
- Len：1 字节，表示 Value 的长度（0~255）
- Value：变长数据

### 内置 TLV 类型

- `0x01`：控制命令（control command），Value 为 1 字节命令码
- `0x02`：整数（integer），Value 为 4 字节无符号整数，小端序（little-endian）
- `0x03`：字符串（string），Value 为 UTF-8 编码字节串
- `0x08`：ACK，Value 为 1 字节：被确认的原始 `FrameId`
- `0x09`：NACK，Value 为 1 字节：被否认的原始 `FrameId`

## 使用方法

示例脚本见：`main.py`（串口 demo）。

一般使用流程：

1. 创建 Transport（例如串口）：`create_transport("serial", ...)`
2. 创建 Dispatcher，并注册 TLV/命令回调
3. 创建 TLVParser，接收到字节后逐字节调用 `process_byte()`
4. 发送时：先用 `create_*_entry()` 拼 TLV，再用 `build_frame()` 生成帧

> 详细 API 以代码 docstring 为准，也可参考 `docs/API.md`。

## Windows 串口注意事项

- 当端口号 ≥ `COM10` 时，Windows 有时需要使用 `\\\\.\\COM10` 的形式。
- 串口可能被独占占用：确保没有其它串口工具（串口调试助手/IDE 监视器）正在占用。

## 性能建议

- 当前 `crc16_ccitt()` 为逐 bit 计算；若需要更高吞吐，可改为 table-driven（查表）CRC。
- `TLVParser.process_byte()` 为逐字节处理；高数据率场景可新增一个
  `process_bytes(data: bytes)` 封装，内部循环喂入，以减少 Python 调用开销。

