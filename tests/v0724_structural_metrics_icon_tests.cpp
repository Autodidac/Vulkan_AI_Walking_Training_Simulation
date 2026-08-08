#include "autonomy.hpp"
#include "training_explainer.hpp"

#include <algorithm>
#include <array>
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

    void verify_rigid_blueprint(CreatureBlueprint blueprint)
    {
        blueprint.rebuild_rest_lengths();
        require(blueprint.valid(), "canonical blueprint must remain valid");
        for (const auto& bone : blueprint.bones)
            require(bone.stiffness == 1.0f, "canonical bones must be rigid");
    }

    void soak(Environment& environment, runner::sim::CourseStage stage, int frames)
    {
        environment.set_course(stage, 0.30f);
        float maximum_error = 0.0f;
        for (int frame = 0; frame < frames; ++frame)
        {
            const auto action = stage == runner::sim::CourseStage::duck_press
                ? runner::rl::duck_teacher_action(environment)
                : runner::rl::balance_teacher_action(environment);
            const auto result = environment.step(action);
            maximum_error = std::max(maximum_error,
                environment.maximum_bone_length_error_ratio());
            require(std::isfinite(maximum_error), "bone error must remain finite");
            require(maximum_error <= 0.035f,
                "load-bearing bone length exceeded rigid tolerance");
            if (result.terminated)
            {
                environment.reset(0x724000u
                    + static_cast<std::uint64_t>(frame) * 17u);
                environment.set_course(stage, 0.30f);
            }
        }
    }
}

int main()
{
    verify_rigid_blueprint(CreatureBlueprint::scaffold());
    verify_rigid_blueprint(CreatureBlueprint::chicken());
    verify_rigid_blueprint(CreatureBlueprint::biped());
    verify_rigid_blueprint(CreatureBlueprint::humanoid());
    verify_rigid_blueprint(CreatureBlueprint::quadruped());
    verify_rigid_blueprint(CreatureBlueprint::crawler4());
    verify_rigid_blueprint(CreatureBlueprint::hexapod());
    verify_rigid_blueprint(CreatureBlueprint::monoped());

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
        Environment balance{ CreatureBlueprint::humanoid(), 0x7241u };
        soak(balance, runner::sim::CourseStage::balance, 360);

        Environment duck{ CreatureBlueprint::humanoid(), 0x7242u };
        soak(duck, runner::sim::CourseStage::duck_press, 540);
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
        require(checksum_text.starts_with(
                "73c533024cdba3abc7b30fbf948a6144c4eac889c448d4beda7ad59da6b02b9e"),
            "generated screenshot PNG checksum must match the exact pixels");
    }

    std::cout
        << "Runner v0.7.24 structural, telemetry, and screenshot-icon tests passed\n";
    return EXIT_SUCCESS;
}
