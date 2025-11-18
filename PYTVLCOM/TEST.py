# -*- coding: utf-8 -*-
# -------------------------------
#  @Project : TVLCOM
#  @Time    : 2025 - 11-18 10:56
#  @FileName: TEST.py
#  @Software: PyCharm 2024.1.6 (Professional Edition)
#  @System  : Windows 11 23H2
#  @Author  : UF4
#  @Contact : 
#  @Python  : 
# -------------------------------
# python
import threading
import serial
import time
from typing import Callable, Dict, List, Optional

# ----- 常量 -----
HEADER = bytes([0xF0, 0x0F])
TAIL = bytes([0xE0, 0x0D])
TLV_TYPE_CONTROL_CMD = 0x10
TLV_TYPE_INTEGER = 0x20
TLV_TYPE_STRING = 0x30
TLV_TYPE_ACK = 0x06
TLV_TYPE_NACK = 0x15
TLV_MAX_DATA_LENGTH = 240


# ----- CRC16 CCITT (init 0xFFFF, poly 0x1021) -----
def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    crc = init
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) & 0xFFFF) ^ 0x1021
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


# ----- TLV 帮助构造 -----
def create_raw_entry(t: int, payload: bytes) -> bytes:
    assert len(payload) <= 0xFF
    return bytes([t, len(payload)]) + payload


def create_int32_entry(t: int, v: int) -> bytes:
    b = int.to_bytes(v & 0xFFFFFFFF, 4, 'little', signed=False)
    return create_raw_entry(t, b)


def create_string_entry(s: str) -> bytes:
    b = s.encode('utf-8')
    return create_raw_entry(TLV_TYPE_STRING, b)


def create_ctrl_cmd_entry(cmd: int) -> bytes:
    return create_raw_entry(TLV_TYPE_CONTROL_CMD, bytes([cmd]))


# ----- 帧构建 -----
def build_frame(frame_id: int, tlv_payload: bytes) -> bytes:
    assert 0 <= frame_id <= 0xFF
    assert len(tlv_payload) <= TLV_MAX_DATA_LENGTH
    header = HEADER + bytes([frame_id, len(tlv_payload)])
    crc = crc16_ccitt(bytes([frame_id, len(tlv_payload)]) + tlv_payload)
    crc_bytes = crc.to_bytes(2, 'big')  # 网络字节序写入（README 使用 big? 若需 little 改此处）
    return header + tlv_payload + crc_bytes + TAIL


# ----- 逐字节解析器 -----
class TLVParser:
    def __init__(self, on_frame: Callable[[int, List[Dict]], None], on_error: Optional[Callable[[str], None]] = None):
        self.on_frame = on_frame
        self.on_error = on_error
        self.reset()

    def reset(self):
        self.state = 'sync'
        self.buf = bytearray()
        self.frame_id = 0
        self.dlen = 0
        self.data = bytearray()
        self.sync_idx = 0

    def process_byte(self, ch: int):
        # 状态机：sync -> hdr_frameid -> dlen -> data -> crc_hi -> crc_lo -> tail
        if self.state == 'sync':
            # match HEADER
            if self.sync_idx == 0 and ch == HEADER[0]:
                self.sync_idx = 1
            elif self.sync_idx == 1 and ch == HEADER[1]:
                self.state = 'frameid'
                self.sync_idx = 0
            else:
                self.sync_idx = 1 if ch == HEADER[0] else 0
        elif self.state == 'frameid':
            self.frame_id = ch
            self.state = 'dlen'
        elif self.state == 'dlen':
            self.dlen = ch
            if self.dlen > TLV_MAX_DATA_LENGTH:
                self._error('data length too large')
                self.reset()
            elif self.dlen == 0:
                self.state = 'crc_hi'
                self.data = bytearray()
            else:
                self.data = bytearray()
                self.remaining = self.dlen
                self.state = 'data'
        elif self.state == 'data':
            self.data.append(ch)
            self.remaining -= 1
            if self.remaining == 0:
                self.state = 'crc_hi'
        elif self.state == 'crc_hi':
            self.crc_hi = ch
            self.state = 'crc_lo'
        elif self.state == 'crc_lo':
            self.crc_lo = ch
            self.calced_crc = crc16_ccitt(bytes([self.frame_id, self.dlen]) + bytes(self.data))
            recv_crc = (self.crc_hi << 8) | self.crc_lo
            if recv_crc != self.calced_crc:
                self._error('crc mismatch')
                self.reset()
            else:
                self.state = 'tail0'
        elif self.state == 'tail0':
            if ch == TAIL[0]:
                self.state = 'tail1'
            else:
                self._error('bad tail')
                self.reset()
        elif self.state == 'tail1':
            if ch == TAIL[1]:
                # parse TLVs
                entries = self._parse_tlvs(bytes(self.data))
                try:
                    self.on_frame(self.frame_id, entries)
                except Exception as e:
                    if self.on_error:
                        self.on_error(str(e))
                self.reset()
            else:
                self._error('bad tail2')
                self.reset()

    def _parse_tlvs(self, buf: bytes) -> List[Dict]:
        out = []
        i = 0
        while i + 2 <= len(buf):
            t = buf[i];
            l = buf[i + 1];
            i += 2
            if i + l > len(buf):
                raise ValueError('TLV length overflow')
            v = buf[i:i + l];
            i += l
            out.append({'type': t, 'len': l, 'value': v})
        return out

    def _error(self, msg: str):
        if self.on_error:
            self.on_error(msg)


# ----- 串口传输封装 -----
class Transport:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 0.1):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.lock = threading.Lock()

    def send(self, data: bytes) -> int:
        with self.lock:
            try:
                return self.ser.write(data)
            except Exception:
                return -1

    def read_bytes(self, size: int = 1) -> bytes:
        return self.ser.read(size)

    def close(self):
        self.ser.close()


# ----- 接收分发与 ACK/NACK 逻辑 -----
class Dispatcher:
    def __init__(self, transport: Transport):
        self.transport = transport
        self.type_handlers: Dict[int, Callable[[bytes], bool]] = {}
        self.cmd_handlers: Dict[int, Callable[[], bool]] = {}

    def register_type_handler(self, t: int, fn: Callable[[bytes], bool]):
        self.type_handlers[t] = fn

    def register_cmd_handler(self, cmd: int, fn: Callable[[], bool]):
        self.cmd_handlers[cmd] = fn

    def handle_frame(self, frame_id: int, entries: List[Dict]):
        # If frame contains only ACK/NACK, do not reply
        only_ack = all(e['type'] in (TLV_TYPE_ACK, TLV_TYPE_NACK) for e in entries) and len(entries) > 0
        success = True
        for e in entries:
            t = e['type'];
            v = e['value']
            if t == TLV_TYPE_CONTROL_CMD:
                cmd = v[0] if len(v) >= 1 else None
                if cmd is None or self.cmd_handlers.get(cmd, lambda: False)() is False:
                    success = False
            elif t in (TLV_TYPE_ACK, TLV_TYPE_NACK):
                # application-level: maybe notify waiting senders
                pass
            else:
                h = self.type_handlers.get(t)
                if h:
                    if not h(v):
                        success = False
                else:
                    success = False
        if not only_ack:
            if success:
                self._send_ack(frame_id)
            else:
                self._send_nack(frame_id)

    def _send_ack(self, frame_id: int):
        tlv = create_raw_entry(TLV_TYPE_ACK, bytes([frame_id]))
        frame = build_frame(0, tlv)  # 回包 frame_id 可约定为 0 或其他策略
        self.transport.send(frame)

    def _send_nack(self, frame_id: int):
        tlv = create_raw_entry(TLV_TYPE_NACK, bytes([frame_id]))
        frame = build_frame(0, tlv)
        self.transport.send(frame)


import random
import time


# python
def send_random_loop(transport: Transport, interval: float = 2.0):
    import random, time
    frame_id = 1
    while True:
        tlv_buf = bytearray()
        desc_list = []

        count = random.randint(1, 3)
        for _ in range(count):
            kind = random.choice(['string', 'int', 'cmd', 'custom'])
            if kind == 'string':
                s = ''.join(random.choices('abcdefghijklmnopqrstuvwxyz0123456789', k=random.randint(3, 12)))
                tlv_buf += create_string_entry(s)
                desc_list.append(f"type=0x{TLV_TYPE_STRING:02X} string=\"{s}\"")
            elif kind == 'int':
                v = random.randint(0, 0xFFFFFFFF)
                tlv_buf += create_int32_entry(TLV_TYPE_INTEGER, v)
                desc_list.append(f"type=0x{TLV_TYPE_INTEGER:02X} int32=0x{v:08X}({v})")
            elif kind == 'cmd':
                cmd = random.choice([1, 2])
                tlv_buf += create_ctrl_cmd_entry(cmd)
                desc_list.append(f"type=0x{TLV_TYPE_CONTROL_CMD:02X} cmd=0x{cmd:02X}({cmd})")
            else:
                t = random.choice([0x40, 0x41, 0x50])
                if t == 0x40:
                    v = random.randint(0, 0xFFFFFFFF)
                    tlv_buf += create_int32_entry(t, v)
                    desc_list.append(f"type=0x{t:02X} int32=0x{v:08X}({v})")
                else:
                    data = bytes(random.getrandbits(8) for _ in range(random.randint(1, 8)))
                    tlv_buf += create_raw_entry(t, data)
                    desc_list.append(f"type=0x{t:02X} raw={data.hex()} len={len(data)}")

        fid = frame_id & 0xFF
        frame = build_frame(fid, bytes(tlv_buf))
        written = transport.send(frame)

        payload_len = len(tlv_buf)  # 即 DLEN
        print(f"sent frame_id={fid} payload_len={payload_len} tlv_count={count} "
              f"tlvs=[{' ; '.join(desc_list)}] frame_len={len(frame)} written={written}")

        frame_id += 1
        time.sleep(interval)


# ----- 示例：主程序 -----
def main():
    # Windows: COM port like 'COM3' 或 r'\\.\COM10'
    port = r'COM3'  # <- 修改为实际端口
    transport = Transport(port, 115200)
    dispatcher = Dispatcher(transport)

    # 注册示例处理函数
    def on_string(v: bytes) -> bool:
        print('recv string:', v.decode('utf-8', errors='ignore'))
        return True

    def on_cmd_1() -> bool:
        print('cmd 1 received')
        return True

    def on_cmd_2() -> bool:
        print('cmd 2 received')
        return True

    def on_type_41(v: bytes) -> bool:
        print('recv type 0x41:', v.hex())
        return True

    def on_type_50(v: bytes) -> bool:
        print('recv type 0x50:', v.hex())
        return True

    def on_type_01(v: bytes) -> bool:
        print('recv type 0x01:', v.hex())
        return True

    dispatcher.register_type_handler(TLV_TYPE_STRING, on_string)
    dispatcher.register_cmd_handler(0x01, on_cmd_1)
    dispatcher.register_cmd_handler(0x02, on_cmd_2)
    dispatcher.register_type_handler(0x41, on_type_41)
    dispatcher.register_type_handler(0x50, on_type_50)
    dispatcher.register_type_handler(0x40, on_type_01)

    # 创建解析器并链接 dispatcher
    parser = TLVParser(on_frame=dispatcher.handle_frame, on_error=lambda e: print('parse error:', e))

    # 读取线程：不断读串口并逐字节推入解析器
    def read_loop():
        while True:
            b = transport.read_bytes(1)
            if not b:
                time.sleep(0.001)
                continue
            parser.process_byte(b[0])

    t = threading.Thread(target=read_loop, daemon=True)
    t.start()

    sender_thread = threading.Thread(target=send_random_loop, args=(transport, 2.0), daemon=True)
    sender_thread.start()
    # 保持运行
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        transport.close()


if __name__ == '__main__':
    main()
