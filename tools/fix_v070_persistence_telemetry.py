from pathlib import Path

path = Path("src/autonomy_runtime.cpp")
text = path.read_text(encoding="utf-8")

old_start = """    void AutonomousTrainer::synchronize()
    {
        cached_status_.pipeline_stage = std::string(routine_stage_name(
            pipeline_stage_.load(std::memory_order_relaxed)));
        cached_status_.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);

        PublishedSnapshot snapshot{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (published_.serial == applied_serial_)
                return;
            snapshot = published_;
        }
"""
new_start = """    void AutonomousTrainer::synchronize()
    {
        const RoutineStage live_pipeline_stage = pipeline_stage_.load(std::memory_order_relaxed);
        const std::uint64_t live_pipeline_suspensions =
            pipeline_suspensions_.load(std::memory_order_relaxed);
        const bool live_persistence_pending =
            persistence_pending_.load(std::memory_order_relaxed);
        const std::uint64_t live_persistence_completed =
            persistence_completed_.load(std::memory_order_relaxed);

        PublishedSnapshot snapshot{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (published_.serial == applied_serial_)
            {
                cached_status_.pipeline_stage = std::string(routine_stage_name(live_pipeline_stage));
                cached_status_.pipeline_suspensions = live_pipeline_suspensions;
                cached_status_.persistence_pending = live_persistence_pending;
                cached_status_.persistence_completed = live_persistence_completed;
                return;
            }
            snapshot = published_;
        }
"""
if new_start not in text:
    if old_start not in text:
        raise SystemExit("synchronize start anchor not found")
    text = text.replace(old_start, new_start, 1)

old_end = """        cached_controller_state_ = snapshot.controller_state;
        cached_status_ = std::move(snapshot.status);
        cached_exploration_ = snapshot.exploration;
"""
new_end = """        cached_controller_state_ = snapshot.controller_state;
        cached_status_ = std::move(snapshot.status);
        cached_status_.pipeline_stage = std::string(routine_stage_name(live_pipeline_stage));
        cached_status_.pipeline_suspensions = live_pipeline_suspensions;
        cached_status_.persistence_pending = live_persistence_pending;
        cached_status_.persistence_completed = live_persistence_completed;
        cached_exploration_ = snapshot.exploration;
"""
if new_end not in text:
    if old_end not in text:
        raise SystemExit("synchronize end anchor not found")
    text = text.replace(old_end, new_end, 1)

path.write_text(text, encoding="utf-8")
