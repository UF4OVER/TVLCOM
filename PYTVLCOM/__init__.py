# -*- coding: utf-8 -*-
# -------------------------------
#  @Project : TVLCOM
#  @Time    : 2025 - 11-18 09:55
#  @FileName: __init__.py.py
#  @Software: PyCharm 2024.1.6 (Professional Edition)
#  @System  : Windows 11 23H2
#  @Author  : UF4
#  @Contact : 
#  @Python  : 
# -------------------------------
"""TVLCOM public package API.

Most users should import from this module instead of individual submodules.

Examples:

    from PYTVLCOM import TLVParser, Dispatcher, build_frame, create_string_entry
"""

from __future__ import annotations

from .COM import (
    Dispatcher,
    FrameDefine,
    StateMachine,
    TLVEntry,
    buildFrame,
    build_frame,
    createCtrlCmdEntry,
    createInt32Entry,
    createRawEntry,
    createStringEntry,
    create_ctrl_cmd_entry,
    create_int32_entry,
    create_raw_entry,
    create_string_entry,
    crc16_ccitt,
)
from .RE import TLVParser, parse_tlvs
from .TR import Transport, available_transports, create_transport, register_transport

__all__ = [
    "Dispatcher",
    "FrameDefine",
    "StateMachine",
    "TLVEntry",
    "TLVParser",
    "Transport",
    "available_transports",
    "create_transport",
    "register_transport",
    "crc16_ccitt",
    "build_frame",
    "create_raw_entry",
    "create_int32_entry",
    "create_string_entry",
    "create_ctrl_cmd_entry",
    "parse_tlvs",
    # Backward-compatible names
    "buildFrame",
    "createRawEntry",
    "createInt32Entry",
    "createStringEntry",
    "createCtrlCmdEntry",
]
