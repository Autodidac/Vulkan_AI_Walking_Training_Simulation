#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import struct
import sys
import zlib
from pathlib import Path

SOURCE_PNG_SHA256 = "7e0e834ba7cb78a39b6f31df12e52440b050327c7a59e46cd70abdc286c808a7"


def png_chunks(data: bytes):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Runner icon source is not a PNG")
    offset = 8
    while offset + 12 <= len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc_expected = struct.unpack_from(">I", data, offset + 8 + length)[0]
        crc_actual = zlib.crc32(kind + payload) & 0xFFFFFFFF
        if crc_actual != crc_expected:
            raise ValueError(f"Runner icon PNG CRC mismatch in {kind!r}")
        yield kind, payload
        offset += 12 + length
        if kind == b"IEND":
            return
    raise ValueError("Runner icon PNG is truncated")


def paeth(a: int, b: int, c: int) -> int:
    prediction = a + b - c
    pa = abs(prediction - a)
    pb = abs(prediction - b)
    pc = abs(prediction - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_rgba(data: bytes) -> tuple[int, int, bytes]:
    width = height = 0
    bit_depth = color_type = interlace = -1
    compressed = bytearray()
    for kind, payload in png_chunks(data):
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if compression != 0 or filtering != 0:
                raise ValueError("Unsupported Runner icon PNG compression/filter method")
        elif kind == b"IDAT":
            compressed.extend(payload)

    if width <= 0 or height <= 0 or bit_depth != 8 or interlace != 0:
        raise ValueError("Runner icon source must be a non-interlaced 8-bit PNG")
    if color_type not in (2, 6):
        raise ValueError("Runner icon source must be RGB or RGBA")

    channels = 4 if color_type == 6 else 3
    stride = width * channels
    raw = zlib.decompress(bytes(compressed))
    expected = height * (stride + 1)
    if len(raw) != expected:
        raise ValueError("Runner icon PNG decompressed size mismatch")

    rows: list[bytearray] = []
    offset = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        source = raw[offset:offset + stride]
        offset += stride
        row = bytearray(stride)
        for index, value in enumerate(source):
            left = row[index - channels] if index >= channels else 0
            up = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 0:
                restored = value
            elif filter_type == 1:
                restored = (value + left) & 0xFF
            elif filter_type == 2:
                restored = (value + up) & 0xFF
            elif filter_type == 3:
                restored = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                restored = (value + paeth(left, up, upper_left)) & 0xFF
            else:
                raise ValueError(f"Unsupported Runner icon PNG filter {filter_type}")
            row[index] = restored
        rows.append(row)
        previous = row

    rgba = bytearray(width * height * 4)
    destination = 0
    for row in rows:
        for x in range(width):
            source = x * channels
            rgba[destination:destination + 3] = row[source:source + 3]
            rgba[destination + 3] = row[source + 3] if channels == 4 else 255
            destination += 4
    return width, height, bytes(rgba)


def resize_nearest(
    source_width: int,
    source_height: int,
    source_rgba: bytes,
    target_width: int,
    target_height: int,
) -> bytes:
    result = bytearray(target_width * target_height * 4)
    for y in range(target_height):
        source_y = min(source_height - 1, (y * source_height) // target_height)
        for x in range(target_width):
            source_x = min(source_width - 1, (x * source_width) // target_width)
            source = (source_y * source_width + source_x) * 4
            target = (y * target_width + x) * 4
            result[target:target + 4] = source_rgba[source:source + 4]
    return bytes(result)


def png_bytes(width: int, height: int, rgba: bytes) -> bytes:
    raw = b"".join(
        b"\x00" + rgba[y * width * 4:(y + 1) * width * 4]
        for y in range(height)
    )

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def bmp_bytes(width: int, height: int, rgba: bytes) -> bytes:
    pixels = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            offset = (y * width + x) * 4
            r, g, b, a = rgba[offset:offset + 4]
            pixels.extend((b, g, r, a))
    file_size = 14 + 40 + len(pixels)
    return (
        b"BM"
        + struct.pack("<IHHI", file_size, 0, 0, 54)
        + struct.pack(
            "<IIIHHIIIIII",
            40,
            width,
            height,
            1,
            32,
            0,
            len(pixels),
            3780,
            3780,
            0,
            0,
        )
        + pixels
    )


def ico_bytes(
    source_width: int,
    source_height: int,
    source_rgba: bytes,
    sizes: tuple[int, ...],
) -> bytes:
    images = [
        png_bytes(
            size,
            size,
            resize_nearest(source_width, source_height, source_rgba, size, size),
        )
        for size in sizes
    ]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + len(images) * 16
    entries = bytearray()
    for size, image in zip(sizes, images, strict=True):
        dimension = 0 if size == 256 else size
        entries.extend(
            struct.pack("<BBBBHHII", dimension, dimension, 0, 0, 1, 32, len(image), offset)
        )
        offset += len(image)
    return header + entries + b"".join(images)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_runner_icon.py <output-directory>", file=sys.stderr)
        return 2

    repository = Path(__file__).resolve().parents[1]
    source_path = repository / "assets" / "ui" / "runner_icon_source.png"
    source = source_path.read_bytes()
    digest = hashlib.sha256(source).hexdigest()
    if digest != SOURCE_PNG_SHA256:
        raise ValueError(
            f"Runner screenshot icon source hash mismatch: {digest} != {SOURCE_PNG_SHA256}"
        )

    width, height, rgba = decode_png_rgba(source)
    if width != height:
        raise ValueError("Runner screenshot icon source must be square")

    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "runner_icon_source.png").write_bytes(source)
    (output / "runner_icon_source.sha256").write_text(
        f"{digest}  runner_icon_source.png\n", encoding="ascii"
    )
    (output / "runner_icon.png").write_bytes(
        png_bytes(256, 256, resize_nearest(width, height, rgba, 256, 256))
    )
    (output / "runner_icon_512.png").write_bytes(
        png_bytes(512, 512, resize_nearest(width, height, rgba, 512, 512))
    )
    (output / "runner_icon.bmp").write_bytes(
        bmp_bytes(64, 64, resize_nearest(width, height, rgba, 64, 64))
    )
    (output / "runner.ico").write_bytes(
        ico_bytes(width, height, rgba, (16, 20, 24, 32, 40, 48, 64, 128, 256))
    )
    print(f"Runner screenshot icon assets generated from {source_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
