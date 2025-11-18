# TVLCOM 协议与使用说明

## 1. 项目简介
TVLCOM 提供一个基于自定义帧 + TLV 的轻量通信层: 可靠结构化负载, 简单扩展, 易于在串口 / TCP / 自定义链路上复用。

## 2. 数据帧格式
| 字段顺序 | 长度 | 描述 |
|----------|------|------|
| Header   | 2B   | 固定 0xF0 0x0F 用于同步 |
| FrameID  | 1B   | 业务帧编号 (由上层指定) |
| DataLen  | 1B   | 后续 TLV 数据区总字节长度 |
| Data     | N    | 若干 TLV 串接 |
| CRC16    | 2B   | 对 FrameID + DataLen + Data 做 CCITT(0x1021, init=0xFFFF) 结果, 大端写入 |
| Tail     | 2B   | 固定 0xE0 0x0D 结束标记 |

DataLen 最大 240 (见 FrameDefine.TLV_MAX_DATA_LENGTH)。

## 3. TLV 编码
单个 TLV: [Type 1B][Len 1B][Value LenB]
- Type 0x01: 控制命令 (Value: 1B 命令码)
- Type 0x02: 32位整型 (小端 4B)
- Type 0x03: UTF-8 字符串
- Type 0x08: ACK
- Type 0x09: NACK

可串接多个 TLV 构成 Data 部分。

## 4. CRC16-CCITT
参数: 多项式 0x1021, 初值 0xFFFF, 无最终异或, 输入逐字节, 每字节高位在前。当前实现将结果以大端序写入帧 (若需改为小端只需调整 buildFrame 中的 to_bytes(2, 'big'))。

## 5. 构建帧示例
```python
from PYTVLCOM import buildFrame, createCtrlCmdEntry, createStringEntry

cmd_tlv = createCtrlCmdEntry(0x10)
str_tlv = createStringEntry("hello")
payload = cmd_tlv + str_tlv
frame = buildFrame(0x01, payload)
transport.send(frame)
```

## 6. Dispatcher 使用
```python
from PYTVLCOM.COM import Dispatcher, FrameDefine, createInt32Entry, buildFrame

# 假设已有 transport 对象 (需实现 send(bytes))
dispatcher = Dispatcher(transport)

def on_string(value_bytes: bytes):
    print("STRING:", value_bytes.decode('utf-8'))
    return True

def on_cmd_0x10():
    print("CMD 0x10 handled")
    return True

dispatcher.RegisterTypeHandler(FrameDefine.TLV_TYPE_STRING, on_string)
dispatcher.RegisterCmdHandler(0x10, on_cmd_0x10)

# 发送一个包含命令 + 整数的帧
int_tlv = createInt32Entry(FrameDefine.TLV_TYPE_INTEGER, 123)
cmd_tlv = createCtrlCmdEntry(0x10)
out_frame = buildFrame(0x05, cmd_tlv + int_tlv)
transport.send(out_frame)
```
接收侧解析出 entries 后调用 dispatcher.handleFrame(frame_id, entries) 自动判断是否回 ACK / NACK。

ACK / NACK 规则: 若帧仅包含 ACK/NACK 不再次回复；否则按所有 TLV 处理结果汇总发送 ACK(成功) 或 NACK(失败)。

## 7. 状态机解析流程
StateMachine:
1 SYNC: 匹配 Header 2B
2 FRAME_ID: 读取 FrameID
3 DATALEN: 读取 TLV 总长度
4 DATA: 累积 TLV 区
5 CRC1/CRC2: 读取并比对校验
7/8 TAIL1/TAIL2: 校验尾部

错误处理建议: 任意阶段失败立即回退到 SYNC, 丢弃临时缓冲。

## 8. 扩展建议
- 增加 TLV_TYPE_BINARY / JSON / 压缩类型。
- 增加重发与超时管理 (结合 ACK/NACK)。
- 添加帧序号与窗口控制支持半可靠流。
- 允许可选的安全层 (HMAC 或加密)。
- CRC 改为可配置策略 (CRC16 / CRC32 / 无校验)。

## 9. 传输层 Transport
需提供 send(bytes) 方法；接收侧应在底层获得原始字节流并驱动状态机逐字节喂入，组帧成功后解析 TLV。

## 10. TLV 解析要点
遍历 Data:
pos=0
while pos < len(Data):
  t = Data[pos]
  l = Data[pos+1]
  v = Data[pos+2:pos+2+l]
  entries.append({'type': t, 'value': v})
  pos += 2 + l
需校验越界与长度一致。

## 11. 常见注意点
- FrameID 仅 1B, 上层如需更多需自行映射。
- 字符串建议 UTF-8 不含零截断；接收按原样解码。
- 整数使用小端序写入 (与 CRC 的大端区分开)。
- 超过最大长度直接拒绝构建。

## 12. 简要测试策略
- 构造不同 DataLen 边界 (0 / 240)。
- CRC 错误故意篡改 1B。
- 随机字节流同步丢失恢复测试。
- 多 TLV 串接顺序与解码完整性。

## 13. 许可证
GPLV3 许可证，详见 LICENSE 文件。
