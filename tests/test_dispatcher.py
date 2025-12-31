"""Tests for Dispatcher ACK/NACK policy."""

import unittest

from PYTVLCOM.COM import Dispatcher, FrameDefine, TLVEntry, create_raw_entry


class FakeTransport:
    """A minimal in-memory Transport for unit testing."""

    def __init__(self) -> None:
        self.sent: list[bytes] = []

    def send(self, data: bytes) -> int:
        self.sent.append(data)
        return len(data)


class TestDispatcher(unittest.TestCase):
    def test_success_sends_ack(self) -> None:
        tr = FakeTransport()
        disp = Dispatcher(tr)  # type: ignore[arg-type]

        @disp.cmd_handler(0x10)
        def _() -> bool:
            return True

        entries = [TLVEntry(type=FrameDefine.TLV_TYPE_CONTROL_CMD, value=b"\x10")]
        disp.handle_frame(0x33, entries)

        self.assertEqual(len(tr.sent), 1)
        self.assertIn(create_raw_entry(FrameDefine.TLV_TYPE_ACK, b"\x33"), tr.sent[0])

    def test_failure_sends_nack(self) -> None:
        tr = FakeTransport()
        disp = Dispatcher(tr)  # type: ignore[arg-type]

        entries = [TLVEntry(type=0x55, value=b"\x00")]
        disp.handle_frame(0x44, entries)

        self.assertEqual(len(tr.sent), 1)
        self.assertIn(
            create_raw_entry(FrameDefine.TLV_TYPE_NACK, b"\x44"),
            tr.sent[0],
        )

    def test_ack_frame_does_not_reply(self) -> None:
        tr = FakeTransport()
        disp = Dispatcher(tr)  # type: ignore[arg-type]

        entries = [TLVEntry(type=FrameDefine.TLV_TYPE_ACK, value=b"\x01")]
        disp.handle_frame(0x01, entries)

        self.assertEqual(len(tr.sent), 0)


if __name__ == "__main__":
    unittest.main()
