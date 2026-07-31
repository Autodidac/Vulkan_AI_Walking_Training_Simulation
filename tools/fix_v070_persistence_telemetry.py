from pathlib import Path

path = Path("src/autonomy_runtime.cpp")
text = path.read_text(encoding="utf-8")
old = """        cached_status_.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        cached_status_.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);

        PublishedSnapshot snapshot{};
"""
new = """        cached_status_.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        cached_status_.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);
        cached_status_.persistence_pending = persistence_pending_.load(std::memory_order_relaxed);
        cached_status_.persistence_completed = persistence_completed_.load(std::memory_order_relaxed);

        PublishedSnapshot snapshot{};
"""
if new not in text:
    if old not in text:
        raise SystemExit("persistence telemetry anchor not found")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")
