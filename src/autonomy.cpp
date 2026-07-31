#include "autonomy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <format>
#include <limits>
#include <thread>
#include <utility>

namespace epochrunner::rl
{
    AutonomousTrainer::TrainingRoutine AutonomousTrainer::TrainingRoutine::promise_type::get_return_object() noexcept
    {
        return TrainingRoutine{ std::coroutine_handle<promise_type>::from_promise(*this) };
    }

    void AutonomousTrainer::TrainingRoutine::promise_type::unhandled_exception() const noexcept
    {
        std::terminate();
    }

    AutonomousTrainer::TrainingRoutine::TrainingRoutine(TrainingRoutine&& other) noexcept
        : handle_(std::exchange(other.handle_, {}))
    {
    }

    AutonomousTrainer::TrainingRoutine& AutonomousTrainer::TrainingRoutine::operator=(TrainingRoutine&& other) noexcept
    {
        if (this == &other)
            return *this;
        if (handle_)
            handle_.destroy();
        handle_ = std::exchange(other.handle_, {});
        return *this;
    }

    AutonomousTrainer::TrainingRoutine::~TrainingRoutine()
    {
        if (handle_)
            handle_.destroy();
    }

    bool AutonomousTrainer::TrainingRoutine::resume()
    {
        if (!handle_ || handle_.done())
            return false;
        handle_.resume();
        return !handle_.done();
    }

    AutonomousTrainer::AutonomousTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count)
        : worker_(blueprint, environment_count), live_(blueprint, 8, false), live_blueprint_(blueprint)
    {
        worker_.set_course(stage_, difficulty_, false);
        live_.set_course(stage_, difficulty_, false);
        publish_locked();
        synchronize();
        worker_thread_ = std::jthread([this](std::stop_token stop_token) { worker_main(stop_token); });
    }

    AutonomousTrainer::~AutonomousTrainer() = default;

    void AutonomousTrainer::synchronize()
    {
        PublishedSnapshot snapshot{};
        {
            std::scoped_lock lock(snapshot_mutex_);
            if (published_.serial == applied_serial_)
                return;
            snapshot = published_;
        }

        const bool rig_changed = snapshot.blueprint.signature() != live_blueprint_.signature();
        const bool best_changed = snapshot.has_best
            && snapshot.metrics.best_update != cached_metrics_.best_update;
        const bool course_changed = snapshot.status.stage != cached_status_.stage
            || std::abs(snapshot.status.difficulty - cached_status_.difficulty) > 1.0e-5f;
        if (rig_changed)
        {
            live_blueprint_ = snapshot.blueprint;
            live_.set_blueprint(live_blueprint_, false);
        }
        live_.set_course(snapshot.status.stage, snapshot.status.difficulty, false);
        live_.policy().parameters() = snapshot.parameters;
        if (rig_changed || best_changed || course_changed)
            live_.reset_preview(0xDEADBEEFu + snapshot.metrics.update + snapshot.metrics.best_update);

        cached_metrics_ = snapshot.metrics;
        cached_reward_history_ = std::move(snapshot.reward_history);
        cached_speed_history_ = std::move(snapshot.speed_history);
        cached_controller_state_ = snapshot.controller_state;
        cached_status_ = std::move(snapshot.status);
        cached_exploration_ = snapshot.exploration;
        cached_optimizer_step_ = snapshot.optimizer_step;
        cached_has_best_ = snapshot.has_best;
        applied_serial_ = snapshot.serial;
    }

    void AutonomousTrainer::set_background_enabled(bool enabled) noexcept
    {
        enabled_.store(enabled);
    }

    void AutonomousTrainer::set_updates_per_cycle(int updates) noexcept
    {
        updates_per_cycle_.store(std::clamp(updates, 1, 4));
    }

    void AutonomousTrainer::set_autosave_paths(std::filesystem::path checkpoint, std::filesystem::path rig,
        std::filesystem::path state)
    {
        std::scoped_lock lock(worker_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }

    bool AutonomousTrainer::load_autosave(std::string& message)
    {
        std::scoped_lock lock(worker_mutex_);
        bool loaded_anything = false;
        read_state_locked();
        if (std::filesystem::exists(autosave_rig_))
        {
            std::string rig_error{};
            const sim::CreatureBlueprint loaded = sim::CreatureBlueprint::load(autosave_rig_, rig_error);
            if (rig_error.empty())
            {
                worker_.set_blueprint(loaded, false);
                loaded_anything = true;
            }
            else
            {
                message = rig_error;
            }
        }
        worker_.set_course(stage_, difficulty_, false);
        if (std::filesystem::exists(autosave_checkpoint_))
        {
            std::string checkpoint_error{};
            if (worker_.load_checkpoint(autosave_checkpoint_, checkpoint_error, false))
            {
                stage_ = worker_.course_stage();
                difficulty_ = worker_.course_difficulty();
                loaded_anything = true;
                worker_message_ = "AUTOSAVE RESUMED - TRAINING CONTINUES IN BACKGROUND";
            }
            else
            {
                message = checkpoint_error;
            }
        }
        publish_locked();
        if (loaded_anything)
            message = worker_message_;
        else if (message.empty())
            message = "NO V0.4 AUTOSAVE FOUND - STARTING WITH BALANCE TRAINING";
        return loaded_anything;
    }

    void AutonomousTrainer::set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy)
    {
        std::scoped_lock lock(worker_mutex_);
        if (!preserve_policy)
        {
            stage_ = sim::CourseStage::balance;
            difficulty_ = 0.25f;
            rig_generation_ = 0;
            accepted_rig_changes_ = 0;
            rejected_rig_changes_ = 0;
            rollback_count_ = 0;
        }
        worker_.set_blueprint(blueprint, preserve_policy);
        worker_.set_course(stage_, difficulty_, false);
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        worker_message_ = preserve_policy
            ? "RIG UPDATED - CONTROLLER TRANSFERRED AND RECALIBRATING"
            : "RIG UPDATED - FRESH CONTROLLER STARTED AUTOMATICALLY";
        publish_locked();
    }

    void AutonomousTrainer::reset_policy(std::uint64_t seed)
    {
        std::scoped_lock lock(worker_mutex_);
        worker_.reset_policy(seed);
        worker_.set_course(stage_, difficulty_, false);
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        worker_message_ = "CONTROLLER RESET - AUTOPILOT RESTARTED CURRENT LESSON";
        publish_locked();
    }

    void AutonomousTrainer::set_exploration(float standard_deviation) noexcept
    {
        std::scoped_lock lock(worker_mutex_);
        worker_.set_exploration(standard_deviation);
        publish_locked();
    }

    void AutonomousTrainer::train_one_update() noexcept
    {
        requested_updates_.fetch_add(1);
    }

    void AutonomousTrainer::step_preview(float dt)
    {
        live_.step_preview(dt);
    }

    void AutonomousTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        live_.reset_preview(seed);
    }

    bool AutonomousTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        std::scoped_lock lock(worker_mutex_);
        return worker_.save_checkpoint(path, error);
    }

    bool AutonomousTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        std::scoped_lock lock(worker_mutex_);
        const bool loaded = worker_.load_checkpoint(path, error, transfer_only);
        if (loaded)
        {
            stage_ = worker_.course_stage();
            difficulty_ = worker_.course_difficulty();
            worker_message_ = transfer_only
                ? "CONTROLLER TRANSFERRED - AUTOPILOT RECALIBRATING"
                : "CHECKPOINT RESUMED - AUTOPILOT CONTINUING";
            publish_locked();
        }
        return loaded;
    }

    bool AutonomousTrainer::restore_best_policy() noexcept
    {
        std::scoped_lock lock(worker_mutex_);
        const bool restored = worker_.restore_best_policy();
        if (restored)
        {
            ++rollback_count_;
            worker_message_ = "BEST VERIFIED CONTROLLER RESTORED";
            publish_locked();
        }
        return restored;
    }

    std::string_view AutonomousTrainer::controller_state_name() const noexcept
    {
        switch (cached_controller_state_)
        {
        case ControllerState::fresh: return "FRESH";
        case ControllerState::training: return "TRAINING";
        case ControllerState::resumed: return "RESUMED";
        case ControllerState::transferred: return "TRANSFERRED";
        }
        return "UNKNOWN";
    }

    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            run_training_cycle();
            co_yield 0;
        }
    }

    void AutonomousTrainer::worker_main(std::stop_token stop_token)
    {
        TrainingRoutine routine = training_routine(stop_token);
        while (!stop_token.stop_requested())
        {
            if (!enabled_.load() && requested_updates_.load() == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(8));
                continue;
            }
            if (!routine.resume())
                break;
            if (updates_per_cycle_.load() < 4)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else
                std::this_thread::yield();
        }
    }

    void AutonomousTrainer::run_training_cycle()
    {
        std::scoped_lock lock(worker_mutex_);
        int update_count = enabled_.load() ? updates_per_cycle_.load() : 0;
        const std::uint32_t requested = requested_updates_.exchange(0);
        update_count += static_cast<int>(requested);
        update_count = std::clamp(update_count, 0, 4);
        if (update_count == 0)
            return;
        for (int update = 0; update < update_count; ++update)
        {
            worker_.train_one_update();
            manage_curriculum_locked();
        }
        publish_locked();
    }

    bool AutonomousTrainer::stage_mastered_locked() const noexcept
    {
        const TrainingMetrics& metrics = worker_.metrics();
        if (!metrics.evaluation_valid)
            return false;
        switch (stage_)
        {
        case sim::CourseStage::balance:
            return metrics.evaluation_survival >= 10.0f && metrics.evaluation_score >= 0.55f;
        case sim::CourseStage::walk:
            return metrics.evaluation_distance >= 3.0f && metrics.evaluation_stride_events >= 3.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_distance >= 3.5f && metrics.evaluation_survival >= 7.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 4.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_collisions <= 3.0f;
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_collisions <= 3.0f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 6.0f && metrics.evaluation_collisions <= 4.0f;
        }
        return false;
    }

    void AutonomousTrainer::manage_curriculum_locked()
    {
        const TrainingMetrics& metrics = worker_.metrics();
        if (metrics.evaluation_count == 0 || metrics.evaluation_count == last_evaluation_count_)
            return;
        last_evaluation_count_ = metrics.evaluation_count;

        if (worker_.has_best_policy() && metrics.best_update != last_saved_best_update_)
        {
            last_saved_best_update_ = metrics.best_update;
            autosave_locked();
        }

        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        if (worker_.has_best_policy() && metrics.evaluation_valid)
        {
            const float tolerance = std::max(0.35f, std::abs(metrics.best_evaluation_score) * 0.35f);
            degradation_streak_ = metrics.evaluation_score + tolerance < metrics.best_evaluation_score
                ? degradation_streak_ + 1 : 0;
            if (degradation_streak_ >= 2 && worker_.restore_best_policy())
            {
                ++rollback_count_;
                degradation_streak_ = 0;
                worker_message_ = "PERFORMANCE DROPPED - RESTORED BEST VALID WALKER";
            }
        }

        if (!metrics.evaluation_valid)
        {
            worker_message_ = std::format("INVALID RUN REJECTED - {} OF 6 FAILED WALKING GATES",
                metrics.evaluation_invalid_runs);
        }
        else if (mastery_streak_ >= 3)
        {
            advance_stage_locked();
        }
        else if (stage_ != sim::CourseStage::balance && metrics.evaluation_count % 4 == 0)
        {
            attempt_rig_evolution_locked();
        }
        else
        {
            worker_message_ = std::format("{} - MASTERY {}/3", sim::course_stage_name(stage_), mastery_streak_);
        }
    }

    void AutonomousTrainer::advance_stage_locked()
    {
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        if (stage_ != sim::CourseStage::moving_hazards)
        {
            stage_ = static_cast<sim::CourseStage>(static_cast<std::uint8_t>(stage_) + 1u);
            difficulty_ = 0.30f;
            worker_message_ = std::format("LESSON COMPLETE - ADVANCING TO {}", sim::course_stage_name(stage_));
        }
        else
        {
            difficulty_ = std::min(1.0f, difficulty_ + 0.10f);
            worker_message_ = std::format("FULL COURSE MASTERED - DIFFICULTY {:.0f}%", difficulty_ * 100.0f);
        }
        worker_.set_course(stage_, difficulty_, false);
        autosave_locked();
    }

    float AutonomousTrainer::evaluate_rig_locked(const sim::CreatureBlueprint& candidate) const
    {
        if (!candidate.valid())
            return -std::numeric_limits<float>::infinity();
        constexpr std::size_t agents = 4;
        constexpr int maximum_steps = 600;
        float total = 0.0f;
        for (std::size_t agent = 0; agent < agents; ++agent)
        {
            sim::Environment environment{ candidate, 0xA100u + agent * 3253u };
            environment.set_course(stage_, difficulty_);
            float reward = 0.0f;
            for (int step = 0; step < maximum_steps; ++step)
            {
                const auto action = worker_.policy().deterministic_action(environment.observation());
                const sim::StepResult result = environment.step(action);
                reward += result.reward;
                if (result.terminated)
                    break;
            }
            const bool gait_valid = stage_ == sim::CourseStage::balance || environment.alternating_steps() >= 2;
            if (!environment.valid_motion() || !gait_valid)
                return -std::numeric_limits<float>::infinity();
            total += reward + environment.distance_travelled() * 0.60f
                + environment.elapsed_seconds() * 0.04f
                + static_cast<float>(environment.alternating_steps()) * 0.02f
                - environment.collision_count() * 0.18f
                - environment.airborne_ratio() * 0.70f;
        }
        return total / static_cast<float>(agents);
    }

    sim::CreatureBlueprint AutonomousTrainer::mutate_rig_locked() noexcept
    {
        sim::CreatureBlueprint candidate = worker_.blueprint();
        const float direction = (rig_generation_ & 1u) == 0u ? 1.0f : -1.0f;
        const std::uint64_t mutation = rig_generation_ % 5u;
        if (mutation == 0u)
        {
            const std::size_t pair = static_cast<std::size_t>((rig_generation_ / 2u) % 2u);
            for (std::size_t index : { pair, pair + 2u })
            {
                if (index < candidate.motors.size())
                    candidate.motors[index].strength = clamp(candidate.motors[index].strength
                        + direction * 0.0020f, 0.025f, 0.10f);
            }
        }
        else if (mutation == 1u)
        {
            const float delta = direction * 1.25f * pi / 180.0f;
            const std::size_t pair = static_cast<std::size_t>((rig_generation_ / 2u) % 2u);
            for (std::size_t index : { pair, pair + 2u })
            {
                if (index >= candidate.motors.size())
                    continue;
                sim::MotorConstraint& motor = candidate.motors[index];
                motor.minimum_angle -= delta;
                motor.maximum_angle += delta;
                motor.minimum_angle = std::min(motor.minimum_angle, motor.neutral_angle - 2.0f * pi / 180.0f);
                motor.maximum_angle = std::max(motor.maximum_angle, motor.neutral_angle + 2.0f * pi / 180.0f);
            }
        }
        else if (mutation == 2u)
        {
            const float delta = direction * 0.015f;
            if (candidate.left_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.left_contact_node].x -= delta;
            if (candidate.right_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.right_contact_node].x += delta;
        }
        else if (mutation == 3u)
        {
            const float delta = direction * 0.015f;
            if (candidate.torso_node < candidate.nodes.size())
                candidate.nodes[candidate.torso_node].y = clamp(candidate.nodes[candidate.torso_node].y + delta, 0.40f, 6.0f);
            if (candidate.head_node < candidate.nodes.size())
                candidate.nodes[candidate.head_node].y = clamp(candidate.nodes[candidate.head_node].y + delta, 0.45f, 6.5f);
        }
        else
        {
            const float delta = direction * 0.012f;
            for (const sim::MotorConstraint& motor : candidate.motors)
            {
                if (motor.pivot < candidate.nodes.size() && motor.pivot != candidate.root_node)
                    candidate.nodes[motor.pivot].y = clamp(candidate.nodes[motor.pivot].y + delta, 0.20f, 5.5f);
            }
        }

        std::array<float, sim::action_count> negative{};
        std::array<float, sim::action_count> positive{};
        std::array<float, sim::action_count> power{};
        for (std::size_t index = 0; index < candidate.motors.size(); ++index)
        {
            negative[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].neutral_angle - candidate.motors[index].minimum_angle);
            positive[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].maximum_angle - candidate.motors[index].neutral_angle);
            power[index] = candidate.motors[index].strength;
        }
        candidate.rebuild_rest_lengths();
        for (std::size_t index = 0; index < candidate.motors.size(); ++index)
        {
            sim::MotorConstraint& motor = candidate.motors[index];
            motor.neutral_angle = candidate.rest_joint_angle(index);
            motor.minimum_angle = motor.neutral_angle - negative[index];
            motor.maximum_angle = motor.neutral_angle + positive[index];
            motor.strength = power[index];
        }
        return candidate;
    }

    void AutonomousTrainer::attempt_rig_evolution_locked()
    {
        const float baseline = evaluate_rig_locked(worker_.blueprint());
        sim::CreatureBlueprint candidate = mutate_rig_locked();
        const float candidate_score = evaluate_rig_locked(candidate);
        const float required_gain = std::max(0.025f, std::abs(baseline) * 0.01f);
        ++rig_generation_;
        if (std::isfinite(candidate_score) && candidate_score > baseline + required_gain)
        {
            worker_.set_blueprint(candidate, true);
            worker_.set_course(stage_, difficulty_, false);
            ++accepted_rig_changes_;
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            worker_message_ = std::format("RIG GENERATION {} ACCEPTED  {:+.3f} VALID SCORE",
                rig_generation_, candidate_score - baseline);
            autosave_locked();
        }
        else
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format("RIG GENERATION {} REJECTED - NO VALID IMPROVEMENT", rig_generation_);
        }
    }

    void AutonomousTrainer::publish_locked()
    {
        PublishedSnapshot snapshot{};
        snapshot.blueprint = worker_.blueprint();
        snapshot.parameters = worker_.has_best_policy() ? worker_.best_policy_parameters() : worker_.policy().parameters();
        snapshot.metrics = worker_.metrics();
        snapshot.reward_history = worker_.reward_history();
        snapshot.speed_history = worker_.speed_history();
        snapshot.controller_state = worker_.controller_state();
        snapshot.exploration = worker_.exploration();
        snapshot.optimizer_step = worker_.optimizer_step();
        snapshot.has_best = worker_.has_best_policy();
        snapshot.status.enabled = enabled_.load();
        snapshot.status.stage = stage_;
        snapshot.status.difficulty = difficulty_;
        snapshot.status.rig_generation = rig_generation_;
        snapshot.status.accepted_rig_changes = accepted_rig_changes_;
        snapshot.status.rejected_rig_changes = rejected_rig_changes_;
        snapshot.status.mastery_streak = mastery_streak_;
        snapshot.status.rollback_count = rollback_count_;
        snapshot.status.rollout_threads = worker_.rollout_worker_count();
        snapshot.status.environment_count = worker_.environment_count();
        snapshot.status.message = worker_message_;
        std::scoped_lock lock(snapshot_mutex_);
        snapshot.serial = published_.serial + 1u;
        published_ = std::move(snapshot);
    }

    void AutonomousTrainer::autosave_locked()
    {
        std::string error{};
        if (!autosave_checkpoint_.empty() && !worker_.save_checkpoint(autosave_checkpoint_, error))
        {
            worker_message_ = error;
            return;
        }
        if (!autosave_rig_.empty() && !worker_.blueprint().save(autosave_rig_, error))
        {
            worker_message_ = error;
            return;
        }
        write_state_locked();
    }

    void AutonomousTrainer::write_state_locked() const
    {
        if (autosave_state_.empty())
            return;
        const std::filesystem::path temporary = autosave_state_.string() + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
            return;
        output << "EPOCHAUTONOMY 2\n";
        output << static_cast<int>(stage_) << ' ' << difficulty_ << ' ' << rig_generation_ << ' '
            << accepted_rig_changes_ << ' ' << rejected_rig_changes_ << ' ' << rollback_count_ << '\n';
        output.close();
        if (!output)
            return;
        std::error_code filesystem_error{};
        std::filesystem::remove(autosave_state_, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, autosave_state_, filesystem_error);
    }

    void AutonomousTrainer::read_state_locked()
    {
        if (autosave_state_.empty() || !std::filesystem::exists(autosave_state_))
            return;
        std::ifstream input(autosave_state_);
        std::string magic{};
        int version{};
        int stage{};
        input >> magic >> version >> stage >> difficulty_ >> rig_generation_
            >> accepted_rig_changes_ >> rejected_rig_changes_ >> rollback_count_;
        if (!input || magic != "EPOCHAUTONOMY" || version != 2 || stage < 0
            || stage >= static_cast<int>(sim::course_stage_count))
        {
            stage_ = sim::CourseStage::balance;
            difficulty_ = 0.25f;
            rig_generation_ = 0;
            accepted_rig_changes_ = 0;
            rejected_rig_changes_ = 0;
            rollback_count_ = 0;
            return;
        }
        stage_ = static_cast<sim::CourseStage>(stage);
        difficulty_ = clamp(difficulty_, 0.10f, 1.0f);
    }
}
