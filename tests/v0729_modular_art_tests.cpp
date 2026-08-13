#include "pixel_art.hpp"
#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>

#ifndef RUNNER_SOURCE_ROOT
#error RUNNER_SOURCE_ROOT is required
#endif

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::string read_text(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        require(static_cast<bool>(input), "required source text could not be opened");
        return { std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
    }

    bool same_color(runner::Color left, runner::Color right) noexcept
    {
        return left.r == right.r && left.g == right.g
            && left.b == right.b && left.a == right.a;
    }
}

int main()
{
    const std::filesystem::path root{ RUNNER_SOURCE_ROOT };
    const std::filesystem::path runtime = root / "assets" / "optional"
        / "runner_armor_concepts" / "runtime";
    struct ExpectedSprite
    {
        std::string_view name;
        int width;
        int height;
    };
    constexpr std::array expected{
        ExpectedSprite{ "foot_side.ppm", 32, 28 },
        ExpectedSprite{ "forearm_side.ppm", 40, 20 },
        ExpectedSprite{ "hand_side.ppm", 40, 28 },
        ExpectedSprite{ "helmet_side.ppm", 32, 32 },
        ExpectedSprite{ "shin_side.ppm", 38, 20 },
        ExpectedSprite{ "thigh_side.ppm", 36, 20 },
        ExpectedSprite{ "torso_side.ppm", 32, 40 },
        ExpectedSprite{ "upper_arm_side.ppm", 36, 20 },
        ExpectedSprite{ "weapon_side.ppm", 44, 24 }
    };

    std::size_t cyan_sprite_count{};
    std::size_t total_cyan_count{};
    std::size_t total_ivory_count{};
    for (const ExpectedSprite& expected_sprite : expected)
    {
        runner::art::PixelArt sprite{};
        std::string error{};
        require(runner::art::load_p3_pixel_art(
                runtime / expected_sprite.name, sprite, error),
            "remade runtime sprite did not load");
        require(error.empty() && sprite.loaded(),
            "remade runtime sprite decoded incompletely");
        require(sprite.width == expected_sprite.width
                && sprite.height == expected_sprite.height,
            "remade runtime sprite dimensions changed");
        require(sprite.chroma_keyed,
            "remade runtime sprite did not detect the explicit chroma key");
        require(sprite.transparent(sprite.pixels.front())
                && sprite.transparent(sprite.pixels.back()),
            "remade runtime sprite corners are not transparent");

        std::size_t opaque_count{};
        std::size_t dark_count{};
        std::size_t cyan_count{};
        std::size_t ivory_count{};
        int minimum_x = sprite.width;
        int minimum_y = sprite.height;
        int maximum_x = -1;
        int maximum_y = -1;
        for (int y = 0; y < sprite.height; ++y)
        {
            for (int x = 0; x < sprite.width; ++x)
            {
                const runner::Color color = sprite.pixels[static_cast<std::size_t>(
                    y * sprite.width + x)];
                if (sprite.transparent(color))
                    continue;
                ++opaque_count;
                minimum_x = std::min(minimum_x, x);
                minimum_y = std::min(minimum_y, y);
                maximum_x = std::max(maximum_x, x);
                maximum_y = std::max(maximum_y, y);
                if (std::max({ color.r, color.g, color.b }) < 0.12f)
                    ++dark_count;
                if (color.g > 0.48f && color.b > 0.58f
                    && color.b > color.r * 1.35f)
                    ++cyan_count;
                if (color.r > 0.58f && color.g > 0.50f
                    && color.b > 0.38f && color.r > color.b)
                    ++ivory_count;
            }
        }
        const std::size_t pixel_count = sprite.pixels.size();
        require(opaque_count > pixel_count / 12u
                && opaque_count < pixel_count * 9u / 10u,
            "remade sprite has implausible subject coverage");
        require(dark_count > 0u,
            "explicit chroma key did not preserve dark armor detail");
        if (expected_sprite.name == "hand_side.ppm")
        {
            require(cyan_count > 0u,
                "supplied graphite glove lost its authored cyan highlight");
        }
        else
        {
            require(ivory_count > 0u,
                "remade sprite lost the shared ivory material language");
        }
        if (cyan_count > 0u)
            ++cyan_sprite_count;
        total_cyan_count += cyan_count;
        total_ivory_count += ivory_count;
        require(minimum_x > 0 && minimum_y > 0
                && maximum_x < sprite.width - 1 && maximum_y < sprite.height - 1,
            "remade sprite is not safely padded inside its bounded canvas");

        runner::art::PixelArt repeated{};
        require(runner::art::load_p3_pixel_art(
                runtime / expected_sprite.name, repeated, error),
            "repeated sprite load failed");
        require(repeated.width == sprite.width && repeated.height == sprite.height
                && repeated.chroma_keyed == sprite.chroma_keyed
                && repeated.pixels.size() == sprite.pixels.size(),
            "repeated sprite load changed metadata");
        for (std::size_t index = 0; index < sprite.pixels.size(); ++index)
            require(same_color(sprite.pixels[index], repeated.pixels[index]),
                "repeated sprite load changed pixel data");
    }

    require(cyan_sprite_count >= expected.size() / 2u
            && total_cyan_count > 40u && total_ivory_count > 500u,
        "remade set lost its shared cyan/ivory material language");

    {
        const std::filesystem::path keyed_path =
            std::filesystem::temp_directory_path() / "runner-v0729-keyed.ppm";
        std::ofstream output(keyed_path, std::ios::binary | std::ios::trunc);
        output << "P3\n3 3\n255\n"
            << "255 0 255  255 0 255  255 0 255\n"
            << "255 0 255  1 1 1        255 0 255\n"
            << "255 0 255  255 0 255  255 0 255\n";
        output.close();
        runner::art::PixelArt keyed{};
        std::string error{};
        require(runner::art::load_p3_pixel_art(keyed_path, keyed, error)
                && keyed.chroma_keyed,
            "adversarial keyed fixture did not detect its key");
        require(keyed.transparent(keyed.pixels.front()),
            "adversarial keyed fixture did not hide magenta");
        require(!keyed.transparent(keyed.pixels[4]),
            "adversarial keyed fixture erased black subject detail");
        std::filesystem::remove(keyed_path);
    }

    {
        const std::filesystem::path malformed_path =
            std::filesystem::temp_directory_path() / "runner-v0729-malformed.ppm";
        std::ofstream output(malformed_path, std::ios::binary | std::ios::trunc);
        output << "P3\n2 2\n255\n255 0";
        output.close();
        runner::art::PixelArt malformed{};
        std::string error{};
        require(!runner::art::load_p3_pixel_art(malformed_path, malformed, error)
                && !error.empty(),
            "malformed optional art was accepted");
        std::filesystem::remove(malformed_path);
    }

    const std::string app = read_text(root / "src" / "app.cpp");
    for (std::string_view reference : {
            "draw_oriented_pixel_art", "draw_fitted_armor", "draw_body_segments",
            "optional_upper_arm_art", "optional_forearm_art",
            "optional_thigh_art", "optional_shin_art",
            "support_boot_transform", "rig.support_branch_mask(motor)",
            "rig.node_support_mask(index)", "art::oriented_box_transform",
            "art::SkinEnvelopeDimensions", "art::skin_envelope_dimensions",
            "minimum_shoulder", "envelope.chest_radius",
            "assembled_art_scale", "23.0f, 74.0f", "presentation_side",
            "art::facing_presented_position",
            "authored_joint_overlap", "0.36f : 0.55f",
            "Modular armor is the exclusive presentation",
            "Authoritative graph bones stay visible beneath authored",
            "Raw fine-cell bottoms never render." })
        require(app.find(reference) != std::string::npos,
            "topology-bound remade-art renderer contract is missing");
    require(app.find("draw_pixel_art(canvas, optional_foot_art")
                == std::string::npos
            && app.find("if (!optional_art_enabled || !rig.paired_leg_chains())")
                == std::string::npos
            && app.find("draw_segment_art") == std::string::npos
            && app.find("rig.motors[4]") == std::string::npos
            && app.find(".fine_cell(") == std::string::npos
            && app.find("neutral connected wrap") == std::string::npos
            && app.find("pelvis_center") == std::string::npos
            && app.find("rgb(0x202a31") == std::string::npos
            && app.find("rgb(0xaeb9c1") == std::string::npos
            && app.find("if (optional_art_enabled && optional_foot_art.loaded())")
                == std::string::npos,
        "axis-aligned boot, raw terrain-cell visual, generated underwrap, bone-strip art, motor-slot, or paired-biped gate remains");
    require(app.find("ui_layout::DistanceUnits distance_units{ ui_layout::DistanceUnits::imperial };")
                != std::string::npos,
        "fresh application presentation does not default to imperial units");
    require(app.find("preset(1, 0, \"SCAFFOLD\"") == std::string::npos
            && app.find("Scaffold stays internal")
                != std::string::npos,
        "near-duplicate calibration scaffold is still a user-facing preset");
    {
        constexpr runner::Vec2 presentation_root{ 5.0f, 2.0f };
        constexpr runner::Vec2 node{ 7.5f, 4.0f };
        const runner::Vec2 right = runner::art::facing_presented_position(
            node, presentation_root, 1.0f);
        const runner::Vec2 left = runner::art::facing_presented_position(
            node, presentation_root, -1.0f);
        const runner::Vec2 repeated = runner::art::facing_presented_position(
            left, presentation_root, -1.0f);
        require(right.x == node.x && right.y == node.y
                && left.x == 2.5f && left.y == node.y
                && repeated.x == node.x && repeated.y == node.y,
            "whole-rig facing reflection changed root-relative geometry");
        const runner::Vec2 invalid = runner::art::facing_presented_position(
            node, presentation_root, std::numeric_limits<float>::quiet_NaN());
        require(invalid.x == node.x && invalid.y == node.y,
            "invalid facing changed presentation geometry");
    }
    {
        const runner::art::OrientedArtTransform upright =
            runner::art::support_boot_transform({ 0.0f, -10.0f },
                { 0.0f, 0.0f }, 20.0f, 8.0f);
        const runner::Vec2 upright_axis = upright.ending - upright.beginning;
        require(std::abs(upright_axis.x - 20.0f) < 1.0e-5f
                && std::abs(upright_axis.y) < 1.0e-5f
                && std::abs(upright.thickness - 8.0f) < 1.0e-5f,
            "upright boot transform is not horizontal and bounded");

        const runner::art::OrientedArtTransform fallen =
            runner::art::support_boot_transform({ -10.0f, 0.0f },
                { 0.0f, 0.0f }, 20.0f, 8.0f);
        const runner::Vec2 fallen_axis = fallen.ending - fallen.beginning;
        require(std::abs(fallen_axis.x) < 1.0e-5f
                && std::abs(fallen_axis.y + 20.0f) < 1.0e-5f,
            "fallen boot did not rotate with the terminal support segment");

        constexpr runner::Vec2 translation{ 7.0f, -3.0f };
        const runner::art::OrientedArtTransform translated =
            runner::art::support_boot_transform(
                runner::Vec2{ -10.0f, 0.0f } + translation,
                runner::Vec2{ 0.0f, 0.0f } + translation, 20.0f, 8.0f);
        require(std::abs((translated.beginning - fallen.beginning).x
                    - translation.x) < 1.0e-5f
                && std::abs((translated.beginning - fallen.beginning).y
                    - translation.y) < 1.0e-5f
                && std::abs((translated.ending - fallen.ending).x
                    - translation.x) < 1.0e-5f
                && std::abs((translated.ending - fallen.ending).y
                    - translation.y) < 1.0e-5f,
            "boot translation changed its rotation or pivot");

        const runner::art::OrientedArtTransform reversed =
            runner::art::support_boot_transform({ 0.0f, 10.0f },
                { 0.0f, 0.0f }, 20.0f, 8.0f);
        require(reversed.ending.x < reversed.beginning.x,
            "reversed terminal segment did not mirror boot direction");
        const runner::art::OrientedArtTransform degenerate =
            runner::art::support_boot_transform({ 2.0f, 3.0f },
                { 2.0f, 3.0f }, 20.0f, 8.0f);
        require(std::isfinite(degenerate.beginning.x)
                && std::isfinite(degenerate.beginning.y)
                && std::isfinite(degenerate.ending.x)
                && std::isfinite(degenerate.ending.y),
            "degenerate support segment produced a non-finite transform");
    }

    {
        const std::array<runner::sim::CreatureBlueprint, 8> rigs{
            runner::sim::CreatureBlueprint::humanoid(),
            runner::sim::CreatureBlueprint::biped(),
            runner::sim::CreatureBlueprint::scaffold(),
            runner::sim::CreatureBlueprint::chicken(),
            runner::sim::CreatureBlueprint::quadruped(),
            runner::sim::CreatureBlueprint::crawler4(),
            runner::sim::CreatureBlueprint::hexapod(),
            runner::sim::CreatureBlueprint::monoped() };
        for (const runner::sim::CreatureBlueprint& rig : rigs)
        {
            std::size_t classified_support_motors = 0u;
            for (std::size_t motor_index = 0;
                motor_index < rig.active_motor_count; ++motor_index)
            {
                const runner::sim::MotorConstraint& motor = rig.motors[motor_index];
                const std::uint8_t mask = rig.support_branch_mask(motor);
                require((mask & ~0x3u) == 0u,
                    "authored motor produced an invalid support-role mask");
                if (mask != 0u)
                    ++classified_support_motors;
            }
            require(classified_support_motors > 0u,
                "user-visible rig has no topology-classified support motor");
            for (std::size_t node = 0; node < rig.nodes.size(); ++node)
            {
                if (rig.is_support_seed(node))
                    require(rig.node_support_mask(node) != 0u,
                        "authored support node lost its renderer role");
            }
        }
        const std::array<runner::sim::CreatureBlueprint, 7> exposed{
            runner::sim::CreatureBlueprint::humanoid(),
            runner::sim::CreatureBlueprint::biped(),
            runner::sim::CreatureBlueprint::chicken(),
            runner::sim::CreatureBlueprint::quadruped(),
            runner::sim::CreatureBlueprint::crawler4(),
            runner::sim::CreatureBlueprint::hexapod(),
            runner::sim::CreatureBlueprint::monoped() };
        for (std::size_t left = 0; left < exposed.size(); ++left)
            for (std::size_t right = left + 1u; right < exposed.size(); ++right)
                require(exposed[left].signature() != exposed[right].signature(),
                    "two exposed presets share one rig-scoped training identity");
        const auto aspect = [](const runner::sim::CreatureBlueprint& rig)
        {
            float minimum_x = rig.nodes.front().x;
            float maximum_x = minimum_x;
            float minimum_y = rig.nodes.front().y;
            float maximum_y = minimum_y;
            for (const runner::Vec2 node : rig.nodes)
            {
                minimum_x = std::min(minimum_x, node.x);
                maximum_x = std::max(maximum_x, node.x);
                minimum_y = std::min(minimum_y, node.y);
                maximum_y = std::max(maximum_y, node.y);
            }
            return (maximum_x - minimum_x) / (maximum_y - minimum_y);
        };
        require(aspect(exposed[4]) > aspect(exposed[3]) + 0.20f,
            "low crawler silhouette is not distinct from the quadruped");
    }

    const std::string main_source = read_text(root / "src" / "main.cpp");
    const std::string renderer_header = read_text(root / "src" / "renderer.hpp");
    require(main_source.find("--diagnose-art") != std::string::npos
            && main_source.find("--art-eye-test") != std::string::npos
            && main_source.find("prepare_art_diagnostic_rig") != std::string::npos
            && main_source.find("prepare_art_fallen_eye_test") != std::string::npos
            && main_source.find("rig_vertices") != std::string::npos
            && main_source.find("fallen_vertices") != std::string::npos
            && main_source.find("headroom_limit") != std::string::npos
            && main_source.find("RUNNER_V0729_MODULAR_ART_REMAKE.md")
                != std::string::npos
            && renderer_header.find("maximum_frame_vertex_bytes")
                != std::string::npos,
        "modular-art vertex-budget diagnostic is missing");
    require(app.find("case 6u: rig = sim::CreatureBlueprint::hexapod()")
                != std::string::npos
            && app.find("set_diagnostic_rigid_rotation") != std::string::npos
            && app.find("HORIZONTAL ROTATION EVIDENCE") != std::string::npos,
        "all-rig or horizontal-pose Vulkan art diagnostic is missing");
    require(app.find("ORTHOGRAPHIC ART CHECK") != std::string::npos
            && app.find("STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING")
                != std::string::npos
            && app.find("FIXED SIDE PROFILE") != std::string::npos,
        "strict side-profile production eye-test contract is missing");

    const std::string simulation = read_text(root / "src" / "simulation.cpp");
    require(simulation.find("runner_armor_concepts") == std::string::npos
            && simulation.find("PixelArt") == std::string::npos,
        "optional art leaked into simulation or training state");

    const std::string policy = read_text(root / "src" / "ppo.hpp");
    const std::string parallel = read_text(root / "src" / "ppo_parallel.cpp");
    require(policy.find("active_motor_count >= 8u") == std::string::npos
            && policy.find("action[4]") == std::string::npos
            && policy.find("action[5]") == std::string::npos
            && policy.find("action[6]") == std::string::npos
            && policy.find("action[7]") == std::string::npos
            && policy.find("motor_drives_support_branch(rig, rig.motors[index])")
                != std::string::npos
            && policy.find("manipulator_chain_count == 2u")
                != std::string::npos,
        "policy assistance still assigns manipulator roles by motor slot");
    require(parallel.find("const auto action = raw_action;")
                != std::string::npos,
        "mastery evaluation still applies lesson assistance to the policy");

    const std::string generator = read_text(
        root / "tools" / "generate_runner_armor_assets.py");
    for (const ExpectedSprite& expected_sprite : expected)
        require(generator.find(expected_sprite.name) != std::string::npos,
            "deterministic generator omits a runtime sprite");
    for (std::string_view source_box : {
            "(0, 0, 350, 560)", "(350, 0, 700, 560)",
            "(700, 0, 1050, 560)", "(1050, 0, 1403, 560)",
            "(0, 560, 350, 1121)", "(350, 560, 700, 1121)",
            "(700, 560, 980, 1121)", "(980, 560, 1403, 1121)" })
        require(generator.find(source_box) != std::string::npos,
            "orthographic atlas source boxes changed or overlap subjects");
    require(generator.find("EXPECTED_SOURCE_SIZE = (1403, 1121)")
            != std::string::npos,
        "deterministic generator does not lock the remade atlas dimensions");
    require(generator.find("EXPECTED_USER_SHEET_SIZE = (1024, 1536)")
                != std::string::npos
            && generator.find("USER_HAND_BOX = (205, 1148, 248, 1203)")
                != std::string::npos
            && generator.find("d1db49b2c376a87a060b9bb18402f8374c1f5730e132e51f5385c1dcc28f1195")
                != std::string::npos,
        "supplied side-view hand source is not dimension, crop, and hash locked");
    require(std::filesystem::is_regular_file(
            root / "tools" / "art_sources" / "runner_v0729_modular_atlas.png"),
        "remade transparent atlas source is missing");
    require(std::filesystem::is_regular_file(
            root / "tools" / "art_sources" / "runner_user_modular_sheet.png"),
        "user-supplied modular sheet source is missing");
    require(!std::filesystem::exists(
            root / "tools" / "art_sources" / "runner_v0732_hand_source.png"),
        "rejected generated hand source remains in the repository");
    require(!std::filesystem::exists(
            root / "assets" / "optional" / "runner_armor_concepts" / "PROVENANCE.md")
            && !std::filesystem::exists(
                root / "assets" / "optional" / "runner_armor_concepts"
                    / "runner_armor_concepts.webp")
            && !std::filesystem::exists(
                root / "assets" / "optional" / "runner_armor_concepts" / "source"),
        "obsolete concept-sheet or attribution package remains");

    {
        runner::sim::Environment first{
            runner::sim::CreatureBlueprint::humanoid(), 0x729345u };
        runner::sim::Environment second{
            runner::sim::CreatureBlueprint::humanoid(), 0x729345u };
        first.set_course(runner::sim::CourseStage::uneven, 0.35f);
        second.set_course(runner::sim::CourseStage::uneven, 0.35f);
        std::array<float, runner::sim::action_count> actions{};
        for (int step = 0; step < 180; ++step)
        {
            actions[static_cast<std::size_t>(step) % actions.size()] =
                static_cast<float>((step % 9) - 4) * 0.08f;
            const runner::sim::StepResult left = first.step(actions);
            const runner::sim::StepResult right = second.step(actions);
            require(left.reward == right.reward
                    && left.terminated == right.terminated
                    && first.particles().size() == second.particles().size(),
                "presentation-only art pass changed deterministic simulation");
            for (std::size_t index = 0; index < first.particles().size(); ++index)
            {
                require(first.particles()[index].position.x
                            == second.particles()[index].position.x
                        && first.particles()[index].position.y
                            == second.particles()[index].position.y,
                    "presentation-only art pass changed a particle trajectory");
            }
            if (left.terminated)
            {
                const std::uint64_t seed = 0x729345u
                    + static_cast<std::uint64_t>(step + 1);
                first.reset(seed);
                second.reset(seed);
                first.set_course(runner::sim::CourseStage::uneven, 0.35f);
                second.set_course(runner::sim::CourseStage::uneven, 0.35f);
            }
        }
    }

    std::cout << "Runner v0.7.29 modular art tests passed\n";
    return EXIT_SUCCESS;
}
