#include "ppo.hpp"
#include "rig_training_diagnostic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.44 rig-training failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    void advance(runner::rl::PpoTrainer& trainer, int frames, float frame_dt)
    {
        for (int frame = 0; frame < frames; ++frame)
            trainer.step_preview(frame_dt);
    }

    void verify_retained_release_gate_contract()
    {
        const runner::sim::CreatureBlueprint dog =
            runner::sim::CreatureBlueprint::crawler4();
        runner::rl::PpoTrainer low_core{ dog, 8u, true, 2u };
        require(low_core.maximum_worker_count() == 2u,
            "explicit low-core worker ceiling was not honored");
        runner::diagnostics::RigTrainingResult result{};
        result.retained_policy = true;
        result.retained_quality = runner::rl::strict_evaluation_quality_bit | 1u;
        result.retained_update =
            runner::rl::foundational_walk_teacher_handoff_update(dog);
        result.retained_probe_distance =
            runner::rl::multi_support_release_distance(dog);
        result.retained_probe_stride_events =
            runner::rl::multi_support_release_stride_events(dog);
        require(runner::diagnostics::retained_policy_release_eligible(result, dog),
            "fresh strict replay was rejected by historical quality metadata");

        const auto baseline = result;
        result.retained_quality = 1u;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "partial training candidate was accepted as a retained champion");
        result.retained_quality = 0u;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "retained policy without training provenance was accepted");
        result = baseline;
        result.retained_probe_invalid_runs = 1u;
        result.retained_probe_rejection_mask = 1u;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "invalid retained replay was accepted");
        result = baseline;
        result.retained_probe_invalid_reason = runner::sim::InvalidMotion::collapsed_posture;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "retained replay with an invalid-motion reason was accepted");
        result = baseline;
        result.teacher_authority = 0.01f;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "teacher-assisted retained replay was accepted");
        result = baseline;
        --result.retained_update;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "pre-handoff retained replay was accepted");
        result = baseline;
        result.retained_probe_distance = std::numeric_limits<float>::quiet_NaN();
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "non-finite retained replay was accepted");
        result = baseline;
        result.rollout_course_motion_enabled = true;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "conveyor-assisted retained replay was accepted");
        result = baseline;
        result.preview_resets = 25u;
        require(!runner::diagnostics::retained_policy_release_eligible(result, dog),
            "unbounded preview resets were accepted");

        const auto strict_result = [](const runner::sim::CreatureBlueprint& rig,
            float distance, float gait_cycles)
        {
            runner::diagnostics::RigTrainingResult candidate{};
            candidate.retained_policy = true;
            candidate.retained_quality = runner::rl::strict_evaluation_quality_bit | 1u;
            candidate.retained_update =
                runner::rl::foundational_walk_teacher_handoff_update(rig);
            candidate.retained_probe_distance = distance;
            candidate.retained_probe_stride_events = gait_cycles;
            return candidate;
        };
        const auto verify_gate = [&strict_result](
            const runner::sim::CreatureBlueprint& rig, float distance,
            float gait_cycles)
        {
            require(runner::diagnostics::retained_policy_release_eligible(
                    strict_result(rig, distance, gait_cycles), rig),
                "strict production-rig replay was rejected at its physical gate");
            require(!runner::diagnostics::retained_policy_release_eligible(
                    strict_result(rig, distance - 0.01f, gait_cycles), rig)
                    && !runner::diagnostics::retained_policy_release_eligible(
                        strict_result(rig, distance, gait_cycles - 0.01f), rig),
                "production-rig replay bypassed distance or gait-cycle evidence");
        };

        const runner::sim::CreatureBlueprint human =
            runner::sim::CreatureBlueprint::humanoid();
        const runner::sim::CreatureBlueprint chicken =
            runner::sim::CreatureBlueprint::chicken();
        const runner::sim::CreatureBlueprint hexapod =
            runner::sim::CreatureBlueprint::hexapod();
        verify_gate(human, runner::rl::walk_mastery_distance,
            runner::rl::walk_mastery_stride_events);
        verify_gate(chicken, 10.0f, 14.0f);
        verify_gate(dog, runner::rl::multi_support_release_distance(dog),
            runner::rl::multi_support_release_stride_events(dog));
        verify_gate(hexapod, runner::rl::multi_support_release_distance(hexapod),
            runner::rl::multi_support_release_stride_events(hexapod));
    }
    void require_same_preview(const runner::rl::PpoTrainer& expected,
        const runner::rl::PpoTrainer& actual, std::string_view cadence)
    {
        require(expected.preview_reset_count() == actual.preview_reset_count(),
            "preview reset count depends on render cadence");
        require(expected.preview_last_reset_reason()
                == actual.preview_last_reset_reason(),
            "preview reset reason depends on render cadence");
        require(expected.lesson_update() == actual.lesson_update(),
            "render cadence advanced a lesson-local policy clock");
        require(std::abs(expected.preview().elapsed_seconds()
                - actual.preview().elapsed_seconds()) < 1.0e-6f,
            "preview elapsed simulation time depends on render cadence");
        require(std::abs(expected.preview().distance_travelled()
                - actual.preview().distance_travelled()) < 1.0e-5f,
            "preview distance depends on render cadence");
        require(expected.preview().alternating_steps()
                == actual.preview().alternating_steps(),
            "preview gait evidence depends on render cadence");
        const auto expected_particles = expected.preview().particles();
        const auto actual_particles = actual.preview().particles();
        require(expected_particles.size() == actual_particles.size(),
            "preview particle count changed across cadence");
        for (std::size_t index = 0; index < expected_particles.size(); ++index)
        {
            const runner::sim::Particle& left = expected_particles[index];
            const runner::sim::Particle& right = actual_particles[index];
            const bool same = std::abs(left.position.x - right.position.x) < 1.0e-5f
                && std::abs(left.position.y - right.position.y) < 1.0e-5f
                && std::abs(left.previous.x - right.previous.x) < 1.0e-5f
                && std::abs(left.previous.y - right.previous.y) < 1.0e-5f;
            if (!same)
            {
                std::cerr << "cadence=" << cadence << " particle=" << index << '\n';
                require(false, "preview physics state depends on render cadence");
            }
        }
        const auto& expected_terrain = expected.preview().terrain();
        const auto& actual_terrain = actual.preview().terrain();
        require(expected_terrain.cells().size() == actual_terrain.cells().size()
                && expected_terrain.fine_cells().size()
                    == actual_terrain.fine_cells().size()
                && expected_terrain.macro_tiles().size()
                    == actual_terrain.macro_tiles().size(),
            "terrain allocation changed across render cadence");
        for (std::size_t index = 0; index < expected_terrain.cells().size(); ++index)
        {
            const auto& left = expected_terrain.cells()[index];
            const auto& right = actual_terrain.cells()[index];
            require(left.height == right.height && left.rest_height == right.rest_height
                    && left.firmness == right.firmness
                    && left.loose_fraction == right.loose_fraction
                    && left.water_surface == right.water_surface
                    && left.water_depth == right.water_depth
                    && left.surface_material == right.surface_material
                    && left.region == right.region,
                "active terrain column depends on render cadence");
        }
        for (std::size_t index = 0; index < expected_terrain.fine_cells().size(); ++index)
        {
            const auto& left = expected_terrain.fine_cells()[index];
            const auto& right = actual_terrain.fine_cells()[index];
            require(left.material_id == right.material_id && left.flags == right.flags
                    && left.fill == right.fill,
                "active terrain fine cell depends on render cadence");
        }
        for (std::size_t index = 0; index < expected_terrain.macro_tiles().size(); ++index)
        {
            const auto& left = expected_terrain.macro_tiles()[index];
            const auto& right = actual_terrain.macro_tiles()[index];
            require(left.occupied_mask == right.occupied_mask
                    && left.structural_mask == right.structural_mask
                    && left.uniform_material == right.uniform_material
                    && left.macro_ready == right.macro_ready
                    && left.active == right.active,
                "active terrain macro tile depends on render cadence");
        }
    }

    struct TeacherOutcome
    {
        float distance{};
        float survival{};
        float minimum_uprightness{ 1.0f };
        std::uint32_t strides{};
        std::uint32_t crossings{};
        runner::sim::TerrainRegion region{ runner::sim::TerrainRegion::firm };
        float firmness{};
        float looseness{};
        float maximum_scissor_seconds{};
        float root_x{};
        float local_height_range{};
        runner::sim::ShuttlePhase phase{ runner::sim::ShuttlePhase::traverse };
        std::uint32_t turns{};
        runner::sim::InvalidMotion reason{ runner::sim::InvalidMotion::none };
    };

    [[nodiscard]] TeacherOutcome run_teacher(
        const runner::sim::CreatureBlueprint& rig, std::uint64_t seed)
    {
        runner::sim::Environment environment{ rig, seed };
        environment.set_course(runner::sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);

        float minimum_uprightness = 1.0f;
        for (int step = 0; step < 1200; ++step)
        {
            minimum_uprightness = std::min(minimum_uprightness, environment.uprightness());
            const auto action = runner::rl::walking_teacher_action(environment);
            if (environment.step(action).terminated)
                break;
        }
        const float root_x = environment.particles()[rig.root_node].position.x;
        float local_minimum = environment.ground_height_at(root_x);
        float local_maximum = local_minimum;
        for (int sample = -6; sample <= 6; ++sample)
        {
            const float height = environment.ground_height_at(
                root_x + static_cast<float>(sample) * 0.25f);
            local_minimum = std::min(local_minimum, height);
            local_maximum = std::max(local_maximum, height);
        }
        return { environment.distance_travelled(), environment.elapsed_seconds(),
            minimum_uprightness, environment.gait_cycles(), environment.limb_crossings(),
            environment.terrain_region_at(root_x),
            environment.terrain_firmness_at(root_x),
            environment.terrain_looseness_at(root_x),
            environment.maximum_lower_leg_scissor_seconds(), root_x,
            local_maximum - local_minimum,
            environment.shuttle_phase(), environment.completed_shuttle_turns(),
            environment.invalid_reason() };
    }
    void verify_walking_teachers_repeated_seeds()
    {
        constexpr std::array seeds{
            std::uint64_t{ 0x7300u }, std::uint64_t{ 0x7301u },
            std::uint64_t{ 0x7f31u }, std::uint64_t{ 0x9e3779b9u },
            std::uint64_t{ 0xE000u }, std::uint64_t{ 0xE000u + 4099u },
            std::uint64_t{ 0xE000u + 2u * 4099u },
            std::uint64_t{ 0xE000u + 3u * 4099u },
            std::uint64_t{ 0xE000u + 4u * 4099u },
            std::uint64_t{ 0xE000u + 5u * 4099u }
        };
        struct TeacherCase
        {
            std::string_view name;
            runner::sim::CreatureBlueprint rig;
        };
        const std::array cases{
            TeacherCase{ "human", runner::sim::CreatureBlueprint::humanoid() },
            TeacherCase{ "chicken", runner::sim::CreatureBlueprint::chicken() },
            TeacherCase{ "dog", runner::sim::CreatureBlueprint::crawler4() },
            TeacherCase{ "hexapod", runner::sim::CreatureBlueprint::hexapod() }
        };
        for (const TeacherCase& test : cases)
        {
            const TeacherOutcome repeated = run_teacher(test.rig, seeds.front());
            const TeacherOutcome repeated_again = run_teacher(test.rig, seeds.front());
            require(repeated.distance == repeated_again.distance
                    && repeated.survival == repeated_again.survival
                    && repeated.strides == repeated_again.strides
                    && repeated.crossings == repeated_again.crossings
                    && repeated.region == repeated_again.region
                    && repeated.firmness == repeated_again.firmness
                    && repeated.looseness == repeated_again.looseness
                    && repeated.maximum_scissor_seconds
                        == repeated_again.maximum_scissor_seconds
                    && repeated.root_x == repeated_again.root_x
                    && repeated.phase == repeated_again.phase
                    && repeated.turns == repeated_again.turns
                    && repeated.reason == repeated_again.reason,
                "walking teacher is not deterministic for a repeated seed");
            bool all_valid = true;
            bool all_sustained = true;
            bool all_walked = true;
            const bool paired_legs = test.rig.paired_leg_chains();
            const bool human = test.rig.presentation_species()
                == runner::sim::CreatureSpecies::human;
            const float minimum_distance = test.rig.avian_gait() ? 10.0f
                : human ? 16.0f : paired_legs ? 18.0f
                : std::min(10.0f,
                    runner::rl::multi_support_release_distance(test.rig));
            // The 20-second uneven-material probe counts only completed alternating
            // support transfers. Twelve is sustained gait while retaining margin for
            // the intentionally adversarial repeated seeds.
            const std::uint32_t minimum_strides = test.rig.avian_gait() ? 14u
                : paired_legs
                ? (runner::rl::rig_has_manipulator_motors(test.rig) ? 12u : 16u)
                : static_cast<std::uint32_t>(
                    runner::rl::multi_support_release_stride_events(test.rig));
            for (const std::uint64_t seed : seeds)
            {
                const TeacherOutcome outcome = run_teacher(test.rig, seed);
                if (outcome.reason != runner::sim::InvalidMotion::none
                    || outcome.survival < 19.9f
                    || outcome.distance < minimum_distance
                    || outcome.strides < minimum_strides
                    || (paired_legs && outcome.maximum_scissor_seconds
                        > runner::sim::sustained_scissor_limit_seconds))
                {
                    std::cerr << test.name << " seed=" << seed
                        << " distance=" << outcome.distance
                        << " strides=" << outcome.strides
                        << " survival=" << outcome.survival
                        << " root_x=" << outcome.root_x
                        << " upright_min=" << outcome.minimum_uprightness
                        << " region=" << runner::sim::terrain_region_name(outcome.region)
                        << " firmness=" << outcome.firmness
                        << " looseness=" << outcome.looseness
                        << " scissor_seconds=" << outcome.maximum_scissor_seconds
                        << " local_height_range=" << outcome.local_height_range
                        << " reason=" << runner::sim::invalid_motion_name(outcome.reason)
                        << '\n';
                }
                all_valid = all_valid
                    && outcome.reason == runner::sim::InvalidMotion::none;
                all_sustained = all_sustained && outcome.survival >= 19.9f;
                all_walked = all_walked
                    && outcome.distance >= minimum_distance
                    && outcome.strides >= minimum_strides
                    && (!paired_legs || outcome.maximum_scissor_seconds
                        <= runner::sim::sustained_scissor_limit_seconds);
            }
            require(all_valid,
                "walking teacher became invalid on a deterministic terrain seed");
            require(all_sustained,
                "walking teacher did not sustain the full probe");
            require(all_walked,
                "v0.7.44 walking teacher regressed to the two-step plateau");
        }
    }
    void verify_frame_independent_preview()
    {
        const runner::sim::CreatureBlueprint rig =
            runner::sim::CreatureBlueprint::crawler4();
        runner::rl::PpoTrainer at_60_hz{ rig, 1u, false };
        runner::rl::PpoTrainer at_20_hz{ rig, 1u, false };
        runner::rl::PpoTrainer at_240_hz{ rig, 1u, false };
        constexpr std::uint64_t seed = 0x727333u;
        at_60_hz.reset_preview(seed);
        at_20_hz.reset_preview(seed);
        at_240_hz.reset_preview(seed);
        advance(at_60_hz, 60, 1.0f / 60.0f);
        advance(at_20_hz, 20, 1.0f / 20.0f);
        advance(at_240_hz, 240, 1.0f / 240.0f);
        require_same_preview(at_60_hz, at_20_hz, "20 Hz");
        require_same_preview(at_60_hz, at_240_hz, "240 Hz");

        runner::rl::PpoTrainer partial{ rig, 1u, false };
        runner::rl::PpoTrainer unadvanced{ rig, 1u, false };
        partial.reset_preview(seed);
        unadvanced.reset_preview(seed);
        partial.step_preview(1.0f / 120.0f);
        require(partial.preview().elapsed_seconds() == 0.0f,
            "substep frame advanced preview physics");
        require_same_preview(unadvanced, partial, "partial frame");
        partial.reset_preview(seed);
        partial.step_preview(1.0f / 120.0f);
        require(partial.preview().elapsed_seconds() == 0.0f,
            "preview reset retained a partial fixed tick");
        partial.step_preview(1.0f / 120.0f);
        require(std::abs(partial.preview().elapsed_seconds() - 1.0f / 60.0f)
                < 1.0e-6f,
            "two half frames did not produce exactly one fixed tick");
        partial.step_preview(-1.0f);
        partial.step_preview(std::numeric_limits<float>::quiet_NaN());
        require(std::abs(partial.preview().elapsed_seconds() - 1.0f / 60.0f)
                < 1.0e-6f,
            "invalid frame delta changed preview physics");
        runner::rl::PpoTrainer one_tick{ rig, 1u, false };
        one_tick.reset_preview(seed);
        one_tick.step_preview(1.0f / 60.0f);
        require_same_preview(one_tick, partial, "invalid frame delta");
    }
}

int main(int argc, char** argv)
{
    const std::string_view mode = argc > 1 ? argv[1] : "--all";
    const bool run_references = mode == "--all" || mode == "--references";
    const bool run_learner = mode == "--all" || mode == "--learner";
    const bool run_chicken = mode == "--learner-chicken";
    const bool run_hexapod = mode == "--learner-hexapod";
    if (!run_references && !run_learner && !run_chicken && !run_hexapod)
    {
        std::cerr << "Unknown v0.7.44 test mode: " << mode << '\n';
        return EXIT_FAILURE;
    }
    verify_retained_release_gate_contract();
    if (run_references)
    {
        verify_walking_teachers_repeated_seeds();
        verify_frame_independent_preview();
        if (!run_learner)
        {
            std::cout << "Runner v0.7.44 four-rig reference gait and frame-independence checks passed\n";
            return EXIT_SUCCESS;
        }
    }
    std::uint64_t updates = 1200u;
    if (argc > 2)
        updates = std::max<std::uint64_t>(1u, std::strtoull(argv[2], nullptr, 10));
    if (run_chicken)
    {
        const runner::sim::CreatureBlueprint chicken =
            runner::sim::CreatureBlueprint::chicken();
        const runner::diagnostics::RigTrainingResult rig =
            runner::diagnostics::run_rig_training_case(
                "chicken", chicken, updates);
        std::cout << rig.name
            << ": mean=" << rig.mean_episode_distance
            << " teacher_distance=" << rig.teacher_distance
            << " teacher_strides=" << rig.teacher_stride_events
            << " evaluation=" << rig.evaluation_distance
            << " strides=" << rig.evaluation_stride_events
            << " invalid=" << rig.evaluation_invalid_runs
            << " evaluation_reason=" << runner::sim::invalid_motion_name(
                rig.evaluation_invalid_reason)
            << " retained=" << rig.retained_policy
            << " retained_probe_distance=" << rig.retained_probe_distance
            << " retained_probe_strides=" << rig.retained_probe_stride_events
            << " retained_probe_invalid=" << rig.retained_probe_invalid_runs
            << " retained_probe_reason=" << runner::sim::invalid_motion_name(
                rig.retained_probe_invalid_reason)
            << " preview_resets=" << rig.preview_resets
            << " reason=" << runner::sim::invalid_motion_name(
                rig.preview_reset_reason) << '\n';
        const bool passed = updates >= 1200u
            && rig.teacher_invalid_reason == runner::sim::InvalidMotion::none
            && rig.teacher_survival >= 19.9f
            && runner::diagnostics::retained_policy_release_eligible(
                rig, chicken);
        if (!passed)
        {
            std::cerr << "Runner v0.7.44 chicken training diagnostic failed\n";
            return EXIT_FAILURE;
        }
        std::cout << "Runner v0.7.44 chicken training checks passed\n";
        return EXIT_SUCCESS;
    }
    if (run_hexapod)
    {
        const runner::sim::CreatureBlueprint hexapod =
            runner::sim::CreatureBlueprint::hexapod();
        const runner::diagnostics::RigTrainingResult rig =
            runner::diagnostics::run_rig_training_case(
                "hexapod", hexapod, updates);
        std::cout << rig.name
            << ": mean=" << rig.mean_episode_distance
            << " teacher_distance=" << rig.teacher_distance
            << " teacher_strides=" << rig.teacher_stride_events
            << " evaluation=" << rig.evaluation_distance
            << " strides=" << rig.evaluation_stride_events
            << " invalid=" << rig.evaluation_invalid_runs
            << " evaluation_reason=" << runner::sim::invalid_motion_name(
                rig.evaluation_invalid_reason)
            << " retained=" << rig.retained_policy
            << " retained_probe_distance=" << rig.retained_probe_distance
            << " retained_probe_strides=" << rig.retained_probe_stride_events
            << " retained_probe_invalid=" << rig.retained_probe_invalid_runs
            << " retained_probe_reason=" << runner::sim::invalid_motion_name(
                rig.retained_probe_invalid_reason)
            << " preview_resets=" << rig.preview_resets
            << " reason=" << runner::sim::invalid_motion_name(
                rig.preview_reset_reason) << '\n';
        const bool passed = updates >= 1200u
            && rig.teacher_invalid_reason == runner::sim::InvalidMotion::none
            && rig.teacher_survival >= 19.9f
            && runner::diagnostics::retained_policy_release_eligible(
                rig, hexapod);
        if (!passed)
        {
            std::cerr << "Runner v0.7.44 hexapod training diagnostic failed\n";
            return EXIT_FAILURE;
        }
        std::cout << "Runner v0.7.44 hexapod training checks passed\n";
        return EXIT_SUCCESS;
    }

    const runner::diagnostics::RigTrainingReport report =
        runner::diagnostics::run_rig_training_diagnostic(updates);
    for (const runner::diagnostics::RigTrainingResult& rig : report.rigs)
    {
        std::cout << rig.name << ": mean=" << rig.mean_episode_distance
            << " teacher_distance=" << rig.teacher_distance
            << " teacher_strides=" << rig.teacher_stride_events
            << " teacher_survival=" << rig.teacher_survival
            << " teacher_reason=" << runner::sim::invalid_motion_name(
                rig.teacher_invalid_reason)
            << " evaluation=" << rig.evaluation_distance
            << " strides=" << rig.evaluation_stride_events
            << " rejection=" << rig.evaluation_rejection_mask
            << " invalid=" << rig.evaluation_invalid_runs
            << " evaluation_reason=" << runner::sim::invalid_motion_name(
                rig.evaluation_invalid_reason)
            << " raw_distance=" << rig.raw_policy_distance
            << " raw_strides=" << rig.raw_policy_stride_events
            << " raw_rejection=" << rig.raw_policy_rejection_mask
            << " raw_invalid=" << rig.raw_policy_invalid_runs
            << " raw_reason=" << runner::sim::invalid_motion_name(
                rig.raw_policy_invalid_reason)
            << " teacher_authority=" << rig.teacher_authority
            << " retained=" << rig.retained_policy
            << " best_update=" << rig.retained_update
            << " best_distance=" << rig.retained_distance
            << " best_quality=" << rig.retained_quality
            << " retained_probe_distance=" << rig.retained_probe_distance
            << " retained_probe_strides=" << rig.retained_probe_stride_events
            << " retained_probe_rejection=" << rig.retained_probe_rejection_mask
            << " retained_probe_invalid=" << rig.retained_probe_invalid_runs
            << " retained_probe_reason=" << runner::sim::invalid_motion_name(
                rig.retained_probe_invalid_reason)
            << " preview_resets=" << rig.preview_resets
            << " workers=" << rig.rollout_workers
            << " reason=" << runner::sim::invalid_motion_name(
                rig.preview_reset_reason)
            << " shuttle_updates=" << rig.shuttle_lesson_updates
            << " shuttle_authority=" << rig.shuttle_teacher_authority
            << " shuttle_retained=" << rig.shuttle_retained_policy
            << " shuttle_best_update=" << rig.shuttle_retained_update
            << " shuttle_quality=" << rig.shuttle_retained_quality
            << " shuttle_distance=" << rig.shuttle_probe_distance
            << " shuttle_strides=" << rig.shuttle_probe_stride_events
            << " shuttle_turns=" << rig.shuttle_probe_turns
            << " shuttle_rejection=" << rig.shuttle_probe_rejection_mask
            << " shuttle_invalid=" << rig.shuttle_probe_invalid_runs
            << " shuttle_reason=" << runner::sim::invalid_motion_name(
                rig.shuttle_probe_invalid_reason) << '\n';
    }
    if (!report.passed)
    {
        std::cerr << "Runner v0.7.44 four-rig training diagnostic failed\n";
        return EXIT_FAILURE;
    }
    if (run_references)
        std::cout << "Runner v0.7.44 four-rig training and frame-independence checks passed\n";
    else
        std::cout << "Runner v0.7.44 four-rig training checks passed\n";
    return EXIT_SUCCESS;
}
