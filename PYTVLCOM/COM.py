# -*- coding: utf-8 -*-
# -------------------------------
#  @Project : TVLCOM
#  @Time    : 2025 - 11-18 09:56
#  @FileName: COM.py
#  @Software: PyCharm 2024.1.6 (Professional Edition)
#  @System  : Windows 11 23H2
#  @Author  : UF4
#  @Contact : 
#  @Python  : 
# -------------------------------
# [Header 2B: 0xF0 0x0F]
# [Frame ID 1B]
# [DataLen 1B]  // 后续 TLV 数据段总长度（不含 CRC/Tail）
# [Data: TLV1 + TLV2 + ...]
# [CRC16 2B]    // 对 FrameID + DataLen + Data 计算（CCITT 0x1021, init 0xFFFF）
# [Tail 2B: 0xE0 0x0D]
from enum import Enum, IntEnum
from typing import List, Dict, Callable, Optional

from PYTVLCOM.TR import Transport

HEADER = bytes([0xF0, 0x0F])
TAIL = bytes([0xE0, 0x0D])


class FrameDefine(IntEnum):
    TLV_TYPE_CONTROL_CMD = 0x01
    TLV_TYPE_INTEGER = 0x02
    TLV_TYPE_STRING = 0x03
    TLV_TYPE_ACK = 0x08
    TLV_TYPE_NACK = 0x09
    TLV_MAX_DATA_LENGTH = 240


class StateMachine(IntEnum):
    SYNC = 1
    FRAME_ID = 2
    DATALEN = 3
    DATA = 4
    CRC1 = 5
    CRC2 = 6
    TAIL1 = 7
    TAIL2 = 8


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
def createRawEntry(t: int, payload: bytes) -> bytes:
    assert len(payload) <= 0xFF
    return bytes([t, len(payload)]) + payload


def createInt32Entry(t: int, v: int) -> bytes:
    b = int.to_bytes(v & 0xFFFFFFFF, 4, 'little', signed=False)
    return createRawEntry(t, b)


def createStringEntry(s: str) -> bytes:
    b = s.encode('utf-8')
    return createRawEntry(FrameDefine.TLV_TYPE_STRING, b)


def createCtrlCmdEntry(cmd: int) -> bytes:
    return createRawEntry(FrameDefine.TLV_TYPE_CONTROL_CMD, bytes([cmd]))


# ----- 帧构建 -----
def buildFrame(frame_id: int, tlv_payload: bytes) -> bytes:
    assert 0 <= frame_id <= 0xFF
    assert len(tlv_payload) <= FrameDefine.TLV_MAX_DATA_LENGTH
    header = HEADER + bytes([frame_id, len(tlv_payload)])
    crc = crc16_ccitt(bytes([frame_id, len(tlv_payload)]) + tlv_payload)
    crc_bytes = crc.to_bytes(2, 'big')  # 网络字节序写入（README 使用 big? 若需 little 改此处）
    return header + tlv_payload + crc_bytes + TAIL


class Dispatcher:
    def __init__(self, transport: Transport):
        self.transport = transport
        self.type_handlers: Dict[int, Callable[[bytes], bool]] = {}
        self.cmd_handlers: Dict[int, Callable[[], bool]] = {}

    def RegisterTypeHandler(self, t: int, fn: Callable[[bytes], bool]):
        self.type_handlers[t] = fn

    def RegisterCmdHandler(self, cmd: int, fn: Callable[[], bool]):
        self.cmd_handlers[cmd] = fn

    def handleFrame(self, frame_id: int, entries: List[Dict]):
        # If frame contains only ACK/NACK, do not reply
        only_ack = all(e['type'] in (FrameDefine.TLV_TYPE_ACK, FrameDefine.TLV_TYPE_NACK) for e in entries) and len(
            entries) > 0
        success = True
        for e in entries:
            t = e['type']
            v = e['value']
            if t == FrameDefine.TLV_TYPE_CONTROL_CMD:
                cmd = v[0] if len(v) >= 1 else None
                if cmd is None or self.cmd_handlers.get(cmd, lambda: False)() is False:
                    success = False
            elif t in (FrameDefine.TLV_TYPE_ACK, FrameDefine.TLV_TYPE_NACK):
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
        tlv = createRawEntry(FrameDefine.TLV_TYPE_ACK, bytes([frame_id]))
        frame = buildFrame(0, tlv)  # 回包 frame_id 可约定为 0 或其他策略
        self.transport.send(frame)

    def _send_nack(self, frame_id: int):
        tlv = createRawEntry(FrameDefine.TLV_TYPE_NACK, bytes([frame_id]))
        frame = buildFrame(0, tlv)
        self.transport.send(frame)


