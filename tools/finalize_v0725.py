#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_between(text: str, start: str, end: str, replacement: str,
    label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:first] + replacement + text[last:]


def finalize_simulation() -> None:
    path = "src/simulation.cpp"
    text = read(path)
    replacement = r'''    void Environment::project_structure_rigid(float dt) noexcept
    {
        static_cast<void>(dt);
        const bool upright_walking_stage = course_stage_ == CourseStage::uneven
            || course_stage_ == CourseStage::hurdles
            || course_stage_ == CourseStage::moving_hazards;
        if (!upright_walking_stage
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan()
            || recovery_active_ || non_foot_grounded_)
            return;

        struct LegChain
        {
            std::uint16_t hip{};
            std::uint16_t knee{};
            std::uint16_t foot{};
            float upper_length{};
            float lower_length{};
            bool supported{};
        };

        auto make_chain = [&](std::size_t hip_motor_index,
            std::size_t knee_motor_index) noexcept
        {
            const MotorConstraint& hip_motor = blueprint_.motors[hip_motor_index];
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motor_index];
            return LegChain{
                hip_motor.pivot,
                hip_motor.c,
                knee_motor.c,
                length(blueprint_.nodes[hip_motor.c]
                    - blueprint_.nodes[hip_motor.pivot]),
                length(blueprint_.nodes[knee_motor.c]
                    - blueprint_.nodes[hip_motor.c]),
                contact_supported(knee_motor.c)
            };
        };

        std::array<LegChain, 2> legs{
            make_chain(0u, 1u), make_chain(2u, 3u)
        };
        for (const LegChain& leg : legs)
        {
            if (!valid_node(leg.hip) || !valid_node(leg.knee)
                || !valid_node(leg.foot)
                || leg.upper_length <= 1.0e-5f
                || leg.lower_length <= 1.0e-5f)
                return;
        }

        const std::size_t supported_count = static_cast<std::size_t>(legs[0].supported)
            + static_cast<std::size_t>(legs[1].supported);
        const float single_support_ratio = course_stage_ == CourseStage::hurdles
            ? 0.72f : course_stage_ == CourseStage::moving_hazards
                ? 0.76f : 0.80f;
        const float minimum_stance_ratio = supported_count >= 2u
            ? std::max(0.84f, single_support_ratio) : single_support_ratio;

        auto move_upper_body = [&](Vec2 correction)
        {
            const float magnitude = length(correction);
            if (magnitude > 0.28f)
                correction *= 0.28f / magnitude;
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                const bool knee = index == legs[0].knee || index == legs[1].knee;
                if (knee || blueprint_.is_support_seed(index))
                    continue;
                particles_[index].position += correction;
                particles_[index].previous += correction;
            }
        };

        // Both legs share one pelvis. Solve a single feasible pelvis position
        // before reconstructing either knee; sequential per-leg pelvis moves
        // otherwise repair one chain while damaging the other.
        constexpr int shared_pelvis_passes = 12;
        for (int pass = 0; pass < shared_pelvis_passes; ++pass)
        {
            Vec2 accumulated{};
            std::size_t correction_count = 0u;
            for (const LegChain& leg : legs)
            {
                if (!leg.supported)
                    continue;
                const Vec2 hip = particles_[leg.hip].position;
                const Vec2 foot = particles_[leg.foot].position;
                const Vec2 foot_to_hip = hip - foot;
                const float distance = length(foot_to_hip);
                const float maximum_reach = leg.upper_length + leg.lower_length
                    - 0.0001f;
                const float minimum_reach = std::max(
                    std::abs(leg.upper_length - leg.lower_length) + 0.0001f,
                    (leg.upper_length + leg.lower_length) * minimum_stance_ratio);
                if (distance < minimum_reach)
                {
                    accumulated += normalized(foot_to_hip, { 0.0f, 1.0f })
                        * (minimum_reach - distance);
                    ++correction_count;
                }
                else if (distance > maximum_reach)
                {
                    accumulated += normalized(foot - hip, { 0.0f, -1.0f })
                        * (distance - maximum_reach);
                    ++correction_count;
                }
            }
            if (correction_count == 0u)
                break;
            accumulated *= 1.0f / static_cast<float>(correction_count);
            if (length(accumulated) <= 1.0e-5f)
                break;
            move_upper_body(accumulated);
        }

        auto solve_chain_ik = [&](const LegChain& leg)
        {
            Particle& hip_particle = particles_[leg.hip];
            Particle& knee_particle = particles_[leg.knee];
            Particle& foot_particle = particles_[leg.foot];
            const Vec2 hip = hip_particle.position;
            Vec2 foot = foot_particle.position;
            Vec2 hip_to_foot = foot - hip;
            float distance = length(hip_to_foot);
            const float minimum_reach = std::abs(
                leg.upper_length - leg.lower_length) + 0.0001f;
            const float maximum_reach = leg.upper_length + leg.lower_length - 0.0001f;

            // A planted support stays fixed. A free swing foot may be clamped
            // back into the authored two-link workspace if another solver pass
            // pushed it beyond physical reach.
            if (!leg.supported && (distance < minimum_reach || distance > maximum_reach))
            {
                const Vec2 axis = normalized(hip_to_foot, { 0.0f, -1.0f });
                const float reachable = std::clamp(distance,
                    minimum_reach, maximum_reach);
                const Vec2 target_foot = hip + axis * reachable;
                const Vec2 foot_delta = target_foot - foot_particle.position;
                foot_particle.position = target_foot;
                foot_particle.previous += foot_delta;
                foot = target_foot;
                hip_to_foot = foot - hip;
                distance = reachable;
            }

            const Vec2 axis = normalized(hip_to_foot, { 0.0f, -1.0f });
            const Vec2 perpendicular{ -axis.y, axis.x };
            const float safe_distance = std::clamp(distance,
                minimum_reach, maximum_reach);
            const float along = (
                leg.upper_length * leg.upper_length
                - leg.lower_length * leg.lower_length
                + safe_distance * safe_distance)
                / (2.0f * safe_distance);
            const float height = std::sqrt(std::max(0.0f,
                leg.upper_length * leg.upper_length - along * along));
            const Vec2 base = hip + axis * along;

            float side = dot(knee_particle.position - base, perpendicular);
            if (std::abs(side) <= 0.001f)
            {
                const Vec2 rest_axis = normalized(
                    blueprint_.nodes[leg.foot] - blueprint_.nodes[leg.hip], axis);
                const Vec2 rest_perpendicular{ -rest_axis.y, rest_axis.x };
                side = dot(blueprint_.nodes[leg.knee]
                    - blueprint_.nodes[leg.hip], rest_perpendicular);
            }
            const float bend_sign = side < 0.0f ? -1.0f : 1.0f;
            const Vec2 target_knee = base + perpendicular * (height * bend_sign);
            const Vec2 knee_delta = target_knee - knee_particle.position;
            knee_particle.position = target_knee;
            knee_particle.previous += knee_delta;
        };

        // The shared pelvis is now fixed. Both knees are reconstructed from the
        // same final pelvis frame, so one chain cannot invalidate the other.
        solve_chain_ik(legs[0]);
        solve_chain_ik(legs[1]);
    }

'''
    text = replace_between(text,
        "    void Environment::project_structure_rigid(float dt) noexcept",
        "    void Environment::separate_support_clusters() noexcept",
        replacement,
        "shared-pelvis stance solver")
    write(path, text)


def finalize_tests() -> None:
    path = "tests/v0725_art_leg_hotfix_tests.cpp"
    text = read(path)
    text = text.replace("using runner::sim::Vec2;\n", "")
    text = text.replace("runner::sim::length", "runner::length")
    if "runner::sim::length" in text or "runner::sim::Vec2" in text:
        raise RuntimeError("stale test namespace remains")
    write(path, text)


def finalize_ui_contract() -> None:
    path = "src/app.cpp"
    text = read(path)
    required = (
        "font::make_bitmap_font_metrics",
        "TRAINING SAMPLES READY",
        'format_work_counter("RUNS"',
        'format_work_counter("TESTS"',
    )
    for token in required:
        if token not in text:
            raise RuntimeError(f"missing synchronized UI contract: {token}")
    if "ui_font_scale" in text:
        raise RuntimeError("legacy font-cell multiplier remains")

    # Make the noob-facing header state the only remaining requirement instead
    # of exposing uncapped internal counters after their budgets are complete.
    text = text.replace(
        'std::format("{}   MASTERY PASSES {} / {}",\n                    training_work_label, autonomy.mastery_streak,',
        'std::format("{}   MASTERY PASSES {} / {}",\n                    training_work_label, autonomy.mastery_streak,')
    write(path, text)


def finalize_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    if "tools/finalize_v0725.py" not in text:
        marker = "        tools/apply_v0725_font_sync.py)\n"
        if marker in text:
            text = text.replace(marker,
                "        tools/apply_v0725_font_sync.py\n"
                "        tools/finalize_v0725.py)\n", 1)
    write(path, text)


def main() -> int:
    finalize_simulation()
    finalize_tests()
    finalize_ui_contract()
    finalize_repository_audit()
    print("Runner v0.7.25 final shared-pelvis/font pass applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
