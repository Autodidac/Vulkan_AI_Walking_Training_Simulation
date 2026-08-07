#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_exact(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match for {old!r}, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")


replace_exact("src/ppo.hpp", "const sim::Vec2 root =", "const Vec2 root =")
replace_exact("src/ppo_trainer.cpp", "    PpoTrainer::PpoTrainer    }\n\n", "")
print("Runner v0.7.19 compile repair applied")
