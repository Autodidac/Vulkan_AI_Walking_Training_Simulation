from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label}: expected source anchor not found")
    return text.replace(old, new, 1)


ppo = Path("src/ppo.hpp")
text = ppo.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    enum class ControllerState : std::uint8_t
    {
        fresh,
        training,
        resumed,
        transferred
    };

    class PolicyNetwork
""",
    """    enum class ControllerState : std::uint8_t
    {
        fresh,
        training,
        resumed,
        transferred
    };

    struct CheckpointSnapshot
    {
        std::uint64_t signature{};
        std::uint64_t adam_step{};
        std::uint64_t random_state{};
        TrainingMetrics metrics{};
        sim::CourseStage stage{ sim::CourseStage::balance };
        float difficulty{ 0.25f };
        std::vector<float> parameters{};
        std::vector<float> first_moment{};
        std::vector<float> second_moment{};
        std::vector<float> best_parameters{};
        std::vector<float> reward_history{};
        std::vector<float> speed_history{};
    };

    class PolicyNetwork
""",
    "checkpoint snapshot type",
)
text = replace_once(
    text,
    """        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
""",
    """        [[nodiscard]] CheckpointSnapshot checkpoint_snapshot() const;
        [[nodiscard]] static bool save_checkpoint_snapshot(const CheckpointSnapshot& snapshot,
            const std::filesystem::path& path, std::string& error);
        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
""",
    "checkpoint snapshot API",
)
ppo.write_text(text, encoding="utf-8")

checkpoint = Path("src/training_checkpoint.cpp")
text = checkpoint.read_text(encoding="utf-8")
start = text.index("    bool PpoTrainer::save_checkpoint(")
end = text.index("\n    bool PpoTrainer::load_checkpoint(", start)
replacement = r'''    CheckpointSnapshot PpoTrainer::checkpoint_snapshot() const
    {
        CheckpointSnapshot snapshot{};
        snapshot.signature = blueprint_.signature();
        snapshot.adam_step = adam_.step;
        snapshot.random_state = random_state_;
        snapshot.metrics = metrics_;
        snapshot.stage = course_stage_;
        snapshot.difficulty = course_difficulty_;
        snapshot.parameters = policy_.parameters();
        snapshot.first_moment = adam_.first_moment;
        snapshot.second_moment = adam_.second_moment;
        snapshot.best_parameters = best_parameters_;
        snapshot.reward_history = reward_history_;
        snapshot.speed_history = speed_history_;
        return snapshot;
    }

    bool PpoTrainer::save_checkpoint_snapshot(const CheckpointSnapshot& snapshot,
        const std::filesystem::path& path, std::string& error)
    {
        if (path.empty())
        {
            error = "Checkpoint path is empty.";
            return false;
        }
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + temporary.string();
            return false;
        }

        const std::uint64_t parameter_count = snapshot.parameters.size();
        const std::uint64_t reward_count = snapshot.reward_history.size();
        const std::uint64_t speed_count = snapshot.speed_history.size();
        const std::uint64_t best_count = snapshot.best_parameters.size();
        const auto stage = static_cast<std::uint8_t>(snapshot.stage);
        const std::uint8_t evaluation_valid = snapshot.metrics.evaluation_valid ? 1u : 0u;
        if (parameter_count == 0
            || snapshot.first_moment.size() != parameter_count
            || snapshot.second_moment.size() != parameter_count
            || (best_count != 0 && best_count != parameter_count))
        {
            error = "Invalid immutable checkpoint payload dimensions.";
            return false;
        }

        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        const TrainingMetrics& metrics = snapshot.metrics;
        bool ok = write_value(output, snapshot.signature) && write_value(output, parameter_count)
            && write_value(output, reward_count) && write_value(output, speed_count)
            && write_value(output, best_count) && write_value(output, snapshot.adam_step)
            && write_value(output, snapshot.random_state) && write_value(output, metrics.update)
            && write_value(output, metrics.environment_steps) && write_value(output, metrics.best_update)
            && write_value(output, metrics.evaluation_count)
            && write_value(output, stage) && write_value(output, snapshot.difficulty)
            && write_value(output, metrics.mean_reward) && write_value(output, metrics.mean_episode_distance)
            && write_value(output, metrics.mean_speed) && write_value(output, metrics.policy_loss)
            && write_value(output, metrics.value_loss) && write_value(output, metrics.entropy)
            && write_value(output, metrics.learning_rate) && write_value(output, metrics.evaluation_reward)
            && write_value(output, metrics.evaluation_distance) && write_value(output, metrics.evaluation_speed)
            && write_value(output, metrics.evaluation_score) && write_value(output, metrics.evaluation_survival)
            && write_value(output, metrics.evaluation_collisions) && write_value(output, metrics.evaluation_airborne_ratio)
            && write_value(output, metrics.evaluation_stride_events) && write_value(output, metrics.evaluation_invalid_runs)
            && write_value(output, evaluation_valid)
            && write_value(output, metrics.best_evaluation_distance)
            && write_value(output, metrics.best_evaluation_score)
            && write_vector(output, snapshot.parameters) && write_vector(output, snapshot.first_moment)
            && write_vector(output, snapshot.second_moment) && write_vector(output, snapshot.best_parameters)
            && write_vector(output, snapshot.reward_history) && write_vector(output, snapshot.speed_history);
        if (!ok)
        {
            error = "Failed while writing checkpoint: " + path.string();
            return false;
        }
        output.close();
        if (!output)
        {
            error = "Failed while finalizing checkpoint: " + temporary.string();
            return false;
        }
        std::error_code filesystem_error{};
        std::filesystem::remove(path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace checkpoint atomically: " + filesystem_error.message();
            return false;
        }
        error.clear();
        return true;
    }

    bool PpoTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        return save_checkpoint_snapshot(checkpoint_snapshot(), path, error);
    }
'''
text = text[:start] + replacement + text[end:]
checkpoint.write_text(text, encoding="utf-8")

header = Path("src/autonomy.hpp")
text = header.read_text(encoding="utf-8")
text = replace_once(
    text,
    "#include <mutex>\n#include <span>\n",
    "#include <mutex>\n#include <optional>\n#include <span>\n",
    "optional include",
)
text = replace_once(
    text,
    """        bool worker_busy{};
        std::uint64_t pipeline_suspensions{};
""",
    """        bool worker_busy{};
        bool persistence_pending{};
        std::uint64_t persistence_completed{};
        std::uint64_t pipeline_suspensions{};
""",
    "persistence status fields",
)
text = replace_once(
    text,
    """        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
""",
    """        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error);
        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,
""",
    "nonblocking checkpoint API",
)
text = replace_once(
    text,
    """            set_exploration,
            restore_best
""",
    """            set_exploration,
            restore_best,
            save_checkpoint,
            load_checkpoint
""",
    "persistence command types",
)
text = replace_once(
    text,
    """            std::uint64_t seed{};
            float scalar{};
        };

        struct PublishedSnapshot
""",
    """            std::uint64_t seed{};
            float scalar{};
            std::filesystem::path path{};
            bool transfer_only{};
        };

        struct PersistenceRequest
        {
            CheckpointSnapshot checkpoint{};
            sim::CreatureBlueprint blueprint{};
            std::filesystem::path checkpoint_path{};
            std::filesystem::path rig_path{};
            std::filesystem::path state_path{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            float difficulty{ 0.25f };
            std::uint64_t rig_generation{};
            std::uint64_t accepted_rig_changes{};
            std::uint64_t rejected_rig_changes{};
            int rollback_count{};
        };

        struct PublishedSnapshot
""",
    "persistence request type",
)
text = replace_once(
    text,
    """        void autosave_locked();
        void write_state_locked() const;
        void read_state_locked();
""",
    """        void autosave_locked();
        void enqueue_persistence(PersistenceRequest request);
        void persistence_main(std::stop_token stop_token);
        [[nodiscard]] static bool write_persistence_state(
            const PersistenceRequest& request, std::string& error);
        void write_state_locked() const;
        void read_state_locked();
""",
    "persistence methods",
)
text = replace_once(
    text,
    """        mutable std::mutex command_mutex_{};
        mutable std::mutex wake_mutex_{};
""",
    """        mutable std::mutex command_mutex_{};
        mutable std::mutex persistence_mutex_{};
        mutable std::mutex wake_mutex_{};
""",
    "persistence mutex",
)
text = replace_once(
    text,
    """        std::condition_variable_any wake_cv_{};
        std::deque<PendingCommand> command_queue_{};
""",
    """        std::condition_variable_any wake_cv_{};
        std::condition_variable_any persistence_cv_{};
        std::deque<PendingCommand> command_queue_{};
        std::optional<PersistenceRequest> pending_persistence_{};
        std::string persistence_error_{};
""",
    "persistence queue",
)
text = replace_once(
    text,
    """        std::atomic<RoutineStage> pipeline_stage_{ RoutineStage::idle };
        std::atomic_uint64_t pipeline_suspensions_{};
        std::jthread worker_thread_{};
""",
    """        std::atomic<RoutineStage> pipeline_stage_{ RoutineStage::idle };
        std::atomic_uint64_t pipeline_suspensions_{};
        std::atomic_bool persistence_pending_{ false };
        std::atomic_uint64_t persistence_completed_{};
        std::jthread persistence_thread_{};
        std::jthread worker_thread_{};
""",
    "persistence thread fields",
)
header.write_text(text, encoding="utf-8")

runtime = Path("src/autonomy_runtime.cpp")
text = runtime.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        publish_locked();
        synchronize();
        worker_thread_ = std::jthread([this](std::stop_token stop_token)
""",
    """        publish_locked();
        synchronize();
        persistence_thread_ = std::jthread([this](std::stop_token stop_token)
        {
            persistence_main(stop_token);
        });
        worker_thread_ = std::jthread([this](std::stop_token stop_token)
""",
    "start persistence thread",
)
text = replace_once(
    text,
    """    AutonomousTrainer::~AutonomousTrainer()
    {
        if (worker_thread_.joinable())
        {
            worker_thread_.request_stop();
            wake_cv_.notify_all();
        }
    }
""",
    """    AutonomousTrainer::~AutonomousTrainer()
    {
        if (worker_thread_.joinable())
        {
            worker_thread_.request_stop();
            wake_cv_.notify_all();
            worker_thread_.join();
        }
        if (persistence_thread_.joinable())
        {
            persistence_thread_.request_stop();
            persistence_cv_.notify_all();
            persistence_thread_.join();
        }
    }
""",
    "join runtime threads",
)
runtime.write_text(text, encoding="utf-8")

commands = Path("src/autonomy_commands.cpp")
text = commands.read_text(encoding="utf-8")
text = replace_once(
    text,
    """    {
        std::scoped_lock lock(worker_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }
""",
    """    {
        std::scoped_lock lock(persistence_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }
""",
    "nonblocking autosave path configuration",
)
start = text.index("    bool AutonomousTrainer::save_checkpoint(")
end = text.index("\n    void AutonomousTrainer::enqueue_command", start)
replacement = r'''    bool AutonomousTrainer::save_checkpoint(
        const std::filesystem::path& path, std::string& error)
    {
        if (path.empty())
        {
            error = "Checkpoint path is empty.";
            return false;
        }
        PendingCommand command{};
        command.type = CommandType::save_checkpoint;
        command.path = path;
        enqueue_command(std::move(command));
        error = "CHECKPOINT SAVE QUEUED";
        return true;
    }

    bool AutonomousTrainer::load_checkpoint(const std::filesystem::path& path,
        std::string& error, bool transfer_only)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            error = "Checkpoint does not exist: " + path.string();
            return false;
        }
        PendingCommand command{};
        command.type = CommandType::load_checkpoint;
        command.path = path;
        command.transfer_only = transfer_only;
        enqueue_command(std::move(command));
        error = "CHECKPOINT LOAD QUEUED";
        return true;
    }
'''
text = text[:start] + replacement + text[end:]
text = replace_once(
    text,
    """        case CommandType::restore_best:
            if (worker_.restore_best_policy())
            {
                ++rollback_count_;
                worker_message_ = "BEST VERIFIED CONTROLLER RESTORED";
            }
            break;
        }
""",
    """        case CommandType::restore_best:
            if (worker_.restore_best_policy())
            {
                ++rollback_count_;
                worker_message_ = "BEST VERIFIED CONTROLLER RESTORED";
            }
            break;

        case CommandType::save_checkpoint:
        {
            PersistenceRequest request{};
            request.checkpoint = worker_.checkpoint_snapshot();
            request.checkpoint_path = std::move(command.path);
            enqueue_persistence(std::move(request));
            worker_message_ = "CHECKPOINT SNAPSHOT QUEUED FOR ASYNCHRONOUS SAVE";
            break;
        }

        case CommandType::load_checkpoint:
        {
            std::string load_error{};
            if (worker_.load_checkpoint(command.path, load_error, command.transfer_only))
            {
                stage_ = worker_.course_stage();
                difficulty_ = worker_.course_difficulty();
                worker_message_ = command.transfer_only
                    ? "CONTROLLER TRANSFERRED - AUTOPILOT RECALIBRATING"
                    : "CHECKPOINT RESUMED - AUTOPILOT CONTINUING";
            }
            else
            {
                worker_message_ = load_error;
            }
            break;
        }
        }
""",
    "persistence command handlers",
)
commands.write_text(text, encoding="utf-8")

persistence = Path("src/autonomy_persistence.cpp")
text = persistence.read_text(encoding="utf-8")
text = text.replace("#include <fstream>\n#include <utility>\n", "#include <fstream>\n#include <utility>\n")
text = replace_once(
    text,
    """        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);
""",
    """        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.persistence_pending = persistence_pending_.load(std::memory_order_relaxed);
        snapshot.status.persistence_completed = persistence_completed_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_suspensions = pipeline_suspensions_.load(std::memory_order_relaxed);
""",
    "persistence telemetry publication",
)
start = text.index("    void AutonomousTrainer::autosave_locked()")
end = text.index("\n    void AutonomousTrainer::write_state_locked() const", start)
replacement = r'''    void AutonomousTrainer::enqueue_persistence(PersistenceRequest request)
    {
        {
            std::scoped_lock lock(persistence_mutex_);
            pending_persistence_ = std::move(request);
            persistence_error_.clear();
            persistence_pending_.store(true, std::memory_order_relaxed);
        }
        persistence_cv_.notify_one();
    }

    bool AutonomousTrainer::write_persistence_state(
        const PersistenceRequest& request, std::string& error)
    {
        if (request.state_path.empty())
            return true;
        const std::filesystem::path temporary = request.state_path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
        {
            error = "Could not open autonomy state for writing: " + temporary.string();
            return false;
        }
        output << "EPOCHAUTONOMY 2\n";
        output << static_cast<int>(request.stage) << ' ' << request.difficulty << ' '
            << request.rig_generation << ' ' << request.accepted_rig_changes << ' '
            << request.rejected_rig_changes << ' ' << request.rollback_count << '\n';
        output.close();
        if (!output)
        {
            error = "Failed while finalizing autonomy state: " + temporary.string();
            return false;
        }
        std::error_code filesystem_error{};
        std::filesystem::remove(request.state_path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, request.state_path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace autonomy state atomically: " + filesystem_error.message();
            return false;
        }
        return true;
    }

    void AutonomousTrainer::persistence_main(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            PersistenceRequest request{};
            {
                std::unique_lock lock(persistence_mutex_);
                persistence_cv_.wait(lock, stop_token, [this]
                {
                    return pending_persistence_.has_value();
                });
                if (stop_token.stop_requested())
                    break;
                request = std::move(*pending_persistence_);
                pending_persistence_.reset();
            }

            std::string error{};
            bool ok = true;
            if (!request.checkpoint_path.empty())
            {
                ok = PpoTrainer::save_checkpoint_snapshot(
                    request.checkpoint, request.checkpoint_path, error);
            }
            if (ok && !request.rig_path.empty())
                ok = request.blueprint.save(request.rig_path, error);
            if (ok)
                ok = write_persistence_state(request, error);

            {
                std::scoped_lock lock(persistence_mutex_);
                persistence_error_ = std::move(error);
                const bool has_newer = pending_persistence_.has_value();
                persistence_pending_.store(has_newer, std::memory_order_relaxed);
                if (ok)
                    persistence_completed_.fetch_add(1u, std::memory_order_relaxed);
            }
        }
        persistence_pending_.store(false, std::memory_order_relaxed);
    }

    void AutonomousTrainer::autosave_locked()
    {
        PersistenceRequest request{};
        request.checkpoint = worker_.checkpoint_snapshot();
        request.blueprint = worker_.blueprint();
        request.stage = stage_;
        request.difficulty = difficulty_;
        request.rig_generation = rig_generation_;
        request.accepted_rig_changes = accepted_rig_changes_;
        request.rejected_rig_changes = rejected_rig_changes_;
        request.rollback_count = rollback_count_;
        {
            std::scoped_lock lock(persistence_mutex_);
            request.checkpoint_path = autosave_checkpoint_;
            request.rig_path = autosave_rig_;
            request.state_path = autosave_state_;
        }
        enqueue_persistence(std::move(request));
    }
'''
text = text[:start] + replacement + text[end:]
persistence.write_text(text, encoding="utf-8")

app = Path("src/app.cpp")
text = app.read_text(encoding="utf-8")
text = replace_once(
    text,
    """            add_text(canvas, cursor, std::format("PIPELINE {}   SUSPENSIONS {}",
                autonomy.pipeline_stage, autonomy.pipeline_suspensions), 1.02f, muted);
            cursor.y += 23.0f;
""",
    """            add_text(canvas, cursor, std::format("PIPELINE {}   SUSPENSIONS {}",
                autonomy.pipeline_stage, autonomy.pipeline_suspensions), 1.02f, muted);
            cursor.y += 23.0f;
            add_text(canvas, cursor, std::format("I/O {}   SAVES {}",
                autonomy.persistence_pending ? "PENDING" : "IDLE",
                autonomy.persistence_completed), 1.02f,
                autonomy.persistence_pending ? yellow : muted);
            cursor.y += 23.0f;
""",
    "persistence UI telemetry",
)
app.write_text(text, encoding="utf-8")

tests = Path("tests/core_tests.cpp")
text = tests.read_text(encoding="utf-8")
text = replace_once(
    text,
    """        require(!autonomous.autonomy_status().pipeline_stage.empty(),
            "coroutine pipeline stage telemetry is missing");
""",
    """        require(!autonomous.autonomy_status().pipeline_stage.empty(),
            "coroutine pipeline stage telemetry is missing");

        const std::filesystem::path async_checkpoint =
            std::filesystem::temp_directory_path() / "epochrunner-v070-async-save.eppo";
        std::string async_error{};
        const auto save_started = std::chrono::steady_clock::now();
        require(autonomous.save_checkpoint(async_checkpoint, async_error),
            "asynchronous checkpoint request was rejected");
        require(std::chrono::steady_clock::now() - save_started < std::chrono::milliseconds(20),
            "checkpoint save blocked the caller on serialization or disk I/O");
        for (int attempt = 0; attempt < 2000 && !std::filesystem::exists(async_checkpoint); ++attempt)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            autonomous.synchronize();
        }
        require(std::filesystem::exists(async_checkpoint),
            "asynchronous checkpoint was never written");
        require(autonomous.autonomy_status().persistence_completed > 0,
            "asynchronous persistence completion was not published");
        std::error_code remove_error{};
        std::filesystem::remove(async_checkpoint, remove_error);
""",
    "asynchronous persistence tests",
)
tests.write_text(text, encoding="utf-8")
