"""PYTVLCOM.COM

This module contains the core protocol primitives for TVLCOM:

- Frame constants (header/tail)
- CRC16-CCITT implementation
- TLV (Type-Length-Value) helpers to build payloads
- Frame builder
- Dispatcher that routes TLVs to user handlers and auto-replies ACK/NACK

Typical usage:

    from PYTVLCOM import Dispatcher, TLVParser, build_frame, create_string_entry

    dispatcher = Dispatcher(transport)

    @dispatcher.type_handler(FrameDefine.TLV_TYPE_STRING)
    def on_string(value: bytes) -> bool:
        print(value.decode())
        return True

    frame = build_frame(0x01, create_string_entry("hello"))
    transport.send(frame)

Notes:
- CRC is written as big-endian.
- Integer TLV uses little-endian uint32.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Callable, MutableMapping, Sequence

from PYTVLCOM.TR import Transport

HEADER = bytes([0xF0, 0x0F])
TAIL = bytes([0xE0, 0x0D])


class FrameDefine(IntEnum):
    """Protocol constants.

    Attributes:
        TLV_TYPE_CONTROL_CMD: Control command TLV type.
        TLV_TYPE_INTEGER: Unsigned 32-bit integer TLV type.
        TLV_TYPE_STRING: UTF-8 string TLV type.
        TLV_TYPE_ACK: ACK TLV type.
        TLV_TYPE_NACK: NACK TLV type.
        TLV_MAX_DATA_LENGTH: Maximum TLV payload size in one frame.
    """

    TLV_TYPE_CONTROL_CMD = 0x01
    TLV_TYPE_INTEGER = 0x02
    TLV_TYPE_STRING = 0x03
    TLV_TYPE_ACK = 0x08
    TLV_TYPE_NACK = 0x09

    TLV_MAX_DATA_LENGTH = 240


class StateMachine(IntEnum):
    """Receiver state machine states.

    This enum is shared between the parser implementation and tests.
    """

    SYNC = 1
    FRAME_ID = 2
    DATALEN = 3
    DATA = 4
    CRC1 = 5
    CRC2 = 6
    TAIL1 = 7
    TAIL2 = 8


@dataclass(frozen=True, slots=True)
class TLVEntry:
    """A parsed TLV item.

    Attributes:
        type: TLV type byte.
        value: Raw TLV value bytes.
    """

    type: int
    value: bytes


def crc16_ccitt_bitwise(data: bytes, init: int = 0xFFFF) -> int:
    """Compute CRC16-CCITT (CCITT-FALSE) with a bit-wise implementation.

    This is kept mainly for readability and reference. For performance-sensitive
    workloads, use :func:`crc16_ccitt` (table-driven).
        init: Initial CRC value. Defaults to 0xFFFF.

    Returns:
        16-bit CRC value.
    """

    crc = init
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) & 0xFFFF) ^ 0x1021
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def create_raw_entry(tlv_type: int, payload: bytes) -> bytes:
    """Create a raw TLV entry.

    TLV format:
        Type(1) + Length(1) + Value(N)

    Args:
        tlv_type: TLV type byte (0..255).
        payload: Raw TLV value bytes.

    Returns:
        Encoded TLV bytes.

    Raises:
        ValueError: If `tlv_type` is out of range or payload > 255 bytes.
    """

    if not (0 <= int(tlv_type) <= 0xFF):
        raise ValueError("tlv_type must be in range 0..255")
    if len(payload) > 0xFF:
        raise ValueError("TLV payload too large (max 255 bytes)")
    return bytes([int(tlv_type) & 0xFF, len(payload) & 0xFF]) + payload


def _build_crc16_table() -> tuple[int, ...]:
    """Build a lookup table for CRC16-CCITT (polynomial 0x1021)."""

    table: list[int] = []
    for byte in range(256):
        crc = byte << 8
        for _ in range(8):
            crc = (((crc << 1) ^ 0x1021) & 0xFFFF) if (crc & 0x8000) else ((crc << 1) & 0xFFFF)
        table.append(crc)
    return tuple(table)


_CRC16_TABLE = _build_crc16_table()


def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    """Compute CRC16-CCITT (CCITT-FALSE) using a table-driven algorithm.

    Args:
        data: Input bytes.
        init: Initial CRC value. Defaults to 0xFFFF.

    Returns:
        16-bit CRC value.
    """

    crc = init & 0xFFFF
    for b in data:
        crc = ((crc << 8) ^ _CRC16_TABLE[((crc >> 8) ^ b) & 0xFF]) & 0xFFFF
    return crc


def crc16_ccitt_update(value: int, data: bytes) -> int:
    """Incrementally update a CRC16-CCITT value.

    This avoids temporary concatenations in hot paths like frame building/parsing.

    Args:
        value: Current CRC16 value.
        data: Next bytes chunk.

    Returns:
        Updated CRC16 value.
    """

    value &= 0xFFFF
    for b in data:
        value = ((value << 8) ^ _CRC16_TABLE[((value >> 8) ^ b) & 0xFF]) & 0xFFFF
    return value


def create_int32_entry(value: int, tlv_type: int = FrameDefine.TLV_TYPE_INTEGER) -> bytes:
    """Create a little-endian uint32 TLV entry.

    Backward-compatibility:
        Older versions of this project used the calling convention
        `create_int32_entry(tlv_type, value)`. We support both:

        - Preferred: create_int32_entry(value, tlv_type=...)
        - Legacy:   create_int32_entry(tlv_type, value)

    Args:
        value: Unsigned 32-bit integer (preferred 1st arg).
        tlv_type: TLV type (defaults to TLV_TYPE_INTEGER).

    Returns:
        Encoded TLV bytes.

    Raises:
        ValueError: If value is out of range.
    """

    # If called as (tlv_type, value), the first arg is a small TLV type and the
    # second arg is a potentially large integer value.
    if 0 <= int(value) <= 0xFF and not (0 <= int(tlv_type) <= 0xFF):
        value, tlv_type = tlv_type, value

    if not (0 <= int(tlv_type) <= 0xFF):
        raise ValueError("tlv_type must be in range 0..255")
    if not (0 <= int(value) <= 0xFFFFFFFF):
        raise ValueError("int32 value out of range (0..0xFFFFFFFF)")

    payload = int(value).to_bytes(4, "little", signed=False)
    return create_raw_entry(int(tlv_type), payload)


def create_string_entry(text: str) -> bytes:
    """Create a UTF-8 string TLV entry.

    Args:
        text: Text to encode.

    Returns:
        Encoded TLV bytes.
    """

    payload = text.encode("utf-8")
    return create_raw_entry(FrameDefine.TLV_TYPE_STRING, payload)


def create_ctrl_cmd_entry(cmd: int) -> bytes:
    """Create a control command TLV entry.

    Args:
        cmd: Command byte (0..255).

    Returns:
        Encoded TLV bytes.

    Raises:
        ValueError: If cmd isn't within 0..255.
    """

    if not (0 <= int(cmd) <= 0xFF):
        raise ValueError("cmd must be in range 0..255")
    return create_raw_entry(FrameDefine.TLV_TYPE_CONTROL_CMD, bytes([int(cmd)]))


def build_frame(frame_id: int, tlv_payload: bytes) -> bytes:
    """Build a complete TVLCOM frame.

    Frame format:
        HEADER(2) + FrameID(1) + DataLen(1) + Data(N) + CRC16(2) + TAIL(2)

    CRC16 is computed over `FrameID + DataLen + Data` and written as big-endian.

    Args:
        frame_id: Frame identifier (0..255).
        tlv_payload: Concatenated TLV-encoded bytes.

    Returns:
        Complete frame bytes.

    Raises:
        ValueError: If `frame_id` is out of range.
        ValueError: If payload size exceeds `FrameDefine.TLV_MAX_DATA_LENGTH`.
    """

    if not (0 <= int(frame_id) <= 0xFF):
        raise ValueError("frame_id must be in range 0..255")
    if len(tlv_payload) > int(FrameDefine.TLV_MAX_DATA_LENGTH):
        raise ValueError(
            f"tlv_payload too large (max {int(FrameDefine.TLV_MAX_DATA_LENGTH)} bytes)"
        )

    data_len = len(tlv_payload)
    header = HEADER + bytes([int(frame_id), data_len])

    # Avoid building a temporary `bytes([frame_id, data_len]) + tlv_payload`.
    crc = 0xFFFF
    crc = crc16_ccitt_update(crc, bytes([int(frame_id), data_len]))
    crc = crc16_ccitt_update(crc, tlv_payload)

    crc_bytes = crc.to_bytes(2, "big")
    return header + tlv_payload + crc_bytes + TAIL


# Pre-built 1-byte payload TLVs used by Dispatcher (hot path).
_ACK_PREFIX = bytes([int(FrameDefine.TLV_TYPE_ACK), 1])
_NACK_PREFIX = bytes([int(FrameDefine.TLV_TYPE_NACK), 1])


class Dispatcher:
    """Dispatch parsed TLVs to registered handlers and auto reply ACK/NACK.

    Dispatcher routes TLVs according to:

    - Control command TLV (`TLV_TYPE_CONTROL_CMD`): dispatched via `cmd_handlers`
    - Other TLVs: dispatched via `type_handlers`

    ACK/NACK policy:

    - If an incoming frame contains *only* ACK/NACK TLVs, dispatcher will not
      reply (prevents infinite ACK loops).
    - Otherwise, dispatcher aggregates handler results:
        - if all handled TLVs succeed -> send ACK
        - if any TLV fails or is unhandled -> send NACK

    Handlers should return `True` on success, `False` on failure.
    """

    def __init__(self, transport: Transport):
        self.transport = transport
        self.type_handlers: MutableMapping[int, Callable[[bytes], bool]] = {}
        self.cmd_handlers: MutableMapping[int, Callable[[], bool]] = {}

    # ---- backward-compatible public registration API ----
    def RegisterTypeHandler(self, tlv_type: int, fn: Callable[[bytes], bool]) -> None:
        """Backward-compatible alias for :meth:`register_type_handler`."""

        self.registerTypeHandler(tlv_type, fn)

    def RegisterCmdHandler(self, cmd: int, fn: Callable[[], bool]) -> None:
        """Backward-compatible alias for :meth:`register_cmd_handler`."""

        self.registerCmdHandler(cmd, fn)

    # ---- preferred snake_case API ----
    def registerTypeHandler(self, tlv_type: int, fn: Callable[[bytes], bool]) -> None:
        """Register a handler for a TLV type."""

        self.type_handlers[int(tlv_type)] = fn

    def registerCmdHandler(self, cmd: int, fn: Callable[[], bool]) -> None:
        """Register a handler for a control command."""

        self.cmd_handlers[int(cmd)] = fn

    def typeHandler(self, tlv_type: int):
        """Decorator variant of :meth:`register_type_handler`."""

        def decorator(fn: Callable[[bytes], bool]):
            self.registerTypeHandler(tlv_type, fn)
            return fn

        return decorator

    def cmdHandler(self, cmd: int):
        """Decorator variant of :meth:`register_cmd_handler`."""

        def decorator(fn: Callable[[], bool]):
            self.registerCmdHandler(cmd, fn)
            return fn

        return decorator

    def handle_frame(self, frame_id: int, entries: Sequence[TLVEntry]) -> None:
        """Handle a parsed frame.

        Args:
            frame_id: Incoming frame id.
            entries: Parsed TLV entries.

        Returns:
            None.
        """

        only_ack = (
            len(entries) > 0
            and all(
                e.type in (FrameDefine.TLV_TYPE_ACK, FrameDefine.TLV_TYPE_NACK)
                for e in entries
            )
        )

        success = True

        for entry in entries:
            tlv_type = int(entry.type)
            value = entry.value

            if tlv_type == int(FrameDefine.TLV_TYPE_CONTROL_CMD):
                cmd = value[0] if value else None
                handler = self.cmd_handlers.get(int(cmd)) if cmd is not None else None
                if handler is None or handler() is False:
                    success = False
                continue

            if tlv_type in (int(FrameDefine.TLV_TYPE_ACK), int(FrameDefine.TLV_TYPE_NACK)):
                # Application may track ACK/NACK for retransmission. No default action.
                continue

            handler = self.type_handlers.get(tlv_type)
            if handler is None or handler(value) is False:
                success = False

        if only_ack:
            return

        if success:
            self._send_ack(frame_id)
        else:
            self._send_nack(frame_id)

    # Backward-compatible alias
    def handleFrame(self, frame_id: int, entries) -> None:  # noqa: N802
        """Backward-compatible alias for :meth:`handle_frame`."""

        normalized = []
        for e in entries:
            if isinstance(e, TLVEntry):
                normalized.append(e)
            else:
                # legacy dict form: {'type': t, 'value': v, ...}
                normalized.append(TLVEntry(type=int(e["type"]), value=bytes(e["value"])))
        self.handle_frame(frame_id, normalized)

    def _send_ack(self, frame_id: int) -> None:
        tlv = _ACK_PREFIX + bytes([int(frame_id) & 0xFF])
        frame = build_frame(0, tlv)
        self.transport.send(frame)

    def _send_nack(self, frame_id: int) -> None:
        tlv = _NACK_PREFIX + bytes([int(frame_id) & 0xFF])
        frame = build_frame(0, tlv)
        self.transport.send(frame)


# --- Backward-compatible function aliases (keep old public names) ---
createRawEntry = create_raw_entry
createInt32Entry = create_int32_entry
createStringEntry = create_string_entry
createCtrlCmdEntry = create_ctrl_cmd_entry
buildFrame = build_frame

