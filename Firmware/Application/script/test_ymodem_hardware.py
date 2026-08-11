#!/usr/bin/env python3
"""COM-port YMODEM RX/TX round-trip acceptance test for Application."""

from __future__ import annotations

import argparse
import time

import serial


SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRC_REQUEST = ord("C")
PAD = 0x1A


def crc16(data: bytes) -> int:
    value = 0
    for byte in data:
        value ^= byte << 8
        for _ in range(8):
            value = ((value << 1) ^ 0x1021) & 0xFFFF if value & 0x8000 else (value << 1) & 0xFFFF
    return value


def packet(start: int, block: int, payload: bytes) -> bytes:
    expected = 128 if start == SOH else 1024
    if len(payload) != expected:
        raise ValueError("invalid payload size")
    check = crc16(payload)
    return bytes((start, block, 0xFF - block)) + payload + check.to_bytes(2, "big")


def header(name: str = "", size: int = 0) -> bytes:
    payload = bytearray(128)
    if name:
        encoded = name.encode("ascii")
        size_text = str(size).encode("ascii")
        payload[: len(encoded)] = encoded
        payload[len(encoded)] = 0
        payload[len(encoded) + 1 : len(encoded) + 1 + len(size_text)] = size_text
    return packet(SOH, 0, bytes(payload))


class PortReader:
    def __init__(self, port: serial.Serial) -> None:
        self.port = port

    def byte(self, timeout: float = 10.0) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = self.port.read(1)
            if data:
                return data[0]
        raise TimeoutError("serial byte timeout")

    def expect(self, expected: int, timeout: float = 10.0) -> None:
        value = self.byte(timeout)
        if value != expected:
            raise RuntimeError(f"expected 0x{expected:02X}, received 0x{value:02X}")

    def marker(self, marker: bytes, timeout: float = 5.0) -> bytes:
        captured = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = self.port.read(1)
            if data:
                captured.extend(data)
                if captured.endswith(marker):
                    return bytes(captured)
        raise TimeoutError(f"marker not received: {marker!r}; got {captured!r}")

    def exact(self, length: int, timeout: float = 10.0) -> bytes:
        result = bytearray()
        deadline = time.monotonic() + timeout
        while len(result) < length and time.monotonic() < deadline:
            result.extend(self.port.read(length - len(result)))
        if len(result) != length:
            raise TimeoutError(f"expected {length} bytes, received {len(result)}")
        return bytes(result)

    def control(self, expected: int, timeout: float = 10.0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = self.byte(max(0.1, deadline - time.monotonic()))
            if value == expected:
                return
            if value == CAN:
                raise RuntimeError("device cancelled YMODEM session")
        raise TimeoutError(f"control 0x{expected:02X} timeout")

    def frame(self, timeout: float = 10.0) -> tuple[int, int, bytes]:
        deadline = time.monotonic() + timeout
        while True:
            start = self.byte(max(0.1, deadline - time.monotonic()))
            if start in (SOH, STX, EOT, CAN):
                break
            if time.monotonic() >= deadline:
                raise TimeoutError("frame start timeout")
        if start in (EOT, CAN):
            return start, 0, b""
        length = 128 if start == SOH else 1024
        body = self.exact(length + 4, max(0.1, deadline - time.monotonic()))
        block, inverse = body[0], body[1]
        payload = body[2 : 2 + length]
        received_crc = int.from_bytes(body[-2:], "big")
        if ((block + inverse) & 0xFF) != 0xFF or crc16(payload) != received_crc:
            raise RuntimeError("invalid YMODEM frame")
        return start, block, payload


def send_file(port: serial.Serial, reader: PortReader, content: bytes) -> None:
    port.write(b"ymodem rx candidate\r")
    ready = reader.marker(b"OK READY YMODEM RX\r\n", 5.0)
    print(ready.decode(errors="replace"), end="")
    reader.control(CRC_REQUEST, 5.0)
    port.write(header("p5-roundtrip.bin", len(content)))
    reader.control(ACK)
    reader.control(CRC_REQUEST)

    block = 1
    offset = 0
    while offset < len(content):
        chunk = content[offset : offset + 1024]
        payload = chunk + bytes((PAD,)) * (1024 - len(chunk))
        port.write(packet(STX, block, payload))
        reader.control(ACK, 12.0)
        offset += len(chunk)
        block = (block + 1) & 0xFF

    port.write(bytes((EOT,)))
    reader.control(NAK)
    port.write(bytes((EOT,)))
    reader.control(ACK)
    reader.control(CRC_REQUEST)
    port.write(header())
    reader.control(ACK)
    # Receiver deliberately lingers one timeout to recover a lost final ACK.
    time.sleep(5.5)


def receive_file(port: serial.Serial, reader: PortReader, size: int) -> bytes:
    port.write(f"ymodem tx candidate {size}\r".encode("ascii"))
    ready = reader.marker(b"OK READY YMODEM TX\r\n", 5.0)
    print(ready.decode(errors="replace"), end="")
    time.sleep(0.1)
    port.write(bytes((CRC_REQUEST,)))

    start, block, payload = reader.frame()
    if start != SOH or block != 0 or not payload.startswith(b"candidate.bin\0"):
        raise RuntimeError("invalid YMODEM file header")
    port.write(bytes((ACK, CRC_REQUEST)))

    result = bytearray()
    expected_block = 1
    while True:
        start, block, payload = reader.frame(12.0)
        if start == EOT:
            port.write(bytes((NAK,)))
            start, _, _ = reader.frame()
            if start != EOT:
                raise RuntimeError("second EOT missing")
            port.write(bytes((ACK, CRC_REQUEST)))
            start, block, payload = reader.frame()
            if start != SOH or block != 0 or payload[0] != 0:
                raise RuntimeError("final empty header missing")
            port.write(bytes((ACK,)))
            break
        if start != STX or block != expected_block:
            raise RuntimeError("unexpected data block")
        result.extend(payload)
        port.write(bytes((ACK,)))
        expected_block = (expected_block + 1) & 0xFF
    return bytes(result[:size])


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--size", type=int, default=2500)
    args = parser.parse_args()

    content = bytes(((index * 37 + 11) & 0xFF) for index in range(args.size))
    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2.0) as port:
        reader = PortReader(port)
        port.reset_input_buffer()
        send_file(port, reader, content)
        received = receive_file(port, reader, len(content))
        if received != content:
            raise RuntimeError("round-trip payload mismatch")
        time.sleep(0.5)
        port.write(b"ymodem status\rstatus\r")
        output = reader.marker(b"app> ", 5.0)
        print(output.decode(errors="replace"), end="")

    print(f"YMODEM hardware round trip: PASS ({len(content)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
