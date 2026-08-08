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
        require(ivory_count > 0u,
            "remade sprite lost the shared ivory material language");
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

    require(cyan_sprite_count >= expected.size() - 1u
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
            "draw_oriented_pixel_art", "draw_segment_art",
            "optional_upper_arm_art", "optional_forearm_art",
            "optional_thigh_art", "optional_shin_art",
            "rig.paired_leg_chains()", "motor_index >= rig.active_motor_count",
            "!optional_torso_art.loaded()", "!optional_forearm_art.loaded()" })
        require(app.find(reference) != std::string::npos,
            "node-bound remade-art renderer contract is missing");
    const std::string main_source = read_text(root / "src" / "main.cpp");
    const std::string renderer_header = read_text(root / "src" / "renderer.hpp");
    require(main_source.find("--diagnose-art") != std::string::npos
            && main_source.find("headroom_limit") != std::string::npos
            && main_source.find("RUNNER_V0729_MODULAR_ART_REMAKE.md")
                != std::string::npos
            && renderer_header.find("maximum_frame_vertex_bytes")
                != std::string::npos,
        "modular-art vertex-budget diagnostic is missing");

    const std::string simulation = read_text(root / "src" / "simulation.cpp");
    require(simulation.find("runner_armor_concepts") == std::string::npos
            && simulation.find("PixelArt") == std::string::npos,
        "optional art leaked into simulation or training state");

    const std::string generator = read_text(
        root / "tools" / "generate_runner_armor_assets.py");
    for (const ExpectedSprite& expected_sprite : expected)
        require(generator.find(expected_sprite.name) != std::string::npos,
            "deterministic generator omits a runtime sprite");
    require(generator.find("EXPECTED_SOURCE_SIZE = (1536, 1024)")
            != std::string::npos,
        "deterministic generator does not lock the remade atlas dimensions");
    require(std::filesystem::is_regular_file(
            root / "tools" / "art_sources" / "runner_v0729_modular_atlas.png"),
        "remade transparent atlas source is missing");
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
