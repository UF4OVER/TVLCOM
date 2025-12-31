# TVLCOM 详细文档：移植到 MCU（HAL 接入指南）

本文档聚焦“怎么把 TVLCOM 放到 MCU 上跑起来”。

## 目录
- [1. 目标与边界](#1-目标与边界)
- [2. 需要对接的接口](#2-需要对接的接口)
- [3. STM32 参考接入（轮询/中断/DMA）](#3-stm32-参考接入轮询中断dma)
- [4. 线程安全与缓冲区建议](#4-线程安全与缓冲区建议)
- [5. 常见坑](#5-常见坑)

---

## 1. 目标与边界
TVLCOM 协议层本质只需要两件事：
1) 能把一段 bytes 发出去
2) 能把收到的 bytes 按顺序喂给解析器

除了 IO，其余尽量保持平台无关。

---

## 2. 需要对接的接口
### 2.1 发送（必须）
你要提供一个函数：
- 输入：接口号 ifc（UART/USB）、数据指针、长度
- 输出：写出的字节数或错误码

然后注册到传输层：
- `Transport_RegisterSender(ifc, your_send_func)`

STM32 上 send_func 内部一般调用：
- 轮询：`HAL_UART_Transmit()`
- DMA：`HAL_UART_Transmit_DMA()`

### 2.2 接收（必须）
无论你用什么方式接收，最终都要：
- 按字节顺序调用 `TLV_ProcessByte(parser, byte)`

典型点位：
- UART RXNE 中断：每来 1 字节喂 1 次
- DMA 环形+IDLE：在 IDLE 回调里把新增数据段逐字节喂入

---

## 3. STM32 参考接入（轮询/中断/DMA）
### 3.1 轮询（最简单，吞吐低）
主循环里读 1 字节（阻塞/非阻塞）-> `TLV_ProcessByte()`

### 3.2 中断（推荐起步）
在 `HAL_UART_RxCpltCallback()` 里：
1) 取到 1 字节
2) `TLV_ProcessByte()`
3) 重新 `HAL_UART_Receive_IT()`

### 3.3 DMA+IDLE（高吞吐）
- DMA 连续收进环形缓冲
- IDLE 中断触发时计算“新进的数据区间”
- 逐字节喂入解析器（注意环回）

---

## 4. 线程安全与缓冲区建议
- 如果协议解析与发送在不同线程：
  - sender 内部要互斥 UART
  - parser 要保证同一时间只被一个上下文推进
- 不建议在 handler 里做长耗时操作。
- 若要异步处理：复制 `tlv_entry_t.value` 数据到你自己的 buffer。

---

## 5. 常见坑
- **串口中断里打印日志**：可能阻塞导致丢字节。
- **未注册 sender**：发送会失败（written=0 或 <0）。
- **长度超限**：多 TLV 拼起来超过 `TLV_MAX_DATA_LENGTH`。
- **大小端不一致**：int32/float 必须双方一致。

