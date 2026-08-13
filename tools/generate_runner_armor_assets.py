#!/usr/bin/env python3
"""Build side-orthographic P3 sprites from the remade atlas and supplied sheet."""

from __future__ import annotations

import hashlib
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tools" / "art_sources" / "runner_v0729_modular_atlas.png"
USER_SHEET = ROOT / "tools" / "art_sources" / "runner_user_modular_sheet.png"
OUTPUT = ROOT / "assets" / "optional" / "runner_armor_concepts" / "runtime"
EXPECTED_SOURCE_SIZE = (1403, 1121)
EXPECTED_USER_SHEET_SIZE = (1024, 1536)
USER_SHEET_SHA256 = "d1db49b2c376a87a060b9bb18402f8374c1f5730e132e51f5385c1dcc28f1195"
USER_HAND_BOX = (205, 1148, 248, 1203)
KEY = (255, 0, 255)
ALPHA_THRESHOLD = 32
SOURCE_PADDING = 12
TARGET_PADDING = 3


@dataclass(frozen=True)
class SpriteSpec:
    name: str
    source_box: tuple[int, int, int, int]
    size: tuple[int, int]
    rotate_counterclockwise: bool = False


SPECS = (
    SpriteSpec("helmet_side.ppm", (0, 0, 350, 560), (32, 32)),
    SpriteSpec("torso_side.ppm", (350, 0, 700, 560), (32, 40)),
    SpriteSpec("upper_arm_side.ppm", (700, 0, 1050, 560), (36, 20), True),
    SpriteSpec("forearm_side.ppm", (1050, 0, 1403, 560), (40, 20), True),
    SpriteSpec("thigh_side.ppm", (0, 560, 350, 1121), (36, 20), True),
    SpriteSpec("shin_side.ppm", (350, 560, 700, 1121), (38, 20), True),
    SpriteSpec("foot_side.ppm", (700, 560, 980, 1121), (32, 28)),
    SpriteSpec("weapon_side.ppm", (980, 560, 1403, 1121), (44, 24)),
)


def crop_cell(atlas: Image.Image, spec: SpriteSpec) -> Image.Image:
    cell = atlas.crop(spec.source_box)
    cell_width, cell_height = cell.size
    bounds = cell.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError(f"{spec.name} atlas cell is empty")
    x0, y0, x1, y1 = bounds
    bounds = (
        max(0, x0 - SOURCE_PADDING),
        max(0, y0 - SOURCE_PADDING),
        min(cell_width, x1 + SOURCE_PADDING),
        min(cell_height, y1 + SOURCE_PADDING),
    )
    sprite = cell.crop(bounds)
    if spec.rotate_counterclockwise:
        sprite = sprite.rotate(90, expand=True)
    return sprite


def extract_hand(source: Image.Image) -> Image.Image:
    """Extract one strict side-view hand without erasing dark internal armor."""
    hand = source.crop(USER_HAND_BOX).convert("RGBA")
    pixels = hand.load()
    frontier: list[tuple[int, int]] = []
    seen: set[tuple[int, int]] = set()

    def enqueue(x: int, y: int) -> None:
        red, green, blue, _ = pixels[x, y]
        if max(red, green, blue) <= 36 and (x, y) not in seen:
            seen.add((x, y))
            frontier.append((x, y))

    for x in range(hand.width):
        enqueue(x, 0)
        enqueue(x, hand.height - 1)
    for y in range(hand.height):
        enqueue(0, y)
        enqueue(hand.width - 1, y)

    while frontier:
        x, y = frontier.pop()
        red, green, blue, _ = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        if x > 0:
            enqueue(x - 1, y)
        if x + 1 < hand.width:
            enqueue(x + 1, y)
        if y > 0:
            enqueue(x, y - 1)
        if y + 1 < hand.height:
            enqueue(x, y + 1)

    bounds = hand.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError("supplied side-view hand contains no opaque subject")
    x0, y0, x1, y1 = bounds
    hand = hand.crop((
        max(0, x0 - SOURCE_PADDING),
        max(0, y0 - SOURCE_PADDING),
        min(hand.width, x1 + SOURCE_PADDING),
        min(hand.height, y1 + SOURCE_PADDING),
    ))
    return hand.rotate(90, expand=True)


def fit_sprite(sprite: Image.Image, size: tuple[int, int]) -> Image.Image:
    available = (size[0] - TARGET_PADDING * 2, size[1] - TARGET_PADDING * 2)
    scale = min(available[0] / sprite.width, available[1] / sprite.height)
    fitted_size = (
        max(1, round(sprite.width * scale)),
        max(1, round(sprite.height * scale)),
    )
    sprite = sprite.resize(fitted_size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    offset = ((size[0] - fitted_size[0]) // 2, (size[1] - fitted_size[1]) // 2)
    canvas.alpha_composite(sprite, offset)
    return canvas


def write_p3(path: Path, sprite: Image.Image) -> str:
    rows: list[str] = [
        "P3",
        "# Runner v0.7.29 remade modular armor sprite; #ff00ff is transparent",
        f"{sprite.width} {sprite.height}",
        "255",
    ]
    for y in range(sprite.height):
        values: list[str] = []
        for x in range(sprite.width):
            red, green, blue, alpha = sprite.getpixel((x, y))
            if alpha < ALPHA_THRESHOLD:
                red, green, blue = KEY
            elif red >= 248 and green <= 8 and blue >= 248:
                # Reserve exact and near-exact key colors for transparency.
                red = 247
            values.extend((str(red), str(green), str(blue)))
        rows.append(" ".join(values))
    path.write_text("\n".join(rows) + "\n", encoding="ascii", newline="\n")
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    atlas = Image.open(SOURCE).convert("RGBA")
    if atlas.size != EXPECTED_SOURCE_SIZE:
        raise RuntimeError(
            f"unexpected atlas size {atlas.size}; expected {EXPECTED_SOURCE_SIZE}"
        )
    corners = (
        atlas.getpixel((0, 0))[3],
        atlas.getpixel((atlas.width - 1, 0))[3],
        atlas.getpixel((0, atlas.height - 1))[3],
        atlas.getpixel((atlas.width - 1, atlas.height - 1))[3],
    )
    if corners != (0, 0, 0, 0):
        raise RuntimeError(f"atlas corners must be transparent, got {corners}")

    OUTPUT.mkdir(parents=True, exist_ok=True)
    for spec in SPECS:
        sprite = fit_sprite(crop_cell(atlas, spec), spec.size)
        digest = write_p3(OUTPUT / spec.name, sprite)
        print(f"{spec.name} {spec.size[0]}x{spec.size[1]} sha256={digest}")

    supplied_digest = hashlib.sha256(USER_SHEET.read_bytes()).hexdigest()
    if supplied_digest != USER_SHEET_SHA256:
        raise RuntimeError(
            f"unexpected supplied sheet sha256 {supplied_digest}; "
            f"expected {USER_SHEET_SHA256}"
        )
    user_sheet = Image.open(USER_SHEET).convert("RGBA")
    if user_sheet.size != EXPECTED_USER_SHEET_SIZE:
        raise RuntimeError(
            f"unexpected supplied sheet size {user_sheet.size}; "
            f"expected {EXPECTED_USER_SHEET_SIZE}"
        )
    hand = fit_sprite(extract_hand(user_sheet), (40, 28))
    digest = write_p3(OUTPUT / "hand_side.ppm", hand)
    print(f"hand_side.ppm 40x28 sha256={digest}")


if __name__ == "__main__":
    main()
