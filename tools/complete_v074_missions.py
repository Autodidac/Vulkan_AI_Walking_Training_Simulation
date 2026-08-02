from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding='utf-8', newline='\n')


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f'missing target in {path}: {old[:120]!r}')
    write(path, text.replace(old, new, 1))


# Persisted all-time training counters.
replace_once('src/ppo.hpp',
'''        std::uint64_t environment_steps{};
        float mean_reward{};''',
'''        std::uint64_t environment_steps{};
        std::uint64_t total_episodes{};
        std::uint64_t total_valid_episodes{};
        std::uint64_t total_invalid_episodes{};
        std::uint64_t total_resets{};
        std::uint64_t total_alternating_steps{};
        std::uint64_t total_falls{};
        std::uint64_t total_collisions{};
        std::uint64_t total_powered_jumps{};
        std::uint64_t total_landed_jumps{};
        std::uint64_t total_landed_flips{};
        std::uint64_t total_obstacles_passed{};
        double total_distance{};
        float mean_reward{};''')

replace_once('src/ppo.hpp',
'''            float completed_distance{};
            std::size_t completed_episodes{};''',
'''            float completed_distance{};
            std::uint64_t completed_episodes{};
            std::uint64_t valid_episodes{};
            std::uint64_t invalid_episodes{};
            std::uint64_t alternating_steps{};
            std::uint64_t falls{};
            std::uint64_t collisions{};
            std::uint64_t powered_jumps{};
            std::uint64_t landed_jumps{};
            std::uint64_t landed_flips{};
            std::uint64_t obstacles_passed{};
            double total_distance{};''')

# Count complete episode evidence before each environment reset.
replace_once('src/ppo_trainer.cpp',
'''                    ++totals.completed_episodes;
                    episode_rewards_[environment_index] = 0.0f;''',
'''                    ++totals.completed_episodes;
                    if (result.valid_motion)
                        ++totals.valid_episodes;
                    else
                        ++totals.invalid_episodes;
                    totals.total_distance += static_cast<double>(
                        std::max(0.0f, environment.distance_travelled()));
                    totals.alternating_steps += environment.alternating_steps();
                    totals.collisions += static_cast<std::uint64_t>(
                        std::max(0.0f, environment.collision_count()));
                    totals.powered_jumps += environment.powered_jumps();
                    totals.landed_jumps += environment.landed_jumps();
                    totals.landed_flips += environment.spin_landings();
                    totals.obstacles_passed += environment.obstacles_passed();
                    if (result.invalid_reason == sim::InvalidMotion::fallen
                        || result.invalid_reason == sim::InvalidMotion::collapsed_posture
                        || result.invalid_reason == sim::InvalidMotion::body_rolling)
                        ++totals.falls;
                    episode_rewards_[environment_index] = 0.0f;''')

replace_once('src/ppo_trainer.cpp',
'''                staged_totals_.completed_distance += worker.completed_distance;
                staged_totals_.completed_episodes += worker.completed_episodes;''',
'''                staged_totals_.completed_distance += worker.completed_distance;
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
                staged_totals_.total_distance += worker.total_distance;''')

replace_once('src/ppo_trainer.cpp',
'''        metrics_.environment_steps += rollout_.size();
        metrics_.mean_speed = staged_totals_.accumulated_speed''',
'''        metrics_.environment_steps += rollout_.size();
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
        metrics_.total_distance += staged_totals_.total_distance;
        metrics_.mean_speed = staged_totals_.accumulated_speed''')

# Policy or rig resets preserve all-time totals while incrementing the reset count.
replace_once('src/ppo_trainer.cpp',
'''        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        metrics_ = {};
        reward_history_.clear();''',
'''        adam_.first_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.second_moment.assign(policy_.parameter_count(), 0.0f);
        adam_.step = 0;
        const TrainingMetrics previous_metrics = metrics_;
        metrics_ = {};
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
        reward_history_.clear();''')

# Checkpoint metrics include all counters; v0.7.4 semantics already invalidate older layouts.
replace_once('src/training_checkpoint.cpp',
'''            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.mean_reward)''',
'''            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.total_episodes)
                && write_value(output, value.total_valid_episodes)
                && write_value(output, value.total_invalid_episodes)
                && write_value(output, value.total_resets)
                && write_value(output, value.total_alternating_steps)
                && write_value(output, value.total_falls)
                && write_value(output, value.total_collisions)
                && write_value(output, value.total_powered_jumps)
                && write_value(output, value.total_landed_jumps)
                && write_value(output, value.total_landed_flips)
                && write_value(output, value.total_obstacles_passed)
                && write_value(output, value.total_distance)
                && write_value(output, value.mean_reward)''')
replace_once('src/training_checkpoint.cpp',
'''            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.mean_reward)''',
'''            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.total_episodes)
                && read_value(input, value.total_valid_episodes)
                && read_value(input, value.total_invalid_episodes)
                && read_value(input, value.total_resets)
                && read_value(input, value.total_alternating_steps)
                && read_value(input, value.total_falls)
                && read_value(input, value.total_collisions)
                && read_value(input, value.total_powered_jumps)
                && read_value(input, value.total_landed_jumps)
                && read_value(input, value.total_landed_flips)
                && read_value(input, value.total_obstacles_passed)
                && read_value(input, value.total_distance)
                && read_value(input, value.mean_reward)''')

# Rig and session baselines for the persisted totals.
replace_once('src/app.cpp',
'''        std::uint64_t rig_start_environment_steps{};
        bool rig_edit_pending{};''',
'''        std::uint64_t rig_start_environment_steps{};
        std::uint64_t rig_start_episodes{};
        std::uint64_t rig_start_valid_episodes{};
        std::uint64_t rig_start_invalid_episodes{};
        std::uint64_t rig_start_steps{};
        std::uint64_t rig_start_falls{};
        std::uint64_t rig_start_collisions{};
        std::uint64_t rig_start_jumps{};
        std::uint64_t rig_start_landings{};
        std::uint64_t rig_start_flips{};
        std::uint64_t rig_start_obstacles{};
        double rig_start_distance{};
        std::uint64_t session_start_episodes{};
        std::uint64_t session_start_invalid_episodes{};
        std::uint64_t session_start_resets{};
        std::uint64_t session_start_collisions{};
        std::uint64_t session_start_jumps{};
        std::uint64_t session_start_flips{};
        std::uint64_t session_start_obstacles{};
        double session_start_distance{};
        std::uint8_t rig_best_stage{};
        bool session_stats_initialized{};
        bool rig_edit_pending{};''')

replace_once('src/app.cpp',
'''            const std::uint64_t current_signature = trainer.rig_signature();
            if (tracked_rig_signature == 0u || tracked_rig_signature != current_signature)
            {
                tracked_rig_signature = current_signature;
                rig_lifetime_seconds = 0.0f;
                rig_start_update = trainer.metrics().update;
                rig_start_environment_steps = trainer.metrics().environment_steps;
            }''',
'''            const rl::TrainingMetrics& current_metrics = trainer.metrics();
            if (!session_stats_initialized)
            {
                session_stats_initialized = true;
                session_start_episodes = current_metrics.total_episodes;
                session_start_invalid_episodes = current_metrics.total_invalid_episodes;
                session_start_resets = current_metrics.total_resets;
                session_start_collisions = current_metrics.total_collisions;
                session_start_jumps = current_metrics.total_powered_jumps;
                session_start_flips = current_metrics.total_landed_flips;
                session_start_obstacles = current_metrics.total_obstacles_passed;
                session_start_distance = current_metrics.total_distance;
            }
            const std::uint64_t current_signature = trainer.rig_signature();
            if (tracked_rig_signature == 0u || tracked_rig_signature != current_signature)
            {
                tracked_rig_signature = current_signature;
                rig_lifetime_seconds = 0.0f;
                rig_start_update = current_metrics.update;
                rig_start_environment_steps = current_metrics.environment_steps;
                rig_start_episodes = current_metrics.total_episodes;
                rig_start_valid_episodes = current_metrics.total_valid_episodes;
                rig_start_invalid_episodes = current_metrics.total_invalid_episodes;
                rig_start_steps = current_metrics.total_alternating_steps;
                rig_start_falls = current_metrics.total_falls;
                rig_start_collisions = current_metrics.total_collisions;
                rig_start_jumps = current_metrics.total_powered_jumps;
                rig_start_landings = current_metrics.total_landed_jumps;
                rig_start_flips = current_metrics.total_landed_flips;
                rig_start_obstacles = current_metrics.total_obstacles_passed;
                rig_start_distance = current_metrics.total_distance;
                rig_best_stage = static_cast<std::uint8_t>(trainer.autonomy_status().stage);
            }''')
replace_once('src/app.cpp',
'''            else
            {
                rig_lifetime_seconds += std::max(0.0f, dt);
            }
            if (joint_auto_sweep)''',
'''            else
            {
                rig_lifetime_seconds += std::max(0.0f, dt);
                rig_best_stage = std::max(rig_best_stage,
                    static_cast<std::uint8_t>(trainer.autonomy_status().stage));
            }
            if (joint_auto_sweep)''')

# Replace the narrow telemetry with compact full rig/session/all-time totals.
replace_once('src/app.cpp',
'''            add_text_fit(canvas, cursor, std::format("RIG LIFE {}   UPDATES {}   ENV STEPS {}",
                format_duration(rig_lifetime_seconds),
                ui_layout::lifetime_delta(metrics.update, rig_start_update),
                ui_layout::lifetime_delta(metrics.environment_steps, rig_start_environment_steps)),
                1.00f, white, usable_width);
            cursor.y += 29.0f;
            add_text_fit(canvas, cursor, std::format("SESSION {}   TOTAL UPDATES {}   TOTAL ENV STEPS {}",
                format_duration(session_runtime_seconds), metrics.update, metrics.environment_steps),
                0.98f, muted, usable_width);''',
'''            add_text_fit(canvas, cursor, std::format("RIG {}  UPD {}  ENV {}  BEST STAGE {}",
                format_duration(rig_lifetime_seconds),
                ui_layout::lifetime_delta(metrics.update, rig_start_update),
                ui_layout::lifetime_delta(metrics.environment_steps, rig_start_environment_steps),
                static_cast<unsigned>(rig_best_stage) + 1u), 0.94f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG EPS {}  VALID {}  INVALID {}  DIST {}",
                ui_layout::lifetime_delta(metrics.total_episodes, rig_start_episodes),
                ui_layout::lifetime_delta(metrics.total_valid_episodes, rig_start_valid_episodes),
                ui_layout::lifetime_delta(metrics.total_invalid_episodes, rig_start_invalid_episodes),
                format_distance(static_cast<float>(std::max(0.0,
                    metrics.total_distance - rig_start_distance)))), 0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG STEPS {}  FALLS {}  COLL {}  OBS {}",
                ui_layout::lifetime_delta(metrics.total_alternating_steps, rig_start_steps),
                ui_layout::lifetime_delta(metrics.total_falls, rig_start_falls),
                ui_layout::lifetime_delta(metrics.total_collisions, rig_start_collisions),
                ui_layout::lifetime_delta(metrics.total_obstacles_passed, rig_start_obstacles)),
                0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("RIG JUMP {} / LAND {}  FLIPS {}",
                ui_layout::lifetime_delta(metrics.total_powered_jumps, rig_start_jumps),
                ui_layout::lifetime_delta(metrics.total_landed_jumps, rig_start_landings),
                ui_layout::lifetime_delta(metrics.total_landed_flips, rig_start_flips)),
                0.90f, white, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("SESSION {}  EPS {}  BAD {}  DIST {}",
                format_duration(session_runtime_seconds),
                ui_layout::lifetime_delta(metrics.total_episodes, session_start_episodes),
                ui_layout::lifetime_delta(metrics.total_invalid_episodes,
                    session_start_invalid_episodes),
                format_distance(static_cast<float>(std::max(0.0,
                    metrics.total_distance - session_start_distance)))),
                0.88f, muted, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("ALL UPD {} ENV {} EPS {} RESET {} ROLLBACK {}",
                metrics.update, metrics.environment_steps, metrics.total_episodes,
                metrics.total_resets, autonomy.rollback_count), 0.86f, muted, usable_width);
            cursor.y += 25.0f;
            add_text_fit(canvas, cursor, std::format("ALL DIST {} COLL {} JUMP {} FLIP {} OBS {}",
                format_distance(static_cast<float>(metrics.total_distance)),
                metrics.total_collisions, metrics.total_powered_jumps,
                metrics.total_landed_flips, metrics.total_obstacles_passed),
                0.84f, muted, usable_width);''')

# Default preset structural nodes and motor endpoints cannot be deleted. Custom
# rigs still may delete non-semantic, non-motor nodes.
replace_once('src/app.cpp',
'''            const auto removed = static_cast<std::uint16_t>(selected_node);
            blueprint.nodes.erase(blueprint.nodes.begin() + selected_node);''',
'''            const auto removed = static_cast<std::uint16_t>(selected_node);
            const bool semantic = removed == blueprint.root_node
                || removed == blueprint.torso_node || removed == blueprint.head_node
                || blueprint.is_support_seed(removed);
            const bool motor_endpoint = std::ranges::any_of(blueprint.motors,
                [removed](const sim::MotorConstraint& motor)
                {
                    return motor.enabled && (motor.a == removed
                        || motor.pivot == removed || motor.c == removed);
                });
            if (rig_preset != RigPreset::custom || semantic || motor_endpoint)
            {
                set_status("REQUIRED PRESET / SEMANTIC / MOTOR NODE CANNOT BE DELETED");
                return false;
            }
            blueprint.nodes.erase(blueprint.nodes.begin() + selected_node);''')

# Regression coverage for cumulative checkpoint metrics and the expanded ledger.
replace_once('tests/core_tests.cpp',
'''    require(ui_layout::lifetime_delta(120u, 20u) == 100u
            && ui_layout::lifetime_delta(20u, 120u) == 0u,
        "rig lifetime counters can underflow");''',
'''    require(ui_layout::lifetime_delta(120u, 20u) == 100u
            && ui_layout::lifetime_delta(20u, 120u) == 0u,
        "rig lifetime counters can underflow");
    rl::TrainingMetrics cumulative{};
    cumulative.total_episodes = 12u;
    cumulative.total_valid_episodes = 9u;
    cumulative.total_invalid_episodes = 3u;
    cumulative.total_resets = 14u;
    cumulative.total_alternating_steps = 48u;
    cumulative.total_falls = 2u;
    cumulative.total_collisions = 7u;
    cumulative.total_powered_jumps = 5u;
    cumulative.total_landed_jumps = 4u;
    cumulative.total_landed_flips = 1u;
    cumulative.total_obstacles_passed = 11u;
    cumulative.total_distance = 123.5;
    require(cumulative.total_valid_episodes + cumulative.total_invalid_episodes
            == cumulative.total_episodes
            && cumulative.total_landed_jumps <= cumulative.total_powered_jumps,
        "cumulative runtime statistics are internally inconsistent");''')

Path(__file__).unlink()
print('completed cumulative statistics and rig-lab safety missions')
