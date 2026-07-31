from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


network = Path("src/ppo_network.cpp")
text = network.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        constexpr float log_two_pi = 1.83787706640934548356f;
        constexpr float epsilon = 1.0e-8f;
""",
    """        constexpr float log_two_pi = 1.83787706640934548356f;
""",
    "unused PPO constant",
)
network.write_text(text, encoding="utf-8")

runtime = Path("src/autonomy_runtime.cpp")
text = runtime.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    void AutonomousTrainer::set_background_enabled(bool enabled) noexcept
    {
        enabled_.store(enabled, std::memory_order_relaxed);
        wake_cv_.notify_all();
    }
""",
    """    void AutonomousTrainer::set_background_enabled(bool enabled) noexcept
    {
        enabled_.store(enabled, std::memory_order_relaxed);
        if (!enabled)
            requested_updates_.store(0u, std::memory_order_relaxed);
        wake_cv_.notify_all();
    }
""",
    "pause command drain",
)
runtime.write_text(text, encoding="utf-8")

for path_name in ("tests/core_tests.cpp", "tests/concurrency_benchmark.cpp"):
    path = Path(path_name)
    text = path.read_text(encoding="utf-8")
    text = text.replace("EpochRunner v0.6.1", "EpochRunner v0.6.2")
    path.write_text(text, encoding="utf-8")

readme = Path("README.md")
text = readme.read_text(encoding="utf-8")
needle = "Walking reward now requires foot-led gait evidence. Sliding forward with both feet planted receives no startup progress credit, grounded foot slip is penalized, sustained wheel-like motion is a hard invalid gate, and a knee crossing a rock or hurdle before its corresponding foot receives an explicit penalty and visible fault count.\n"
replacement = needle + "\nPausing background training now clears queued single-update backlog immediately, and the unused PPO constant that blocked strict Linux warning builds has been removed.\n"
text = replace_once(text, needle, replacement, "README runtime cleanup note")
readme.write_text(text, encoding="utf-8")
