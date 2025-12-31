# TVLCOM Documentation

## Overview

TVLCOM is a lightweight communication layer based on a fixed frame format and
TLV (Type-Length-Value) payload encoding. It's designed for serial links
(PC ↔ MCU), but the transport layer is pluggable.

## Modules

- `PYTVLCOM.COM`
  - Frame/TLV constants (`HEADER`, `TAIL`, `FrameDefine`)
  - CRC16-CCITT (`crc16_ccitt`)
  - TLV builders (`create_*_entry`)
  - Frame builder (`build_frame`)
  - Dispatcher for ACK/NACK and callback routing (`Dispatcher`)

- `PYTVLCOM.RE`
  - Stream parser state machine (`TLVParser`)
  - TLV parsing helpers (`parse_tlvs`, `TLVEntry`)

- `PYTVLCOM.TR`
  - Transport abstraction (`Transport`)
  - Transport registry/factory (`register_transport`, `create_transport`)
  - Serial transport (`SerialTransport`, based on `pyserial`)

## Protocol

### Frame format

- Header: 2 bytes, fixed `0xF0 0x0F`
- FrameId: 1 byte
- DataLen: 1 byte (TLV payload length)
- Data: N bytes (concatenated TLVs)
- CRC16: 2 bytes big-endian, computed over `FrameId + DataLen + Data`
- Tail: 2 bytes, fixed `0xE0 0x0D`

### TLV format

`[Type:1][Len:1][Value:Len]`

### Built-in type definitions

- `0x01`: control command (Value is 1 byte command)
- `0x02`: integer (Value is 4 bytes, little-endian unsigned)
- `0x03`: UTF-8 string
- `0x08`: ACK (Value is 1 byte: the original `FrameId`)
- `0x09`: NACK (Value is 1 byte: the original `FrameId`)

## Usage

See `main.py` for a serial demo.

## Windows serial notes

- For ports ≥ `COM10`, Windows sometimes requires the `\\\\.\\COM10` form.
- A serial port can be exclusive: ensure other tools (e.g. serial monitor)
  aren't holding it.

## Performance notes

- Current CRC is bit-wise; if you need higher throughput, consider a
  table-driven CRC implementation.
- `TLVParser.process_byte()` is byte-wise; for high data rates you can add an
  optional `process_bytes(data: bytes)` wrapper to reduce Python overhead.
