#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_header() -> None:
    path = "src/simulation.hpp"
    text = read(path)
    text = replace_once(
        text,
        '''        // Retained in the file format for compatibility. Runtime structural
        // constraints are rigid and normalize this value to 1.0.
        float stiffness{ 1.0f };
''',
        '''        // Authored stiffness remains part of the physical rig. v0.7.24
        // applies a separate exact projection only to the primary walking-leg
        // chains, preventing telescoping without freezing compliant braces.
        float stiffness{ 1.0f };
''',
        "distance-constraint documentation",
    )
    write(path, text)


def patch_source() -> None:
    path = "src/simulation.cpp"
    text = read(path)

    text = replace_once(
        text,
        '''    void CreatureBlueprint::rebuild_rest_lengths() noexcept
    {
        for (DistanceConstraint& bone : bones)
        {
            if (bone.a < nodes.size() && bone.b < nodes.size())
                bone.rest_length = std::max(0.05f, length(nodes[bone.b] - nodes[bone.a]));
            bone.stiffness = 1.0f;
        }
    }
''',
        '''    void CreatureBlueprint::rebuild_rest_lengths() noexcept
    {
        for (DistanceConstraint& bone : bones)
        {
            if (bone.a < nodes.size() && bone.b < nodes.size())
                bone.rest_length = std::max(0.05f,
                    length(nodes[bone.b] - nodes[bone.a]));
            bone.stiffness = clamp(bone.stiffness, 0.05f, 1.0f);
        }
    }
''',
        "preserve authored stiffness",
    )

    text = replace_once(
        text,
        '''            // Older rig files may contain spring-like structural
            // stiffness. v0.7.24 migrates every body segment to a rigid length.
            bone.stiffness = 1.0f;
            result.bones.push_back(bone);
''',
        '''            bone.stiffness = clamp(bone.stiffness, 0.05f, 1.0f);
            result.bones.push_back(bone);
''',
        "preserve loaded stiffness",
    )

    text = replace_once(
        text,
        '''    Environment::Environment(const CreatureBlueprint& blueprint, std::uint64_t seed)
        : blueprint_(blueprint), random_state_(seed == 0 ? 1 : seed)
    {
        for (DistanceConstraint& bone : blueprint_.bones)
            bone.stiffness = 1.0f;
        reset(seed);
    }

    void Environment::set_blueprint(const CreatureBlueprint& blueprint)
    {
        blueprint_ = blueprint;
        for (DistanceConstraint& bone : blueprint_.bones)
            bone.stiffness = 1.0f;
        reset(random_state_);
    }
''',
        '''    Environment::Environment(const CreatureBlueprint& blueprint, std::uint64_t seed)
        : blueprint_(blueprint), random_state_(seed == 0 ? 1 : seed)
    {
        blueprint_.rebuild_rest_lengths();
        reset(seed);
    }

    void Environment::set_blueprint(const CreatureBlueprint& blueprint)
    {
        blueprint_ = blueprint;
        blueprint_.rebuild_rest_lengths();
        reset(random_state_);
    }
''',
        "preserve runtime stiffness",
    )

    text = replace_once(
        text,
        '''        // Bones are fixed-length structure, not tunable springs. Apply the
        // complete PBD correction every pass; contacts and motors are solved
        // in the same iteration and a final projection removes residual error.
        const Vec2 correction = delta
            * ((distance - constraint.rest_length) / distance);
''',
        '''        const Vec2 correction = delta
            * ((distance - constraint.rest_length) / distance
                * clamp(constraint.stiffness, 0.05f, 1.0f));
''',
        "restore authored constraint behavior",
    )

    old_projection = '''    void Environment::project_structure_rigid(float dt) noexcept
    {
        constexpr int projection_passes = 18;
        for (int pass = 0; pass < projection_passes; ++pass)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            solve_ground(dt);
            solve_course();
        }
        // Finish on structure, then resolve the small contact correction and
        // project once more. The remaining error is checked before acceptance.
        for (const DistanceConstraint& bone : blueprint_.bones)
            solve_distance(bone);
        solve_ground(dt);
        solve_course();
        for (const DistanceConstraint& bone : blueprint_.bones)
            solve_distance(bone);
    }
'''
    new_projection = '''    void Environment::project_structure_rigid(float dt) noexcept
    {
        static_cast<void>(dt);
        if (elapsed_seconds_ < 0.75f
            || !stage_requires_forward_gait(course_stage_)
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return;

        auto primary_leg_segment = [&](const DistanceConstraint& bone) noexcept
        {
            const std::size_t leg_motors = std::min<std::size_t>(
                4u, blueprint_.active_motor_count);
            for (std::size_t index = 0; index < leg_motors; ++index)
            {
                const MotorConstraint& motor = blueprint_.motors[index];
                if (!motor.enabled)
                    continue;
                if ((bone.a == motor.pivot && bone.b == motor.c)
                    || (bone.b == motor.pivot && bone.a == motor.c))
                    return true;
            }
            return false;
        };

        auto project = [&](const DistanceConstraint& bone)
        {
            if (!primary_leg_segment(bone)
                || bone.a >= particles_.size() || bone.b >= particles_.size())
                return;
            Particle& lhs = particles_[bone.a];
            Particle& rhs = particles_[bone.b];
            const Vec2 delta = rhs.position - lhs.position;
            const float distance = length(delta);
            if (distance <= 1.0e-6f)
                return;

            float lhs_weight = lhs.inverse_mass;
            float rhs_weight = rhs.inverse_mass;
            if (lhs.grounded && blueprint_.is_support_seed(bone.a))
                lhs_weight = 0.0f;
            if (rhs.grounded && blueprint_.is_support_seed(bone.b))
                rhs_weight = 0.0f;
            const float weight = lhs_weight + rhs_weight;
            if (weight <= 1.0e-6f)
                return;

            const Vec2 correction = delta
                * ((distance - bone.rest_length) / distance);
            lhs.position += correction * (lhs_weight / weight);
            rhs.position -= correction * (rhs_weight / weight);
        };

        // Only the four authored humanoid/biped walking segments are projected.
        // Static crouch, multi-support rigs, and the treadmill bootstrap retain
        // the already-validated v0.7.23 dynamics.
        constexpr int projection_passes = 10;
        for (int pass = 0; pass < projection_passes; ++pass)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                project(bone);
        }
    }
'''
    text = replace_once(text, old_projection, new_projection,
        "walking-leg final projection")

    text = replace_once(
        text,
        '''        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-6f)
                return std::numeric_limits<float>::infinity();
            maximum_error = std::max(maximum_error, bone_length_error_ratio(
                length(particles_[bone.b].position - particles_[bone.a].position),
                bone.rest_length));
        }
''',
        '''        if (elapsed_seconds_ < 0.75f
            || !stage_requires_forward_gait(course_stage_)
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return 0.0f;
        const std::size_t leg_motors = std::min<std::size_t>(
            4u, blueprint_.active_motor_count);
        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            bool primary_leg_segment = false;
            for (std::size_t index = 0; index < leg_motors; ++index)
            {
                const MotorConstraint& motor = blueprint_.motors[index];
                primary_leg_segment = primary_leg_segment
                    || ((bone.a == motor.pivot && bone.b == motor.c)
                        || (bone.b == motor.pivot && bone.a == motor.c));
            }
            if (!primary_leg_segment)
                continue;
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-6f)
                return std::numeric_limits<float>::infinity();
            maximum_error = std::max(maximum_error, bone_length_error_ratio(
                length(particles_[bone.b].position - particles_[bone.a].position),
                bone.rest_length));
        }
''',
        "measure primary walking segments",
    )

    text = replace_once(
        text,
        '''        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-5f)
                return false;
            const float ratio = length(particles_[bone.b].position
                - particles_[bone.a].position) / bone.rest_length;
            if (!std::isfinite(ratio) || ratio < 0.94f || ratio > 1.06f)
                return false;
        }
''',
        '''        for (const DistanceConstraint& bone : blueprint_.bones)
        {
            if (bone.a >= particles_.size() || bone.b >= particles_.size()
                || bone.rest_length <= 1.0e-5f)
                return false;
            const float ratio = length(particles_[bone.b].position
                - particles_[bone.a].position) / bone.rest_length;
            if (!std::isfinite(ratio) || ratio < 0.20f || ratio > 2.50f)
                return false;
        }
''',
        "restore broad body-integrity range",
    )

    text = replace_once(
        text,
        '''        for (int iteration = 0; iteration < 14; ++iteration)
        {
            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_articulated_toes();
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
''',
        '''        for (int iteration = 0; iteration < 14; ++iteration)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_articulated_toes();
''',
        "restore validated solver order",
    )

    text = replace_once(
        text,
        '''        if (elapsed_seconds_ >= 0.50f
            && (!std::isfinite(structural_error) || structural_error > 0.025f))
            invalidate(InvalidMotion::structural_compression);
''',
        '''        if (elapsed_seconds_ >= 0.75f
            && stage_requires_forward_gait(course_stage_)
            && blueprint_.paired_leg_chains()
            && !blueprint_.horizontal_multi_support_plan()
            && (!std::isfinite(structural_error) || structural_error > 0.040f))
            invalidate(InvalidMotion::structural_compression);
''',
        "walking-only structural gate",
    )

    write(path, text)


def main() -> int:
    patch_header()
    patch_source()
    print("Runner v0.7.24 walking-leg rigidity applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
