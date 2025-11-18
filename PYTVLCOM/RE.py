# -*- coding: utf-8 -*-
# -------------------------------
#  @Project : TVLCOM
#  @Time    : 2025 - 11-18 09:55
#  @FileName: RE.py
#  @Software: PyCharm 2024.1.6 (Professional Edition)
#  @System  : Windows 11 23H2
#  @Author  : UF4
#  @Contact : 
#  @Python  : 
# -------------------------------
from typing import Callable, List, Optional, Dict

from PYTVLCOM import Transport
from PYTVLCOM.COM import StateMachine, crc16_ccitt, TAIL, FrameDefine, HEADER


class TLVParser:
    def __init__(self, on_frame: Callable[[int, List[Dict]], None], on_error: Optional[Callable[[str], None]] = None):
        self.on_frame = on_frame
        self.on_error = on_error

        self._crc = None
        self.crc_lo = None
        self.crc_hi = None
        self.state = StateMachine.SYNC
        self.buf = bytearray()
        self.frame_id = 0XFF
        self.dlen = 0
        self.data = bytearray()
        self.sync_idx = 0
        self.remaining = None

    def process_byte(self, ch: int):
        # 状态机：sync -> hdr_frameid -> dlen -> data -> crc_hi -> crc_lo -> tail
        if self.state == StateMachine.SYNC:
            # match HEADER
            if self.sync_idx == 0 and ch == HEADER[0]:
                self.sync_idx = 1
            elif self.sync_idx == 1 and ch == HEADER[1]:
                self.state = StateMachine.FRAME_ID
                self.sync_idx = 0
            else:
                self.sync_idx = 1 if ch == HEADER[0] else 0
        elif self.state == StateMachine.FRAME_ID:
            self.frame_id = ch
            self.state = StateMachine.DATALEN
        elif self.state == StateMachine.DATALEN:
            self.dlen = ch
            if self.dlen > FrameDefine.TLV_MAX_DATA_LENGTH:
                self._error('data length too large')
                self._reset()
            elif self.dlen == 0:
                self.state = StateMachine.CRC1
                self.data = bytearray()
            else:
                self.data = bytearray()
                self.remaining = self.dlen
                self.state = StateMachine.DATA
        elif self.state == StateMachine.DATA:
            self.data.append(ch)
            self.remaining -= 1
            if self.remaining == 0:
                self.state = StateMachine.CRC1
        elif self.state == StateMachine.CRC1:
            self.crc_hi = ch
            self.state = StateMachine.CRC2
        elif self.state == StateMachine.CRC2:
            self.crc_lo = ch
            self._crc = crc16_ccitt(bytes([self.frame_id, self.dlen]) + bytes(self.data))
            recv_crc = (self.crc_hi << 8) | self.crc_lo
            if recv_crc != self._crc:
                self._error('crc mismatch')
                self._reset()
            else:
                self.state = StateMachine.TAIL1
        elif self.state == StateMachine.TAIL1:
            if ch == TAIL[0]:
                self.state = StateMachine.TAIL2
            else:
                self._error('bad tail')
                self._reset()
        elif self.state == StateMachine.TAIL2:
            if ch == TAIL[1]:
                # parse TLVs
                entries = self._parse_tlvs(bytes(self.data))
                try:
                    self.on_frame(self.frame_id, entries)
                except Exception as e:
                    if self.on_error:
                        self.on_error(str(e))
                self._reset()
            else:
                self._error('bad tail2')
                self._reset()

    def _reset(self):
        self.state = StateMachine.SYNC
        self.buf = bytearray()
        self.frame_id = 0
        self.dlen = 0
        self.data = bytearray()
        self.sync_idx = 0

    def _parse_tlvs(self, buf: bytes) -> List[Dict]:
        out = []
        i = 0
        while i + 2 <= len(buf):
            t = buf[i]
            l = buf[i + 1]
            i += 2
            if i + l > len(buf):
                raise ValueError('TLV length overflow')
            v = buf[i:i + l]
            i += l
            out.append({'type': t, 'len': l, 'value': v})
        return out

    def _error(self, msg: str):
        if self.on_error:
            self.on_error(msg)

