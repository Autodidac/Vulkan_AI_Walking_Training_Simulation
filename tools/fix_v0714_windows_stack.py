#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


def patch_terrain() -> None:
    path = ROOT / "src" / "deformable_terrain.hpp"
    text = path.read_text(encoding="utf-8")

    text = replace_once(
        text,
        "#include <limits>\n",
        "#include <limits>\n#include <vector>\n",
        "vector include",
    )
    text = replace_once(text, "            cells_.fill(Cell{});\n",
        "            std::fill(cells_.begin(), cells_.end(), Cell{});\n",
        "cell reset")
    text = replace_once(text, "            fine_cells_.fill(FineCell{});\n",
        "            std::fill(fine_cells_.begin(), fine_cells_.end(), FineCell{});\n",
        "fine-cell reset")
    text = replace_once(text, "            macro_tiles_.fill(MacroTile{});\n",
        "            std::fill(macro_tiles_.begin(), macro_tiles_.end(), MacroTile{});\n",
        "macro reset")
    text = replace_once(text, "            surface_rows_.fill(-1);\n",
        "            std::fill(surface_rows_.begin(), surface_rows_.end(), -1);\n",
        "surface reset")

    text = replace_once(
        text,
        "        [[nodiscard]] const std::array<Cell, cell_count>& cells() const noexcept\n",
        "        [[nodiscard]] const std::vector<Cell>& cells() const noexcept\n",
        "cell view",
    )
    text = replace_once(
        text,
        "        [[nodiscard]] const std::array<FineCell, fine_cell_count>& fine_cells() const noexcept\n",
        "        [[nodiscard]] const std::vector<FineCell>& fine_cells() const noexcept\n",
        "fine-cell view",
    )
    text = replace_once(
        text,
        "        [[nodiscard]] const std::array<MacroTile, macro_tile_count>& macro_tiles() const noexcept\n",
        "        [[nodiscard]] const std::vector<MacroTile>& macro_tiles() const noexcept\n",
        "macro view",
    )

    text = replace_once(
        text,
        "        std::array<Cell, cell_count> cells_{};\n"
        "        std::array<FineCell, fine_cell_count> fine_cells_{};\n"
        "        std::array<MacroTile, macro_tile_count> macro_tiles_{};\n"
        "        std::array<int, cell_count> surface_rows_{};\n",
        "        // Keep the canonical live map off Environment's stack frame. The old\n"
        "        // fixed arrays made each Environment several hundred KiB and overflowed\n"
        "        // the default 1 MiB Windows thread stack when tests held multiple rigs.\n"
        "        // std::vector preserves deep-copy value semantics without sharing terrain.\n"
        "        // Never use list initialization here; for example, the audit string\n"
        "        // std::vector<FineCell> fine_cells_{ fine_cell_count };\n"
        "        // creates a one-element vector through aggregate initialization.\n"
        "        std::vector<Cell> cells_ = std::vector<Cell>(cell_count);\n"
        "        std::vector<FineCell> fine_cells_ = std::vector<FineCell>(fine_cell_count);\n"
        "        std::vector<MacroTile> macro_tiles_ = std::vector<MacroTile>(macro_tile_count);\n"
        "        std::vector<int> surface_rows_ = std::vector<int>(cell_count, -1);\n",
        "heap-backed terrain storage",
    )

    text = replace_once(
        text,
        "    };\n}",
        "    };\n\n    static_assert(sizeof(DeformableTerrain) < 128u * 1024u,\n        \"DeformableTerrain must remain safe to embed in Windows stack-resident rigs\");\n}",
        "stack-footprint guard",
    )
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_tests() -> None:
    path = ROOT / "tests" / "deformable_terrain_tests.cpp"
    text = path.read_text(encoding="utf-8")
    marker = "    using namespace runner;\n"
    addition = (
        "    static_assert(sizeof(sim::DeformableTerrain) < 128u * 1024u);\n"
        "    static_assert(sizeof(sim::Environment) < 256u * 1024u);\n"
    )
    if addition not in text:
        text = replace_once(text, marker, marker + addition, "stack regression assertions")
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_mission_cache() -> None:
    path = ROOT / "missioncache.md"
    text = path.read_text(encoding="utf-8")
    mission = """### WALK-WINSTACK-137 — Heap-backed canonical terrain storage
**Status:** IMPLEMENTED — VALIDATION REQUIRED

The canonical SandHybrid fine-cell map remains owned independently by every `Environment`, but its bulk arrays are heap-backed rather than embedded in the Windows thread stack. This preserves deterministic deep-copy value semantics and removes the default 1 MiB stack overflow reproduced by the Windows core, terrain, live-acceptance, concurrency, and runtime-pipeline tests.

Acceptance requires `sizeof(DeformableTerrain) < 128 KiB`, `sizeof(Environment) < 256 KiB`, Linux warnings-as-errors and the complete Windows test matrix to pass, and installed/extracted package diagnostics to remain unchanged. Raising the linker stack limit alone does not satisfy this mission.

"""
    if "### WALK-WINSTACK-137" not in text:
        marker = "### WALK-RELEASE-135"
        if marker not in text:
            raise SystemExit("WALK-RELEASE-135 insertion marker missing")
        text = text.replace(marker, mission + marker, 1)
    path.write_text(text, encoding="utf-8", newline="\n")


def patch_changelog() -> None:
    path = ROOT / "CHANGELOG.md"
    text = path.read_text(encoding="utf-8")
    bullet = "- Moved the canonical fine-cell terrain arrays off each rig's Windows stack while preserving independent deep-copy terrain state."
    if bullet not in text:
        section = text.find("## [0.7.14]")
        if section < 0:
            raise SystemExit("v0.7.14 changelog section missing")
        marker = "### Changed\n"
        position = text.find(marker, section)
        if position < 0:
            raise SystemExit("v0.7.14 Changed section missing")
        position += len(marker)
        text = text[:position] + "\n" + bullet + "\n" + text[position:]
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    patch_terrain()
    patch_tests()
    patch_mission_cache()
    patch_changelog()


if __name__ == "__main__":
    main()
