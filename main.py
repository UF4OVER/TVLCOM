import threading
import time

import serial
from PYTVLCOM import (Transport,
                      Dispatcher,
                      FrameDefine,
                      TLVParser,
                      createStringEntry,
                      createInt32Entry,
                      createCtrlCmdEntry,
                      createRawEntry, buildFrame)


class Transport:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 0.1):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        self.lock = threading.Lock()
        self.in_waiting = 2

    def send(self, data: bytes) -> int:
        with self.lock:
            try:
                return self.ser.write(data)
            except Exception:
                return -1

    def feed(self, size: int = 1) -> bytes:
        return self.ser.readall()

    def close(self):
        self.ser.close()


def pumpSerialOnce(transport: Transport, parser: TLVParser, max_bytes: int = 4096) -> int:
    try:
        n = int(getattr(transport.ser, "in_waiting", 0))  # 获取可读字节数
    except Exception:
        n = 0
    if n <= 0:
        return 0
    n = min(n, max_bytes)
    data = transport.feed(n)  # 非阻塞读取 n 字节
    for b in data:
        parser.process_byte(b)
    return len(data)


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
                tlv_buf += createStringEntry(s)
                desc_list.append(f"type=0x{FrameDefine.TLV_TYPE_STRING:02X} string=\"{s}\"")
            elif kind == 'int':
                v = random.randint(0, 0xFFFFFFFF)
                tlv_buf += createInt32Entry(FrameDefine.TLV_TYPE_INTEGER, v)
                desc_list.append(f"type=0x{FrameDefine.TLV_TYPE_INTEGER:02X} int32=0x{v:08X}({v})")
            elif kind == 'cmd':
                cmd = random.choice([1, 2])
                tlv_buf += createCtrlCmdEntry(cmd)
                desc_list.append(f"type=0x{FrameDefine.TLV_TYPE_CONTROL_CMD:02X} cmd=0x{cmd:02X}({cmd})")
            else:
                t = random.choice([0x40, 0x41, 0x50])
                if t == 0x40:
                    v = random.randint(0, 0xFFFFFFFF)
                    tlv_buf += createInt32Entry(t, v)
                    desc_list.append(f"type=0x{t:02X} int32=0x{v:08X}({v})")
                else:
                    data = bytes(random.getrandbits(8) for _ in range(random.randint(1, 8)))
                    tlv_buf += createRawEntry(t, data)
                    desc_list.append(f"type=0x{t:02X} raw={data.hex()} len={len(data)}")

        fid = frame_id & 0xFF
        frame = buildFrame(fid, bytes(tlv_buf))
        written = transport.send(frame)

        payload_len = len(tlv_buf)  # 即 DLEN
        print(f"sent frame_id={fid} payload_len={payload_len} tlv_count={count} "
              f"tlvs=[{' ; '.join(desc_list)}] frame_len={len(frame)} written={written}")

        frame_id += 1
        time.sleep(interval)


def main():
    transport = Transport(port='COM3', baud=115200, timeout=0.1)
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
        print('recv type 0x40:', v.hex())
        return True

    # 注册处理器
    dispatcher.RegisterTypeHandler(FrameDefine.TLV_TYPE_STRING, on_string)

    dispatcher.RegisterTypeHandler(0x41, on_type_41)
    dispatcher.RegisterTypeHandler(0x50, on_type_50)
    dispatcher.RegisterTypeHandler(0x40, on_type_01)

    dispatcher.RegisterCmdHandler(0x01, on_cmd_1)
    dispatcher.RegisterCmdHandler(0x02, on_cmd_2)
    # 创建解析器并链接 dispatcher
    parser = TLVParser(on_frame=dispatcher.handleFrame, on_error=lambda e: print('parse error:', e))

    sender_thread = threading.Thread(target=send_random_loop, args=(transport, 2.0), daemon=True)
    sender_thread.start()

    # 读取线程：不断读串口并逐字节推入解析器
    try:
        while True:
            processed = pumpSerialOnce(transport, parser)
            if processed == 0:
                time.sleep(0.001)  # 空闲时小睡，降低 CPU
    except KeyboardInterrupt:
        transport.close()


if __name__ == '__main__':
    main()
