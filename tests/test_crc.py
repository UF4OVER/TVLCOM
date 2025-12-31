"""Tests for CRC16-CCITT implementation."""

import unittest

from PYTVLCOM.COM import crc16_ccitt


class TestCrc16Ccitt(unittest.TestCase):
    def test_known_vector_123456789(self) -> None:
        """CRC16-CCITT(FALSE) of ASCII '123456789' should be 0x29B1."""

        self.assertEqual(crc16_ccitt(b"123456789"), 0x29B1)

    def test_empty(self) -> None:
        """CRC of empty input should equal init value 0xFFFF."""

        self.assertEqual(crc16_ccitt(b""), 0xFFFF)


if __name__ == "__main__":
    unittest.main()
