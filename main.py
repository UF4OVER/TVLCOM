"""Serial demo for TVLCOM.

This script demonstrates how to:

- create a serial transport via `create_transport`
- set up a `Dispatcher` and `TLVParser`
- register handlers for TLV types and control commands
- periodically send random frames

Note:
    This file is an example. The library code lives in `PYTVLCOM/`.
"""

from __future__ import annotations

import random
import threading
import time

from PYTVLCOM import (
    Dispatcher,
    FrameDefine,
    TLVParser,
    Transport,
    build_frame,
    create_ctrl_cmd_entry,
    create_int32_entry,
    create_raw_entry,
    create_string_entry,
    create_transport,
)


def pump_serial_once(
    transport: Transport, parser: TLVParser, max_bytes: int = 4096
) -> int:
    """Read currently available bytes from the serial port and feed the parser.

    Args:
        transport: Transport instance (serial).
        parser: TLVParser.
        max_bytes: Upper bound for one pump iteration.

    Returns:
        Number of bytes processed.
    """

    # SerialTransport exposes `.ser`, so we can peek in_waiting for the demo.
    in_waiting = int(getattr(getattr(transport, "ser", None), "in_waiting", 0) or 0)
    if in_waiting <= 0:
        return 0

    n = min(in_waiting, max_bytes)
    data = transport.feed(n)
    for b in data:
        parser.process_byte(b)
    return len(data)


def send_random_loop(transport: Transport, interval: float = 2.0) -> None:
    """Background thread: send random TLV frames periodically."""

    frame_id = 1
    while True:
        tlv_buf = bytearray()
        desc_list: list[str] = []

        count = random.randint(1, 3)
        for _ in range(count):
            kind = random.choice(["string", "int", "cmd", "custom"])

            if kind == "string":
                s = "".join(
                    random.choices(
                        "abcdefghijklmnopqrstuvwxyz0123456789",
                        k=random.randint(3, 12),
                    )
                )
                tlv_buf += create_string_entry(s)
                desc_list.append(
                    f"type=0x{FrameDefine.TLV_TYPE_STRING:02X} string=\"{s}\""
                )
                continue

            if kind == "int":
                v = random.randint(0, 0xFFFFFFFF)
                tlv_buf += create_int32_entry(FrameDefine.TLV_TYPE_INTEGER, v)
                desc_list.append(
                    f"type=0x{FrameDefine.TLV_TYPE_INTEGER:02X} int32=0x{v:08X}({v})"
                )
                continue

            if kind == "cmd":
                cmd = random.choice([1, 2])
                tlv_buf += create_ctrl_cmd_entry(cmd)
                desc_list.append(
                    f"type=0x{FrameDefine.TLV_TYPE_CONTROL_CMD:02X} cmd=0x{cmd:02X}({cmd})"
                )
                continue

            # custom
            tlv_type = random.choice([0x40, 0x41, 0x50])
            if tlv_type == 0x40:
                v = random.randint(0, 0xFFFFFFFF)
                tlv_buf += create_int32_entry(tlv_type, v)
                desc_list.append(f"type=0x{tlv_type:02X} int32=0x{v:08X}({v})")
            else:
                raw = bytes(random.getrandbits(8) for _ in range(random.randint(1, 8)))
                tlv_buf += create_raw_entry(tlv_type, raw)
                desc_list.append(
                    f"type=0x{tlv_type:02X} raw={raw.hex()} len={len(raw)}"
                )

        fid = frame_id & 0xFF
        frame = build_frame(fid, bytes(tlv_buf))
        written = transport.send(frame)

        payload_len = len(tlv_buf)
        print(
            f"sent frame_id={fid} payload_len={payload_len} tlv_count={count} "
            f"tlvs=[{' ; '.join(desc_list)}] frame_len={len(frame)} written={written}"
        )

        frame_id += 1
        time.sleep(interval)


def main() -> None:
    """Entry point."""

    transport = create_transport("serial", port="COM3", baud=115200, timeout=0.1)
    dispatcher = Dispatcher(transport)

    parser = TLVParser(
        on_frame=dispatcher.handleFrame,  # keep demo compatible with old name
        on_error=lambda e: print("parse error:", e),
        legacy_dict_entries=True,
    )

    @dispatcher.typeHandler(FrameDefine.TLV_TYPE_STRING)
    def on_string(v: bytes) -> bool:
        print("recv string:", v.decode("utf-8", errors="ignore"))
        return True

    @dispatcher.cmdHandler(0x01)
    def on_cmd_1() -> bool:
        print("cmd 1 received")
        return True

    @dispatcher.cmdHandler(0x02)
    def on_cmd_2() -> bool:
        print("cmd 2 received")
        return True

    @dispatcher.typeHandler(0x41)
    def on_type_41(v: bytes) -> bool:
        print("recv type 0x41:", v.hex())
        return True

    @dispatcher.typeHandler(0x50)
    def on_type_50(v: bytes) -> bool:
        print("recv type 0x50:", v.hex())
        return True

    @dispatcher.typeHandler(0x40)
    def on_type_40(v: bytes) -> bool:
        print("recv type 0x40:", v.hex())
        return True

    sender_thread = threading.Thread(
        target=send_random_loop, args=(transport, 2.0), daemon=True
    )
    sender_thread.start()

    try:
        while True:
            processed = pump_serial_once(transport, parser)
            if processed == 0:
                # Sleep briefly when idle to reduce CPU usage.
                time.sleep(0.001)
    except KeyboardInterrupt:
        transport.close()


if __name__ == "__main__":
    main()
