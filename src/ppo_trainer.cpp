#include "ppo.hpp"
#include "training_budget.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

namespace runner::rl
{
    namespace detail
    {
        [[nodiscard]] float assisted_action_history_new_weight(
            bool foundational_human_walk,
            float visible_teacher_authority) noexcept
        {
            if (!foundational_human_walk)
                return 0.60f;
            const float authority = std::isfinite(visible_teacher_authority)
                ? clamp(visible_teacher_authority, 0.0f, 1.0f)
                : 1.0f;
            return 1.0f - 0.40f * authority;
        }
    }

    namespace
    {
        constexpr float epsilon = 1.0e-8f;
        constexpr float persistent_preview_time_limit_seconds = 96.0f;

        [[nodiscard]] float next_uniform(std::uint64_t& state) noexcept
        {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            const std::uint64_t value = state * 2685821657736338717ULL;
            return std::max(1.0e-7f, static_cast<float>(value >> 40) * (1.0f / 16777216.0f));
        }

        [[nodiscard]] float next_normal(std::uint64_t& state) noexcept
        {
            const float u1 = next_uniform(state);
            const float u2 = next_uniform(state);
            return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * pi * u2);
        }

        [[nodiscard]] float skill_bootstrap_weight(std::uint64_t update,
            sim::CourseStage stage,
            const sim::CreatureBlueprint& blueprint) noexcept
        {
            if (stage == sim::CourseStage::balance)
            {
                if (update < 200u)
                    return 0.66f;
                if (update < 1600u)
                    return lerp(0.66f, 0.20f,
                        static_cast<float>(update - 200u) / 1400.0f);
                if (update < 4500u)
                    return lerp(0.20f, 0.03f,
                        static_cast<float>(update - 1600u) / 2900.0f);
                return 0.0f;
            }
            if (stage == sim::CourseStage::duck_press)
            {
                if (update < crouch_teacher_fade_begin_update)
                    return 0.70f;
                if (update < crouch_teacher_handoff_update)
                    return lerp(0.70f, 0.0f,
                        static_cast<float>(update - crouch_teacher_fade_begin_update)
                            / static_cast<float>(crouch_teacher_handoff_update
                                - crouch_teacher_fade_begin_update));
                return 0.0f;
            }
            if (stage == sim::CourseStage::crouch_walk)
            {
                if (update < 300u)
                    return 0.70f;
                if (update < 2000u)
                    return lerp(0.70f, 0.24f,
                        static_cast<float>(update - 300u) / 1700.0f);
                if (update < 5200u)
                    return lerp(0.24f, 0.06f,
                        static_cast<float>(update - 2000u) / 3200.0f);
                return 0.04f;
            }
            if (stage == sim::CourseStage::ramps
                || stage == sim::CourseStage::duck_bars)
                return update < 1200u ? 0.36f : 0.10f;
            if (!sim::stage_requires_forward_gait(stage))
                return 0.0f;
            if (stage == sim::CourseStage::uneven
                || stage == sim::CourseStage::shuttle)
                return 0.62f * foundational_walk_teacher_authority(
                    update, blueprint);
            if (update < 1200u)
                return 0.62f;
            if (update < 4000u)
                return lerp(0.62f, 0.24f,
                    static_cast<float>(update - 1200u) / 2800.0f);
            if (update < 9000u)
                return lerp(0.24f, 0.06f,
                    static_cast<float>(update - 4000u) / 5000.0f);
            return 0.03f;
        }

        [[nodiscard]] std::array<float, sim::action_count>
        skill_bootstrap_action(const sim::Environment& environment,
            sim::CourseStage stage) noexcept
        {
            if (stage == sim::CourseStage::balance)
                return balance_teacher_action(environment);
            if (stage == sim::CourseStage::duck_press)
                return duck_teacher_action(environment);
            if (stage == sim::CourseStage::crouch_walk)
                return crouch_walk_teacher_action(environment);
            if (sim::stage_requires_forward_gait(stage))
                // Imitation targets live in raw policy action space. The common
                // topology filter is applied once when the action is executed.
                return raw_walking_teacher_action(environment);
            return balance_teacher_action(environment);
        }

        [[nodiscard]] bool strict_raw_human_walk(std::uint64_t lesson_update,
            sim::CourseStage stage, sim::GaitTask gait_task,
            const sim::CreatureBlueprint& blueprint,
            std::size_t cohort_size) noexcept
        {
            return stage == sim::CourseStage::uneven
                && gait_task == sim::GaitTask::walk
                && blueprint.human_casual_gait_plan()
                && cohort_size > 0u
                && foundational_walk_raw_rollout_count(
                    lesson_update, stage, blueprint, cohort_size)
                    == cohort_size;
        }
    }

    PpoTrainer::PpoTrainer(const sim::CreatureBlueprint& blueprint, std::size_t environment_count,
            bool enable_rollout_workers, std::size_t maximum_rollout_workers)
            : blueprint_(blueprint), preview_(blueprint, 0xDEADBEEFu), policy_(0xC0FFEEu),
              preview_policy_(0xBEEFBEEFu)
        {
            environment_count = std::clamp<std::size_t>(environment_count, 8, 256);
            const std::size_t hardware = std::max<std::size_t>(1, std::thread::hardware_concurrency());
            const std::size_t available = hardware > 2 ? hardware - 2 : hardware;
            const std::size_t worker_capacity = std::min<std::size_t>(16, environment_count);
            const std::size_t worker_ceiling = maximum_rollout_workers == 0u
                ? std::min(worker_capacity, training_budget::default_worker_ceiling)
                : std::min(worker_capacity, maximum_rollout_workers);
            rollout_worker_count_ = enable_rollout_workers
                ? std::clamp<std::size_t>(available, 1, worker_ceiling)
                : 0;

            environments_.reserve(environment_count);
            for (std::size_t index = 0; index < environment_count; ++index)
            {
                environments_.emplace_back(blueprint_, 0x1000u + index * 7919u);
                environments_.back().set_course(course_stage_, course_difficulty_);
                environments_.back().set_course_motion_enabled(false);
            }
            preview_.set_course(course_stage_, course_difficulty_);
            preview_.set_trial_time_limit(persistent_preview_time_limit_seconds);
            preview_.set_course_motion_enabled(false);
            const ActionMask action_mask = action_mask_for(preview_, course_stage_);
            transition_action_mask(action_mask);
            preview_accumulator_seconds_ = 0.0;
            episode_rewards_.assign(environment_count, 0.0f);
            episode_distances_.assign(environment_count, 0.0f);
            rollout_previous_actions_.assign(environment_count,
                std::array<float, sim::action_count>{});
            adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
            adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
            rollout_worker_totals_.resize(rollout_worker_count_);
            rollout_workers_.reserve(rollout_worker_count_);
            for (std::size_t worker = 0; worker < rollout_worker_count_; ++worker)
            {
                rollout_workers_.emplace_back([this, worker](std::stop_token stop_token)
                {
                    rollout_worker_main(worker, stop_token);
                });
            }
            set_cpu_mode(1);
            initialize_parallel_workers();
        }

    PpoTrainer::~PpoTrainer()
        {
            shutdown_parallel_workers();
            for (std::jthread& worker : rollout_workers_)
                worker.request_stop();
            rollout_start_cv_.notify_all();
            rollout_workers_.clear();
        }

    PpoTrainer::RolloutTotals PpoTrainer::collect_rollout_partition(std::size_t worker_index,
        std::size_t worker_count, std::uint64_t update_seed)
    {
        RolloutTotals totals{};
        std::uint64_t local_random = update_seed ^ (worker_index + 1u) * 0xD1B54A32D192ED03ULL;
        const std::size_t environment_count = environments_.size();
        for (std::size_t environment_index = worker_index; environment_index < environment_count;
            environment_index += worker_count)
        {
            sim::Environment& environment = environments_[environment_index];
            for (std::size_t step = 0; step < rollout_horizon; ++step)
            {
                const bool foundational_human_walk =
                    course_stage_ == sim::CourseStage::uneven
                    && gait_task_ == sim::GaitTask::walk
                    && blueprint_.human_casual_gait_plan();
                const bool raw_human_walk = foundational_human_walk
                    && foundational_walk_raw_rollout_environment(
                        lesson_update_, course_stage_, blueprint_,
                        environment_index, environment_count);
                const sim::GuidanceMode rollout_guidance = (raw_human_walk
                        || trains_unaided_stand())
                    ? sim::GuidanceMode::raw_policy_audit
                    : sim::GuidanceMode::assisted;
                const float visible_teacher_authority =
                    rollout_guidance == sim::GuidanceMode::assisted
                    ? lesson_teacher_authority(lesson_update_, course_stage_,
                        environment.blueprint())
                    : 0.0f;
                Transition& transition = rollout_[
                    step * environment_count + environment_index];
                transition.requested_direction = 0.0f;
                transition.stop_requested = false;
                transition.foundational_private_phase_scrubbed = false;
                transition.origin = RolloutOrigin::unspecified;
                transition.guided_action.fill(0.0f);
                if (foundational_human_walk)
                {
                    // Direction and stop are task observations, not joint
                    // authority. Present the exact same signed/stop task graph
                    // during teacher/bootstrap and Raw collection so the
                    // handoff changes action authority, not the observation
                    // distribution. After handoff every executed joint remains
                    // sampled from the Raw policy below.
                    const RawPlayerIntentCurriculum intent =
                        raw_player_intent_curriculum(environment_index, step);
                    const bool accepted = environment.set_locomotion_intent(
                        intent.direction, intent.stop);
                    (void)accepted;
                    transition.requested_direction = intent.direction;
                    transition.stop_requested = intent.stop;
                }
                environment.set_guidance_mode(rollout_guidance);
                environment.set_physical_guidance_authority(
                    visible_teacher_authority);
                transition.observation = foundational_policy_observation(
                    environment, course_stage_, gait_task_, rollout_guidance);
                transition.foundational_private_phase_scrubbed =
                    !foundational_human_walk
                    || transition.observation
                        == encode_foundational_human_support_observation(
                            environment.observation(),
                            environment.human_support_feedback(),
                            environment.facing_direction());
                const PolicyNetwork::Evaluation evaluation = policy_.evaluate(transition.observation);
                transition.value = evaluation.value;
                transition.action = sample_action(evaluation, local_random, transition.log_probability);
                const auto unguided_policy_action = transition.action;

                std::array<float, sim::action_count>& previous_action
                    = rollout_previous_actions_[environment_index];
                if (rollout_guidance == sim::GuidanceMode::assisted)
                {
                    const float bootstrap = skill_bootstrap_weight(
                        lesson_update_, course_stage_, blueprint_);
                    const auto guided = skill_bootstrap_action(
                        environment, course_stage_);
                    const float action_history_new_weight =
                        detail::assisted_action_history_new_weight(
                            foundational_human_walk,
                            visible_teacher_authority);
                    transition.guided_action = guided;
                    transition.origin = bootstrap > 0.0f
                            && visible_teacher_authority > 0.0f
                        ? RolloutOrigin::teacher_bootstrap
                        : RolloutOrigin::guided_policy;
                    for (std::size_t action_index = 0;
                        action_index < transition.action.size(); ++action_index)
                    {
                        const float guided_action = lerp(
                            transition.action[action_index],
                            guided[action_index], bootstrap);
                        transition.action[action_index] = clamp(
                            lerp(previous_action[action_index], guided_action,
                                action_history_new_weight),
                            -1.0f, 1.0f);
                    }
                    const MotorDiscoveryProbe probe =
                        course_stage_ == sim::CourseStage::balance
                        ? motor_discovery_probe(environment, environment_index,
                            lesson_update_, step)
                        : MotorDiscoveryProbe{};
                    for (std::size_t action_index = 0;
                        action_index < transition.action.size(); ++action_index)
                        transition.action[action_index] = lerp(
                            transition.action[action_index],
                            probe.action[action_index], probe.weight);
                    transition.action =
                        effective_policy_action_with_unguided_baseline(
                            environment, transition.action,
                            unguided_policy_action, course_stage_,
                            visible_teacher_authority, rollout_guidance);
                    previous_action = transition.action;
                    if (course_stage_ == sim::CourseStage::crouch_walk)
                        // Crouch imitation follows the same finite physical
                        // authority boundary as the executed action. At zero
                        // this is the bounded/masked network action exactly.
                        transition.guided_action = transition.action;
                }
                else
                {
                    // Raw rollout samples must be the actions PPO attributes to
                    // the policy. Do not silently mix a teacher, history filter,
                    // or discovery probe and then score the composite as though
                    // the network sampled it.
                    previous_action = transition.action;
                    transition.guided_action.fill(0.0f);
                    transition.origin = RolloutOrigin::raw_policy;
                }
                transition.log_probability = policy_.log_probability(transition.action, evaluation);
                const sim::StepResult result = environment.step(transition.action);
                if (rollout_sample_sink_ && worker_index == 0u
                    && step % 4u == 0u && !result.terminated)
                    rollout_sample_sink_(environment);
                transition.reward = result.reward;
                transition.terminal = result.terminated;
                totals.total_distance += static_cast<double>(
                    result.forward_odometer_progress);
                episode_rewards_[environment_index] += result.reward;
                episode_distances_[environment_index] = environment.distance_travelled();
                totals.accumulated_speed += result.forward_speed;
                if (result.terminated)
                {
                    totals.completed_reward += episode_rewards_[environment_index];
                    totals.completed_distance += episode_distances_[environment_index];
                    ++totals.completed_episodes;
                    if (completed_episode_passes_stage_checks(
                            course_stage_, environment))
                        ++totals.valid_episodes;
                    else
                        ++totals.invalid_episodes;
                    totals.alternating_steps += environment.gait_cycles();
                    totals.collisions += static_cast<std::uint64_t>(
                        std::max(0.0f, environment.collision_count()));
                    totals.powered_jumps += environment.powered_jumps();
                    totals.landed_jumps += environment.landed_jumps();
                    totals.landed_flips += environment.spin_landings();
                    totals.obstacles_passed += environment.obstacles_passed();
                    const sim::CreatureBlueprint& terminal_rig = environment.blueprint();
                    const auto terminal_particles = environment.particles();
                    Vec2 terminal_position{};
                    if (terminal_rig.root_node < terminal_particles.size())
                        terminal_position = terminal_particles[terminal_rig.root_node].position;
                    const float terminal_progress = environment.distance_travelled()
                        + static_cast<float>(environment.obstacles_passed()) * 1000.0f;
                    if (!totals.terminal_preview.has_value()
                        || terminal_progress > totals.terminal_progress)
                    {
                        // Preserve the exact physical terminal world before
                        // the rollout worker starts its declared next trial.
                        totals.terminal_preview = environment;
                        totals.terminal_reason = result.invalid_reason;
                        totals.terminal_cause = result.terminal_cause;
                        totals.terminal_position = terminal_position;
                        totals.terminal_terrain = environment.terrain_region_at(
                            terminal_position.x);
                        totals.terminal_water_depth = environment.water_depth_at(
                            terminal_position.x);
                        totals.terminal_progress = terminal_progress;
                    }
                    if (result.invalid_reason == sim::InvalidMotion::fallen
                        || result.invalid_reason == sim::InvalidMotion::collapsed_posture
                        || result.invalid_reason == sim::InvalidMotion::body_rolling)
                        ++totals.falls;
                    episode_rewards_[environment_index] = 0.0f;
                    episode_distances_[environment_index] = 0.0f;
                    rollout_previous_actions_[environment_index].fill(0.0f);
                    environment.reset(0x100000u + metrics_.environment_steps
                        + environment_index * 17u + step + metrics_.update * 131u);
                }
            }
        }
        return totals;
    }

    void PpoTrainer::rollout_worker_main(std::size_t worker_index, std::stop_token stop_token)
    {
        std::uint64_t observed_generation = 0;
        while (!stop_token.stop_requested())
        {
            std::uint64_t update_seed = 0;
            std::size_t active_workers = 1;
            {
                std::unique_lock lock(rollout_mutex_);
                rollout_start_cv_.wait(lock, stop_token, [this, observed_generation]
                {
                    return rollout_generation_ != observed_generation;
                });
                if (stop_token.stop_requested())
                    return;
                observed_generation = rollout_generation_;
                update_seed = rollout_update_seed_;
                active_workers = rollout_active_worker_count_;
            }

            RolloutTotals totals{};
            if (worker_index < active_workers)
                totals = collect_rollout_partition(worker_index, active_workers, update_seed);
            {
                std::scoped_lock lock(rollout_mutex_);
                rollout_worker_totals_[worker_index] = totals;
                ++rollout_completed_;
            }
            rollout_done_cv_.notify_one();
        }
    }

    void PpoTrainer::set_blueprint(const sim::CreatureBlueprint& blueprint, bool preserve_policy)
    {
        verified_stand_parameters_ = 0u;
        const bool changed = blueprint.signature() != blueprint_.signature();
        blueprint_ = blueprint;
        for (sim::Environment& environment : environments_)
        {
            environment.set_blueprint(blueprint_);
            environment.set_course(course_stage_, course_difficulty_);
        }
        preview_.set_blueprint(blueprint_);
        preview_.set_course(course_stage_, course_difficulty_);
        const ActionMask action_mask = action_mask_for(preview_, course_stage_);
        transition_action_mask(action_mask);
        preview_accumulator_seconds_ = 0.0;
        if (!changed)
            return;
        refresh_foundational_teacher_prior();

        if (preserve_policy)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
        }
        else
        {
            // A canonical rig switch is a new policy subject, but the explicitly
            // labelled ALL TIME ledger must survive it. Per-selection UI baselines
            // keep THIS RIG separate without erasing cumulative work.
            reset_policy(0xC0FFEEu, false);
        }
    }

    void PpoTrainer::set_course(sim::CourseStage stage, float difficulty, bool preserve_best)
    {
        verified_stand_parameters_ = 0u;
        difficulty = clamp(difficulty, 0.10f, 1.0f);
        const ActionMask action_mask = action_mask_for(preview_, stage);
        transition_action_mask(action_mask);
        if (stage == course_stage_ && std::abs(difficulty - course_difficulty_) < 1.0e-5f)
            return;
        retained_training_preview_.reset();
        retained_training_reason_ = sim::InvalidMotion::none;
        retained_training_cause_ = sim::TrialTerminalCause::none;
        const bool stage_changed = stage != course_stage_;
        course_stage_ = stage;
        if (stage_changed)
        {
            lesson_update_ = 0u;
            raw_handoff_reset_complete_ = false;
            staged_raw_ppo_evidence_ = {};
        }
        course_difficulty_ = difficulty;
        for (sim::Environment& environment : environments_)
        {
            environment.set_course(course_stage_, course_difficulty_);
            environment.set_gait_task(gait_task_);
            environment.set_course_motion_enabled(false);
        }
        preview_.set_course(course_stage_, course_difficulty_);
        preview_.set_gait_task(gait_task_);
        preview_.set_course_motion_enabled(false);
        preview_accumulator_seconds_ = 0.0;
        refresh_foundational_teacher_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        metrics_.evaluation_reward = 0.0f;
        metrics_.evaluation_distance = 0.0f;
        metrics_.evaluation_speed = 0.0f;
        metrics_.evaluation_score = -std::numeric_limits<float>::infinity();
        metrics_.evaluation_survival = 0.0f;
        metrics_.evaluation_collisions = 0.0f;
        metrics_.evaluation_airborne_ratio = 0.0f;
        metrics_.evaluation_stride_events = 0.0f;
        metrics_.evaluation_duck_seconds = 0.0f;
        metrics_.evaluation_powered_jumps = 0.0f;
        metrics_.evaluation_jump_landings = 0.0f;
        metrics_.evaluation_spin_turns = 0.0f;
        metrics_.evaluation_spin_landings = 0.0f;
        metrics_.evaluation_obstacles_passed = 0.0f;
        metrics_.evaluation_stable_stance = 0.0f;
        metrics_.evaluation_longest_stance = 0.0f;
        metrics_.evaluation_duck_recoveries = 0.0f;
        metrics_.evaluation_max_joint_speed = 0.0f;
        metrics_.evaluation_max_backward_brace_seconds = 0.0f;
        metrics_.evaluation_hand_contacts = 0.0f;
        metrics_.evaluation_climb_transfers = 0.0f;
        metrics_.evaluation_climbs = 0.0f;
        metrics_.evaluation_descents = 0.0f;
        metrics_.evaluation_shots = 0.0f;
        metrics_.evaluation_target_hits = 0.0f;
        metrics_.evaluation_equipment_transitions = 0.0f;
        metrics_.evaluation_quality_key = 0u;
        metrics_.evaluation_rejection_mask = 0u;
        metrics_.evaluation_invalid_runs = 0;
        metrics_.evaluation_valid = false;
        metrics_.learning_rate = 3.0e-4f;
        if (!preserve_best)
        {
            best_parameters_.clear();
            best_policy_guidance_mode_ = sim::GuidanceMode::assisted;
            best_raw_ppo_receipt_ = {};
            clear_self_imitation_prior();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();
            metrics_.best_quality_key = 0u;
            metrics_.best_update = 0;
        }
        set_preview_guidance_mode(best_parameters_.empty()
            ? sim::GuidanceMode::assisted : best_policy_guidance_mode_);
    }

    void PpoTrainer::set_gait_task(sim::GaitTask task, bool preserve_best)
    {
        if (task == gait_task_)
            return;
        retained_training_preview_.reset();
        retained_training_reason_ = sim::InvalidMotion::none;
        retained_training_cause_ = sim::TrialTerminalCause::none;
        gait_task_ = task;
        lesson_update_ = 0u;
        raw_handoff_reset_complete_ = false;
        staged_raw_ppo_evidence_ = {};
        last_raw_ppo_receipt_ = {};
        for (sim::Environment& environment : environments_)
        {
            environment.set_course(course_stage_, course_difficulty_);
            environment.set_gait_task(gait_task_);
            environment.set_course_motion_enabled(false);
        }
        preview_.set_course(course_stage_, course_difficulty_);
        preview_.set_gait_task(gait_task_);
        preview_.set_course_motion_enabled(false);
        preview_accumulator_seconds_ = 0.0;
        refresh_foundational_teacher_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        metrics_.evaluation_valid = false;
        metrics_.evaluation_quality_key = 0u;
        metrics_.evaluation_rejection_mask = 0u;
        metrics_.evaluation_invalid_runs = 0u;
        metrics_.evaluation_distance = 0.0f;
        metrics_.evaluation_speed = 0.0f;
        metrics_.evaluation_stride_events = 0.0f;
        if (!preserve_best)
        {
            best_parameters_.clear();
            best_policy_guidance_mode_ = sim::GuidanceMode::assisted;
            best_raw_ppo_receipt_ = {};
            clear_self_imitation_prior();
            metrics_.best_evaluation_distance = -std::numeric_limits<float>::infinity();
            metrics_.best_evaluation_score = -std::numeric_limits<float>::infinity();
            metrics_.best_quality_key = 0u;
            metrics_.best_update = 0u;
        }
        set_preview_guidance_mode(best_parameters_.empty()
            ? sim::GuidanceMode::assisted : best_policy_guidance_mode_);
    }

    void PpoTrainer::set_equipment_directive(
        sim::EquipmentDirective directive) noexcept
    {
        if (directive == equipment_directive_)
            return;
        equipment_directive_ = directive;
        retained_training_preview_.reset();
        retained_training_reason_ = sim::InvalidMotion::none;
        retained_training_cause_ = sim::TrialTerminalCause::none;
        lesson_update_ = 0u;
        raw_handoff_reset_complete_ = false;
        staged_raw_ppo_evidence_ = {};
        last_raw_ppo_receipt_ = {};
        for (sim::Environment& environment : environments_)
            environment.set_equipment_directive(equipment_directive_);
        preview_.set_equipment_directive(equipment_directive_);
        preview_accumulator_seconds_ = 0.0;
        refresh_foundational_teacher_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        metrics_.evaluation_valid = false;
        metrics_.evaluation_quality_key = 0u;
        metrics_.evaluation_rejection_mask = 0u;
        metrics_.evaluation_invalid_runs = 0u;
    }

    void PpoTrainer::reset_training_state(bool clear_best,
        bool clear_totals) noexcept
    {
        retained_training_preview_.reset();
        retained_training_reason_ = sim::InvalidMotion::none;
        retained_training_cause_ = sim::TrialTerminalCause::none;
        retained_training_position_ = {};
        retained_training_terrain_ = sim::TerrainRegion::firm;
        retained_training_water_depth_ = 0.0f;
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        const TrainingMetrics previous_metrics = metrics_;
        metrics_ = {};
        lesson_update_ = 0u;
        raw_handoff_reset_complete_ = false;
        staged_raw_ppo_evidence_ = {};
        last_raw_ppo_receipt_ = {};
        if (!clear_totals)
        {
            metrics_.total_updates = previous_metrics.total_updates;
            metrics_.total_environment_steps = previous_metrics.total_environment_steps;
            metrics_.total_episodes = previous_metrics.total_episodes;
            metrics_.total_valid_episodes = previous_metrics.total_valid_episodes;
            metrics_.total_invalid_episodes = previous_metrics.total_invalid_episodes;
            metrics_.total_resets = previous_metrics.total_resets + 1u;
            metrics_.total_alternating_steps = previous_metrics.total_alternating_steps;
            metrics_.total_falls = previous_metrics.total_falls;
            metrics_.total_collisions = previous_metrics.total_collisions;
            metrics_.total_powered_jumps = previous_metrics.total_powered_jumps;
            metrics_.total_landed_jumps = previous_metrics.total_landed_jumps;
            metrics_.total_landed_flips = previous_metrics.total_landed_flips;
            metrics_.total_obstacles_passed = previous_metrics.total_obstacles_passed;
            metrics_.total_distance = previous_metrics.total_distance;
            metrics_.total_training_seconds = previous_metrics.total_training_seconds;
            metrics_.evaluation_count = previous_metrics.evaluation_count;
        }
        if (!clear_best)
        {
            metrics_.best_evaluation_distance = previous_metrics.best_evaluation_distance;
            metrics_.best_evaluation_score = previous_metrics.best_evaluation_score;
            metrics_.best_quality_key = previous_metrics.best_quality_key;
            metrics_.best_update = previous_metrics.best_update;
        }
        reward_history_.clear();
        speed_history_.clear();
        if (clear_best)
        {
            best_parameters_.clear();
            best_policy_guidance_mode_ = sim::GuidanceMode::assisted;
            best_raw_ppo_receipt_ = {};
            clear_self_imitation_prior();
        }
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
        {
            environments_[index].set_course(course_stage_, course_difficulty_);
            environments_[index].reset(0x1000u + index * 7919u);
        }
        preview_.set_course(course_stage_, course_difficulty_);
        set_preview_guidance_mode(best_parameters_.empty()
            ? sim::GuidanceMode::assisted : best_policy_guidance_mode_);
        preview_.reset(0xDEADBEEFu);
        preview_accumulator_seconds_ = 0.0;
    }

    void PpoTrainer::reset_policy(std::uint64_t seed, bool clear_totals)
    {
        verified_stand_parameters_ = 0u;
        policy_ = PolicyNetwork(seed);
        const ActionMask action_mask = action_mask_for(preview_, course_stage_);
        transition_action_mask(action_mask);
        preview_policy_.parameters() = policy_.parameters();
        reset_training_state(true, clear_totals);
        controller_state_ = ControllerState::fresh;
    }

    void PpoTrainer::transition_action_mask(
        const ActionMask& action_mask) noexcept
    {
        const ActionMask previous = policy_.action_mask();
        for (std::size_t slot = 0; slot < action_mask.size(); ++slot)
        {
            if (previous[slot] && !action_mask[slot])
            {
                policy_.clear_action_slot_state(adam_.first_moment, slot);
                policy_.clear_action_slot_state(adam_.second_moment, slot);
            }
        }
        policy_.set_action_mask(action_mask);
        preview_policy_.set_action_mask(action_mask);
    }

    void PpoTrainer::set_exploration(float standard_deviation) noexcept
    {
        policy_.set_exploration(standard_deviation);
    }

    std::string_view PpoTrainer::controller_state_name() const noexcept
    {
        switch (controller_state_)
        {
        case ControllerState::fresh: return "FRESH";
        case ControllerState::training: return "TRAINING";
        case ControllerState::resumed: return "RESUMED";
        case ControllerState::transferred: return "TRANSFERRED";
        }
        return "UNKNOWN";
    }

    float PpoTrainer::random_uniform() noexcept
    {
        return next_uniform(random_state_);
    }

    float PpoTrainer::random_normal() noexcept
    {
        return next_normal(random_state_);
    }

    std::array<float, sim::action_count> PpoTrainer::sample_action(
        const PolicyNetwork::Evaluation& evaluation,
        std::uint64_t& random_state,
        float& log_probability) const noexcept
    {
        return policy_.sample_action(evaluation, random_state, log_probability);
    }

    void PpoTrainer::begin_staged_update()
    {
        if (staged_update_active_)
            return;
        staged_raw_ppo_evidence_ = {};
        const bool raw_human_walk = strict_raw_human_walk(lesson_update_,
            course_stage_, gait_task_, blueprint_, environments_.size());
        if (!raw_human_walk
            && course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan())
        {
            raw_handoff_reset_complete_ = false;
        }
        if (raw_human_walk)
        {
            staged_raw_ppo_evidence_.training_semantics =
                training_semantics_version;
            staged_raw_ppo_evidence_.rig_signature = blueprint_.signature();
            staged_raw_ppo_evidence_.handoff_update =
                foundational_walk_teacher_handoff_update(blueprint_);
            staged_raw_ppo_evidence_.rollout_lesson_update = lesson_update_;
            staged_raw_ppo_evidence_.optimizer_update = lesson_update_ + 1u;
            staged_raw_ppo_evidence_.global_update = metrics_.update + 1u;
            staged_raw_ppo_evidence_.optimizer_step_before = adam_.step;
            staged_raw_ppo_evidence_.parameters_before_digest =
                parameter_digest(policy_.parameters());
            staged_raw_ppo_evidence_.active_actor_before_digest =
                policy_.active_actor_parameter_digest();
            staged_raw_ppo_evidence_.action_mask_bits =
                action_mask_bits(policy_.action_mask());
            staged_raw_ppo_evidence_.stage =
                static_cast<std::uint8_t>(course_stage_);
            staged_raw_ppo_evidence_.gait_task =
                static_cast<std::uint8_t>(gait_task_);
            staged_raw_ppo_evidence_.equipment_directive =
                static_cast<std::uint8_t>(equipment_directive_);
            staged_raw_ppo_evidence_.environments_reset_at_handoff =
                raw_handoff_reset_complete_;
            staged_raw_ppo_evidence_.actions_reset_at_handoff =
                raw_handoff_reset_complete_;
            staged_raw_ppo_evidence_.episodes_reset_at_handoff =
                raw_handoff_reset_complete_;
        }
        constexpr std::size_t horizon = rollout_horizon;
        const std::size_t environment_count = environments_.size();
        rollout_.clear();
        rollout_.resize(horizon * environment_count);
        if (raw_human_walk)
            staged_raw_ppo_evidence_.expected_sample_count = rollout_.size();

        const std::uint64_t update_seed = random_state_
            ^ (metrics_.update + 1u) * 0x9E3779B97F4A7C15ULL;
        staged_totals_ = {};
        if (rollout_workers_.empty())
        {
            staged_totals_ = collect_rollout_partition(0, 1, update_seed);
        }
        else
        {
            {
                std::scoped_lock lock(rollout_mutex_);
                rollout_completed_ = 0;
                rollout_update_seed_ = update_seed;
                rollout_active_worker_count_ = std::min(active_worker_count_, rollout_worker_count_);
                ++rollout_generation_;
            }
            rollout_start_cv_.notify_all();
            {
                std::unique_lock lock(rollout_mutex_);
                rollout_done_cv_.wait(lock, [this]
                {
                    return rollout_completed_ == rollout_worker_count_;
                });
            }
            for (const RolloutTotals& worker : rollout_worker_totals_)
            {
                staged_totals_.accumulated_speed += worker.accumulated_speed;
                staged_totals_.completed_reward += worker.completed_reward;
                staged_totals_.completed_distance += worker.completed_distance;
                staged_totals_.completed_episodes += worker.completed_episodes;
                staged_totals_.valid_episodes += worker.valid_episodes;
                staged_totals_.invalid_episodes += worker.invalid_episodes;
                staged_totals_.alternating_steps += worker.alternating_steps;
                staged_totals_.falls += worker.falls;
                staged_totals_.collisions += worker.collisions;
                staged_totals_.powered_jumps += worker.powered_jumps;
                staged_totals_.landed_jumps += worker.landed_jumps;
                staged_totals_.landed_flips += worker.landed_flips;
                staged_totals_.obstacles_passed += worker.obstacles_passed;
                staged_totals_.total_distance += worker.total_distance;
                if (worker.terminal_preview.has_value()
                    && worker.terminal_progress > staged_totals_.terminal_progress)
                {
                    staged_totals_.terminal_preview = worker.terminal_preview;
                    staged_totals_.terminal_reason = worker.terminal_reason;
                    staged_totals_.terminal_cause = worker.terminal_cause;
                    staged_totals_.terminal_position = worker.terminal_position;
                    staged_totals_.terminal_terrain = worker.terminal_terrain;
                    staged_totals_.terminal_water_depth = worker.terminal_water_depth;
                    staged_totals_.terminal_progress = worker.terminal_progress;
                }
            }
        }
        if (raw_human_walk)
        {
            for (const Transition& transition : rollout_)
            {
                const std::size_t origin = static_cast<std::size_t>(
                    transition.origin);
                if (origin < staged_raw_ppo_evidence_.origin_counts.size())
                    ++staged_raw_ppo_evidence_.origin_counts[origin];
                else
                {
                    ++staged_raw_ppo_evidence_.origin_counts[
                        static_cast<std::size_t>(RolloutOrigin::unspecified)];
                }
                if (!transition.foundational_private_phase_scrubbed)
                    ++staged_raw_ppo_evidence_.elapsed_phase_samples;
                if (transition.stop_requested)
                    ++staged_raw_ppo_evidence_.stop_intent_samples;
                else if (transition.requested_direction > 0.5f)
                    ++staged_raw_ppo_evidence_.forward_intent_samples;
                else if (transition.requested_direction < -0.5f)
                    ++staged_raw_ppo_evidence_.reverse_intent_samples;
            }
            staged_raw_ppo_evidence_.whole_cohort_raw =
                raw_handoff_reset_complete_
                && std::ranges::all_of(environments_,
                    [](const sim::Environment& environment) noexcept
                    {
                        return environment.guidance_mode()
                            == sim::GuidanceMode::raw_policy_audit;
                    });
        }
        if (staged_totals_.terminal_preview.has_value())
        {
            const float retained_progress = retained_training_preview_.has_value()
                ? retained_training_preview_->distance_travelled()
                    + static_cast<float>(retained_training_preview_->obstacles_passed()) * 1000.0f
                : -std::numeric_limits<float>::infinity();
            if (!retained_training_preview_.has_value()
                || staged_totals_.terminal_progress >= retained_progress)
            {
                retained_training_preview_ = std::move(staged_totals_.terminal_preview);
                retained_training_reason_ = staged_totals_.terminal_reason;
                retained_training_cause_ = staged_totals_.terminal_cause;
                retained_training_position_ = staged_totals_.terminal_position;
                retained_training_terrain_ = staged_totals_.terminal_terrain;
                retained_training_water_depth_ = staged_totals_.terminal_water_depth;
                retained_training_trial_id_ = ++retained_training_trial_sequence_;
            }
        }
        random_state_ ^= update_seed + 0xA0761D6478BD642FULL;
        staged_update_active_ = true;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
    }

    void PpoTrainer::compute_staged_advantages()
    {
        if (!staged_update_active_ || staged_advantages_ready_)
            return;
        constexpr float gamma = 0.995f;
        constexpr float gae_lambda = 0.95f;
        const std::size_t environment_count = environments_.size();
        std::vector<float> next_values(environment_count, 0.0f);
        for (std::size_t environment_index = 0;
            environment_index < environment_count; ++environment_index)
        {
            const auto observation = foundational_policy_observation(
                environments_[environment_index], course_stage_, gait_task_,
                environments_[environment_index].guidance_mode());
            next_values[environment_index] =
                policy_.evaluate(observation).value;
        }

        for (std::size_t environment_index = 0; environment_index < environment_count; ++environment_index)
        {
            float next_value = next_values[environment_index];
            float next_advantage = 0.0f;
            for (std::size_t reverse = rollout_horizon; reverse-- > 0;)
            {
                Transition& transition = rollout_[reverse * environment_count + environment_index];
                const float continuation = transition.terminal ? 0.0f : 1.0f;
                const float delta = transition.reward + gamma * next_value * continuation - transition.value;
                transition.advantage = delta + gamma * gae_lambda * continuation * next_advantage;
                transition.return_value = transition.advantage + transition.value;
                next_advantage = transition.advantage;
                next_value = transition.value;
            }
        }

        const std::size_t raw_environment_count =
            foundational_walk_raw_rollout_count(
                lesson_update_, course_stage_, blueprint_, environment_count);
        const bool mixed_foundational_rollout =
            course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan()
            && raw_environment_count > 0u
            && raw_environment_count < environment_count;
        const auto ppo_sample = [mixed_foundational_rollout](
            const Transition& transition) noexcept
        {
            return !mixed_foundational_rollout
                || transition.origin == RolloutOrigin::raw_policy;
        };
        float mean_advantage = 0.0f;
        std::size_t advantage_count{};
        for (const Transition& transition : rollout_)
        {
            if (!ppo_sample(transition))
                continue;
            mean_advantage += transition.advantage;
            ++advantage_count;
        }
        mean_advantage /= static_cast<float>(advantage_count);
        float variance = 0.0f;
        for (const Transition& transition : rollout_)
        {
            if (!ppo_sample(transition))
                continue;
            const float delta = transition.advantage - mean_advantage;
            variance += delta * delta;
        }
        const float inverse_std = 1.0f
            / std::sqrt(variance / static_cast<float>(advantage_count)
                + 1.0e-6f);
        staged_raw_ppo_evidence_.absolute_advantage_sum = 0.0;
        for (Transition& transition : rollout_)
        {
            if (ppo_sample(transition))
            {
                transition.advantage =
                    (transition.advantage - mean_advantage) * inverse_std;
            }
            else
                transition.advantage = 0.0f;
            staged_raw_ppo_evidence_.absolute_advantage_sum +=
                std::abs(static_cast<double>(transition.advantage));
        }
        staged_advantages_ready_ = true;
    }

    void PpoTrainer::optimize_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || staged_optimized_)
            return;
        update_policy();
        staged_raw_ppo_evidence_.optimizer_step_after = adam_.step;
        staged_raw_ppo_evidence_.parameters_after_digest =
            parameter_digest(policy_.parameters());
        staged_raw_ppo_evidence_.active_actor_after_digest =
            policy_.active_actor_parameter_digest();
        staged_optimized_ = true;
    }

    void PpoTrainer::reset_raw_handoff_boundary() noexcept
    {
        raw_handoff_reset_complete_ = false;
        clear_foundational_teacher_prior();
        rollout_.assign(rollout_.size(), Transition{});
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
        {
            sim::Environment& environment = environments_[index];
            environment.set_course(course_stage_, course_difficulty_);
            environment.set_gait_task(gait_task_);
            environment.set_equipment_directive(equipment_directive_);
            environment.set_course_motion_enabled(false);
            environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
            environment.reset(0x75130000u
                + metrics_.update * 65'537u + index * 7'919u);
        }
        reset_preview(0x7513DEADu + metrics_.update);
        metrics_.total_resets += environments_.size();
        raw_handoff_reset_complete_ = true;
    }

    void PpoTrainer::finish_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || !staged_optimized_)
            return;
        ++metrics_.update;
        ++lesson_update_;
        const bool lesson_handoff =
            (course_stage_ == sim::CourseStage::duck_press
                && lesson_update_ == crouch_teacher_handoff_update)
            || (course_stage_ == sim::CourseStage::crouch_walk
                && lesson_update_ == crouch_walk_teacher_handoff_update)
            || ((course_stage_ == sim::CourseStage::uneven
                    || course_stage_ == sim::CourseStage::shuttle)
                && lesson_update_
                    == foundational_walk_teacher_handoff_update(blueprint_));
        const bool raw_human_walk_handoff =
            course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan()
            && lesson_update_
                == foundational_walk_teacher_handoff_update(blueprint_);
        const bool whole_raw_human_walk_handoff =
            course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan()
            && !environments_.empty()
            && foundational_walk_raw_rollout_count(
                lesson_update_, course_stage_, blueprint_,
                environments_.size()) == environments_.size()
            && foundational_walk_raw_rollout_count(
                lesson_update_ - 1u, course_stage_, blueprint_,
                environments_.size()) < environments_.size();
        if (lesson_handoff)
        {
            // Assisted and unassisted scores are not comparable. Keep the
            // learned network and optimizer, but make every retained champion
            // after this lesson-local boundary prove itself with zero authority.
            best_parameters_.clear();
            best_policy_guidance_mode_ = sim::GuidanceMode::assisted;
            best_raw_ppo_receipt_ = {};
            clear_self_imitation_prior();
            metrics_.best_evaluation_distance =
                -std::numeric_limits<float>::infinity();
            metrics_.best_evaluation_score =
                -std::numeric_limits<float>::infinity();
            metrics_.best_quality_key = 0u;
            metrics_.best_update = 0u;
            preview_policy_.parameters() = policy_.parameters();
            set_preview_guidance_mode(raw_human_walk_handoff
                ? sim::GuidanceMode::raw_policy_audit
                : sim::GuidanceMode::assisted);
        }
        if (raw_human_walk_handoff || whole_raw_human_walk_handoff)
            // A provenance boundary is also an episode boundary. The same
            // routine is used by the non-installed focused test seam.
            reset_raw_handoff_boundary();
        metrics_.environment_steps += rollout_.size();
        ++metrics_.total_updates;
        metrics_.total_environment_steps += rollout_.size();
        metrics_.total_episodes += staged_totals_.completed_episodes;
        metrics_.total_valid_episodes += staged_totals_.valid_episodes;
        metrics_.total_invalid_episodes += staged_totals_.invalid_episodes;
        metrics_.total_resets += staged_totals_.completed_episodes;
        metrics_.total_alternating_steps += staged_totals_.alternating_steps;
        metrics_.total_falls += staged_totals_.falls;
        metrics_.total_collisions += staged_totals_.collisions;
        metrics_.total_powered_jumps += staged_totals_.powered_jumps;
        metrics_.total_landed_jumps += staged_totals_.landed_jumps;
        metrics_.total_landed_flips += staged_totals_.landed_flips;
        metrics_.total_obstacles_passed += staged_totals_.obstacles_passed;
        if (!environments_.empty())
            metrics_.total_distance += staged_totals_.total_distance
                / static_cast<double>(environments_.size());
        if (!environments_.empty())
        {
            metrics_.total_training_seconds += static_cast<double>(rollout_.size())
                / static_cast<double>(environments_.size()) / 60.0;
        }
        metrics_.mean_speed = staged_totals_.accumulated_speed
            / static_cast<float>(rollout_.size());
        if (staged_totals_.completed_episodes > 0)
        {
            metrics_.mean_reward = staged_totals_.completed_reward
                / static_cast<float>(staged_totals_.completed_episodes);
            metrics_.mean_episode_distance = staged_totals_.completed_distance
                / static_cast<float>(staged_totals_.completed_episodes);
        }
        else
        {
            const float partial_reward = std::accumulate(
                episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
            metrics_.mean_reward = partial_reward / static_cast<float>(episode_rewards_.size());
        }
        append_history(reward_history_, metrics_.mean_reward);
        append_history(speed_history_, metrics_.mean_speed * 3.6f);
        controller_state_ = ControllerState::training;
        const bool evaluation_due =
            metrics_.update == 1 || metrics_.update % 5 == 0;
        if (evaluation_due && !raw_human_walk_handoff)
            evaluate_policy(trains_unaided_stand() ? metrics_.evaluation_count : 0u);
        staged_update_active_ = false;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
        staged_totals_ = {};
    }

    void PpoTrainer::train_one_update()
    {
        begin_staged_update();
        compute_staged_advantages();
        optimize_staged_update();
        finish_staged_update();
    }

    void PpoTrainer::evaluate_policy(std::uint64_t evaluation_sequence)
    {
        parallel_evaluate_policy(evaluation_sequence);
    }

    bool PpoTrainer::restore_best_policy() noexcept
    {
        if (best_parameters_.size() != policy_.parameter_count())
            return false;
        policy_.parameters() = best_parameters_;
        preview_policy_.parameters() = best_parameters_;
        set_preview_guidance_mode(best_policy_guidance_mode_);
        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        preview_.reset(0xDEADBEEFu + metrics_.update);
        if (best_policy_guidance_mode_ == sim::GuidanceMode::raw_policy_audit)
            clear_self_imitation_prior();
        else if (self_imitation_prior_.empty())
            refresh_self_imitation_prior();
        controller_state_ = ControllerState::resumed;
        return true;
    }

    bool PpoTrainer::evaluate_retained_policy() noexcept
    {
        if (!restore_best_policy())
            return false;
        // Candidate retention keeps the fixed release probe. Only mastery
        // confirmations move to a distinct deterministic seed batch.
        evaluate_policy(std::max<std::uint64_t>(
            1u, metrics_.evaluation_count));
        return true;
    }

    void PpoTrainer::update_policy()
    {
        last_ppo_raw_actor_samples_ = 0u;
        last_ppo_assisted_actor_samples_ = 0u;
        last_guided_raw_sources_rejected_ = 0u;
        last_guided_assisted_sources_consumed_ = 0u;
        const bool raw_collection =
            staged_raw_ppo_evidence_.training_semantics
                == training_semantics_version
            && strict_raw_human_walk(lesson_update_, course_stage_,
                gait_task_, blueprint_, environments_.size());
        constexpr std::size_t optimization_passes = 2;
        constexpr std::size_t minibatch_size = 256;
        constexpr float clip_range = 0.12f;
        constexpr float value_coefficient = 0.42f;
        const float entropy_coefficient = 0.0012f
            * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 3500.0f);
        constexpr float max_gradient_norm = 0.38f;

        const std::size_t raw_environment_count =
            foundational_walk_raw_rollout_count(
                lesson_update_, course_stage_, blueprint_,
                environments_.size());
        const bool mixed_foundational_rollout =
            course_stage_ == sim::CourseStage::uneven
            && gait_task_ == sim::GaitTask::walk
            && blueprint_.human_casual_gait_plan()
            && raw_environment_count > 0u
            && raw_environment_count < environments_.size();
        const bool raw_actor_update = raw_collection || trains_unaided_stand()
            || mixed_foundational_rollout;
        std::vector<std::size_t> indices{};
        indices.reserve(rollout_.size());
        for (std::size_t index = 0u; index < rollout_.size(); ++index)
        {
            const bool raw =
                rollout_[index].origin == RolloutOrigin::raw_policy;
            if (!mixed_foundational_rollout || raw)
            {
                indices.push_back(index);
                if (raw)
                    ++last_ppo_raw_actor_samples_;
                else
                    ++last_ppo_assisted_actor_samples_;
            }
        }
        float total_policy_loss = 0.0f;
        float total_value_loss = 0.0f;
        float total_entropy = 0.0f;
        std::size_t sample_count = 0;

        for (std::size_t optimization_pass = 0;
            optimization_pass < optimization_passes; ++optimization_pass)
        {
            for (std::size_t index = indices.size(); index > 1; --index)
            {
                const std::size_t other = static_cast<std::size_t>(
                    random_uniform() * static_cast<float>(index));
                std::swap(indices[index - 1], indices[std::min(other, index - 1)]);
            }
            for (std::size_t begin = 0; begin < indices.size(); begin += minibatch_size)
            {
                const std::size_t end = std::min(indices.size(), begin + minibatch_size);
                float batch_policy_loss = 0.0f;
                float batch_value_loss = 0.0f;
                float batch_entropy = 0.0f;
                parallel_accumulate_batch(
                    indices, begin, end, clip_range, value_coefficient, entropy_coefficient,
                    batch_policy_loss, batch_value_loss, batch_entropy);
                if (!raw_actor_update)
                {
                    apply_self_imitation_prior();
                    ++staged_raw_ppo_evidence_.self_imitation_updates;
                }

                const float inverse_batch = 1.0f / static_cast<float>(end - begin);
                float norm_squared = 0.0f;
                for (const float gradient : policy_.gradients())
                {
                    const float scaled = gradient * inverse_batch;
                    norm_squared += scaled * scaled;
                }
                const float norm = std::sqrt(norm_squared);
                const float clip_scale = norm > max_gradient_norm
                    ? max_gradient_norm / norm
                    : 1.0f;
                const float learning_rate = metrics_.learning_rate
                    * std::max(0.10f, 1.0f - static_cast<float>(metrics_.update) / 5000.0f);
                apply_adam(learning_rate, inverse_batch * clip_scale);
                total_policy_loss += batch_policy_loss;
                total_value_loss += batch_value_loss;
                total_entropy += batch_entropy;
                sample_count += end - begin;
            }
        }
        const bool fragile_support_topology = blueprint_.monopedal_gait()
            || blueprint_.avian_gait();
        const float guided_weight = guided_rollout_imitation_weight(
            lesson_update_, course_stage_, &blueprint_);
        const bool use_clean_foundational_prior =
            (course_stage_ == sim::CourseStage::uneven
                || course_stage_ == sim::CourseStage::shuttle)
            && !foundational_teacher_prior_.empty();
        std::vector<std::size_t> assisted_rollout_indices{};
        if (!use_clean_foundational_prior)
        {
            assisted_rollout_indices.reserve(rollout_.size());
            for (std::size_t index = 0u; index < rollout_.size(); ++index)
            {
                if (rollout_[index].origin == RolloutOrigin::raw_policy)
                    ++last_guided_raw_sources_rejected_;
                else
                    assisted_rollout_indices.push_back(index);
            }
        }
        const std::size_t guided_source_count = use_clean_foundational_prior
            ? foundational_teacher_prior_.size()
            : assisted_rollout_indices.size();
        if (!raw_collection && !trains_unaided_stand() && guided_weight > 0.0f
            && guided_source_count > 0u)
        {
            constexpr std::size_t guided_passes = 4u;
            constexpr std::size_t guided_samples_per_pass = 1024u;
            constexpr float guided_gradient_limit = 1.0f;
            constexpr float guided_learning_rate = 1.0e-3f;
            const std::size_t guided_count = std::min(
                guided_samples_per_pass, guided_source_count);
            const std::size_t guided_stride = std::max<std::size_t>(
                1u, guided_source_count / guided_count);
            for (std::size_t pass = 0; pass < guided_passes; ++pass)
            {
                policy_.zero_gradients();
                float guided_loss{};
                const std::size_t offset = static_cast<std::size_t>(
                    (lesson_update_ * 131u
                        + pass * guided_source_count / guided_passes)
                    % guided_source_count);
                for (std::size_t sample_index = 0;
                    sample_index < guided_count; ++sample_index)
                {
                    const std::size_t cursor =
                        (offset + sample_index * guided_stride)
                        % guided_source_count;
                    if (use_clean_foundational_prior)
                    {
                        const ImitationSample& demonstration =
                            foundational_teacher_prior_[cursor];
                        ++last_guided_assisted_sources_consumed_;
                        policy_.accumulate_imitation_gradient(
                            demonstration.observation,
                            demonstration.action,
                            guided_weight,
                            guided_loss);
                    }
                    else
                    {
                        const Transition& demonstration =
                            rollout_[assisted_rollout_indices[cursor]];
                        ++last_guided_assisted_sources_consumed_;
                        policy_.accumulate_imitation_gradient(
                            demonstration.observation,
                            demonstration.guided_action,
                            guided_weight,
                            guided_loss);
                    }
                }
                const float inverse_guided = 1.0f
                    / static_cast<float>(guided_count);
                float guided_norm_squared{};
                for (const float gradient : policy_.gradients())
                {
                    const float scaled = gradient * inverse_guided;
                    guided_norm_squared += scaled * scaled;
                }
                const float guided_norm = std::sqrt(guided_norm_squared);
                const float guided_clip = guided_norm > guided_gradient_limit
                    ? guided_gradient_limit / guided_norm : 1.0f;
                apply_adam(guided_learning_rate,
                    inverse_guided * guided_clip);
                total_policy_loss += guided_loss;
            }
        }

        const float inverse_samples = sample_count > 0
            ? 1.0f / static_cast<float>(sample_count)
            : 0.0f;
        const bool consolidating_foundational_walk =
            foundational_walk_consolidation_active(
                lesson_update_, course_stage_, blueprint_);
        if (best_parameters_.size() == policy_.parameter_count()
            && !raw_actor_update
            && (!sim::stage_requires_forward_gait(course_stage_)
                || lesson_teacher_authority(
                    lesson_update_, course_stage_, blueprint_) >= 0.999f
                || (consolidating_foundational_walk
                    && !fragile_support_topology)))
        {
            ++staged_raw_ppo_evidence_.best_anchor_updates;
            const float anchor = consolidating_foundational_walk
                ? 0.012f : metrics_.update < 1500u ? 0.004f : 0.010f;
            std::vector<float>& current = policy_.parameters();
            for (std::size_t index = 0; index < current.size(); ++index)
                current[index] = lerp(current[index], best_parameters_[index], anchor);
        }
        metrics_.policy_loss = total_policy_loss * inverse_samples;
        metrics_.value_loss = total_value_loss * inverse_samples;
        metrics_.entropy = total_entropy * inverse_samples;
    }

    void PpoTrainer::apply_adam(float learning_rate, float gradient_scale)
    {
        constexpr float beta1 = 0.9f;
        constexpr float beta2 = 0.999f;
        ++adam_.step;
        const float bias1 = 1.0f - std::pow(beta1, static_cast<float>(adam_.step));
        const float bias2 = 1.0f - std::pow(beta2, static_cast<float>(adam_.step));
        std::vector<float>& parameters = policy_.parameters();
        const std::vector<float>& gradients = policy_.gradients();
        for (std::size_t index = 0; index < parameters.size(); ++index)
        {
            const float gradient = gradients[index] * gradient_scale;
            adam_.first_moment[index] = beta1 * adam_.first_moment[index] + (1.0f - beta1) * gradient;
            adam_.second_moment[index] = beta2 * adam_.second_moment[index] + (1.0f - beta2) * gradient * gradient;
            const float first = adam_.first_moment[index] / bias1;
            const float second = adam_.second_moment[index] / bias2;
            parameters[index] -= learning_rate * first / (std::sqrt(second) + epsilon);
        }
    }

    void PpoTrainer::configure_preview_equipment(
        sim::WeaponClass weapon, float target_distance)
    {
        preview_equipment_test_enabled_ = weapon != sim::WeaponClass::none;
        preview_.configure_equipment(weapon, target_distance);
        preview_accumulator_seconds_ = 0.0;
    }
    void PpoTrainer::step_preview(float dt)
    {
        if (preview_terminal_)
            return;
        if (!std::isfinite(dt) || dt <= 0.0f)
            return;
        constexpr double fixed_step = static_cast<double>(1.0f / 60.0f);
        constexpr double maximum_backlog = 0.25;
        preview_accumulator_seconds_ = std::min(maximum_backlog,
            preview_accumulator_seconds_ + static_cast<double>(dt));
        while (preview_accumulator_seconds_ + 1.0e-8 >= fixed_step)
        {
            const float preview_teacher_authority =
                preview_guidance_mode_ == sim::GuidanceMode::assisted
                ? lesson_teacher_authority(lesson_update_, course_stage_,
                    preview_.blueprint())
                : 0.0f;
            preview_.set_physical_guidance_authority(
                preview_teacher_authority);
            const PolicyNetwork& display_policy = best_parameters_.empty()
                ? policy_ : preview_policy_;
            const auto observation = foundational_policy_observation(
                preview_, course_stage_, gait_task_, preview_guidance_mode_);
            const auto raw_action =
                display_policy.deterministic_action(observation);
            auto action = raw_action;
            if (preview_guidance_mode_ == sim::GuidanceMode::assisted)
            {
                action = effective_policy_action(
                    preview_, raw_action, course_stage_,
                    preview_teacher_authority, preview_guidance_mode_);
            }
            if (preview_equipment_test_enabled_
                && preview_.equipment_target().active)
                // Rig Lab exercises the same stop/arm/barrel/fire path as an
                // autonomous combat lesson, independent of the selected gait.
                action = effective_policy_action(preview_, raw_action,
                    sim::CourseStage::equipment_targets, 1.0f,
                    sim::GuidanceMode::assisted);
            const sim::StepResult result = preview_.step(action,
                static_cast<float>(fixed_step));
            preview_accumulator_seconds_ -= fixed_step;
            if (result.terminated)
            {
                preview_last_reset_reason_ = result.invalid_reason;
                preview_terminal_cause_ = result.terminal_cause;
                preview_terminal_ = true;
                ++preview_reset_sequence_;
                const sim::CreatureBlueprint& rig = preview_.blueprint();
                const auto particles = preview_.particles();
                if (rig.root_node < particles.size())
                    preview_terminal_position_ = particles[rig.root_node].position;
                preview_terminal_terrain_ = preview_.terrain_region_at(
                    preview_terminal_position_.x);
                preview_terminal_water_depth_ = preview_.water_depth_at(
                    preview_terminal_position_.x);
                preview_accumulator_seconds_ = 0.0;
                break;
            }
        }
        preview_accumulator_seconds_ = std::max(0.0, preview_accumulator_seconds_);
    }

    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept
    {
        ++preview_trial_id_;
        preview_terminal_ = false;
        preview_terminal_cause_ = sim::TrialTerminalCause::none;
        preview_last_reset_reason_ = sim::InvalidMotion::none;
        preview_terminal_position_ = {};
        preview_terminal_terrain_ = sim::TerrainRegion::firm;
        preview_terminal_water_depth_ = 0.0f;
        preview_accumulator_seconds_ = 0.0;
        preview_.reset(seed);
        preview_.set_guidance_mode(preview_guidance_mode_);
        preview_.set_physical_guidance_authority(
            preview_guidance_mode_ == sim::GuidanceMode::assisted
            ? lesson_teacher_authority(lesson_update_, course_stage_,
                preview_.blueprint())
            : 0.0f);
    }

    void PpoTrainer::append_history(std::vector<float>& history, float value)
    {
        constexpr std::size_t maximum = 240;
        if (history.size() == maximum)
            history.erase(history.begin());
        history.push_back(value);
    }
}
