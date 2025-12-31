"""PYTVLCOM.TR

Transport layer abstraction and implementations.

This module defines:

- `Transport`: an abstract base class for link layers.
- a registration/factory mechanism (`register_transport`, `create_transport`)
- `SerialTransport`: a `pyserial` based implementation.

The goal is to keep protocol logic independent from the underlying link.
"""

from __future__ import annotations

import threading
from abc import ABC, abstractmethod
from typing import Callable, Dict, Mapping, Type, TypeVar

try:
    import serial  # type: ignore
except ModuleNotFoundError:  # pragma: no cover
    serial = None  # type: ignore[assignment]


class Transport(ABC):
    """Abstract transport interface.

    A Transport represents a bidirectional link.

    Implementations must be thread-safe for `send()`.
    """

    @abstractmethod
    def send(self, data: bytes) -> int:
        """Send bytes.

        Args:
            data: Bytes to send.

        Returns:
            Number of bytes written, or a negative number on failure.
        """

    @abstractmethod
    def feed(self, size: int = 1) -> bytes:
        """Read bytes from the link.

        Args:
            size: Max bytes to read.

        Returns:
            Bytes read. Empty bytes means "no data".
        """

    @abstractmethod
    def close(self) -> None:
        """Close the transport and release resources."""


TTransport = TypeVar("TTransport", bound=Transport)

_transport_registry: Dict[str, Type[Transport]] = {}


def register_transport(name: str) -> Callable[[Type[TTransport]], Type[TTransport]]:
    """Class decorator: register a `Transport` implementation under a name.

    Args:
        name: Transport name, e.g. "serial".

    Returns:
        Decorator that registers the given class.

    Raises:
        TypeError: If decorated class isn't a `Transport`.
    """

    def decorator(cls: Type[TTransport]) -> Type[TTransport]:
        if not issubclass(cls, Transport):
            raise TypeError("registered transport must inherit from Transport")
        _transport_registry[name] = cls
        return cls

    return decorator


def create_transport(name: str, *args, **kwargs) -> Transport:
    """Create a transport instance from the registry.

    Args:
        name: Registered transport name.
        *args: Positional args forwarded to transport constructor.
        **kwargs: Keyword args forwarded to transport constructor.

    Returns:
        Transport instance.

    Raises:
        KeyError: If name is not registered.
    """

    try:
        cls = _transport_registry[name]
    except KeyError as exc:
        raise KeyError(f"unknown transport '{name}'") from exc
    return cls(*args, **kwargs)  # type: ignore[call-arg]


def available_transports() -> Mapping[str, Type[Transport]]:
    """Return a snapshot of currently registered transports."""

    return dict(_transport_registry)


@register_transport("serial")
class SerialTransport(Transport):
    """Serial implementation using `pyserial`.

    Args:
        port: Serial port, e.g. "COM3".
        baud: Baud rate.
        timeout: Read timeout in seconds.
    """

    def __init__(self, port: str, baud: int = 115200, timeout: float = 0.1):
        if serial is None:  # pragma: no cover
            raise RuntimeError(
                "pyserial is required for SerialTransport. Install with: pip install pyserial"
            )
        # At this point, `serial` is the imported pyserial module.
        self.ser = serial.Serial(port, baud, timeout=timeout)  # type: ignore[union-attr]
        self.lock = threading.Lock()

    def send(self, data: bytes) -> int:
        with self.lock:
            try:
                return int(self.ser.write(data))
            except Exception:
                return -1

    def feed(self, size: int = 1) -> bytes:
        return self.ser.read(size)

    def close(self) -> None:
        self.ser.close()
