# TVLCOM 详细文档：调试、日志与问题定位

## 1. 推荐的调试开关
- `TLV_DEBUG_ENABLE=1`：打印协议解析细节

建议：
- PC 端 demo 开启
- MCU 端默认关闭，只在定位问题时打开

## 2. 你会在日志里看到什么
### 2.1 发送侧
- frame_id、payload_len、tlv_count
- 每条 TLV 的 type/值
- frame_len
- written（底层写出的字节数）

### 2.2 接收侧
- `[FRAME id=... len=...]` + hex dump
- `[TLV type=... len=...]` + value hex
- `[RX]` 业务层打印（取决于是否注册 handler）
- `[ACK]/[NACK]` 对应 frame_id

## 3. 常见错误与定位
### 3.1 written=0（或 <0）
优先排查：
- 串口是否成功打开
- 句柄是否有效
- 是否被其它程序占用
- 写超时/配置错误

### 3.2 收到 NACK
优先排查：
- 对端的 handler 是否注册/支持该 TLV type
- DataLen 是否正确
- CRC 覆盖范围/初值是否一致

### 3.3 收到帧但没看到业务打印
原因常见是：
- 你只开启了协议 debug（会看到 TLV 拆包），但业务 handler 没有打印
- 你注册了 handler，但 handler 返回失败导致 NACK

建议：
- 在 handler 里统一打印 type、len、value
- 或在“帧完成回调”里 dump 整帧内容

