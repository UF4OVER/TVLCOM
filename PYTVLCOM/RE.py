"""PYTVLCOM.RE

This module implements the streaming receiver/parser for TVLCOM frames.

- `TLVParser` is a byte-by-byte state machine that synchronizes on HEADER,
  validates length/CRC/tail, and emits parsed TLV entries.
- `parse_tlvs` parses a TLV payload buffer into Python objects.

The parser is designed for serial-like streams where data arrives incrementally.
"""

from __future__ import annotations

from typing import Callable, List, Optional, Sequence

from PYTVLCOM.COM import (
    FrameDefine,
    HEADER,
    TAIL,
    StateMachine,
    TLVEntry,
    _CRC16_TABLE,
)


def parse_tlvs(buf: bytes) -> List[TLVEntry]:
    """Parse a TLV payload buffer.

    Args:
        buf: Raw TLV bytes, which should be exactly `DataLen` bytes.

    Returns:
        A list of TLVEntry items.

    Raises:
        ValueError: If the buffer is truncated or contains a malformed TLV.
    """

    entries: List[TLVEntry] = []
    mv = memoryview(buf)
    n = len(mv)
    i = 0
    while i < n:
        if i + 2 > n:
            raise ValueError("truncated TLV header")
        tlv_type = mv[i]
        length = mv[i + 1]
        i += 2
        end = i + int(length)
        if end > n:
            raise ValueError("TLV length overflow")
        # bytes(...) copies just the TLV value (no extra slicing bytes objects).
        entries.append(TLVEntry(type=int(tlv_type), value=bytes(mv[i:end])))
        i = end
    return entries


class TLVParser:
    """Stream parser for TVLCOM frames.

    The parser consumes bytes one at a time and calls `on_frame` when a full
    frame is received and validated.

    Args:
        on_frame: Callback invoked as `on_frame(frame_id, entries)`.
        on_error: Optional callback invoked with a human-readable message.

    Notes:
        - CRC16 is verified over `FrameId + DataLen + Data`.
        - When an error occurs, the parser resets to SYNC state.
    """

    def __init__(
        self,
        on_frame: Callable[[int, Sequence[TLVEntry]], None],
        on_error: Optional[Callable[[str], None]] = None,
        *,
        legacy_dict_entries: bool = False,
    ):
        self.on_frame = on_frame
        self.on_error = on_error
        self.legacy_dict_entries = legacy_dict_entries

        self.state = StateMachine.SYNC
        self.sync_idx = 0

        self.frame_id = 0
        self.dlen = 0
        self.data = bytearray()
        self.remaining: Optional[int] = None

        self._crc_hi: Optional[int] = None
        self._crc_value: int = 0xFFFF

    @staticmethod
    def _crc16_update_byte(value: int, b: int) -> int:
        """Update CRC16-CCITT with a single byte (no allocations)."""

        value &= 0xFFFF
        b &= 0xFF
        return ((value << 8) ^ _CRC16_TABLE[((value >> 8) ^ b) & 0xFF]) & 0xFFFF

    def process_byte(self, ch: int) -> None:
        """Feed one byte into the state machine.

        Args:
            ch: A single byte value (0..255).

        Returns:
            None.
        """

        ch = int(ch) & 0xFF

        if self.state == StateMachine.SYNC:
            # Match HEADER (2 bytes).
            if self.sync_idx == 0 and ch == HEADER[0]:
                self.sync_idx = 1
                return
            if self.sync_idx == 1 and ch == HEADER[1]:
                self.state = StateMachine.FRAME_ID
                self.sync_idx = 0
                return
            self.sync_idx = 1 if ch == HEADER[0] else 0
            return

        if self.state == StateMachine.FRAME_ID:
            self.frame_id = ch
            self._crc_value = 0xFFFF
            self._crc_value = self._crc16_update_byte(self._crc_value, self.frame_id)
            self.state = StateMachine.DATALEN
            return

        if self.state == StateMachine.DATALEN:
            self.dlen = ch
            if self.dlen > int(FrameDefine.TLV_MAX_DATA_LENGTH):
                self._error("data length too large")
                self._reset()
                return

            self.data = bytearray()
            self._crc_value = self._crc16_update_byte(self._crc_value, self.dlen)

            if self.dlen == 0:
                self.state = StateMachine.CRC1
            else:
                self.remaining = self.dlen
                self.state = StateMachine.DATA
            return

        if self.state == StateMachine.DATA:
            self.data.append(ch)
            self._crc_value = self._crc16_update_byte(self._crc_value, ch)
            assert self.remaining is not None
            self.remaining -= 1
            if self.remaining == 0:
                self.state = StateMachine.CRC1
            return

        if self.state == StateMachine.CRC1:
            self._crc_hi = ch
            self.state = StateMachine.CRC2
            return

        if self.state == StateMachine.CRC2:
            if self._crc_hi is None:
                self._error("internal crc state error")
                self._reset()
                return

            recv_crc = (self._crc_hi << 8) | ch
            # Prefer incremental path to avoid building `bytes([fid,dlen]) + data`.
            calc_crc = self._crc_value
            if recv_crc != calc_crc:
                self._error("crc mismatch")
                self._reset()
                return

            self.state = StateMachine.TAIL1
            return

        if self.state == StateMachine.TAIL1:
            if ch != TAIL[0]:
                self._error("bad tail")
                self._reset()
                return
            self.state = StateMachine.TAIL2
            return

        if self.state == StateMachine.TAIL2:
            if ch != TAIL[1]:
                self._error("bad tail")
                self._reset()
                return

            try:
                entries = parse_tlvs(bytes(self.data))
                if self.legacy_dict_entries:
                    legacy = [
                        {"type": e.type, "len": len(e.value), "value": e.value}
                        for e in entries
                    ]
                    self.on_frame(self.frame_id, legacy)  # type: ignore[arg-type]
                else:
                    self.on_frame(self.frame_id, entries)
            except Exception as exc:
                self._error(str(exc))
            finally:
                self._reset()
            return

    def _reset(self) -> None:
        self.state = StateMachine.SYNC
        self.sync_idx = 0

        self.frame_id = 0
        self.dlen = 0
        self.data = bytearray()
        self.remaining = None
        self._crc_hi = None
        self._crc_value = 0xFFFF

    def _error(self, msg: str) -> None:
        if self.on_error is not None:
            self.on_error(msg)
