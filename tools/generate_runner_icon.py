#!/usr/bin/env python3
from __future__ import annotations

import math
import struct
import sys
import zlib
from pathlib import Path

RGBA = tuple[int, int, int, int]


def blend(pixels: bytearray, size: int, x: int, y: int, color: RGBA) -> None:
    if x < 0 or y < 0 or x >= size or y >= size:
        return
    offset = (y * size + x) * 4
    alpha = color[3] / 255.0
    inverse = 1.0 - alpha
    pixels[offset] = round(color[0] * alpha + pixels[offset] * inverse)
    pixels[offset + 1] = round(color[1] * alpha + pixels[offset + 1] * inverse)
    pixels[offset + 2] = round(color[2] * alpha + pixels[offset + 2] * inverse)
    pixels[offset + 3] = min(255, round(color[3] + pixels[offset + 3] * inverse))


def circle(pixels: bytearray, size: int, cx: float, cy: float,
           radius: float, color: RGBA) -> None:
    minimum_x = max(0, math.floor(cx - radius - 1))
    maximum_x = min(size - 1, math.ceil(cx + radius + 1))
    minimum_y = max(0, math.floor(cy - radius - 1))
    maximum_y = min(size - 1, math.ceil(cy + radius + 1))
    radius_squared = radius * radius
    for y in range(minimum_y, maximum_y + 1):
        for x in range(minimum_x, maximum_x + 1):
            dx = x + 0.5 - cx
            dy = y + 0.5 - cy
            if dx * dx + dy * dy <= radius_squared:
                blend(pixels, size, x, y, color)


def line(pixels: bytearray, size: int, ax: float, ay: float,
         bx: float, by: float, thickness: float, color: RGBA) -> None:
    minimum_x = max(0, math.floor(min(ax, bx) - thickness - 1))
    maximum_x = min(size - 1, math.ceil(max(ax, bx) + thickness + 1))
    minimum_y = max(0, math.floor(min(ay, by) - thickness - 1))
    maximum_y = min(size - 1, math.ceil(max(ay, by) + thickness + 1))
    dx = bx - ax
    dy = by - ay
    length_squared = max(1.0e-6, dx * dx + dy * dy)
    radius_squared = (thickness * 0.5) ** 2
    for y in range(minimum_y, maximum_y + 1):
        for x in range(minimum_x, maximum_x + 1):
            px = x + 0.5
            py = y + 0.5
            t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
            qx = ax + dx * t
            qy = ay + dy * t
            if (px - qx) ** 2 + (py - qy) ** 2 <= radius_squared:
                blend(pixels, size, x, y, color)


def polygon(pixels: bytearray, size: int,
            points: list[tuple[float, float]], color: RGBA) -> None:
    minimum_y = max(0, math.floor(min(point[1] for point in points)))
    maximum_y = min(size - 1, math.ceil(max(point[1] for point in points)))
    for y in range(minimum_y, maximum_y + 1):
        scan_y = y + 0.5
        intersections: list[float] = []
        previous = points[-1]
        for current in points:
            if (current[1] > scan_y) != (previous[1] > scan_y):
                t = (scan_y - previous[1]) / (current[1] - previous[1])
                intersections.append(previous[0] + (current[0] - previous[0]) * t)
            previous = current
        intersections.sort()
        for index in range(0, len(intersections) - 1, 2):
            start = max(0, math.ceil(intersections[index]))
            finish = min(size - 1, math.floor(intersections[index + 1]))
            for x in range(start, finish + 1):
                blend(pixels, size, x, y, color)


def rounded_background(pixels: bytearray, size: int) -> None:
    radius = size * 0.18
    for y in range(size):
        for x in range(size):
            nearest_x = min(max(x + 0.5, radius), size - radius)
            nearest_y = min(max(y + 0.5, radius), size - radius)
            dx = x + 0.5 - nearest_x
            dy = y + 0.5 - nearest_y
            if dx * dx + dy * dy > radius * radius:
                continue
            fraction = y / max(1, size - 1)
            blend(pixels, size, x, y,
                  (7 + round(8 * fraction), 19 + round(15 * fraction),
                   31 + round(22 * fraction), 255))


def render(size: int) -> bytes:
    scale = 4
    high = size * scale
    pixels = bytearray(high * high * 4)
    rounded_background(pixels, high)
    cyan = (40, 205, 255, 245)
    cyan_dim = (14, 110, 148, 210)
    gold = (255, 194, 61, 255)
    gold_light = (255, 231, 154, 255)
    amber = (244, 116, 31, 245)

    border = high * 0.035
    line(pixels, high, high * 0.15, high * 0.055,
         high * 0.85, high * 0.055, border, cyan)
    line(pixels, high, high * 0.055, high * 0.15,
         high * 0.055, high * 0.85, border, cyan_dim)
    line(pixels, high, high * 0.15, high * 0.945,
         high * 0.85, high * 0.945, border, cyan_dim)
    line(pixels, high, high * 0.945, high * 0.15,
         high * 0.945, high * 0.85, border, cyan)

    for index, length in enumerate((0.34, 0.46, 0.28, 0.40)):
        y = high * (0.25 + index * 0.12)
        line(pixels, high, high * 0.08, y,
             high * (0.08 + length), y, high * 0.020, cyan_dim)
    polygon(pixels, high, [
        (high * 0.18, high * 0.71), (high * 0.42, high * 0.65),
        (high * 0.36, high * 0.75), (high * 0.64, high * 0.69),
        (high * 0.58, high * 0.80), (high * 0.82, high * 0.73),
        (high * 0.75, high * 0.88), (high * 0.22, high * 0.88)
    ], (26, 83, 104, 235))
    line(pixels, high, high * 0.15, high * 0.88,
         high * 0.86, high * 0.88, high * 0.028, amber)

    head = (high * 0.61, high * 0.22)
    neck = (high * 0.57, high * 0.31)
    hip = (high * 0.49, high * 0.52)
    left_hand = (high * 0.30, high * 0.43)
    right_hand = (high * 0.76, high * 0.37)
    left_knee = (high * 0.34, high * 0.65)
    left_foot = (high * 0.20, high * 0.82)
    right_knee = (high * 0.64, high * 0.63)
    right_foot = (high * 0.82, high * 0.75)
    limb = high * 0.052
    line(pixels, high, *neck, *hip, limb * 1.15, gold)
    line(pixels, high, neck[0], neck[1], high * 0.43, high * 0.37, limb, gold)
    line(pixels, high, high * 0.43, high * 0.37, *left_hand, limb * 0.82, gold)
    line(pixels, high, neck[0], neck[1], high * 0.68, high * 0.32, limb, gold_light)
    line(pixels, high, high * 0.68, high * 0.32, *right_hand, limb * 0.82, gold_light)
    line(pixels, high, *hip, *left_knee, limb * 1.05, gold)
    line(pixels, high, *left_knee, *left_foot, limb * 0.92, gold)
    line(pixels, high, *hip, *right_knee, limb * 1.05, gold_light)
    line(pixels, high, *right_knee, *right_foot, limb * 0.92, gold_light)
    line(pixels, high, left_foot[0] - high * 0.03, left_foot[1],
         left_foot[0] + high * 0.10, left_foot[1], limb * 0.50, amber)
    line(pixels, high, right_foot[0] - high * 0.03, right_foot[1],
         right_foot[0] + high * 0.10, right_foot[1], limb * 0.50, amber)
    circle(pixels, high, *head, high * 0.073, gold_light)
    circle(pixels, high, hip[0], hip[1], high * 0.040, amber)
    polygon(pixels, high, [
        (high * 0.74, high * 0.18), (high * 0.86, high * 0.18),
        (high * 0.80, high * 0.29), (high * 0.89, high * 0.29),
        (high * 0.72, high * 0.49), (high * 0.77, high * 0.34),
        (high * 0.68, high * 0.34)
    ], cyan)

    result = bytearray(size * size * 4)
    sample_count = scale * scale
    for y in range(size):
        for x in range(size):
            totals = [0, 0, 0, 0]
            for sy in range(scale):
                for sx in range(scale):
                    source = ((y * scale + sy) * high + x * scale + sx) * 4
                    for channel in range(4):
                        totals[channel] += pixels[source + channel]
            destination = (y * size + x) * 4
            for channel in range(4):
                result[destination + channel] = totals[channel] // sample_count
    return bytes(result)


def png_bytes(size: int, rgba: bytes) -> bytes:
    raw = b"".join(b"\x00" + rgba[y * size * 4:(y + 1) * size * 4]
                   for y in range(size))
    signature = b"\x89PNG\r\n\x1a\n"

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(
            ">I", zlib.crc32(kind + data) & 0xffffffff)

    return signature + chunk(b"IHDR", struct.pack(
        ">IIBBBBB", size, size, 8, 6, 0, 0, 0)) + chunk(
            b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def bmp_bytes(size: int, rgba: bytes) -> bytes:
    row_bytes = size * 4
    pixels = bytearray()
    for y in range(size - 1, -1, -1):
        for x in range(size):
            offset = (y * size + x) * 4
            r, g, b, a = rgba[offset:offset + 4]
            pixels.extend((b, g, r, a))
    file_size = 14 + 40 + len(pixels)
    return (b"BM" + struct.pack("<IHHI", file_size, 0, 0, 54)
            + struct.pack("<IIIHHIIIIII", 40, size, size, 1, 32, 0,
                          len(pixels), 3780, 3780, 0, 0) + pixels)


def ico_bytes(sizes: tuple[int, ...]) -> bytes:
    images = [png_bytes(size, render(size)) for size in sizes]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + len(images) * 16
    entries = bytearray()
    for size, image in zip(sizes, images, strict=True):
        dimension = 0 if size == 256 else size
        entries.extend(struct.pack("<BBBBHHII", dimension, dimension,
                                   0, 0, 1, 32, len(image), offset))
        offset += len(image)
    return header + entries + b"".join(images)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: generate_runner_icon.py <output-directory>", file=sys.stderr)
        return 2
    output = Path(sys.argv[1]).resolve()
    output.mkdir(parents=True, exist_ok=True)
    rgba_256 = render(256)
    (output / "runner_icon.png").write_bytes(png_bytes(256, rgba_256))
    (output / "runner_icon_512.png").write_bytes(png_bytes(512, render(512)))
    (output / "runner_icon.bmp").write_bytes(bmp_bytes(64, render(64)))
    (output / "runner.ico").write_bytes(ico_bytes((16, 20, 24, 32, 40, 48, 64, 128, 256)))
    print(f"Runner icon assets generated in {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
