from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:220]!r}")
    write(path, text.replace(old, new, 1))


def insert_before(path: str, marker: str, addition: str) -> None:
    text = read(path)
    count = text.count(marker)
    if count != 1:
        raise RuntimeError(f"{path}: expected one marker, found {count}: {marker[:220]!r}")
    write(path, text.replace(marker, addition + marker, 1))


# ---- Zero movement means reset ----------------------------------------------
insert_before(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool ground_clearance_hazard(CourseFeatureKind kind) noexcept
''',
    '''    [[nodiscard]] inline bool zero_progress_window(float net_progress,
        std::uint32_t new_steps, float useful_foot_lift, bool recovering) noexcept
    {
        return !recovering && net_progress < 0.045f
            && new_steps == 0u && useful_foot_lift < 0.11f;
    }

    [[nodiscard]] inline float update_zero_progress_seconds(float previous_seconds,
        bool zero_progress, float window_seconds) noexcept
    {
        return zero_progress
            ? previous_seconds + window_seconds
            : std::max(0.0f, previous_seconds - window_seconds * 2.0f);
    }

    inline constexpr float zero_progress_reset_seconds = 1.80f;

''')

replace_once(
    "src/simulation.hpp",
    '''        body_rolling,
        foot_pivot_rolling,
        hazard_quiver
''',
    '''        body_rolling,
        foot_pivot_rolling,
        zero_progress,
        hazard_quiver
''')
replace_once(
    "src/simulation.hpp",
    '''        case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE ROLLING";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
''',
    '''        case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE ROLLING";
        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
''')

replace_once(
    "src/simulation.hpp",
    '''        [[nodiscard]] float foot_pivot_rolling_seconds() const noexcept { return foot_pivot_rolling_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
''',
    '''        [[nodiscard]] float foot_pivot_rolling_seconds() const noexcept { return foot_pivot_rolling_seconds_; }
        [[nodiscard]] float zero_progress_seconds() const noexcept { return zero_progress_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
''')

replace_once(
    "src/simulation.hpp",
    '''        std::uint32_t alternating_steps_{};
        std::uint32_t knee_first_faults_{};
''',
    '''        std::uint32_t alternating_steps_{};
        std::uint32_t progress_window_start_steps_{};
        std::uint32_t knee_first_faults_{};
''')
replace_once(
    "src/simulation.hpp",
    '''        float foot_pivot_rolling_seconds_{};
        float head_contact_seconds_{};
''',
    '''        float foot_pivot_rolling_seconds_{};
        float zero_progress_seconds_{};
        float head_contact_seconds_{};
''')

# Every additional semantic foot gets foot mass/traction treatment at spawn.
replace_once(
    "src/simulation.cpp",
    '''            const bool contact_semantic = index == blueprint_.left_contact_node
                || index == blueprint_.right_contact_node;
''',
    '''            const bool contact_semantic = blueprint_.is_support_seed(index);
''')

replace_once(
    "src/simulation.cpp",
    '''        alternating_steps_ = 0;
        knee_first_faults_ = 0;
''',
    '''        alternating_steps_ = 0;
        progress_window_start_steps_ = 0;
        knee_first_faults_ = 0;
''')
replace_once(
    "src/simulation.cpp",
    '''        foot_pivot_rolling_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
''',
    '''        foot_pivot_rolling_seconds_ = 0.0f;
        zero_progress_seconds_ = 0.0f;
        head_contact_seconds_ = 0.0f;
''')

replace_once(
    "src/simulation.cpp",
    '''            if (course_stage_ != CourseStage::balance && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);
            progress_window_start_x_ = root_x;
''',
    '''            if (course_stage_ != CourseStage::balance && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);

            const std::uint32_t new_steps = alternating_steps_ - progress_window_start_steps_;
            const bool idle_window = course_stage_ != CourseStage::balance
                && elapsed_seconds_ > rolling_gate_warmup_end_seconds
                && zero_progress_window(net_progress, new_steps,
                    obstacle_lift_clearance_, recovery_active_);
            zero_progress_seconds_ = update_zero_progress_seconds(
                zero_progress_seconds_, idle_window, progress_window_seconds_);
            progress_window_start_steps_ = alternating_steps_;
            progress_window_start_x_ = root_x;
''')

replace_once(
    "src/simulation.cpp",
    '''        if (micro_motion_seconds_ >= 3.0f)
            invalidate(InvalidMotion::micro_motion);
''',
    '''        if (micro_motion_seconds_ >= 3.0f)
            invalidate(InvalidMotion::micro_motion);
        if (zero_progress_seconds_ >= zero_progress_reset_seconds)
            invalidate(InvalidMotion::zero_progress);
''')

# ---- Automatic self-imitation prior from the best valid result --------------
insert_before(
    "src/ppo.hpp",
    '''    enum class ControllerState : std::uint8_t
''',
    '''    [[nodiscard]] inline float self_imitation_prior_weight(std::uint64_t age_updates,
        std::size_t sample_count) noexcept
    {
        if (sample_count == 0)
            return 0.0f;
        const float age = static_cast<float>(std::min<std::uint64_t>(age_updates, 2000u));
        return clamp(0.18f / (1.0f + age / 240.0f), 0.040f, 0.18f);
    }

    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds) noexcept
    {
        if (!valid_motion)
            return false;
        if (stage == sim::CourseStage::balance)
            return survival_seconds >= 3.0f;
        return alternating_steps >= 2u && distance >= 0.60f;
    }

''')

replace_once(
    "src/ppo.hpp",
    '''        std::uint64_t best_update{};
        std::uint64_t evaluation_count{};
''',
    '''        std::uint64_t best_update{};
        std::uint64_t evaluation_count{};
        std::uint32_t imitation_samples{};
        float imitation_weight{};
        float imitation_source_score{ -std::numeric_limits<float>::infinity() };
''')

replace_once(
    "src/ppo.hpp",
    '''        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
''',
    '''        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
        [[nodiscard]] std::size_t self_imitation_sample_count() const noexcept
        {
            return self_imitation_prior_.size();
        }
''')

insert_before(
    "src/ppo.hpp",
    '''        struct AdamState
''',
    '''        struct ImitationSample
        {
            std::array<float, sim::observation_count> observation{};
            std::array<float, sim::action_count> action{};
        };

''')

replace_once(
    "src/ppo.hpp",
    '''        void update_policy();
        void evaluate_policy();
        void reset_training_state(bool clear_best = true) noexcept;
''',
    '''        void update_policy();
        void evaluate_policy();
        void refresh_self_imitation_prior();
        void clear_self_imitation_prior() noexcept;
        void apply_self_imitation_prior();
        void reset_training_state(bool clear_best = true) noexcept;
''')

replace_once(
    "src/ppo.hpp",
    '''        std::vector<float> best_parameters_{};
        TrainingMetrics metrics_{};
''',
    '''        std::vector<float> best_parameters_{};
        std::vector<ImitationSample> self_imitation_prior_{};
        float self_imitation_source_score_{ -std::numeric_limits<float>::infinity() };
        TrainingMetrics metrics_{};
''')

# Keep prior lifecycle tied to the current stage/rig/best policy.
replace_once(
    "src/ppo_trainer.cpp",
    '''        if (!preserve_best)
        {
            best_parameters_.clear();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
''',
    '''        if (!preserve_best)
        {
            best_parameters_.clear();
            clear_self_imitation_prior();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
''')
replace_once(
    "src/ppo_trainer.cpp",
    '''        if (clear_best)
            best_parameters_.clear();
''',
    '''        if (clear_best)
        {
            best_parameters_.clear();
            clear_self_imitation_prior();
        }
''')
replace_once(
    "src/ppo_trainer.cpp",
    '''                    parallel_accumulate_batch(
                        indices, begin, end, clip_range, value_coefficient, entropy_coefficient,
                        batch_policy_loss, batch_value_loss, batch_entropy);

                    const float inverse_batch = 1.0f / static_cast<float>(end - begin);
''',
    '''                    parallel_accumulate_batch(
                        indices, begin, end, clip_range, value_coefficient, entropy_coefficient,
                        batch_policy_loss, batch_value_loss, batch_entropy);
                    apply_self_imitation_prior();

                    const float inverse_batch = 1.0f / static_cast<float>(end - begin);
''')
replace_once(
    "src/ppo_trainer.cpp",
    '''        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        return true;
''',
    '''        preview_.reset(0xDEADBEEFu + metrics_.update);
        if (self_imitation_prior_.empty())
            refresh_self_imitation_prior();
        controller_state_ = ControllerState::resumed;
        return true;
''')
replace_once(
    "src/ppo_parallel.cpp",
    '''            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_evaluation_score = metrics_.evaluation_score;
            metrics_.best_update = metrics_.update;
''',
    '''            best_parameters_ = policy_.parameters();
            metrics_.best_evaluation_distance = metrics_.evaluation_distance;
            metrics_.best_evaluation_score = metrics_.evaluation_score;
            metrics_.best_update = metrics_.update;
            refresh_self_imitation_prior();
''')
replace_once(
    "src/training_checkpoint.cpp",
    '''        best_parameters_ = std::move(best);
        reward_history_ = std::move(rewards);
        speed_history_ = std::move(speeds);
''',
    '''        best_parameters_ = std::move(best);
        reward_history_ = std::move(rewards);
        speed_history_ = std::move(speeds);
        refresh_self_imitation_prior();
''')

replace_once(
    "CMakeLists.txt",
    '''    src/ppo_parallel.cpp
    src/training_checkpoint.cpp
''',
    '''    src/ppo_parallel.cpp
    src/self_imitation.cpp
    src/training_checkpoint.cpp
''')

write("src/self_imitation.cpp", r'''#include "ppo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace epochrunner::rl
{
    void PpoTrainer::clear_self_imitation_prior() noexcept
    {
        self_imitation_prior_.clear();
        self_imitation_source_score_ = -std::numeric_limits<float>::infinity();
        metrics_.imitation_samples = 0;
        metrics_.imitation_weight = 0.0f;
        metrics_.imitation_source_score = -std::numeric_limits<float>::infinity();
    }

    void PpoTrainer::refresh_self_imitation_prior()
    {
        if (best_parameters_.size() != policy_.parameter_count())
        {
            clear_self_imitation_prior();
            return;
        }

        constexpr std::size_t candidate_agents = 6;
        constexpr std::size_t maximum_prior_samples = 512;
        const int maximum_steps = static_cast<std::uint8_t>(course_stage_)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 2400 : 1200;
        PolicyNetwork teacher{ 0x51E17Eu };
        teacher.parameters() = best_parameters_;
        std::vector<ImitationSample> best_trajectory{};
        float best_score = -std::numeric_limits<float>::infinity();

        for (std::size_t agent = 0; agent < candidate_agents; ++agent)
        {
            sim::Environment environment{ blueprint_, 0xB500u + agent * 4099u };
            environment.set_course(course_stage_, course_difficulty_);
            std::vector<ImitationSample> trajectory{};
            trajectory.reserve(static_cast<std::size_t>(maximum_steps));
            float reward = 0.0f;
            for (int step = 0; step < maximum_steps; ++step)
            {
                ImitationSample sample{};
                sample.observation = environment.observation();
                sample.action = teacher.deterministic_action(sample.observation);
                const sim::StepResult result = environment.step(sample.action);
                reward += result.reward;
                const bool clean_demonstration_frame = environment.valid_motion()
                    && !environment.non_foot_grounded()
                    && environment.foot_pivot_rolling_seconds() < 0.08f;
                if (clean_demonstration_frame)
                    trajectory.push_back(sample);
                if (result.terminated)
                    break;
            }

            if (!elite_motion_eligible(course_stage_, environment.valid_motion(),
                environment.alternating_steps(), environment.distance_travelled(),
                environment.elapsed_seconds()))
                continue;

            const float score = reward + environment.distance_travelled() * 0.75f
                + environment.elapsed_seconds() * 0.025f
                + static_cast<float>(environment.alternating_steps()) * 0.03f
                - environment.collision_count() * 0.18f
                - environment.airborne_ratio() * 0.75f;
            if (score > best_score && !trajectory.empty())
            {
                best_score = score;
                best_trajectory = std::move(trajectory);
            }
        }

        if (best_trajectory.empty())
        {
            clear_self_imitation_prior();
            return;
        }

        self_imitation_prior_.clear();
        const std::size_t stride = std::max<std::size_t>(1,
            (best_trajectory.size() + maximum_prior_samples - 1u) / maximum_prior_samples);
        for (std::size_t index = 0; index < best_trajectory.size(); index += stride)
            self_imitation_prior_.push_back(best_trajectory[index]);
        if (self_imitation_prior_.size() > maximum_prior_samples)
            self_imitation_prior_.resize(maximum_prior_samples);

        self_imitation_source_score_ = best_score;
        metrics_.imitation_samples = static_cast<std::uint32_t>(self_imitation_prior_.size());
        metrics_.imitation_source_score = best_score;
        metrics_.imitation_weight = self_imitation_prior_weight(0, self_imitation_prior_.size());
    }

    void PpoTrainer::apply_self_imitation_prior()
    {
        if (self_imitation_prior_.empty())
        {
            metrics_.imitation_weight = 0.0f;
            return;
        }

        constexpr std::size_t samples_per_batch = 32;
        const std::uint64_t age = metrics_.update >= metrics_.best_update
            ? metrics_.update - metrics_.best_update : 0u;
        const float weight = self_imitation_prior_weight(age, self_imitation_prior_.size());
        const std::size_t count = std::min(samples_per_batch, self_imitation_prior_.size());
        const std::size_t offset = static_cast<std::size_t>(
            (metrics_.update * 131u + adam_.step * 17u) % self_imitation_prior_.size());
        const std::size_t sample_stride = std::max<std::size_t>(1,
            self_imitation_prior_.size() / count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const ImitationSample& sample = self_imitation_prior_[
                (offset + index * sample_stride) % self_imitation_prior_.size()];
            const PolicyNetwork::Evaluation evaluation = policy_.evaluate(sample.observation);
            const float old_log_probability = policy_.log_probability(sample.action, evaluation);
            float ignored_policy_loss = 0.0f;
            float ignored_value_loss = 0.0f;
            float ignored_entropy = 0.0f;
            policy_.accumulate_gradient(sample.observation, sample.action,
                old_log_probability, weight, evaluation.value,
                0.20f, 0.0f, 0.0f,
                ignored_policy_loss, ignored_value_loss, ignored_entropy);
        }

        metrics_.imitation_weight = weight;
        metrics_.imitation_samples = static_cast<std::uint32_t>(self_imitation_prior_.size());
        metrics_.imitation_source_score = self_imitation_source_score_;
    }
}
''')

# ---- UI telemetry for idle reset and the automatic prior --------------------
replace_once(
    "src/app.cpp",
    '''            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 218.0f } },
''',
    '''            add_rounded_rect(canvas, { cursor - Vec2{ 7.0f, 5.0f }, { usable_width + 14.0f, 247.0f } },
''')
replace_once(
    "src/app.cpp",
    '''            add_text_fit(canvas, cursor, std::format("COLLISIONS {:.1f}   AIRBORNE {:.0f}%",
                metrics.evaluation_collisions, metrics.evaluation_airborne_ratio * 100.0f),
                1.08f, white, usable_width);
            cursor.y += 45.0f;
''',
    '''            add_text_fit(canvas, cursor, std::format("COLLISIONS {:.1f}   AIRBORNE {:.0f}%",
                metrics.evaluation_collisions, metrics.evaluation_airborne_ratio * 100.0f),
                1.08f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("BEST-RESULT GUIDE {} FRAMES   WEIGHT {:.0f}%",
                metrics.imitation_samples, metrics.imitation_weight * 100.0f),
                1.04f, metrics.imitation_samples > 0 ? accent : muted, usable_width);
            cursor.y += 45.0f;
''')
replace_once(
    "src/app.cpp",
    '''                std::format("RECOVERY {}  FEET {}/{}  STEPS {}  LIFT {:.2f} M  FOOT-ROLL {:.1f} S",
                    environment.recovering() ? "ACTIVE" : "READY",
                    environment.left_supported() ? "L" : "-",
                    environment.right_supported() ? "R" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.foot_pivot_rolling_seconds()),
''',
    '''                std::format("FEET {}/{}  STEPS {}  LIFT {:.2f} M  FOOT-ROLL {:.1f} S  IDLE {:.1f} S",
                    environment.left_supported() ? "A" : "-",
                    environment.right_supported() ? "B" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.foot_pivot_rolling_seconds(), environment.zero_progress_seconds()),
''')

# ---- Tests ------------------------------------------------------------------
insert_before(
    "tests/core_tests.cpp",
    '''    require(sim::hazard_approach_weight(0.40f) == 1.0f,
''',
    '''    require(sim::zero_progress_window(0.0f, 0u, 0.0f, false),
        "zero movement is not classified for reset");
    require(!sim::zero_progress_window(0.08f, 0u, 0.0f, false),
        "meaningful translation is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 1u, 0.0f, false),
        "a new gait step is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.18f, false),
        "useful leg lift is incorrectly classified as zero movement");
    require(!sim::zero_progress_window(0.0f, 0u, 0.0f, true),
        "active recovery is incorrectly reset as idle");
    require(sim::update_zero_progress_seconds(1.0f, true, 1.0f)
            >= sim::zero_progress_reset_seconds,
        "two idle windows do not reach the reset threshold");
    require(sim::update_zero_progress_seconds(1.0f, false, 1.0f) == 0.0f,
        "useful motion does not rapidly clear the idle-reset accumulator");
    require(rl::self_imitation_prior_weight(0, 128) > rl::self_imitation_prior_weight(500, 128)
            && rl::self_imitation_prior_weight(500, 128) > 0.0f,
        "best-result imitation guide does not decay into a light prior");
    require(rl::self_imitation_prior_weight(0, 0) == 0.0f,
        "empty imitation memory still changes PPO gradients");
    require(rl::elite_motion_eligible(sim::CourseStage::walk, true, 3, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");
    require(!rl::elite_motion_eligible(sim::CourseStage::walk, false, 8, 12.0f, 20.0f),
        "invalid rolling result can seed self-imitation");
''')

# ---- Mission ledger: add the accepted scope, but do not falsely close visual
# or learned-behavior missions before the user confirms the runtime result. ----
insert_before(
    "MISSIONS.md",
    '''## Current warning
''',
    '''## WALK-IDLE-005 — Zero movement resets the episode

**Status:** ACTIVE

A non-balance controller may not occupy a rollout for most of an episode while producing no translation, no new gait step, and no useful leg lift.

**Acceptance:**

- Startup settling and active recovery are exempt.
- Two consecutive one-second zero-progress windows terminate as `ZERO MOVEMENT - RESET`.
- A real step, useful obstacle lift, or meaningful translation clears the idle accumulator quickly.
- Live telemetry shows the accumulated idle time.
- Deterministic tests and a packaged runtime trace confirm prompt reset without rejecting useful get-up behavior.

## WALK-GUIDE-006 — Automatic best-result self-imitation prior

**Status:** ACTIVE

The trainer automatically converts its best valid result into a small behavioral guide. The user does not need to author a demonstration, but may still correct the rig or controller through the existing tools.

**Acceptance:**

- Only a valid, grounded, stepped best result can seed the guide.
- Frames containing body contact or orange-foot pivot rolling are excluded.
- The guide contributes a bounded actor-only gradient and never replaces PPO reward learning.
- Prior weight decays from a modest maximum toward a small floor as the best result ages.
- A new stage, incompatible rig, reset, or better best result clears or rebuilds the guide.
- UI reports guide frame count and current weight.

## WALK-PIP-007 — Real training picture-in-picture

**Status:** ACTIVE

Publish one representative worker-owned rollout environment as an immutable snapshot and render it in a small upper-right picture-in-picture before the controls. It must show actual exploratory training, not a second deterministic live replay.

**Acceptance:**

- Snapshot copying occurs only at publication boundaries under worker ownership.
- The PIP identifies itself as a raw training sample and displays its own distance and invalid-motion state.
- Live rendering remains responsive while MAX CPU training is active.
- User confirms the PIP is visible, useful, and does not obscure primary telemetry.

''')

replace_once(
    "MISSIONS.md",
    '''- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use semantic foot clusters rather than treating extra feet as body contact.
''',
    '''- The QUADRUPED preset is a true four-leg body, not a two-leg articulated biped with a long torso.
- Near/far legs are staggered enough to remain distinguishable in side view.
- Four-legged crawler and six-legged hexapod presets are structurally valid, selectable, trainable, and use multi-foot semantic support groups rather than treating extra feet as body contact.
''')

print("Applied guided-training phase 2")
