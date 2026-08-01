from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}")
    target.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "src/ppo.hpp",
    '''#include "simulation.hpp"

#include <array>''',
    '''#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''            rollout_previous_actions_.assign(environment_count, {});''',
    '''            rollout_previous_actions_.assign(environment_count,
                std::array<float, sim::action_count>{});'''
)

print("Applied training-stability compile fixups.")
