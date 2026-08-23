#!/usr/bin/env python3
"""Build deterministic side-view species sprites with a horizontal bone axis."""

from __future__ import annotations

import hashlib
from collections import deque
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ATLAS_DIRECTORY = ROOT / "assets" / "optional" / "species_atlases"
OUTPUT_DIRECTORY = ROOT / "assets" / "optional" / "species_runtime"
DOG_SOURCE = ROOT / "tools" / "art_sources" / "runner_v0744_dog_modules_source.png"
DOG_SOURCE_SHA256 = "56613bf204bb9abb7a1420d9fbd0e3b6516c5f3928310207da65269bccc72e0f"
EXPECTED_SIZE = (1536, 1024)
KEY = (255, 0, 255)
SOURCE_PADDING = 12
TARGET_PADDING = 3


@dataclass(frozen=True)
class SpriteSpec:
    name: str
    cell: tuple[int, int, int, int]
    size: tuple[int, int]
    rotate_counterclockwise: bool = False


SPECS = (
    SpriteSpec("head", (0, 0, 440, 512), (76, 60)),
    SpriteSpec("body", (440, 0, 1070, 512), (96, 56)),
    SpriteSpec("tail", (1070, 0, 1536, 512), (76, 34)),
    SpriteSpec("upper_leg", (0, 512, 512, 1024), (64, 36), True),
    SpriteSpec("lower_leg", (512, 512, 1024, 1024), (60, 34), True),
    SpriteSpec("foot", (1024, 512, 1536, 1024), (56, 34)),
)


def remove_generated_checkerboard(source: Image.Image) -> Image.Image:
    """Flood-fill the baked neutral checker without erasing enclosed white armor."""
    result = source.convert("RGBA")
    pixels = result.load()
    width, height = result.size
    background = bytearray(width * height)
    queue: deque[tuple[int, int]] = deque()

    def is_background_candidate(x: int, y: int) -> bool:
        red, green, blue, _ = pixels[x, y]
        return min(red, green, blue) >= 218 and max(red, green, blue) - min(red, green, blue) <= 18

    def enqueue(x: int, y: int) -> None:
        index = y * width + x
        if background[index] or not is_background_candidate(x, y):
            return
        background[index] = 1
        queue.append((x, y))

    for x in range(width):
        enqueue(x, 0)
        enqueue(x, height - 1)
    for y in range(height):
        enqueue(0, y)
        enqueue(width - 1, y)

    while queue:
        x, y = queue.popleft()
        if x > 0:
            enqueue(x - 1, y)
        if x + 1 < width:
            enqueue(x + 1, y)
        if y > 0:
            enqueue(x, y - 1)
        if y + 1 < height:
            enqueue(x, y + 1)

    for y in range(height):
        for x in range(width):
            red, green, blue, _ = pixels[x, y]
            pixels[x, y] = (red, green, blue, 0 if background[y * width + x] else 255)
    return result


def crop_module(atlas: Image.Image, spec: SpriteSpec, rotate: bool) -> Image.Image:
    cell = atlas.crop(spec.cell)
    bounds = cell.getchannel("A").getbbox()
    if bounds is None:
        raise RuntimeError(f"{spec.name} atlas cell is empty")
    x0, y0, x1, y1 = bounds
    bounds = (
        max(0, x0 - SOURCE_PADDING),
        max(0, y0 - SOURCE_PADDING),
        min(cell.width, x1 + SOURCE_PADDING),
        min(cell.height, y1 + SOURCE_PADDING),
    )
    sprite = cell.crop(bounds)
    if rotate and spec.rotate_counterclockwise and sprite.height > sprite.width:
        sprite = sprite.rotate(90, expand=True)
    return sprite


def fit_sprite(sprite: Image.Image, target_size: tuple[int, int]) -> Image.Image:
    width, height = target_size
    available = (width - TARGET_PADDING * 2, height - TARGET_PADDING * 2)
    fitted = sprite.copy()
    fitted.thumbnail(available, Image.Resampling.LANCZOS)
    output = Image.new("RGBA", target_size, (0, 0, 0, 0))
    output.alpha_composite(fitted, (
        (width - fitted.width) // 2,
        (height - fitted.height) // 2,
    ))
    return output


def write_p3(path: Path, sprite: Image.Image) -> None:
    rgb = Image.new("RGB", sprite.size, KEY)
    rgb.paste(sprite.convert("RGB"), mask=sprite.getchannel("A"))
    lines = ["P3", f"{rgb.width} {rgb.height}", "255"]
    pixels = list(rgb.get_flattened_data())
    for offset in range(0, len(pixels), 12):
        lines.append(" ".join(
            f"{red} {green} {blue}" for red, green, blue in pixels[offset:offset + 12]
        ))
    path.write_text("\n".join(lines) + "\n", encoding="ascii", newline="\n")


def build_species(species: str, atlas: Image.Image, rotate_limbs: bool) -> None:
    for spec in SPECS:
        sprite = fit_sprite(crop_module(atlas, spec, rotate_limbs), spec.size)
        if spec.name in {"upper_leg", "lower_leg"} and sprite.width <= sprite.height:
            raise RuntimeError(f"{species} {spec.name} is not horizontal after normalization")
        write_p3(OUTPUT_DIRECTORY / f"{species}_{spec.name}_side.ppm", sprite)


def main() -> None:
    if hashlib.sha256(DOG_SOURCE.read_bytes()).hexdigest() != DOG_SOURCE_SHA256:
        raise RuntimeError("Dog source atlas checksum does not match the reviewed source")
    dog_source = Image.open(DOG_SOURCE)
    if dog_source.size != EXPECTED_SIZE:
        raise RuntimeError(f"Dog source atlas must be {EXPECTED_SIZE}, got {dog_source.size}")
    dog_atlas = remove_generated_checkerboard(dog_source)
    dog_atlas.save(ATLAS_DIRECTORY / "dog_modules.png")

    sources = {
        "chicken": Image.open(ATLAS_DIRECTORY / "chicken_modules.png").convert("RGBA"),
        "dog": dog_atlas,
        "hexapod": Image.open(ATLAS_DIRECTORY / "hexapod_modules.png").convert("RGBA"),
    }
    OUTPUT_DIRECTORY.mkdir(parents=True, exist_ok=True)
    for species, atlas in sources.items():
        if atlas.size != EXPECTED_SIZE:
            raise RuntimeError(f"{species} atlas must be {EXPECTED_SIZE}, got {atlas.size}")
        build_species(species, atlas, rotate_limbs=species != "dog")
    print("Generated horizontal-axis Chicken, Dog, and Hexapod runtime art")


if __name__ == "__main__":
    main()
