#include "autonomy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <thread>

namespace runner::rl
{
    bool AutonomousTrainer::stage_mastered_locked() const noexcept
    {
        const TrainingMetrics& metrics = worker_.metrics();
        if (!metrics.evaluation_valid || metrics.evaluation_quality_key == 0u)
            return false;
        switch (stage_)
        {
        case sim::CourseStage::balance:
            return strict_balance_mastery(metrics);
        case sim::CourseStage::duck_press:
            return strict_duck_press_mastery(metrics);
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_speed >= 0.70f
                && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::crouch_walk:
            return metrics.evaluation_duck_recoveries >= 1.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_duck_seconds >= 3.5f
                && metrics.evaluation_distance >= 1.50f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 1.0f
                && metrics.evaluation_survival >= 14.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_jump_landings >= 3.0f
                && metrics.evaluation_powered_jumps >= 3.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_distance >= 3.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 8.0f
                && metrics.evaluation_stride_events >= 6.0f
                && metrics.evaluation_obstacles_passed >= 3.0f
                && metrics.evaluation_collisions <= 2.0f
                && (metrics.evaluation_jump_landings >= 1.0f
                    || metrics.evaluation_duck_seconds >= 1.0f);
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_spin_landings >= 2.0f
                && metrics.evaluation_powered_jumps >= 2.0f
                && metrics.evaluation_spin_turns >= 0.85f
                && metrics.evaluation_spin_turns <= 3.00f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 11.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_obstacles_passed >= 4.0f
                && metrics.evaluation_collisions <= 3.0f;
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
            queue_autosave();
        }

        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        const int required_confirmations = required_mastery_confirmations(stage_);
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
            const bool catastrophic_invalid = metrics.evaluation_quality_key == 0u
                || metrics.evaluation_distance < -0.25f
                || metrics.evaluation_invalid_runs >= 3u;
            if (catastrophic_invalid && worker_.has_best_policy()
                && worker_.restore_best_policy())
            {
                ++rollback_count_;
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_.set_course(stage_, difficulty_, false);
                worker_message_ = "INVALID/BACKWARD GENERATION - RESTORED CHAMPION AND RESTARTED LESSON";
                queue_autosave();
                return;
            }
            if (catastrophic_invalid && !worker_.has_best_policy()
                && metrics.evaluation_count % 3u == 0u)
            {
                worker_.reset_policy(0x715000u
                    + metrics.evaluation_count * 0x9E3779B97F4A7C15ULL);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_message_ = "NO VALID CHAMPION AFTER THREE EVALUATIONS - RESET POLICY NURSERY";
                queue_autosave();
                return;
            }
            worker_message_ = std::format("INVALID RUN REJECTED - {} / {}",
                metrics.evaluation_invalid_runs,
                primary_motion_rejection_name(metrics.evaluation_rejection_mask));
        }
        else if (mastery_streak_ >= required_confirmations)
        {
            advance_stage_locked();
        }
        else if (stage_ != sim::CourseStage::balance && metrics.evaluation_count % 4 == 0)
        {
            attempt_rig_evolution_locked();
        }
        else
        {
            if (stage_ == sim::CourseStage::balance)
            {
                const std::uint32_t valid_seeds = 6u
                    - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                worker_message_ = std::format(
                    "STAGE VALID {}/6 SEEDS - STAND {:.1f}/{:.1f}S  SPIN {:.2f}/{:.2f}  JOINT {:.1f}/{:.1f}  MASTERY {}/{}",
                    valid_seeds, metrics.evaluation_longest_stance,
                    standing_mastery_seconds, metrics.evaluation_spin_turns,
                    standing_mastery_spin_limit, metrics.evaluation_max_joint_speed,
                    standing_mastery_joint_speed_limit, mastery_streak_,
                    required_confirmations);
            }
            else
            {
                worker_message_ = std::format("{} - STRICT MASTERY {}/{}",
                    sim::course_stage_name(stage_), mastery_streak_,
                    required_confirmations);
            }
        }
    }

    void AutonomousTrainer::advance_stage_locked()
    {
        if (worker_.has_best_policy())
            (void)worker_.restore_best_policy();
        mastery_streak_ = 0;
        degradation_streak_ = 0;
        if (stage_ != sim::CourseStage::moving_hazards)
        {
            stage_ = static_cast<sim::CourseStage>(static_cast<std::uint8_t>(stage_) + 1u);
            difficulty_ = 0.30f;
            worker_message_ = std::format("SKILL LOCKED - ADVANCING TO {}",
                sim::course_stage_name(stage_));
        }
        else
        {
            difficulty_ = std::min(1.0f, difficulty_ + 0.10f);
            worker_message_ = std::format("FULL COURSE MASTERED - DIFFICULTY {:.0f}%", difficulty_ * 100.0f);
        }
        worker_.set_course(stage_, difficulty_, false);
        queue_autosave();
    }

    namespace
    {
        [[nodiscard]] bool same_edge(std::uint16_t a, std::uint16_t b,
            std::uint16_t c, std::uint16_t d) noexcept
        {
            return (a == c && b == d) || (a == d && b == c);
        }

        [[nodiscard]] std::size_t node_degree(const sim::CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            return static_cast<std::size_t>(std::ranges::count_if(
                rig.bones, [node](const sim::DistanceConstraint& bone)
                {
                    return bone.a == node || bone.b == node;
                }));
        }

        [[nodiscard]] bool motor_references_node(const sim::CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const sim::MotorConstraint& motor = rig.motors[index];
                if (motor.enabled && (motor.a == node
                    || motor.pivot == node || motor.c == node))
                    return true;
            }
            return false;
        }

        void recalibrate_after_geometry(sim::CreatureBlueprint& rig,
            const std::array<float, sim::action_count>& negative,
            const std::array<float, sim::action_count>& positive,
            const std::array<float, sim::action_count>& power) noexcept
        {
            rig.rebuild_rest_lengths();
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                sim::MotorConstraint& motor = rig.motors[index];
                if (!motor.enabled)
                    continue;
                motor.neutral_angle = rig.rest_joint_angle(index);
                motor.minimum_angle = motor.neutral_angle - negative[index];
                motor.maximum_angle = motor.neutral_angle + positive[index];
                motor.strength = power[index];
            }
        }

        void remove_node(sim::CreatureBlueprint& rig, std::uint16_t removed) noexcept
        {
            rig.nodes.erase(rig.nodes.begin() + removed);
            rig.radii.erase(rig.radii.begin() + removed);
            std::erase_if(rig.bones, [removed](const sim::DistanceConstraint& bone)
            {
                return bone.a == removed || bone.b == removed;
            });
            auto remap = [removed](std::uint16_t& value)
            {
                if (value > removed)
                    --value;
            };
            for (sim::DistanceConstraint& bone : rig.bones)
            {
                remap(bone.a);
                remap(bone.b);
            }
            remap(rig.root_node);
            remap(rig.torso_node);
            remap(rig.head_node);
            remap(rig.left_contact_node);
            remap(rig.right_contact_node);
            for (std::uint16_t& node : rig.additional_left_contact_nodes)
                remap(node);
            for (std::uint16_t& node : rig.additional_right_contact_nodes)
                remap(node);
            for (sim::MotorConstraint& motor : rig.motors)
            {
                remap(motor.a);
                remap(motor.pivot);
                remap(motor.c);
            }
        }

        [[nodiscard]] std::string_view mutation_name(RigMutationKind kind) noexcept
        {
            switch (kind)
            {
            case RigMutationKind::motor_strength: return "MOTOR STRENGTH";
            case RigMutationKind::joint_range: return "JOINT RANGE";
            case RigMutationKind::support_width: return "SUPPORT WIDTH";
            case RigMutationKind::torso_height: return "TORSO HEIGHT";
            case RigMutationKind::pivot_height: return "PIVOT HEIGHT";
            case RigMutationKind::split_bone: return "SPLIT BONE";
            case RigMutationKind::append_leaf: return "APPEND BRANCH";
            case RigMutationKind::duplicate_support: return "DUPLICATE SUPPORT";
            case RigMutationKind::remove_leaf: return "REMOVE LEAF";
            case RigMutationKind::node_radius: return "NODE RADIUS";
            case RigMutationKind::bone_stiffness: return "BONE STIFFNESS";
            }
            return "UNKNOWN";
        }
    }

    RigMutationCandidate evolve_rig_candidate(const sim::CreatureBlueprint& source,
        std::uint64_t generation) noexcept
    {
        RigMutationCandidate result{};
        result.blueprint = source;
        result.kind = static_cast<RigMutationKind>(generation % 11u);
        sim::CreatureBlueprint& candidate = result.blueprint;
        const std::uint64_t original_signature = source.signature();
        const float direction = ((generation / 11u) & 1u) == 0u ? 1.0f : -1.0f;

        std::array<float, sim::action_count> negative{};
        std::array<float, sim::action_count> positive{};
        std::array<float, sim::action_count> power{};
        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
        {
            negative[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].neutral_angle - candidate.motors[index].minimum_angle);
            positive[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].maximum_angle - candidate.motors[index].neutral_angle);
            power[index] = candidate.motors[index].strength;
        }

        switch (result.kind)
        {
        case RigMutationKind::motor_strength:
        {
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                power[index] = clamp(power[index] + direction * 0.0020f,
                    0.020f, 0.11f);
            }
            break;
        }
        case RigMutationKind::joint_range:
        {
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                const float delta = direction * 1.5f * pi / 180.0f;
                negative[index] = clamp(negative[index] + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
                positive[index] = clamp(positive[index] + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
            }
            break;
        }
        case RigMutationKind::support_width:
        {
            const float delta = direction * 0.018f;
            if (candidate.left_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.left_contact_node].x -= delta;
            if (candidate.right_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.right_contact_node].x += delta;
            break;
        }
        case RigMutationKind::torso_height:
        {
            const float delta = direction * 0.018f;
            if (candidate.torso_node < candidate.nodes.size())
                candidate.nodes[candidate.torso_node].y = clamp(
                    candidate.nodes[candidate.torso_node].y + delta, 0.40f, 6.0f);
            if (candidate.head_node < candidate.nodes.size())
                candidate.nodes[candidate.head_node].y = clamp(
                    candidate.nodes[candidate.head_node].y + delta, 0.45f, 6.5f);
            break;
        }
        case RigMutationKind::pivot_height:
        {
            const float delta = direction * 0.014f;
            for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
            {
                const std::uint16_t pivot = candidate.motors[index].pivot;
                if (pivot < candidate.nodes.size() && pivot != candidate.root_node)
                    candidate.nodes[pivot].y = clamp(candidate.nodes[pivot].y + delta,
                        0.20f, 5.5f);
            }
            break;
        }
        case RigMutationKind::split_bone:
        {
            if (!candidate.bones.empty() && candidate.nodes.size() < 128u
                && candidate.bones.size() < 256u)
            {
                const std::size_t bone_index = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                const sim::DistanceConstraint original = candidate.bones[bone_index];
                const auto inserted = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back((candidate.nodes[original.a]
                    + candidate.nodes[original.b]) * 0.5f);
                candidate.radii.push_back(clamp(0.5f
                    * (candidate.radii[original.a] + candidate.radii[original.b])
                    * 0.86f, 0.07f, 0.45f));
                candidate.bones[bone_index].b = inserted;
                candidate.bones.push_back({ inserted, original.b, 0.0f,
                    original.stiffness });
                for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
                {
                    sim::MotorConstraint& motor = candidate.motors[index];
                    if (same_edge(motor.a, motor.pivot, original.a, original.b))
                        motor.a = inserted;
                    if (same_edge(motor.pivot, motor.c, original.a, original.b))
                        motor.c = inserted;
                }
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::append_leaf:
        {
            if (candidate.nodes.size() < 128u && candidate.bones.size() < 256u)
            {
                const std::uint16_t parent = candidate.torso_node < candidate.nodes.size()
                    ? candidate.torso_node : candidate.root_node;
                const auto appended = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back(candidate.nodes[parent]
                    + Vec2{ direction * 0.34f, 0.10f });
                candidate.radii.push_back(0.11f);
                candidate.bones.push_back({ parent, appended, 0.0f, 0.72f });
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::duplicate_support:
        {
            const bool left = ((generation / 11u) & 1u) == 0u;
            const std::uint16_t contact = left
                ? candidate.left_contact_node : candidate.right_contact_node;
            if (contact < candidate.nodes.size() && candidate.nodes.size() < 128u
                && candidate.bones.size() < 256u)
            {
                std::uint16_t parent = candidate.root_node;
                for (const sim::DistanceConstraint& bone : candidate.bones)
                {
                    if (bone.a == contact) { parent = bone.b; break; }
                    if (bone.b == contact) { parent = bone.a; break; }
                }
                const auto duplicated = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back(candidate.nodes[contact]
                    + Vec2{ left ? -0.12f : 0.12f, 0.015f });
                candidate.radii.push_back(clamp(candidate.radii[contact] * 0.82f,
                    0.06f, 0.24f));
                candidate.bones.push_back({ parent, duplicated, 0.0f, 0.92f });
                if (left)
                    candidate.additional_left_contact_nodes.push_back(duplicated);
                else
                    candidate.additional_right_contact_nodes.push_back(duplicated);
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::remove_leaf:
        {
            std::vector<std::uint16_t> removable{};
            for (std::size_t node = 0; node < candidate.nodes.size(); ++node)
            {
                if (node == candidate.root_node || node == candidate.torso_node
                    || node == candidate.head_node || candidate.is_support_seed(node)
                    || motor_references_node(candidate, node)
                    || node_degree(candidate, node) != 1u)
                    continue;
                removable.push_back(static_cast<std::uint16_t>(node));
            }
            if (!removable.empty() && candidate.nodes.size() > 3u)
            {
                const std::uint16_t removed = removable[static_cast<std::size_t>(
                    generation % removable.size())];
                remove_node(candidate, removed);
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::node_radius:
        {
            if (!candidate.radii.empty())
            {
                const std::size_t node = static_cast<std::size_t>(
                    generation % candidate.radii.size());
                candidate.radii[node] = clamp(candidate.radii[node]
                    + direction * 0.008f, 0.055f, 0.60f);
            }
            break;
        }
        case RigMutationKind::bone_stiffness:
        {
            if (!candidate.bones.empty())
            {
                const std::size_t bone = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                candidate.bones[bone].stiffness = clamp(
                    candidate.bones[bone].stiffness + direction * 0.025f,
                    0.20f, 1.0f);
            }
            break;
        }
        }

        recalibrate_after_geometry(candidate, negative, positive, power);
        result.changed = candidate.valid()
            && candidate.signature() != original_signature;
        if (!result.changed)
        {
            result.blueprint = source;
            result.topology_changed = false;
        }
        return result;
    }

    float AutonomousTrainer::evaluate_rig_locked(
        const sim::CreatureBlueprint& candidate, const PolicyNetwork& policy) const
    {
        if (!candidate.valid())
            return -std::numeric_limits<float>::infinity();

        constexpr std::size_t agents = 4;
        const sim::CourseStage stage = stage_;
        const float difficulty = difficulty_;
        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 1500 : 900;
        std::array<float, agents> scores{};
        std::array<std::jthread, agents> evaluators{};

        for (std::size_t agent = 0; agent < agents; ++agent)
        {
            evaluators[agent] = std::jthread([&candidate, &policy, &scores,
                stage, difficulty, maximum_steps, agent]
            {
                const std::uint64_t seed = 0xA100u
                    + static_cast<std::uint64_t>(agent) * 3253u;
                sim::Environment environment{ candidate, seed };
                environment.set_course(stage, difficulty);
                float reward = 0.0f;
                for (int step = 0; step < maximum_steps; ++step)
                {
                    const auto raw_action = policy.deterministic_action(
                        environment.observation());
                    const auto action = effective_policy_action(
                        environment, raw_action, stage);
                    const sim::StepResult result = environment.step(action);
                    reward += result.reward;
                    if (result.terminated)
                        break;
                }
                const StageMotionQualification qualification =
                    stage_motion_qualification(stage, environment);
                if (!qualification.valid)
                {
                    scores[agent] = -std::numeric_limits<float>::infinity();
                    return;
                }
                scores[agent] = reward + environment.distance_travelled() * 0.75f
                    + environment.elapsed_seconds() * 0.03f
                    + static_cast<float>(environment.gait_cycles()) * 0.03f
                    + environment.duck_seconds() * 0.06f
                    + static_cast<float>(environment.landed_jumps()) * 0.16f
                    + std::min(environment.maximum_spin_turns(), 3.0f) * 0.18f
                    + static_cast<float>(environment.obstacles_passed()) * 0.28f
                    - environment.collision_count() * 0.10f
                    - environment.airborne_ratio() * 0.20f
                    - environment.body_rolling_seconds() * 2.00f;
            });
        }
        for (std::jthread& evaluator : evaluators)
        {
            if (evaluator.joinable())
                evaluator.join();
        }

        float total = 0.0f;
        for (const float score : scores)
        {
            if (!std::isfinite(score))
                return -std::numeric_limits<float>::infinity();
            total += score;
        }
        return total / static_cast<float>(agents);
    }

    RigMutationCandidate AutonomousTrainer::mutate_rig_locked() noexcept
    {
        return evolve_rig_candidate(worker_.blueprint(), rig_generation_);
    }

    void AutonomousTrainer::attempt_rig_evolution_locked()
    {
        const sim::CreatureBlueprint champion_rig = worker_.blueprint();
        const PpoTrainer::CheckpointData champion_checkpoint = worker_.checkpoint_data();
        const float baseline = evaluate_rig_locked(champion_rig, worker_.policy());
        RigMutationCandidate mutation = mutate_rig_locked();
        ++rig_generation_;
        if (!mutation.changed)
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "RIG GENERATION {} REJECTED - INVALID/EMPTY {} MUTATION",
                rig_generation_, mutation_name(mutation.kind));
            return;
        }

        sim::CreatureBlueprint candidate = std::move(mutation.blueprint);
        PpoTrainer nursery(candidate, 16, false);
        PpoTrainer::CheckpointData transfer = champion_checkpoint;
        std::string transfer_error{};
        if (!nursery.apply_checkpoint_data(std::move(transfer), transfer_error, true))
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} REJECTED - POLICY TRANSFER FAILED",
                rig_generation_);
            return;
        }
        nursery.set_course(stage_, difficulty_, false);
        nursery.set_exploration(std::max(0.10f, worker_.exploration()));
        constexpr int nursery_updates = 4;
        for (int update = 0; update < nursery_updates; ++update)
            nursery.train_one_update();

        const float candidate_score = evaluate_rig_locked(candidate, nursery.policy());
        const float required_gain = std::isfinite(baseline)
            ? std::max(0.025f, std::abs(baseline) * 0.01f) : 0.025f;
        if (std::isfinite(candidate_score)
            && (!std::isfinite(baseline) || candidate_score > baseline + required_gain))
        {
            worker_.set_blueprint(candidate, true);
            PpoTrainer::CheckpointData adapted = nursery.checkpoint_data();
            std::string apply_error{};
            if (!worker_.apply_checkpoint_data(std::move(adapted), apply_error, false))
            {
                worker_.set_blueprint(champion_rig, true);
                PpoTrainer::CheckpointData restore = champion_checkpoint;
                std::string restore_error{};
                static_cast<void>(worker_.apply_checkpoint_data(
                    std::move(restore), restore_error, false));
                ++rejected_rig_changes_;
                ++rollback_count_;
                worker_message_ = std::format(
                    "TOPOLOGY NURSERY {} ROLLED BACK - ADAPTED POLICY APPLY FAILED",
                    rig_generation_);
                return;
            }
            ++accepted_rig_changes_;
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} ACCEPTED {}  {:+.3f} VALID SCORE",
                rig_generation_, mutation_name(mutation.kind),
                candidate_score - (std::isfinite(baseline) ? baseline : 0.0f));
            queue_autosave();
        }
        else
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} REJECTED {} - NO VALID IMPROVEMENT",
                rig_generation_, mutation_name(mutation.kind));
        }
    }

}
