#include "autonomy.hpp"
#include "training_explainer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#ifndef RUNNER_GENERATED_ASSET_DIRECTORY
#error RUNNER_GENERATED_ASSET_DIRECTORY is required
#endif

#ifndef RUNNER_SOURCE_ICON_PATH
#error RUNNER_SOURCE_ICON_PATH is required
#endif

namespace
{
    using runner::rl::RigMutationCandidate;
    using runner::sim::CreatureBlueprint;
    using runner::sim::Environment;

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::vector<unsigned char> read_binary(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        require(static_cast<bool>(input), "required icon file could not be opened");
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

    std::string read_text(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        require(static_cast<bool>(input), "required icon source could not be opened");
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

    bool same_anatomy(const CreatureBlueprint& lhs, const CreatureBlueprint& rhs)
    {
        if (lhs.nodes.size() != rhs.nodes.size()
            || lhs.radii.size() != rhs.radii.size()
            || lhs.bones.size() != rhs.bones.size()
            || lhs.root_node != rhs.root_node
            || lhs.torso_node != rhs.torso_node
            || lhs.head_node != rhs.head_node
            || lhs.left_contact_node != rhs.left_contact_node
            || lhs.right_contact_node != rhs.right_contact_node
            || lhs.additional_left_contact_nodes != rhs.additional_left_contact_nodes
            || lhs.additional_right_contact_nodes != rhs.additional_right_contact_nodes)
            return false;

        for (std::size_t index = 0; index < lhs.nodes.size(); ++index)
        {
            if (lhs.nodes[index].x != rhs.nodes[index].x
                || lhs.nodes[index].y != rhs.nodes[index].y
                || lhs.radii[index] != rhs.radii[index])
                return false;
        }
        for (std::size_t index = 0; index < lhs.bones.size(); ++index)
        {
            const auto& a = lhs.bones[index];
            const auto& b = rhs.bones[index];
            if (a.a != b.a || a.b != b.b
                || a.rest_length != b.rest_length
                || a.stiffness != b.stiffness)
                return false;
        }
        return true;
    }

    void verify_blueprint(CreatureBlueprint blueprint)
    {
        blueprint.rebuild_rest_lengths();
        require(blueprint.valid(), "canonical blueprint must remain valid");
        require(!blueprint.bones.empty(), "canonical blueprint must contain structure");
        for (const auto& bone : blueprint.bones)
        {
            require(bone.stiffness >= 0.05f && bone.stiffness <= 1.0f,
                "authored stiffness must remain in the physical range");
        }
    }

    void verify_primary_walking_segments(const CreatureBlueprint& blueprint)
    {
        require(blueprint.paired_leg_chains(),
            "walking rigidity fixture must contain paired leg chains");
        const std::size_t leg_motors = std::min<std::size_t>(
            4u, blueprint.active_motor_count);
        require(leg_motors == 4u,
            "walking rigidity fixture must expose four primary leg motors");
        for (std::size_t index = 0; index < leg_motors; ++index)
        {
            const auto& motor = blueprint.motors[index];
            const bool found = std::ranges::any_of(blueprint.bones,
                [&](const runner::sim::DistanceConstraint& bone)
                {
                    return (bone.a == motor.pivot && bone.b == motor.c)
                        || (bone.b == motor.pivot && bone.a == motor.c);
                });
            require(found,
                "every primary walking motor must terminate on an authored bone");
        }
    }

    void walking_soak(Environment& environment, int frames)
    {
        environment.set_course(runner::sim::CourseStage::uneven, 0.30f);
        for (int frame = 0; frame < frames; ++frame)
        {
            const auto action = runner::rl::walking_teacher_action(environment);
            const auto result = environment.step(action);
            const float error = environment.maximum_bone_length_error_ratio();
            require(std::isfinite(error), "walking-leg error must remain finite");
            require(error <= 0.0405f,
                "primary walking-leg length exceeded rigid tolerance");
            if (result.terminated)
            {
                environment.reset(0x724000u
                    + static_cast<std::uint64_t>(frame) * 17u);
                environment.set_course(runner::sim::CourseStage::uneven, 0.30f);
            }
        }
    }
}

int main()
{
    verify_blueprint(CreatureBlueprint::scaffold());
    verify_blueprint(CreatureBlueprint::chicken());
    verify_blueprint(CreatureBlueprint::biped());
    verify_blueprint(CreatureBlueprint::humanoid());
    verify_blueprint(CreatureBlueprint::quadruped());
    verify_blueprint(CreatureBlueprint::crawler4());
    verify_blueprint(CreatureBlueprint::hexapod());
    verify_blueprint(CreatureBlueprint::monoped());
    verify_primary_walking_segments(CreatureBlueprint::biped());
    verify_primary_walking_segments(CreatureBlueprint::humanoid());

    {
        const CreatureBlueprint source = CreatureBlueprint::humanoid();
        for (std::uint64_t generation = 0; generation < 24u; ++generation)
        {
            const RigMutationCandidate candidate =
                runner::rl::automatic_rig_tuning_candidate(source, generation);
            require(!candidate.topology_changed,
                "automatic controller tuning must not change topology");
            require(same_anatomy(source, candidate.blueprint),
                "automatic controller tuning must preserve anatomy and stiffness");
        }
    }

    {
        runner::rl::AutonomyStatus status{};
        status.stage = runner::sim::CourseStage::uneven;
        status.stage_required_updates = 420u;
        status.stage_fresh_updates = 420u;
        status.stage_required_episodes = 8u;
        status.stage_fresh_episodes = 8u;
        status.stage_required_evaluations = 8u;
        status.stage_fresh_evaluations = 8u;
        status.mastery_streak = 0;

        auto progress = runner::telemetry::lesson_progress(status);
        require(progress.training_work == 1.0f,
            "complete sample budget must report complete training work");
        require(progress.overall >= 0.79f && progress.overall <= 0.81f,
            "zero mastery must reserve the mastery portion of completion");
        require(progress.overall < 1.0f,
            "zero mastery may not report 100 percent lesson completion");

        status.mastery_streak =
            runner::rl::required_mastery_confirmations(status.stage);
        progress = runner::telemetry::lesson_progress(status);
        require(progress.mastery == 1.0f && progress.overall == 1.0f,
            "full work and mastery must report complete lesson");
    }

    {
        Environment fresh{ CreatureBlueprint::humanoid(), 0x724u };
        fresh.set_course(runner::sim::CourseStage::uneven, 0.30f);
        require(!runner::rl::completed_episode_passes_stage_checks(
                runner::sim::CourseStage::uneven, fresh),
            "an intact fresh rig without gait evidence is not a passed stage check");
    }

    {
        Environment biped{ CreatureBlueprint::biped(), 0x7241u };
        walking_soak(biped, 360);
        Environment humanoid{ CreatureBlueprint::humanoid(), 0x7242u };
        walking_soak(humanoid, 360);
    }

    {
        const std::filesystem::path generated{ RUNNER_GENERATED_ASSET_DIRECTORY };
        const std::filesystem::path source{ RUNNER_SOURCE_ICON_PATH };
        const std::string source_text = read_text(source);
        require(source_text.find("WIDTH=320") != std::string::npos
                && source_text.find("HEIGHT=320") != std::string::npos,
            "screenshot pixel source dimensions must remain canonical");
        require(source_text.find(
                "RGBA_SHA256=6b623661307a430c6ec8cf5689531324dc30249137a7005155fa047592dcb1ad")
                != std::string::npos,
            "screenshot pixel source hash must remain canonical");

        const auto source_png = read_binary(generated / "runner_icon_source.png");
        const auto png = read_binary(generated / "runner_icon.png");
        const auto png512 = read_binary(generated / "runner_icon_512.png");
        const auto bmp = read_binary(generated / "runner_icon.bmp");
        const auto ico = read_binary(generated / "runner.ico");
        require(source_png.size() > 8u
                && source_png[0] == 0x89u && source_png[1] == 0x50u,
            "generated screenshot source must be a PNG");
        require(png.size() > 8u && png[0] == 0x89u && png[1] == 0x50u,
            "256 icon must be a PNG");
        require(png512.size() > 8u && png512[0] == 0x89u && png512[1] == 0x50u,
            "512 icon must be a PNG");
        require(bmp.size() > 54u && bmp[0] == 'B' && bmp[1] == 'M',
            "runtime icon must be a BMP");
        require(ico.size() > 6u
                && ico[0] == 0u && ico[1] == 0u
                && ico[2] == 1u && ico[3] == 0u
                && ico[4] == 9u && ico[5] == 0u,
            "Windows icon must contain nine image entries");

        std::ifstream checksum(generated / "runner_icon_source.sha256");
        require(static_cast<bool>(checksum),
            "screenshot source checksum must be generated");
        std::string checksum_text;
        std::getline(checksum, checksum_text);
        require(checksum_text.size() > 66u,
            "generated screenshot checksum must include a SHA-256 and file name");
        require(std::ranges::all_of(checksum_text.substr(0, 64),
                [](unsigned char value) { return std::isxdigit(value) != 0; }),
            "generated screenshot checksum must begin with hexadecimal SHA-256");
        require(checksum_text.ends_with("  runner_icon_source.png"),
            "generated screenshot checksum must name the source PNG");
    }

    std::cout
        << "Runner v0.7.24 structural, telemetry, and screenshot-icon tests passed\n";
    return EXIT_SUCCESS;
}
