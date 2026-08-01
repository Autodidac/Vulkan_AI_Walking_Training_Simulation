from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}")
    target.write_text(text.replace(old, new), encoding="utf-8")


replace_exact(
    "CMakeLists.txt",
    "project(EpochRunner VERSION 0.6.5 LANGUAGES CXX)",
    "project(EpochRunner VERSION 0.6.6 LANGUAGES CXX)"
)
replace_exact(
    "src/ppo.hpp",
    '''        float evaluation_stride_events{};
        std::uint32_t evaluation_invalid_runs{};''',
    '''        float evaluation_stride_events{};
        float evaluation_duck_seconds{};
        float evaluation_powered_jumps{};
        float evaluation_jump_landings{};
        float evaluation_spin_turns{};
        float evaluation_spin_landings{};
        float evaluation_obstacles_passed{};
        std::uint32_t evaluation_invalid_runs{};'''
)
replace_exact(
    "src/ppo.hpp",
    '''    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds) noexcept
    {
        if (!valid_motion)
            return false;
        if (stage == sim::CourseStage::balance)
            return survival_seconds >= 3.0f;
        return alternating_steps >= 2u && distance >= 0.60f;
    }''',
    '''    [[nodiscard]] inline bool elite_motion_eligible(sim::CourseStage stage, bool valid_motion,
        std::uint32_t alternating_steps, float distance, float survival_seconds,
        float duck_seconds = 0.0f, std::uint32_t landed_jumps = 0u,
        float maximum_spin_turns = 0.0f, std::uint32_t spin_landings = 0u,
        std::uint32_t obstacles_passed = 0u) noexcept
    {
        if (!valid_motion)
            return false;
        if (stage == sim::CourseStage::balance)
            return survival_seconds >= 3.0f;
        if (!sim::stage_skill_evidence(stage, alternating_steps, duck_seconds,
            landed_jumps, maximum_spin_turns, spin_landings, obstacles_passed))
            return false;
        return sim::stage_requires_forward_gait(stage) ? distance >= 0.60f : true;
    }'''
)
replace_exact(
    "src/ppo_trainer.cpp",
    '''        metrics_.evaluation_stride_events = 0.0f;
        metrics_.evaluation_invalid_runs = 0;''',
    '''        metrics_.evaluation_stride_events = 0.0f;
        metrics_.evaluation_duck_seconds = 0.0f;
        metrics_.evaluation_powered_jumps = 0.0f;
        metrics_.evaluation_jump_landings = 0.0f;
        metrics_.evaluation_spin_turns = 0.0f;
        metrics_.evaluation_spin_landings = 0.0f;
        metrics_.evaluation_obstacles_passed = 0.0f;
        metrics_.evaluation_invalid_runs = 0;'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''            float airborne{};
            float strides{};
            std::size_t speed_samples{};''',
    '''            float airborne{};
            float strides{};
            float duck_seconds{};
            float powered_jumps{};
            float jump_landings{};
            float spin_turns{};
            float spin_landings{};
            float obstacles_passed{};
            std::size_t speed_samples{};'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''                            const bool gait_valid = current_stage == sim::CourseStage::balance
                                || environment.alternating_steps() >= 2;
                            if (!environment.valid_motion() || !gait_valid)
                                ++totals.invalid_runs;''',
    '''                            const bool skill_valid = sim::stage_skill_evidence(current_stage,
                                environment.alternating_steps(), environment.duck_seconds(),
                                environment.landed_jumps(), environment.maximum_spin_turns(),
                                environment.spin_landings(), environment.obstacles_passed());
                            if (!environment.valid_motion() || !skill_valid)
                                ++totals.invalid_runs;'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''                            totals.airborne += environment.airborne_ratio();
                            totals.strides += static_cast<float>(environment.alternating_steps());''',
    '''                            totals.airborne += environment.airborne_ratio();
                            totals.strides += static_cast<float>(environment.alternating_steps());
                            totals.duck_seconds += environment.duck_seconds();
                            totals.powered_jumps += static_cast<float>(environment.powered_jumps());
                            totals.jump_landings += static_cast<float>(environment.landed_jumps());
                            totals.spin_turns += environment.maximum_spin_turns();
                            totals.spin_landings += static_cast<float>(environment.spin_landings());
                            totals.obstacles_passed += static_cast<float>(environment.obstacles_passed());'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''            totals.airborne += local.airborne;
            totals.strides += local.strides;
            totals.speed_samples += local.speed_samples;''',
    '''            totals.airborne += local.airborne;
            totals.strides += local.strides;
            totals.duck_seconds += local.duck_seconds;
            totals.powered_jumps += local.powered_jumps;
            totals.jump_landings += local.jump_landings;
            totals.spin_turns += local.spin_turns;
            totals.spin_landings += local.spin_landings;
            totals.obstacles_passed += local.obstacles_passed;
            totals.speed_samples += local.speed_samples;'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''        metrics_.evaluation_stride_events = totals.strides * inverse_agents;
        metrics_.evaluation_invalid_runs = totals.invalid_runs;''',
    '''        metrics_.evaluation_stride_events = totals.strides * inverse_agents;
        metrics_.evaluation_duck_seconds = totals.duck_seconds * inverse_agents;
        metrics_.evaluation_powered_jumps = totals.powered_jumps * inverse_agents;
        metrics_.evaluation_jump_landings = totals.jump_landings * inverse_agents;
        metrics_.evaluation_spin_turns = totals.spin_turns * inverse_agents;
        metrics_.evaluation_spin_landings = totals.spin_landings * inverse_agents;
        metrics_.evaluation_obstacles_passed = totals.obstacles_passed * inverse_agents;
        metrics_.evaluation_invalid_runs = totals.invalid_runs;'''
)
replace_exact(
    "src/ppo_parallel.cpp",
    '''        if (course_stage_ == sim::CourseStage::balance)
        {
            metrics_.evaluation_score = metrics_.evaluation_valid
                ? metrics_.evaluation_survival * 0.10f + metrics_.evaluation_reward
                    - std::abs(metrics_.evaluation_distance) * 0.20f
                : -1000.0f - static_cast<float>(totals.invalid_runs) * 100.0f;
        }
        else
        {
            metrics_.evaluation_score = metrics_.evaluation_valid
                ? metrics_.evaluation_reward + metrics_.evaluation_distance * 0.75f
                    + metrics_.evaluation_survival * 0.025f
                    + metrics_.evaluation_stride_events * 0.03f
                    - metrics_.evaluation_collisions * 0.18f
                    - metrics_.evaluation_airborne_ratio * 0.75f
                : -1000.0f - static_cast<float>(totals.invalid_runs) * 100.0f;
        }''',
    '''        if (!metrics_.evaluation_valid)
        {
            metrics_.evaluation_score = -1000.0f
                - static_cast<float>(totals.invalid_runs) * 100.0f;
        }
        else
        {
            switch (course_stage_)
            {
            case sim::CourseStage::balance:
                metrics_.evaluation_score = metrics_.evaluation_survival * 0.10f
                    + metrics_.evaluation_reward
                    - std::abs(metrics_.evaluation_distance) * 0.20f;
                break;
            case sim::CourseStage::walk:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_duck_seconds * 0.30f
                    + metrics_.evaluation_survival * 0.03f
                    - std::abs(metrics_.evaluation_distance) * 0.10f;
                break;
            case sim::CourseStage::ramps:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_jump_landings * 0.60f
                    + metrics_.evaluation_powered_jumps * 0.15f
                    - metrics_.evaluation_airborne_ratio * 0.10f;
                break;
            case sim::CourseStage::uneven:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.75f
                    + metrics_.evaluation_stride_events * 0.04f
                    + metrics_.evaluation_speed * 0.12f;
                break;
            case sim::CourseStage::hurdles:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.70f
                    + metrics_.evaluation_obstacles_passed * 0.45f
                    + metrics_.evaluation_jump_landings * 0.25f
                    + metrics_.evaluation_duck_seconds * 0.12f
                    - metrics_.evaluation_collisions * 0.10f;
                break;
            case sim::CourseStage::duck_bars:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_spin_landings * 0.90f
                    + std::min(metrics_.evaluation_spin_turns, 3.0f) * 0.40f
                    + metrics_.evaluation_jump_landings * 0.20f;
                break;
            case sim::CourseStage::moving_hazards:
                metrics_.evaluation_score = metrics_.evaluation_reward
                    + metrics_.evaluation_distance * 0.70f
                    + metrics_.evaluation_obstacles_passed * 0.55f
                    + metrics_.evaluation_stride_events * 0.03f
                    + metrics_.evaluation_jump_landings * 0.20f
                    + metrics_.evaluation_spin_landings * 0.30f
                    + metrics_.evaluation_duck_seconds * 0.08f
                    - metrics_.evaluation_collisions * 0.10f;
                break;
            }
        }'''
)
replace_exact(
    "src/self_imitation.cpp",
    '''            if (!elite_motion_eligible(course_stage_, environment.valid_motion(),
                environment.alternating_steps(), environment.distance_travelled(),
                environment.elapsed_seconds()))''',
    '''            if (!elite_motion_eligible(course_stage_, environment.valid_motion(),
                environment.alternating_steps(), environment.distance_travelled(),
                environment.elapsed_seconds(), environment.duck_seconds(),
                environment.landed_jumps(), environment.maximum_spin_turns(),
                environment.spin_landings(), environment.obstacles_passed()))'''
)
replace_exact(
    "src/self_imitation.cpp",
    '''                - environment.collision_count() * 0.18f
                - environment.airborne_ratio() * 0.75f;''',
    '''                + environment.duck_seconds() * 0.08f
                + static_cast<float>(environment.landed_jumps()) * 0.20f
                + std::min(environment.maximum_spin_turns(), 3.0f) * 0.25f
                + static_cast<float>(environment.obstacles_passed()) * 0.35f
                - environment.collision_count() * 0.10f
                - environment.airborne_ratio() * 0.20f;'''
)
replace_exact(
    "src/autonomy_curriculum.cpp",
    '''        switch (stage_)
        {
        case sim::CourseStage::balance:
            return metrics.evaluation_survival >= 10.0f && metrics.evaluation_score >= 0.55f;
        case sim::CourseStage::walk:
            return metrics.evaluation_distance >= 4.0f && metrics.evaluation_stride_events >= 4.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_distance >= 5.0f && metrics.evaluation_survival >= 8.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 6.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 7.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_distance >= 8.0f && metrics.evaluation_collisions <= 1.0f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 9.0f && metrics.evaluation_collisions <= 2.0f;
        }''',
    '''        switch (stage_)
        {
        case sim::CourseStage::balance:
            return metrics.evaluation_survival >= 10.0f && metrics.evaluation_score >= 0.55f;
        case sim::CourseStage::walk:
            return metrics.evaluation_duck_seconds >= 2.0f
                && metrics.evaluation_survival >= 8.0f;
        case sim::CourseStage::ramps:
            return metrics.evaluation_jump_landings >= 2.0f
                && metrics.evaluation_powered_jumps >= 2.0f;
        case sim::CourseStage::uneven:
            return metrics.evaluation_distance >= 5.0f
                && metrics.evaluation_stride_events >= 4.0f
                && metrics.evaluation_speed >= 0.65f;
        case sim::CourseStage::hurdles:
            return metrics.evaluation_distance >= 7.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && (metrics.evaluation_jump_landings >= 1.0f
                    || metrics.evaluation_duck_seconds >= 0.75f);
        case sim::CourseStage::duck_bars:
            return metrics.evaluation_spin_landings >= 1.0f
                && metrics.evaluation_spin_turns >= 0.85f
                && metrics.evaluation_spin_turns <= 3.05f;
        case sim::CourseStage::moving_hazards:
            return metrics.evaluation_distance >= 9.0f
                && metrics.evaluation_obstacles_passed >= 2.0f
                && metrics.evaluation_collisions <= 4.0f;
        }'''
)
replace_exact(
    "src/autonomy_curriculum.cpp",
    '''                const bool gait_valid = stage == sim::CourseStage::balance || environment.alternating_steps() >= 3;
                if (!environment.valid_motion() || !gait_valid)''',
    '''                const bool skill_valid = sim::stage_skill_evidence(stage,
                    environment.alternating_steps(), environment.duck_seconds(),
                    environment.landed_jumps(), environment.maximum_spin_turns(),
                    environment.spin_landings(), environment.obstacles_passed());
                if (!environment.valid_motion() || !skill_valid)'''
)
replace_exact(
    "src/autonomy_curriculum.cpp",
    '''                    + static_cast<float>(environment.alternating_steps()) * 0.03f
                    - environment.collision_count() * 0.30f
                    - environment.airborne_ratio() * 1.00f
                    - environment.body_rolling_seconds() * 2.00f;''',
    '''                    + static_cast<float>(environment.alternating_steps()) * 0.03f
                    + environment.duck_seconds() * 0.06f
                    + static_cast<float>(environment.landed_jumps()) * 0.16f
                    + std::min(environment.maximum_spin_turns(), 3.0f) * 0.18f
                    + static_cast<float>(environment.obstacles_passed()) * 0.28f
                    - environment.collision_count() * 0.10f
                    - environment.airborne_ratio() * 0.20f
                    - environment.body_rolling_seconds() * 2.00f;'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(rl::elite_motion_eligible(sim::CourseStage::walk, true, 3, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");''',
    '''    require(rl::elite_motion_eligible(sim::CourseStage::uneven, true, 3, 1.2f, 4.0f),
        "valid stepped best result cannot seed self-imitation");
    require(rl::elite_motion_eligible(sim::CourseStage::walk, true, 0, 0.0f, 4.0f, 0.8f),
        "valid duck result cannot seed self-imitation");'''
)
replace_exact(
    "tests/core_tests.cpp",
    '''    require(!rl::elite_motion_eligible(sim::CourseStage::walk, false, 8, 12.0f, 20.0f),
        "invalid rolling result can seed self-imitation");''',
    '''    require(!rl::elite_motion_eligible(sim::CourseStage::uneven, false, 8, 12.0f, 20.0f),
        "invalid rolling result can seed self-imitation");'''
)
replace_exact(
    "src/app.cpp",
    '''        std::filesystem::path autosave_policy_path{ "epochrunner-v065-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "epochrunner-v065-evolved.epochrig" };
        std::filesystem::path autosave_state_path{ "epochrunner-v065-autonomy.state" };''',
    '''        std::filesystem::path autosave_policy_path{ "epochrunner-v066-skill-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "epochrunner-v066-skill-evolved.epochrig" };
        std::filesystem::path autosave_state_path{ "epochrunner-v066-skill-autonomy.state" };'''
)
replace_exact(
    "src/app.cpp",
    '''            add_text_fit(canvas, cursor, std::format("ROLLBACKS {}   NO FLY / FLIP / >50 KM/H",
                autonomy.rollback_count), 1.00f, white, usable_width);''',
    '''            add_text_fit(canvas, cursor, std::format("ROLLBACKS {}   POWERED AIR / <=3 SPINS / <50 KM/H",
                autonomy.rollback_count), 1.00f, white, usable_width);'''
)
replace_exact(
    "src/app.cpp",
    '''                "NO ROLLING / NO BODY-SURFING / HAZARDS NEVER PAY REWARD",''',
    '''                "NO GROUND ROLLING / HAZARD TOUCH ALLOWED / PASS THE GOAL",'''
)
replace_exact(
    "src/app.cpp",
    '''                std::format("FEET {}/{}  STEPS {}  LIFT {:.2f} M  FOOT-ROLL {:.1f} S  IDLE {:.1f} S",
                    environment.left_supported() ? "A" : "-",
                    environment.right_supported() ? "B" : "-",
                    environment.alternating_steps(), environment.obstacle_lift_clearance(),
                    environment.foot_pivot_rolling_seconds(), environment.zero_progress_seconds()),''',
    '''                std::format("STEPS {}  DUCK {:.1f} S  JUMP {}/{}  SPIN {:.1f}  PASSED {}",
                    environment.alternating_steps(), environment.duck_seconds(),
                    environment.powered_jumps(), environment.landed_jumps(),
                    environment.maximum_spin_turns(), environment.obstacles_passed()),'''
)

missions = ROOT / "MISSIONS.md"
mission_text = missions.read_text(encoding="utf-8")
marker = "## v0.6.5 release closure"
new_mission = '''## WALK-SKILL-008 — Ordered locomotion and acrobatics curriculum

**Status:** ACTIVE

Teach reusable skills in prerequisite order instead of exposing terrain and combat hazards before the controller owns basic body control:

1. stand upright,
2. duck and return to standing,
3. jump from joint power and land upright,
4. walk, then run,
5. duck or jump while walking/running,
6. perform controlled airborne flips and land,
7. combine standing, ducking, jumping, walking/running, and up to three spins to pass a mixed goal course.

Hazard contact is allowed. Touching an obstacle applies physical response and a bounded event penalty, but contact alone never terminates the episode. Passing the obstacle is the goal and earns progress. Ground rolling/body surfing remains invalid. A powered launch may remain airborne for a bounded stage-specific interval; hovering or unpowered sustained flight remains invalid. A fourth spin invalidates the run.

**Acceptance:**

- Curriculum labels and advancement follow the seven prerequisite stages above.
- Stationary duck, jump, and flip lessons do not trigger zero-movement rejection.
- Powered takeoff is recognized only when joint action energy and vertical launch speed exceed thresholds.
- Jump and flip lessons require supported upright landings.
- Flip lessons allow up to three airborne spins, while ground rolling and a fourth spin remain invalid.
- Moving-skill and mixed-goal lessons require forward gait and at least one passed obstacle.
- Ordinary hazard contact never creates a pickup/reward loop and never terminates by itself.
- Evaluation, self-imitation eligibility, telemetry, and deterministic tests use duck, jump, landing, spin, and obstacle-pass evidence.
- The four-action controller remains intact for this pass. Independently controllable humanoid arms require a later controller/output and checkpoint-format expansion rather than stealing leg controls.

'''
if marker not in mission_text:
    raise RuntimeError("MISSIONS.md closure marker not found")
missions.write_text(mission_text.replace(marker, new_mission + marker, 1), encoding="utf-8")

readme = ROOT / "README.md"
readme_text = readme.read_text(encoding="utf-8")
anchor = "# EpochRunner\n"
summary = '''
## Ordered skill curriculum

Training now advances through stand, duck/recover, jump/land, walk/run, moving duck/jump, controlled flips, and a mixed goal course. Hazard contact is legal: collision applies physics and a bounded event penalty, while passing the obstacle earns progress. Joint-powered launches receive bounded airtime, controlled airborne flips may reach three spins, and a fourth spin, ground rolling, hovering, or unpowered sustained flight remains invalid.

'''
if summary.strip() not in readme_text:
    if anchor not in readme_text:
        raise RuntimeError("README heading not found")
    readme_text = readme_text.replace(anchor, anchor + summary, 1)
readme.write_text(readme_text, encoding="utf-8")

print("Integrated ordered skill training and mission ledger.")
