"""Integration-ish tests: frame build + stream parser."""

import unittest

from PYTVLCOM.COM import (
    FrameDefine,
    build_frame,
    create_ctrl_cmd_entry,
    create_string_entry,
)
from PYTVLCOM.RE import TLVParser


class TestFrameAndParser(unittest.TestCase):
    def test_build_and_parse(self) -> None:
        payload = create_ctrl_cmd_entry(0x01) + create_string_entry("abc")
        frame = build_frame(0x22, payload)

        got: dict[str, object] = {}

        def on_frame(frame_id: int, entries) -> None:
            got["id"] = frame_id
            got["entries"] = entries

        parser = TLVParser(on_frame=on_frame, on_error=lambda _: None)
        for b in frame:
            parser.process_byte(b)

        self.assertEqual(got["id"], 0x22)
        entries = got["entries"]
        assert isinstance(entries, list)
        self.assertEqual(
            [e.type for e in entries],
            [FrameDefine.TLV_TYPE_CONTROL_CMD, FrameDefine.TLV_TYPE_STRING],
        )
        self.assertEqual(entries[1].value, b"abc")


if __name__ == "__main__":
    unittest.main()
