"""Tests for TLV construction and parsing."""

import unittest

from PYTVLCOM.COM import (
    FrameDefine,
    create_ctrl_cmd_entry,
    create_int32_entry,
    create_raw_entry,
    create_string_entry,
)
from PYTVLCOM.RE import parse_tlvs


class TestTlv(unittest.TestCase):
    def test_round_trip(self) -> None:
        payload = (
            create_ctrl_cmd_entry(0x10)
            + create_int32_entry(FrameDefine.TLV_TYPE_INTEGER, 0x12345678)
            + create_string_entry("hello")
            + create_raw_entry(0x40, b"\x01\x02")
        )
        entries = parse_tlvs(payload)

        self.assertEqual([e.type for e in entries], [0x01, 0x02, 0x03, 0x40])
        self.assertEqual(entries[0].value, b"\x10")
        self.assertEqual(entries[2].value, b"hello")

    def test_truncated_raises(self) -> None:
        # Type=1 Len=3 but only two bytes of value
        with self.assertRaises(ValueError):
            parse_tlvs(bytes([0x01, 0x03, 0xAA, 0xBB]))


if __name__ == "__main__":
    unittest.main()
