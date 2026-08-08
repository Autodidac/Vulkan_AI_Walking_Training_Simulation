#include "ppo.hpp"
#include "simulation.hpp"
#include "ui_font.hpp"
#include "ui_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

#ifndef RUNNER_SOURCE_ROOT
#error RUNNER_SOURCE_ROOT is required
#endif

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void force_compressed_double_support(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::uneven, 0.30f);
            const CreatureBlueprint& rig = environment.blueprint_;
            if (!rig.paired_leg_chains() || rig.active_motor_count < 4u)
                return;

            const std::uint16_t hip = rig.motors[0].pivot;
            const std::uint16_t left_knee = rig.motors[0].c;
            const std::uint16_t left_foot = rig.motors[1].c;
            const std::uint16_t right_knee = rig.motors[2].c;
            const std::uint16_t right_foot = rig.motors[3].c;
            if (!environment.valid_node(hip)
                || !environment.valid_node(left_knee)
                || !environment.valid_node(left_foot)
                || !environment.valid_node(right_knee)
                || !environment.valid_node(right_foot))
                return;

            for (Particle& particle : environment.particles_)
                particle.grounded = false;
            environment.particles_[left_foot].grounded = true;
            environment.particles_[right_foot].grounded = true;

            const Vec2 left_support = environment.particles_[left_foot].position;
            const Vec2 right_support = environment.particles_[right_foot].position;
            const Vec2 old_hip = environment.particles_[hip].position;
            const Vec2 collapsed_hip{
                0.5f * (left_support.x + right_support.x),
                0.5f * (left_support.y + right_support.y) + 0.52f
            };
            const Vec2 upper_body_delta = collapsed_hip - old_hip;

            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (index == left_knee || index == right_knee
                    || rig.is_support_seed(index))
                    continue;
                environment.particles_[index].position += upper_body_delta;
                environment.particles_[index].previous += upper_body_delta;
            }

            environment.particles_[left_knee].position =
                (collapsed_hip + left_support) * 0.5f + Vec2{ -0.03f, 0.02f };
            environment.particles_[right_knee].position =
                (collapsed_hip + right_support) * 0.5f + Vec2{ 0.03f, 0.02f };
            environment.particles_[left_knee].previous =
                environment.particles_[left_knee].position;
            environment.particles_[right_knee].previous =
                environment.particles_[right_knee].position;
            environment.particles_[hip].previous = environment.particles_[hip].position;
            environment.elapsed_seconds_ = 0.10f;
        }

        static void project_walking_chain(Environment& environment) noexcept
        {
            environment.project_structure_rigid(1.0f / 60.0f);
        }
    };
}

namespace
{
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

    std::string read_text(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        require(static_cast<bool>(input), "required source file could not be opened");
        return { std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
    }

    float leg_extension_ratio(const Environment& environment, bool left)
    {
        const CreatureBlueprint& rig = environment.blueprint();
        const std::size_t hip_motor = left ? 0u : 2u;
        const std::size_t knee_motor = left ? 1u : 3u;
        const std::uint16_t hip = rig.motors[hip_motor].pivot;
        const std::uint16_t knee = rig.motors[hip_motor].c;
        const std::uint16_t foot = rig.motors[knee_motor].c;
        const auto& particles = environment.particles();
        const float maximum = runner::length(rig.nodes[knee] - rig.nodes[hip])
            + runner::length(rig.nodes[foot] - rig.nodes[knee]);
        return runner::length(particles[foot].position - particles[hip].position)
            / std::max(maximum, 1.0e-5f);
    }

    float segment_error(const Environment& environment, bool left)
    {
        const CreatureBlueprint& rig = environment.blueprint();
        const std::size_t hip_motor = left ? 0u : 2u;
        const std::size_t knee_motor = left ? 1u : 3u;
        const std::uint16_t hip = rig.motors[hip_motor].pivot;
        const std::uint16_t knee = rig.motors[hip_motor].c;
        const std::uint16_t foot = rig.motors[knee_motor].c;
        const auto& particles = environment.particles();
        const float upper_rest = runner::length(rig.nodes[knee] - rig.nodes[hip]);
        const float lower_rest = runner::length(rig.nodes[foot] - rig.nodes[knee]);
        const float upper = runner::length(
            particles[knee].position - particles[hip].position);
        const float lower = runner::length(
            particles[foot].position - particles[knee].position);
        return std::max(
            std::abs(upper - upper_rest) / std::max(upper_rest, 1.0e-5f),
            std::abs(lower - lower_rest) / std::max(lower_rest, 1.0e-5f));
    }
}

int main()
{
    {
        using namespace runner::ui_font;
        require(epochgui_font_contract_commit
                == "130f33fe31d73564a35a622f3bb5ddcc2b5105d5",
            "Runner font contract is not synchronized to the latest EpochGui commit");
        const BitmapFontMetrics metrics = make_bitmap_font_metrics(FontSize{
            .logical_height = 16.0f,
            .dpi_scale = 1.5f
        });
        require(std::abs(metrics.pixel_height - 24.0f) <= 1.0e-5f,
            "logical font height does not resolve through DPI scaling");
        require(std::abs(metrics.cell_size * 7.0f - metrics.pixel_height)
                <= 1.0e-5f,
            "font geometry and requested glyph height disagree");
        require(default_glyph('%').rows != default_glyph('?').rows,
            "percent still falls back to the question-mark glyph");
        require(measure_text("80%", FontSize{}).x
                > measure_text("80", FontSize{}).x,
            "percent glyph does not participate in shared text measurement");
        require(default_logical_height
                * runner::ui_layout::minimum_readable_text_scale
                >= minimum_readable_logical_height,
            "default Runner minimum text scale violates EpochGui readability");
    }

    {
        Environment environment{ CreatureBlueprint::humanoid(), 0x725u };
        runner::sim::EnvironmentTestAccess::force_compressed_double_support(environment);
        require(leg_extension_ratio(environment, true) < 0.45f
                && leg_extension_ratio(environment, false) < 0.45f,
            "forced regression pose must begin visibly compressed");

        runner::sim::EnvironmentTestAccess::project_walking_chain(environment);
        require(leg_extension_ratio(environment, true) >= 0.82f,
            "left supported leg was not restored to a usable stance extension");
        require(leg_extension_ratio(environment, false) >= 0.82f,
            "right supported leg was not restored to a usable stance extension");
        require(segment_error(environment, true) <= 0.004f,
            "left upper/lower leg lengths changed during stance repair");
        require(segment_error(environment, false) <= 0.004f,
            "right upper/lower leg lengths changed during stance repair");
    }

    {
        Environment environment{ CreatureBlueprint::humanoid(), 0x7251u };
        environment.set_course(runner::sim::CourseStage::uneven, 0.30f);
        float minimum_supported_ratio = 1.0f;
        float maximum_segment_error = 0.0f;
        for (int frame = 0; frame < 1200; ++frame)
        {
            const auto action = runner::rl::walking_teacher_action(environment);
            const runner::sim::StepResult result = environment.step(action);
            if (environment.left_supported())
                minimum_supported_ratio = std::min(minimum_supported_ratio,
                    leg_extension_ratio(environment, true));
            if (environment.right_supported())
                minimum_supported_ratio = std::min(minimum_supported_ratio,
                    leg_extension_ratio(environment, false));
            maximum_segment_error = std::max({ maximum_segment_error,
                segment_error(environment, true), segment_error(environment, false) });
            if (result.terminated)
            {
                environment.reset(0x725100u + static_cast<std::uint64_t>(frame));
                environment.set_course(runner::sim::CourseStage::uneven, 0.30f);
            }
        }
        require(minimum_supported_ratio >= 0.74f,
            "walking soak allowed a supported leg to fold into the pelvis");
        require(maximum_segment_error <= 0.012f,
            "walking soak changed an authored leg-segment length");
    }

    {
        const std::filesystem::path source_root{ RUNNER_SOURCE_ROOT };
        const std::string app = read_text(source_root / "src/app.cpp");
        require(app.find("draw_pixel_art(canvas, optional_torso_art")
                != std::string::npos
                && app.find("User-supplied modular armor, bounded to the physical torso")
                    != std::string::npos
                && app.find("std::clamp(body_span * 0.72f, 42.0f, 76.0f)")
                    != std::string::npos,
            "bounded v0.7.26 torso component is missing or unbounded");
        require(app.find("COMPACT SEGMENTED BODY ARMOR") != std::string::npos,
            "compact node-attached armor implementation is missing");
        require(app.find("optional_helmet_art.loaded()") != std::string::npos,
            "approved helmet art was removed");
        require(app.find("optional_foot_art.loaded()") != std::string::npos,
            "approved foot art was removed");
        require(app.find("shoulder_cap_radius") != std::string::npos,
            "compact shoulder caps are not bounded explicitly");
        require(app.find("ui_font_scale") == std::string::npos,
            "legacy bitmap-cell font multiplier remains in the renderer");
        require(app.find("font::make_bitmap_font_metrics") != std::string::npos,
            "renderer is not using shared logical font metrics");
        require(app.find("TRAINING SAMPLES READY") != std::string::npos,
            "sample-ready state is not explained in plain language");
        require(app.find("format_work_counter(\"RUNS\"") != std::string::npos
                && app.find("format_work_counter(\"TESTS\"") != std::string::npos,
            "overflowing compact progress fractions were not replaced by READY states");
    }

    std::cout << "Runner v0.7.25 art, leg-chain, font, and progress tests passed\n";
    return EXIT_SUCCESS;
}
