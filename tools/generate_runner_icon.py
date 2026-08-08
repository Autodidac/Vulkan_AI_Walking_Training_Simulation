#!/usr/bin/env python3
from __future__ import annotations

import base64
import hashlib
import struct
import sys
import zlib
from pathlib import Path

SOURCE_RGBA_SHA256 = "6b623661307a430c6ec8cf5689531324dc30249137a7005155fa047592dcb1ad"


def load_source(path: Path) -> tuple[int, int, bytes]:
    lines = path.read_text(encoding="ascii").splitlines()
    values: dict[str, str] = {}
    encoded: list[str] = []
    inside_payload = False
    for line in lines:
        if line == "ZLIB_BASE64_BEGIN":
            inside_payload = True
            continue
        if line == "ZLIB_BASE64_END":
            inside_payload = False
            continue
        if inside_payload:
            encoded.append(line.strip())
        elif "=" in line:
            key, value = line.split("=", 1)
            values[key] = value

    width = int(values["WIDTH"])
    height = int(values["HEIGHT"])
    declared_hash = values["RGBA_SHA256"]
    if width <= 0 or height <= 0:
        raise ValueError("Runner screenshot icon dimensions are invalid")
    if declared_hash != SOURCE_RGBA_SHA256:
        raise ValueError("Runner screenshot source metadata hash changed")

    compressed = base64.b64decode("".join(encoded), validate=True)
    rgba = zlib.decompress(compressed)
    expected_size = width * height * 4
    if len(rgba) != expected_size:
        raise ValueError(
            f"Runner screenshot source size mismatch: {len(rgba)} != {expected_size}"
        )
    actual_hash = hashlib.sha256(rgba).hexdigest()
    if actual_hash != SOURCE_RGBA_SHA256:
        raise ValueError(
            f"Runner screenshot pixel hash mismatch: {actual_hash} != {SOURCE_RGBA_SHA256}"
        )
    return width, height, rgba


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
    source_path = (
        repository / "assets" / "ui" / "runner_icon_source.rgba.zlib.b64"
    )
    width, height, rgba = load_source(source_path)
    if width != height:
        raise ValueError("Runner screenshot icon source must be square")

    # The source of truth is the exact RGBA pixel hash above. PNG compression
    # bytes legitimately differ between zlib versions/platforms, so record the
    # generated PNG hash rather than requiring one platform's compressed bytes.
    source_png = png_bytes(width, height, rgba)
    source_png_hash = hashlib.sha256(source_png).hexdigest()

    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "runner_icon_source.png").write_bytes(source_png)
    (output / "runner_icon_source.sha256").write_text(
        f"{source_png_hash}  runner_icon_source.png\n", encoding="ascii"
    )
    (output / "runner_icon_source.rgba.zlib.b64").write_text(
        source_path.read_text(encoding="ascii"), encoding="ascii", newline="\n"
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
