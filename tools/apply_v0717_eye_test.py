#!/usr/bin/env python3
from __future__ import annotations
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")

def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"{label}: marker not found")
    return text.replace(old, new, 1)

def regex_once(text: str, pattern: str, replacement: str, label: str, flags=0) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=flags)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, got {count}")
    return result

# --- Physics feet and press recovery ---
path = "src/simulation.cpp"
text = read(path)
text = regex_once(
    text,
    r'''        void add_passive_feet\(CreatureBlueprint& rig, float heel_reach = 0\.20f,\n            float toe_reach = 0\.34f\) noexcept\n        \{.*?\n        \}\n\n        void calibrate_grounded_defaults''',
    r'''        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            // v0.7.17 uses one short physical support stub per leg. The visible
            // forward boot is a sprite and never participates in collision.
            // Retain the historical helper name to keep saved-preset call sites
            // source-compatible while changing the actual topology.
            static_cast<void>(heel_reach);
            static_cast<void>(toe_reach);
            auto add_stub = [&](std::uint16_t ankle) -> std::uint16_t
            {
                if (ankle >= rig.nodes.size() || rig.nodes.size() >= 127u)
                    return ankle;
                const Vec2 ankle_position = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.58f, 0.085f, 0.115f)
                    : 0.10f;
                const auto stub = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({
                    ankle_position.x + 0.055f,
                    ankle_position.y - 0.205f
                });
                rig.radii.push_back(radius);
                rig.bones.push_back({ ankle, stub, 0.0f, 1.0f });
                return stub;
            };

            rig.left_contact_node = add_stub(rig.left_contact_node);
            rig.right_contact_node = add_stub(rig.right_contact_node);
            rig.additional_left_contact_nodes.clear();
            rig.additional_right_contact_nodes.clear();
        }

        void calibrate_grounded_defaults''',
    "replace multi-node feet",
    re.S,
)
text = text.replace(
    "duck_press_profile(\n                    elapsed_seconds_, course_difficulty_, rest_head_top);",
    "duck_press_profile(\n                    elapsed_seconds_, course_difficulty_, rest_head_top,\n                    blueprint_.horizontal_body_plan());",
)
text = text.replace(
    "duck_press_profile(\n            elapsed_seconds_, course_difficulty_, rest_head_top);",
    "duck_press_profile(\n            elapsed_seconds_, course_difficulty_, rest_head_top,\n            blueprint_.horizontal_body_plan());",
)
text = replace_once(
    text,
    '''        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height, 0.52f, 1.0f);
        const float horizontal_scale = 1.0f + (1.0f - vertical_scale) * 0.12f;
''',
    '''        const bool horizontal_body = blueprint_.horizontal_body_plan();
        const float minimum_vertical_scale = horizontal_body ? 0.78f : 0.58f;
        const float vertical_scale = clamp(
            (rest_height - requested_drop) / rest_height,
            minimum_vertical_scale, 1.0f);
        // A quadruped crouches by flexing four support chains, not by widening
        // and flattening the entire body into the floor.
        const float horizontal_scale = horizontal_body
            ? 1.0f : 1.0f + (1.0f - vertical_scale) * 0.08f;
''',
    "rig-aware compression scale",
)
text = replace_once(
    text,
    '''            constexpr float maximum_step = 0.60f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const Vec2 applied = correction * phase_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied;
''',
    '''            const float maximum_step = horizontal_body ? 0.028f : 0.055f;
            if (magnitude > maximum_step && magnitude > 1.0e-6f)
                correction *= maximum_step / magnitude;
            const float guide_strength = recovery_guide
                ? (horizontal_body ? 0.62f : 0.48f)
                : (horizontal_body ? 0.24f : 0.32f) * phase_strength;
            const Vec2 applied = correction * guide_strength;
            particles_[node].position += applied;
            particles_[node].previous += applied * 0.94f;
''',
    "bounded horizontal recovery guide",
)
text = replace_once(
    text,
    '''            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid()
                && current_uprightness >= 0.78f
                && head_height_ratio >= 0.82f
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f
                && !duck_press_completed_)
''',
    '''            const bool horizontal_recovery = blueprint_.horizontal_body_plan();
            const float recovery_uprightness = horizontal_recovery ? 0.70f : 0.78f;
            const float recovery_head_ratio = horizontal_recovery ? 0.72f : 0.82f;
            const float recovery_hold = horizontal_recovery ? 1.10f : 0.55f;
            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && body_integrity_valid()
                && current_uprightness >= recovery_uprightness
                && head_height_ratio >= recovery_head_ratio
                && std::abs(torso_angle) <= 0.40f
                && stance_slip_speed_ <= 0.16f
                && stable_stance_seconds_ >= recovery_hold
                && !duck_press_completed_)
''',
    "require stable quadruped recovery",
)
write(path, text)

# --- Shared contracts, safer press profile, and public gait evidence ---
path = "src/simulation.hpp"
text = read(path)
text = text.replace("return alternating_steps >= 4u;", "return alternating_steps >= 10u;", 1)
text = text.replace(
    "return alternating_steps >= 4u && duck_seconds >= 2.0f",
    "return alternating_steps >= 8u && duck_seconds >= 2.0f",
    1,
)
insert_marker = '''    [[nodiscard]] inline bool friction_driven_shuffle(float root_speed,
'''
contracts = '''    [[nodiscard]] inline bool sagittal_gait_evidence(
        std::uint32_t alternating_steps, std::uint32_t limb_crossings,
        float distance, float elapsed_seconds, float support_span_ratio) noexcept
    {
        return alternating_steps >= 10u
            && limb_crossings >= 8u
            && distance >= 6.0f
            && elapsed_seconds >= 8.0f
            && support_span_ratio >= 0.42f
            && support_span_ratio <= 1.45f;
    }

    [[nodiscard]] inline bool crab_walking_motion(
        std::uint32_t alternating_steps, std::uint32_t limb_crossings,
        float distance, float elapsed_seconds, float support_span_ratio) noexcept
    {
        return elapsed_seconds >= 4.0f
            && distance >= 0.75f
            && (support_span_ratio > 1.55f
                || (alternating_steps >= 4u && limb_crossings < 2u));
    }

'''
text = replace_once(text, insert_marker, contracts + insert_marker, "add gait contracts")
text = regex_once(
    text,
    r'''    \[\[nodiscard\]\] inline DuckPressProfile duck_press_profile\(float elapsed_seconds,\n        float difficulty, float standing_head_top\) noexcept\n    \{.*?\n    \}\n\n''',
    '''    [[nodiscard]] inline DuckPressProfile duck_press_profile(float elapsed_seconds,
        float difficulty, float standing_head_top,
        bool horizontal_body_plan = false) noexcept
    {
        constexpr float settle_end = 2.75f;
        constexpr float descend_end = 6.25f;
        constexpr float hold_end = 8.25f;
        constexpr float retract_end = 10.75f;
        constexpr float cycle = 12.25f;
        float local = std::fmod(std::max(0.0f, elapsed_seconds), cycle);
        if (local < 0.0f)
            local += cycle;
        const float start = standing_head_top + (horizontal_body_plan ? 0.72f : 1.10f);
        const float crouch_drop = horizontal_body_plan
            ? clamp(standing_head_top * 0.085f, 0.28f, 0.46f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.035f
            : clamp(standing_head_top * 0.15f, 0.72f, 0.84f)
                + clamp(difficulty, 0.0f, 1.0f) * 0.06f;
        const float target = standing_head_top - crouch_drop;
        if (local < settle_end)
            return { start, 0.0f, false, false, false };
        if (local < descend_end)
        {
            const float duration = descend_end - settle_end;
            const float t = (local - settle_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(start, target, smooth),
                (target - start) * derivative, true, false, false };
        }
        if (local < hold_end)
            return { target, 0.0f, false, true, false };
        if (local < retract_end)
        {
            const float duration = retract_end - hold_end;
            const float t = (local - hold_end) / duration;
            const float smooth = t * t * (3.0f - 2.0f * t);
            const float derivative = 6.0f * t * (1.0f - t) / duration;
            return { lerp(target, start, smooth),
                (start - target) * derivative, false, false, true };
        }
        return { start, 0.0f, false, false, false };
    }

''',
    "replace press profile",
    re.S,
)
write(path, text)

# --- PPO semantics, teacher, crab rejection, stricter sustained gait ---
path = "src/ppo.hpp"
text = read(path)
text = replace_once(text, "0x0007'1600u", "0x0007'1700u", "training semantics")
old_walk = '''        const float phase = environment.elapsed_seconds() * 2.0f * pi * 1.12f;
        const float swing = std::sin(phase);
        action[0] = clamp(action[0] + 0.42f * swing, -0.82f, 0.82f);
        action[1] = clamp(action[1] + 0.34f * std::max(0.0f, swing), -0.88f, 0.88f);
        action[2] = clamp(action[2] - 0.42f * swing, -0.82f, 0.82f);
        action[3] = clamp(action[3] - 0.34f * std::max(0.0f, -swing), -0.88f, 0.88f);
'''
new_walk = '''        const float phase = environment.elapsed_seconds() * 2.0f * pi * 0.96f;
        const float swing = std::sin(phase);
        const float left_lift = std::max(0.0f, swing);
        const float right_lift = std::max(0.0f, -swing);
        const float span_brake = clamp(
            (environment.primary_support_span_ratio() - 1.08f) * 0.52f,
            0.0f, 0.34f);
        // Sagittal side-view gate: hips drive fore/aft in opposite phase;
        // knees lift only the swing chain, then extend before landing.
        action[0] = clamp(action[0] + 0.58f * swing - span_brake, -0.90f, 0.90f);
        action[1] = clamp(action[1] + 0.54f * left_lift
            - 0.18f * right_lift, -0.92f, 0.92f);
        action[2] = clamp(action[2] - 0.58f * swing + span_brake, -0.90f, 0.90f);
        action[3] = clamp(action[3] - 0.54f * right_lift
            + 0.18f * left_lift, -0.92f, 0.92f);
'''
text = replace_once(text, old_walk, new_walk, "sagittal walk teacher")
text = replace_once(
    text,
    "        invalid_crouch_posture = 1u << 9u\n",
    "        invalid_crouch_posture = 1u << 9u,\n        lateral_crab_gait = 1u << 10u\n",
    "crab evidence enum",
)
text = replace_once(
    text,
    '''        if ((mask & evidence_bit(MotionEvidenceFailure::invalid_crouch_posture)) != 0u)
            return "HIP HINGE - NOT A CROUCH";
''',
    '''        if ((mask & evidence_bit(MotionEvidenceFailure::invalid_crouch_posture)) != 0u)
            return "HIP HINGE - NOT A CROUCH";
        if ((mask & evidence_bit(MotionEvidenceFailure::lateral_crab_gait)) != 0u)
            return "CRAB WALK - NO SAGITTAL CROSSING";
''',
    "crab rejection label",
)
text = replace_once(
    text,
    '''        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.gait_cycles() < 4u
                || (environment.blueprint().paired_leg_chains()
                    && environment.limb_crossings() < 4u))
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 1.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
''',
    '''        case sim::CourseStage::uneven:
            if (environment.longest_stable_stance_seconds() < 1.25f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.blueprint().paired_leg_chains()
                && sim::crab_walking_motion(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio()))
                rejection |= evidence_bit(MotionEvidenceFailure::lateral_crab_gait);
            if (environment.blueprint().paired_leg_chains()
                ? !sim::sagittal_gait_evidence(environment.alternating_steps(),
                    environment.limb_crossings(), environment.distance_travelled(),
                    environment.elapsed_seconds(),
                    environment.primary_support_span_ratio())
                : environment.gait_cycles() < 10u)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_skill);
            if (environment.distance_travelled() < 6.0f
                || environment.elapsed_seconds() < 8.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::missing_progress);
            break;
''',
    "sustained sagittal qualification",
)
write(path, text)

# --- Curriculum dwell baselines and stronger walking mastery ---
path = "src/autonomy.hpp"
text = read(path)
dwell_insert = '''    [[nodiscard]] inline bool strict_balance_mastery(
'''
dwell_helpers = '''    [[nodiscard]] inline std::uint64_t stage_minimum_fresh_updates(
        sim::CourseStage stage) noexcept
    {
        switch (stage)
        {
        case sim::CourseStage::balance: return 120u;
        case sim::CourseStage::duck_press: return 180u;
        case sim::CourseStage::uneven: return 420u;
        case sim::CourseStage::crouch_walk: return 360u;
        default: return 240u;
        }
    }

    [[nodiscard]] inline std::uint64_t stage_minimum_fresh_episodes(
        sim::CourseStage stage) noexcept
    {
        switch (stage)
        {
        case sim::CourseStage::balance: return 3u;
        case sim::CourseStage::duck_press: return 4u;
        case sim::CourseStage::uneven: return 8u;
        default: return 5u;
        }
    }

    [[nodiscard]] inline bool stage_fresh_work_complete(
        sim::CourseStage stage, std::uint64_t fresh_updates,
        std::uint64_t fresh_episodes, std::uint64_t fresh_evaluations) noexcept
    {
        return fresh_updates >= stage_minimum_fresh_updates(stage)
            && fresh_episodes >= stage_minimum_fresh_episodes(stage)
            && fresh_evaluations >= static_cast<std::uint64_t>(
                required_mastery_confirmations(stage));
    }

'''
text = replace_once(text, dwell_insert, dwell_helpers + dwell_insert, "dwell helpers")
fields_marker = '''        std::uint64_t last_evaluation_count_{};
        std::uint64_t last_saved_best_update_{};
'''
text = replace_once(
    text,
    fields_marker,
    '''        std::uint64_t last_evaluation_count_{};
        std::uint64_t last_saved_best_update_{};
        std::uint64_t stage_entry_total_updates_{};
        std::uint64_t stage_entry_total_episodes_{};
        std::uint64_t stage_entry_evaluation_count_{};
        bool stage_entry_baseline_initialized_{};
''',
    "stage baseline fields",
)
write(path, text)

path = "src/autonomy_curriculum.cpp"
text = read(path)
text = replace_once(
    text,
    '''        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_stride_events >= 8.0f
                && metrics.evaluation_speed >= 0.70f
                && metrics.evaluation_collisions <= 1.0f;
''',
    '''        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 18.0f
                && metrics.evaluation_stride_events >= 16.0f
                && metrics.evaluation_speed >= 0.55f
                && metrics.evaluation_survival >= 18.0f
                && metrics.evaluation_collisions <= 1.0f;
''',
    "sustained walking mastery",
)
manage_marker = '''        if (metrics.evaluation_count == 0 || metrics.evaluation_count == last_evaluation_count_)
            return;
        last_evaluation_count_ = metrics.evaluation_count;
'''
manage_repl = '''        if (!stage_entry_baseline_initialized_)
        {
            stage_entry_total_updates_ = metrics.total_updates;
            stage_entry_total_episodes_ = metrics.total_episodes;
            stage_entry_evaluation_count_ = metrics.evaluation_count;
            stage_entry_baseline_initialized_ = true;
        }
        if (metrics.evaluation_count == 0 || metrics.evaluation_count == last_evaluation_count_)
            return;
        last_evaluation_count_ = metrics.evaluation_count;
'''
text = replace_once(text, manage_marker, manage_repl, "initialize stage baselines")
text = replace_once(
    text,
    '''        mastery_streak_ = stage_mastered_locked() ? mastery_streak_ + 1 : 0;
        const int required_confirmations = required_mastery_confirmations(stage_);
''',
    '''        const std::uint64_t fresh_updates = metrics.total_updates
            >= stage_entry_total_updates_
            ? metrics.total_updates - stage_entry_total_updates_ : 0u;
        const std::uint64_t fresh_episodes = metrics.total_episodes
            >= stage_entry_total_episodes_
            ? metrics.total_episodes - stage_entry_total_episodes_ : 0u;
        const std::uint64_t fresh_evaluations = metrics.evaluation_count
            >= stage_entry_evaluation_count_
            ? metrics.evaluation_count - stage_entry_evaluation_count_ : 0u;
        const bool dwell_complete = stage_fresh_work_complete(stage_,
            fresh_updates, fresh_episodes, fresh_evaluations);
        mastery_streak_ = dwell_complete && stage_mastered_locked()
            ? mastery_streak_ + 1 : 0;
        const int required_confirmations = required_mastery_confirmations(stage_);
''',
    "enforce fresh stage dwell",
)
text = replace_once(
    text,
    '''        worker_.set_course(stage_, difficulty_, false);
        queue_autosave();
''',
    '''        worker_.set_course(stage_, difficulty_, false);
        const TrainingMetrics& entered = worker_.metrics();
        stage_entry_total_updates_ = entered.total_updates;
        stage_entry_total_episodes_ = entered.total_episodes;
        stage_entry_evaluation_count_ = entered.evaluation_count;
        stage_entry_baseline_initialized_ = true;
        queue_autosave();
''',
    "reset stage baselines on advance",
)
write(path, text)

# --- App version paths, optional art loading, shared pixel-art draw, and layered creature ---
path = "src/app.cpp"
text = read(path)
text = text.replace("runner-v0716-gait-", "runner-v0717-gait-")
text = replace_once(
    text,
    '''        art::PixelArt original_runner_art{};
        std::string status{ "AUTOPILOT STARTING" };
''',
    '''        art::PixelArt original_runner_art{};
        art::PixelArt optional_foot_art{};
        art::PixelArt optional_helmet_art{};
        art::PixelArt optional_torso_art{};
        art::PixelArt optional_weapon_art{};
        bool optional_art_enabled{ true };
        bool debug_skeleton_overlay{};
        std::string status{ "AUTOPILOT STARTING" };
''',
    "optional art state",
)
helper_marker = '''        [[nodiscard]] float fit_text_scale(std::string_view text, float requested_scale,
'''
pixel_helper = '''        void draw_pixel_art(render::Canvas& canvas, const art::PixelArt& art,
            Rect target, float alpha = 1.0f)
        {
            if (!art.loaded() || target.size.x <= 0.0f || target.size.y <= 0.0f)
                return;
            const float pixel_width = target.size.x / static_cast<float>(art.width);
            const float pixel_height = target.size.y / static_cast<float>(art.height);
            for (int y = 0; y < art.height; ++y)
            {
                for (int x = 0; x < art.width; ++x)
                {
                    Color color = art.pixels[static_cast<std::size_t>(
                        y * art.width + x)];
                    if (std::max({ color.r, color.g, color.b }) < 0.035f)
                        continue;
                    color.a *= alpha;
                    const Vec2 minimum = target.position + Vec2{
                        static_cast<float>(x) * pixel_width,
                        static_cast<float>(y) * pixel_height
                    };
                    canvas.quad(minimum,
                        minimum + Vec2{ pixel_width + 0.35f,
                            pixel_height + 0.35f }, color);
                }
            }
        }

'''
text = replace_once(text, helper_marker, pixel_helper + helper_marker, "pixel art canvas helper")
init_marker = '''        impl_->trainer.set_autosave_paths(impl_->autosave_policy_path,
'''
init_assets = '''        auto load_optional = [&](std::string_view name, art::PixelArt& destination)
        {
            std::string optional_error{};
            const std::filesystem::path path = asset_directory / "optional"
                / "runner_armor_concepts" / "runtime" / std::string(name);
            if (!art::load_p3_pixel_art(path, destination, optional_error))
                destination = {};
        };
        load_optional("foot_side.ppm", impl_->optional_foot_art);
        load_optional("helmet_side.ppm", impl_->optional_helmet_art);
        load_optional("torso_side.ppm", impl_->optional_torso_art);
        load_optional("weapon_side.ppm", impl_->optional_weapon_art);

'''
text = replace_once(text, init_marker, init_assets + init_marker, "load optional art")
text = regex_once(
    text,
    r'''        void draw_creature\(const sim::Environment& environment, Rect viewport, float camera,\n            float scale, bool show_nodes = false\)\n        \{.*?\n        \}\n\n        void draw_training_pip''',
    r'''        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty())
                return;
            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            auto leg_side = [&](std::size_t index) noexcept
            {
                if (rig.is_left_support_seed(index))
                    return -1;
                if (rig.is_right_support_seed(index))
                    return 1;
                if (rig.paired_leg_chains())
                {
                    if (index == rig.motors[0].pivot || index == rig.motors[0].c
                        || index == rig.motors[1].pivot || index == rig.motors[1].c)
                        return -1;
                    if (index == rig.motors[2].pivot || index == rig.motors[2].c
                        || index == rig.motors[3].pivot || index == rig.motors[3].c)
                        return 1;
                }
                return 0;
            };
            auto draw_bones = [&](int pass)
            {
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    if (bone.a >= particles.size() || bone.b >= particles.size())
                        continue;
                    const int side_a = leg_side(bone.a);
                    const int side_b = leg_side(bone.b);
                    const int side = side_a != 0 ? side_a : side_b;
                    const bool near = side != 0 && ((side > 0) == right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;
                    const float radius_a = bone.a < rig.radii.size()
                        ? rig.radii[bone.a] : 0.15f;
                    const float radius_b = bone.b < rig.radii.size()
                        ? rig.radii[bone.b] : 0.15f;
                    const float radius = std::max(0.050f,
                        std::min(radius_a, radius_b) * 0.52f) * scale;
                    const Color color = side == 0 ? body
                        : near ? leg : rgb(0x5f493b);
                    canvas.capsule(point(bone.a), point(bone.b), radius, color, 16);
                }
            };
            auto draw_nodes = [&](int pass)
            {
                for (std::size_t index = 0; index < particles.size(); ++index)
                {
                    const int side = leg_side(index);
                    const bool near = side != 0 && ((side > 0) == right_leg_near);
                    const int layer = side == 0 ? 1 : near ? 2 : 0;
                    if (layer != pass)
                        continue;
                    const float radius = (index < rig.radii.size()
                        ? rig.radii[index] : 0.15f) * scale;
                    Color color = index == rig.head_node ? body_light : body;
                    if (side != 0)
                        color = near ? leg : rgb(0x5f493b);
                    if (rig.is_support_seed(index))
                    {
                        const Vec2 center = point(index);
                        if (optional_art_enabled && optional_foot_art.loaded())
                        {
                            const float width = std::max(34.0f, scale * 0.78f);
                            const float height = width
                                * static_cast<float>(optional_foot_art.height)
                                / static_cast<float>(optional_foot_art.width);
                            draw_pixel_art(canvas, optional_foot_art,
                                { center + Vec2{ -width * 0.24f, -height * 0.74f },
                                  { width, height } },
                                near || side == 0 ? 1.0f : 0.58f);
                        }
                        else
                        {
                            // Asymmetric +X procedural boot. Unlike the old
                            // capsule it visibly communicates forward direction.
                            const float height = std::max(7.0f, radius * 0.55f);
                            canvas.capsule(center - Vec2{ radius * 0.18f, 0.0f },
                                center + Vec2{ radius * 1.45f, 0.0f },
                                height, color, 14);
                        }
                    }
                    else
                    {
                        canvas.circle(point(index), radius, color, 22);
                    }
                    if (show_nodes || debug_skeleton_overlay)
                    {
                        canvas.circle(point(index), 7.0f,
                            index == static_cast<std::size_t>(selected_node)
                                ? accent : white, 18);
                        add_text(canvas, point(index) + Vec2{ 10.0f, -8.0f },
                            std::to_string(index), 1.05f, white);
                    }
                }
            };

            draw_bones(0);
            draw_nodes(0);
            draw_bones(1);
            draw_nodes(1);

            if (optional_art_enabled && optional_torso_art.loaded()
                && rig.root_node < particles.size()
                && rig.torso_node < particles.size())
            {
                const Vec2 root = point(rig.root_node);
                const Vec2 torso = point(rig.torso_node);
                const Vec2 center = (root + torso) * 0.5f;
                const float height = std::max(42.0f, length(torso - root) * 1.18f);
                const float width = height
                    * static_cast<float>(optional_torso_art.width)
                    / static_cast<float>(optional_torso_art.height);
                draw_pixel_art(canvas, optional_torso_art,
                    { center - Vec2{ width * 0.5f, height * 0.55f },
                      { width, height } }, 0.86f);
            }
            if (optional_art_enabled && optional_helmet_art.loaded()
                && rig.head_node < particles.size())
            {
                const Vec2 center = point(rig.head_node);
                const float height = std::max(38.0f,
                    particles[rig.head_node].radius * scale * 2.55f);
                const float width = height
                    * static_cast<float>(optional_helmet_art.width)
                    / static_cast<float>(optional_helmet_art.height);
                draw_pixel_art(canvas, optional_helmet_art,
                    { center - Vec2{ width * 0.50f, height * 0.54f },
                      { width, height } }, 0.92f);
            }
            draw_bones(2);
            draw_nodes(2);

            if (show_nodes && optional_art_enabled && optional_weapon_art.loaded()
                && rig.active_motor_count >= 8u)
            {
                std::size_t hand = rig.motors[7].c;
                if (hand < particles.size())
                {
                    const Vec2 anchor = point(hand);
                    const float width = 92.0f;
                    const float height = width
                        * static_cast<float>(optional_weapon_art.height)
                        / static_cast<float>(optional_weapon_art.width);
                    draw_pixel_art(canvas, optional_weapon_art,
                        { anchor + Vec2{ -12.0f, -height * 0.58f },
                          { width, height } }, 0.90f);
                }
            }
        }

        void draw_training_pip''',
    "replace creature renderer",
    re.S,
)
toggle_marker = '''                if (button({ cursor, { control_third, 35.0f } },
                    right_leg_near ? "NEAR LEG: RIGHT" : "NEAR LEG: LEFT",
                    input, right_leg_near))
                    right_leg_near = !right_leg_near;
'''
toggle_repl = toggle_marker + '''                cursor.y += 43.0f;
                if (button({ cursor, { control_third, 35.0f } },
                    optional_art_enabled ? "OPTIONAL ART: ON" : "OPTIONAL ART: OFF",
                    input, optional_art_enabled))
                    optional_art_enabled = !optional_art_enabled;
                if (button({ cursor + Vec2{ control_third + 6.0f, 0.0f },
                    { control_third, 35.0f } },
                    debug_skeleton_overlay ? "SKELETON: ON" : "SKELETON: OFF",
                    input, debug_skeleton_overlay))
                    debug_skeleton_overlay = !debug_skeleton_overlay;
                cursor.y -= 43.0f;
'''
text = replace_once(text, toggle_marker, toggle_repl, "rig lab art controls")
write(path, text)

# --- Version and package ---
path = "CMakeLists.txt"
text = read(path)
text = replace_once(text, "project(Runner VERSION 0.7.16 LANGUAGES CXX)",
                    "project(Runner VERSION 0.7.17 LANGUAGES CXX)", "CMake version")
test_block = '''
    add_executable(RunnerV0717EyeTestTests tests/v0717_eye_test_tests.cpp)
    target_link_libraries(RunnerV0717EyeTestTests PRIVATE RunnerCore)
    runner_enable_warnings(RunnerV0717EyeTestTests)
    add_test(NAME runner_v0717_eye_test COMMAND RunnerV0717EyeTestTests)
'''
anchor = '''    add_test(NAME runner_view_camera COMMAND RunnerViewCameraTests)
'''
text = replace_once(text, anchor, anchor + test_block, "CMake v0717 test")
write(path, text)

test_source = r'''#include "autonomy.hpp"
#include "pixel_art.hpp"
#include "ppo.hpp"
#include "simulation.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.17 eye-test contract failed: "
                  << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv)
{
    using namespace runner;
    const sim::CreatureBlueprint biped = sim::CreatureBlueprint::biped();
    require(biped.additional_left_contact_nodes.empty()
            && biped.additional_right_contact_nodes.empty(),
        "biped still has heel/ball/toe contact arrays");
    require(biped.left_contact_node != biped.right_contact_node,
        "stub supports are fused");
    require(biped.is_support_seed(biped.left_contact_node)
            && biped.is_support_seed(biped.right_contact_node),
        "stub supports are not semantic contacts");

    const sim::DuckPressProfile upright =
        sim::duck_press_profile(6.5f, 0.5f, 4.8f, false);
    const sim::DuckPressProfile quadruped =
        sim::duck_press_profile(6.5f, 0.5f, 3.2f, true);
    require(quadruped.bottom_y > 2.65f,
        "horizontal press target still crushes the body plan");
    require((3.2f - quadruped.bottom_y) < (4.8f - upright.bottom_y),
        "quadruped press drop is not shallower than biped drop");

    require(!rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            419u, 8u, 8u),
        "walk can master before minimum fresh updates");
    require(!rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            420u, 7u, 8u),
        "walk can master before minimum fresh episodes");
    require(rl::stage_fresh_work_complete(sim::CourseStage::uneven,
            420u, 8u, 8u),
        "valid fresh walk work is rejected");

    require(sim::sagittal_gait_evidence(
            12u, 10u, 8.0f, 12.0f, 1.05f),
        "sustained sagittal gait does not qualify");
    require(!sim::sagittal_gait_evidence(
            2u, 1u, 1.0f, 2.0f, 1.05f),
        "two steps incorrectly qualify");
    require(sim::crab_walking_motion(
            8u, 0u, 2.0f, 8.0f, 1.80f),
        "wide lateral crab gait is not rejected");
    require(!sim::crab_walking_motion(
            12u, 10u, 8.0f, 12.0f, 1.05f),
        "normal sagittal gait is marked as crab walking");

    sim::Environment quad{ sim::CreatureBlueprint::quadruped(), 0x717200u };
    quad.set_course(sim::CourseStage::duck_press, 0.45f);
    bool terminated = false;
    for (int frame = 0; frame < 900 && !quad.duck_press_completed(); ++frame)
    {
        const auto action = rl::effective_policy_action(
            quad, {}, sim::CourseStage::duck_press);
        const sim::StepResult result = quad.step(action, 1.0f / 60.0f);
        terminated = result.terminated;
        if (terminated)
            break;
    }
    require(!terminated, "quadruped terminates under the press");
    require(quad.duck_press_completed(),
        "quadruped does not hold and recover from the press");
    require(quad.duck_recoveries() >= 1u
            && quad.stable_stance_seconds() >= 1.0f,
        "quadruped recovery is not stably held");

    if (argc > 1)
    {
        art::PixelArt foot{};
        std::string error{};
        require(art::load_p3_pixel_art(
                std::filesystem::path(argv[1]) / "optional"
                    / "runner_armor_concepts" / "runtime"
                    / "foot_side.ppm",
                foot, error),
            "runtime foot atlas does not load");
        require(foot.width == 64 && foot.height == 40,
            "runtime foot atlas dimensions changed");
    }

    std::cout << "Runner v0.7.17 eye-test contracts passed\n";
    return EXIT_SUCCESS;
}
'''
write("tests/v0717_eye_test_tests.cpp", test_source)

doc = r'''# Runner v0.7.17 eye-test correction

The v0.7.16 released package was reopened by direct runtime review. Automated
acceptance did not overrule the visible failures.

## Physical feet

Bipedal presets now use one short physical support stub per leg. A side-view
boot sprite is anchored to the stub but is not collision geometry. This avoids
the old heel/ball/toe triangle bridging and snagging across SandHybrid cells.

## Gait truth

Walk qualification requires sustained distance, fresh stage work, repeated
alternating cycles, and signed leg crossing. A widened lateral stance or
forward motion without sagittal crossing is reported as crab walking and cannot
become a champion, imitation source, evolved-rig seed, or PIP-valid result.

## Quadruped press

Horizontal body plans receive a shallower, slower press target. Four-chain
compression and authored-pose extension are bounded, and recovery must be held
stably after retraction before the stage can complete.

## Optional user art

The four supplied concept sheets are packaged under
`assets/optional/runner_armor_concepts/source/`. Derived P3 runtime sprites live
under `runtime/` for feet, helmet, torso, and a Rig Lab-only fictional weapon
preview. Optional art is visual only and can be disabled independently from the
debug skeleton. Missing or malformed optional assets fall back to procedural
rendering without changing physics or training.
'''
write("docs/RUNNER_V0717_EYE_TEST_CORRECTION.md", doc)

path = "README.md"
text = read(path)
readme_section = r'''
## v0.7.17 eye-test corrections

- One physical support stub per biped leg; visible forward boots are sprites.
- Sustained sagittal side-view walking is required; crab walking is rejected.
- Quadrupeds must survive, hold, retract, and stably recover from the press.
- Stage advancement requires fresh updates, episodes, and evaluations.
- Optional user armor/foot art is packaged and has a procedural fallback.
- Rig Lab exposes optional-art and debug-skeleton toggles.
'''
if "## v0.7.17 eye-test corrections" not in text:
    text += "\n" + readme_section
write(path, text)

path = "CHANGELOG.md"
text = read(path)
entry = r'''## 0.7.17

- Reopened released runtime failures from the v0.7.16 eye test.
- Replaced multi-node heel/ball/toe collision feet with terrain-conforming support stubs and non-physical side-view boot sprites.
- Added sustained sagittal gait evidence and explicit crab-walk rejection.
- Added fresh stage dwell so Stand, Crouch, and Walk cannot be skipped after a short inherited sample.
- Added a shallower rig-aware quadruped press, bounded four-chain crouch guidance, and stable post-retraction recovery.
- Added all four supplied optional concept sheets, provenance/hashes, derived P3 foot/helmet/torso/weapon sprites, safe fallback, layered side-view rendering, and Rig Lab controls.
- Isolated v0.7.17 learned state and added deterministic eye-test regression coverage.

'''
if not text.startswith("## 0.7.17"):
    text = entry + text
write(path, text)

old = ROOT / ".github/workflows/runner-v0716-release.yml"
new = ROOT / ".github/workflows/runner-v0717-release.yml"
workflow = old.read_text(encoding="utf-8")
workflow = workflow.replace("0.7.16", "0.7.17").replace("v0716", "v0717")
workflow = workflow.replace(
    "docs/RUNNER_V0716_CAMERA_BATCH.md'))",
    "docs/RUNNER_V0716_CAMERA_BATCH.md','docs/RUNNER_V0717_EYE_TEST_CORRECTION.md',\n"
    "            'assets/optional/runner_armor_concepts/PROVENANCE.md',\n"
    "            'assets/optional/runner_armor_concepts/runtime/foot_side.ppm'))",
)
workflow = workflow.replace(
    "Runner v0.7.17 completes a cache-first 25-mission adaptive viewport and usability pass.",
    "Runner v0.7.17 corrects released crouch, gait, terrain-foot, and optional-art failures plus 25 compatible missions.",
)
workflow = workflow.replace(
    "- Keeps the corrected physical world scale while replacing the distant fixed 22 px/m live camera.",
    "- Replaces terrain-hostile heel/ball/toe collision feet with support stubs and forward side-view sprite boots.",
)
workflow = workflow.replace(
    "- Adds bounded rig-height auto fitting, viewport-only wheel zoom, direct panel controls, reset-to-auto, useful lookahead, elapsed-time smoothing, and a camera dead zone.",
    "- Requires sustained sagittal gait, rejects crab walking, and prevents short inherited samples from skipping stages.",
)
workflow = workflow.replace(
    "- Enlarges and tightens the training PIP without shrinking the rig for distant hazards.",
    "- Makes the quadruped press shallower and requires a stable four-chain recovery after retraction.",
)
workflow = workflow.replace(
    "- Adds camera telemetry, deterministic camera/layout tests, and packaged `--diagnose-camera` coverage.",
    "- Packages all four supplied concept sheets and safe P3 foot, armor, and Rig Lab weapon sprites with fallback.",
)
workflow = workflow.replace(
    "- Adds `AGENTS.md` and reconciles mission cache, changelog, README, focused documentation, packaging, and release automation.",
    "- Adds deterministic dwell, sagittal/crab, stub-foot, quadruped press/recovery, optional-art, and package gates.",
)
workflow = workflow.replace(
    "Equipment, target, policy-extension, and combined carry/fire curriculum remain explicitly cached for Runner v0.7.17.",
    "Equipment, target, policy-extension, and combined carry/fire curriculum remain explicitly cached for Runner v0.7.18.",
)
new.write_text(workflow, encoding="utf-8", newline="\n")
old.unlink()

print("v0.7.17 applicator completed")
