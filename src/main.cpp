#include "acceptance.hpp"
#include "course_completion_diagnostic.hpp"
#include "hybrid_brain_diagnostic.hpp"
#include "app.hpp"
#include "pixel_art.hpp"
#include "renderer.hpp"
#include "rig_training_diagnostic.hpp"
#include "runtime_diagnostics.hpp"
#include "simulation.hpp"
#include "ui_layout.hpp"
#include "ui_frame_probe.hpp"
#include "view_camera.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#ifndef RUNNER_SHADER_DIRECTORY
#define RUNNER_SHADER_DIRECTORY "shaders"
#endif

#ifndef RUNNER_ASSET_DIRECTORY
#define RUNNER_ASSET_DIRECTORY "assets"
#endif

#ifndef RUNNER_VERSION
#define RUNNER_VERSION "development"
#endif

namespace
{
    [[nodiscard]] bool is_down(SDL_MouseButtonFlags buttons, SDL_MouseButtonFlags button) noexcept
    {
        return (buttons & button) != 0;
    }

    [[nodiscard]] bool wants_version(int argc, char** argv) noexcept
    {
        return argc > 1 && argv != nullptr && argv[1] != nullptr
            && std::string_view(argv[1]) == "--version";
    }

    [[nodiscard]] bool wants_vulkan_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-vulkan";
    }

    [[nodiscard]] bool wants_package_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-package";
    }

    [[nodiscard]] bool wants_acceptance_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-acceptance";
    }

    [[nodiscard]] bool wants_camera_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-camera";
    }

    [[nodiscard]] bool wants_course_completion_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-course";
    }
    [[nodiscard]] bool wants_hybrid_brain_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-hybrid-brain";
    }

    [[nodiscard]] bool wants_art_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-art";
    }

    [[nodiscard]] bool wants_course_eye_test(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--course-eye-test";
    }

    struct ArtEyeTestRequest
    {
        bool requested{ false };
        bool valid{ true };
        std::size_t rig_index{};
    };

    [[nodiscard]] ArtEyeTestRequest art_eye_test_request(
        int argc, char** argv) noexcept
    {
        if (argc <= 1 || argv == nullptr || argv[1] == nullptr)
            return {};

        const std::string_view argument{ argv[1] };
        if (argument == "--art-eye-test" || argument == "--art-eye-test=human")
            return { true, true, 0u };
        if (argument == "--art-eye-test=chicken")
            return { true, true, 1u };
        if (argument == "--art-eye-test=dog")
            return { true, true, 2u };
        if (argument == "--art-eye-test=hexapod")
            return { true, true, 3u };
        if (argument.starts_with("--art-eye-test="))
            return { true, false, 0u };
        return {};
    }

    [[nodiscard]] bool wants_walk_eye_test(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--walk-eye-test";
    }
    [[nodiscard]] bool wants_walk_eye_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-walk-eye";
    }
    [[nodiscard]] bool wants_speed_walk_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-speed-walk";
    }
    [[nodiscard]] bool wants_rig_training_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-rig-training";
    }

    [[nodiscard]] bool wants_ui_diagnostic(int argc, char** argv) noexcept
    {
        return argc > 1
            && argv != nullptr
            && argv[1] != nullptr
            && std::string_view(argv[1]) == "--diagnose-ui";
    }

    [[nodiscard]] std::filesystem::path executable_directory()
    {
        const char* const base_path = SDL_GetBasePath();
        if (base_path == nullptr || *base_path == '\0')
            return std::filesystem::current_path();
        return std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(base_path) } };
    }

    [[nodiscard]] bool validate_runtime_layout(const std::filesystem::path& base_directory,
        std::string& error)
    {
        const std::array required_files{
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.vert.spv",
            std::filesystem::path{ RUNNER_SHADER_DIRECTORY } / "flat.frag.spv",
            std::filesystem::path{ "human.rig" },
            std::filesystem::path{ "chicken.rig" },
            std::filesystem::path{ "dog.rig" },
            std::filesystem::path{ "hexapod.rig" },
            std::filesystem::path{ "docs" } / "SANDHYBRID_INTEGRATION_BRIDGE.md",
            std::filesystem::path{ "docs" } / "SandHybrid-missioncache.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0728_COURSE_COMPLETION.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0729_MODULAR_ART_REMAKE.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0730_SUSTAINED_WALK_RECOVERY.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0731_ACTIVE_TERRAIN_CURRICULUM_ART.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0732_SHUTTLE_FACING_HANDS.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0733_GRANULAR_FACING_RIGLAB.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0734_CURRICULUM_SAFE_RIG_OPTIMIZATION.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0735_PIP_POSTURE_TRUTH.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0736_AUTHORED_GAIT_RUNTIME.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0737_HYBRID_LOCOMOTION_TERRAIN.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0738_TURN_TOPOLOGY_STATS.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0739_STATIC_CELLS_DIRECTION.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0740_PHYSICAL_FACING_RETURN.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0741_FOUR_RIG_NATURAL_GAIT.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0742_SPECIES_ANATOMY_SCALE.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0743_AUTHORED_LIVE_MORPHOLOGY.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0744_DOG_ART_RIG_ASSEMBLY.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0745_HUMAN_SUPPORT_SPECIES_ART.md",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_body_side.ppm",
            std::filesystem::path{ "docs" } / "RUNNER_V0746_EXACT_CELLS_NATURAL_GAIT_TURN.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0747_SPECIES_RIGLAB_MASTERY_RELEASE.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0748_CONTACT_LED_GAIT_SPECIES_ART.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0749_PHYSICAL_COMBAT_TERRAIN_ART.md",
            std::filesystem::path{ "docs" } / "RUNNER_V0750_PERSISTENT_DIRECTOR_ART_AUTHORING.md",
            std::filesystem::path{ "docs" } / "EPOCH2D_WALK_ENGINE_LIBRARY.md",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_head_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_upper_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_lower_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_foot_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "chicken_tail_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_body_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_head_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_upper_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_lower_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_foot_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "dog_tail_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_body_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_head_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_upper_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_lower_leg_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_foot_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "species_runtime" / "hexapod_tail_side.ppm",
            std::filesystem::path{ "assets" } / "optional" / "runner_armor_concepts"
                / "runtime" / "hand_side.ppm",
            std::filesystem::path{ "assets" } / "ui" / "runner_icon.png",
            std::filesystem::path{ "assets" } / "ui" / "runner_icon.bmp",
            std::filesystem::path{ "assets" } / "ui" / "runner.ico"
        };
        std::error_code filesystem_error{};
        for (const std::filesystem::path& relative : required_files)
        {
            const std::filesystem::path absolute = base_directory / relative;
            if (!std::filesystem::is_regular_file(absolute, filesystem_error))
            {
                error = "Missing packaged runtime file: " + absolute.string();
                if (filesystem_error)
                    error += " (" + filesystem_error.message() + ")";
                return false;
            }
            filesystem_error.clear();
        }

#ifdef _WIN32
        const std::array required_windows_runtime_files{
            std::filesystem::path{ "SDL3.dll" },
            std::filesystem::path{ "vulkan-1.dll" }
        };
        for (const std::filesystem::path& relative : required_windows_runtime_files)
        {
            const std::filesystem::path absolute = base_directory / relative;
            if (!std::filesystem::is_regular_file(absolute, filesystem_error))
            {
                error = "Missing packaged Windows runtime dependency: " + absolute.string();
                if (filesystem_error)
                    error += " (" + filesystem_error.message() + ")";
                return false;
            }
            filesystem_error.clear();
        }
#endif

        const std::array packaged_rigs{
            std::pair{ std::filesystem::path{ "human.rig" },
                runner::sim::CreatureSpecies::human },
            std::pair{ std::filesystem::path{ "chicken.rig" },
                runner::sim::CreatureSpecies::chicken },
            std::pair{ std::filesystem::path{ "dog.rig" },
                runner::sim::CreatureSpecies::dog },
            std::pair{ std::filesystem::path{ "hexapod.rig" },
                runner::sim::CreatureSpecies::hexapod }
        };
        for (const auto& [relative, expected_species] : packaged_rigs)
        {
            std::string rig_error{};
            if (!runner::sim::CreatureBlueprint::load_for_species(
                    base_directory / relative, expected_species, rig_error))
            {
                error = "Invalid packaged species rig " + relative.string()
                    + ": " + rig_error;
                return false;
            }
        }

        const std::filesystem::path asset_directory =
            base_directory / RUNNER_ASSET_DIRECTORY;
        if (!std::filesystem::is_directory(asset_directory, filesystem_error))
        {
            error = "Missing packaged asset directory: " + asset_directory.string();
            if (filesystem_error)
                error += " (" + filesystem_error.message() + ")";
            return false;
        }

        runner::art::PixelArt packaged_art{};
        if (!runner::art::load_p3_pixel_art(
                asset_directory / "chicken.ppm", packaged_art, error))
            return false;
        if (!packaged_art.loaded())
        {
            error = "Packaged Runner artwork decoded incompletely";
            return false;
        }

        const std::filesystem::path optional_root = asset_directory / "optional"
            / "runner_armor_concepts";
        filesystem_error.clear();
        if (std::filesystem::is_directory(optional_root, filesystem_error))
        {
            const std::array optional_metadata{
                std::filesystem::path{ "README.md" }
            };
            for (const std::filesystem::path& relative : optional_metadata)
            {
                const std::filesystem::path absolute = optional_root / relative;
                filesystem_error.clear();
                if (!std::filesystem::is_regular_file(absolute, filesystem_error))
                {
                    error = "Incomplete optional Runner art package: "
                        + absolute.string();
                    return false;
                }
            }

            const std::array optional_runtime{
                std::filesystem::path{ "runtime" } / "foot_side.ppm",
                std::filesystem::path{ "runtime" } / "forearm_side.ppm",
                std::filesystem::path{ "runtime" } / "hand_side.ppm",
                std::filesystem::path{ "runtime" } / "helmet_side.ppm",
                std::filesystem::path{ "runtime" } / "shin_side.ppm",
                std::filesystem::path{ "runtime" } / "thigh_side.ppm",
                std::filesystem::path{ "runtime" } / "torso_side.ppm",
                std::filesystem::path{ "runtime" } / "upper_arm_side.ppm",
                std::filesystem::path{ "runtime" } / "weapon_side.ppm"
            };
            for (const std::filesystem::path& relative : optional_runtime)
            {
                runner::art::PixelArt optional_art{};
                if (!runner::art::load_p3_pixel_art(
                        optional_root / relative, optional_art, error)
                    || !optional_art.loaded())
                {
                    if (error.empty())
                        error = "Optional Runner art decoded incompletely: "
                            + (optional_root / relative).string();
                    return false;
                }
            }
        }
        filesystem_error.clear();
        error.clear();
        return true;
    }


    [[nodiscard]] bool visible_application_frames(int width, int height)
    {
        constexpr float dt = 1.0f / 60.0f;
        const runner::ui_layout::Box content = runner::ui_layout::content_box(
            static_cast<float>(width), static_cast<float>(height));
        const runner::ui_layout::Box live_world =
            runner::ui_layout::live_world_box(content);
        const runner::ui_layout::Box live_panel =
            runner::ui_layout::live_panel_box(content);
        const runner::ui_layout::Box live_pip =
            runner::ui_layout::training_pip_box(live_world);
        const runner::ui_layout::Box rig_panel =
            runner::ui_layout::rig_lab_panel_box(content);
        const runner::ui_layout::Box rig_world =
            runner::ui_layout::rig_lab_world_box(content);
        const runner::ui_layout::Box rig_live =
            runner::ui_layout::rig_lab_live_box(content);
        const runner::ui_layout::Box rig_trainer =
            runner::ui_layout::rig_lab_trainer_panel_box(content);

        runner::Application application{};
        auto visible = [&](runner::ui_layout::Box region)
        {
            return runner::ui_frame_probe::visibly_populated(
                runner::ui_frame_probe::analyze(application.vertices(), region));
        };

        runner::InputState input{};
        application.frame(input, dt, width, height);
        if (!visible(live_world) || !visible(live_panel) || !visible(live_pip))
            return false;

        runner::InputState switch_to_rig{};
        switch_to_rig.tab_pressed = true;
        application.frame(switch_to_rig, dt, width, height);
        if (!visible(rig_panel) || !visible(rig_world) || !visible(rig_trainer)
            || (runner::ui_layout::rig_lab_shows_live(content) && !visible(rig_live)))
            return false;

        const float usable_width = rig_panel.width - 36.0f;
        const float tab_width = (usable_width - 18.0f) * 0.25f;
        const float tab_y = rig_panel.y + 71.5f;
        for (int slot = 1; slot < 4; ++slot)
        {
            runner::InputState click{};
            click.left_pressed = true;
            click.mouse = {
                rig_panel.x + 18.0f
                    + static_cast<float>(slot) * (tab_width + 6.0f)
                    + tab_width * 0.5f,
                tab_y
            };
            application.frame(click, dt, width, height);
            if (!visible(rig_panel) || !visible(rig_world) || !visible(rig_trainer)
                || (runner::ui_layout::rig_lab_shows_live(content) && !visible(rig_live)))
                return false;
        }
        return true;
    }

    [[nodiscard]] std::filesystem::path invocation_directory(
        int argc, char** argv)
    {
        if (argc > 0 && argv != nullptr && argv[0] != nullptr)
        {
            std::error_code error{};
            const std::filesystem::path absolute =
                std::filesystem::absolute(argv[0], error);
            if (!error && absolute.has_parent_path())
                return absolute.parent_path();
        }
        return std::filesystem::current_path();
    }

    [[nodiscard]] int run_art_diagnostic(
        const std::filesystem::path& base_directory)
    {
        runner::Application application{};
        std::string error{};
        if (!application.initialize(
                base_directory / RUNNER_ASSET_DIRECTORY, error))
        {
            std::fprintf(stderr,
                "Runner %s art diagnostic initialization failed: %s\n",
                RUNNER_VERSION, error.c_str());
            return 1;
        }
        application.prepare_course_eye_test();
        application.frame(runner::InputState{}, 1.0f / 60.0f, 1900, 1180);
        const std::size_t course_vertices = application.vertices().size();
        const std::size_t course_bytes = application.vertices().size_bytes();
        std::array<std::size_t, 4> rig_vertices{};
        std::size_t vertex_count = course_vertices;
        std::size_t vertex_bytes = course_bytes;
        bool all_rigs_rendered = true;
        for (std::size_t index = 0; index < rig_vertices.size(); ++index)
        {
            all_rigs_rendered = application.prepare_art_diagnostic_rig(index)
                && all_rigs_rendered;
            application.frame(runner::InputState{}, 1.0f / 60.0f, 1900, 1180);
            rig_vertices[index] = application.vertices().size();
            all_rigs_rendered = rig_vertices[index] > 0u && all_rigs_rendered;
            vertex_count = std::max(vertex_count, rig_vertices[index]);
            vertex_bytes = std::max(vertex_bytes,
                application.vertices().size_bytes());
        }
        application.prepare_art_fallen_eye_test();
        application.frame(runner::InputState{}, 1.0f / 60.0f, 1900, 1180);
        const std::size_t fallen_vertices = application.vertices().size();
        vertex_count = std::max(vertex_count, fallen_vertices);
        vertex_bytes = std::max(vertex_bytes, application.vertices().size_bytes());
        const std::size_t headroom_limit =
            runner::render::maximum_frame_vertex_bytes * 3u / 4u;
        const bool valid = course_vertices > 0u && all_rigs_rendered
            && fallen_vertices > 0u && vertex_bytes <= headroom_limit;
        std::printf(
            "Runner %s art diagnostic: %s; vertices=%zu bytes=%zu "
            "course_vertices=%zu fallen_vertices=%zu "
            "rig_vertices=%zu,%zu,%zu,%zu "
            "headroom_limit=%zu hard_limit=%zu",
            RUNNER_VERSION, valid ? "passed" : "failed", vertex_count,
            vertex_bytes, course_vertices, fallen_vertices,
            rig_vertices[0], rig_vertices[1], rig_vertices[2], rig_vertices[3],
            headroom_limit,
            runner::render::maximum_frame_vertex_bytes);
        std::putchar(10);
        return valid ? 0 : 1;
    }

}

int main(int argc, char** argv)
{
    const ArtEyeTestRequest art_eye_test = art_eye_test_request(argc, argv);
    if (art_eye_test.requested && !art_eye_test.valid)
    {
        std::fprintf(stderr,
            "Unknown art eye-test subject. Expected human, chicken, dog, or hexapod.\n");
        return 2;
    }

    if (wants_version(argc, argv))
    {
        std::printf("Runner %s\n", RUNNER_VERSION);
        return 0;
    }

    if (wants_walk_eye_diagnostic(argc, argv))
    {
        const runner::diagnostics::WalkEyeTestProof proof =
            runner::diagnostics::run_walk_eye_test_proof();
        std::printf(
            "last evaluation: count=%llu distance=%.3fm strides=%.2f "
            "survival=%.3fs valid=%u quality=0x%016llX invalid=%u "
            "reject=0x%08X reason=%.*s\n",
            static_cast<unsigned long long>(proof.evaluation_count),
            proof.evaluation_distance, proof.evaluation_stride_events,
            proof.evaluation_survival, proof.evaluation_valid ? 1u : 0u,
            static_cast<unsigned long long>(proof.evaluation_quality_key),
            proof.evaluation_invalid_runs, proof.evaluation_rejection_mask,
            static_cast<int>(runner::sim::invalid_motion_name(
                proof.evaluation_invalid_reason).size()),
            runner::sim::invalid_motion_name(
                proof.evaluation_invalid_reason).data());
        std::printf(
            "Runner %s walk-eye proof: %s; updates=%llu retained=%llu "
            "authority=%.3f mean=%.3fm/%.2f steps invalid=%u/6 "
            "reject=0x%08X reason=%.*s display=%.3fm/%u steps/%u crossings "
            "scissor=%.3fs seed=%u training_seed=0x%llX candidates=%u\n",
            RUNNER_VERSION, proof.passed ? "passed" : "failed",
            static_cast<unsigned long long>(proof.updates),
            static_cast<unsigned long long>(proof.retained_update),
            proof.teacher_authority, proof.retained_distance,
            proof.retained_stride_events, proof.retained_invalid_runs,
            proof.retained_rejection_mask,
            static_cast<int>(runner::sim::invalid_motion_name(
                proof.retained_invalid_reason).size()),
            runner::sim::invalid_motion_name(
                proof.retained_invalid_reason).data(),
            proof.displayed_distance, proof.displayed_steps,
            proof.displayed_crossings, proof.displayed_max_scissor_seconds,
            proof.selected_seed,
            static_cast<unsigned long long>(proof.selected_training_seed),
            proof.candidate_attempts);
        std::printf(
            "raw policy audit: mean=%.3fm/%.2f cycles invalid=%u/6 "
            "reject=0x%08X reason=%.*s course_motion=disabled "
            "optional_guidance=disabled mandatory_joint_cluster=enabled\n",
            proof.raw_policy_distance, proof.raw_policy_stride_events,
            proof.raw_policy_invalid_runs, proof.raw_policy_rejection_mask,
            static_cast<int>(runner::sim::invalid_motion_name(
                proof.raw_policy_invalid_reason).size()),
            runner::sim::invalid_motion_name(
                proof.raw_policy_invalid_reason).data());
        return proof.passed ? 0 : 1;
    }
    if (wants_speed_walk_diagnostic(argc, argv))
    {
        const runner::diagnostics::SpeedWalkGraphProof proof =
            runner::diagnostics::run_speed_walk_graph_proof();
        std::printf(
            "Runner %s Speed Walk graph diagnostic: %s; walk=%llu/%llu "
            "speed_walk=%llu/%llu retained=%.3fm replay=%s %.3fm/%.2f "
            "strides/%.3fmps/%.3fs collisions=%.2f invalid=%u/6 "
            "reject=0x%08X failed_seed=%llu failed=%.3fm/%.3fs/%.*s "
            "authority=%.3f evaluation=%s course_motion=%s\n",
            RUNNER_VERSION, proof.passed ? "passed" : "failed",
            static_cast<unsigned long long>(proof.walk_updates),
            static_cast<unsigned long long>(proof.walk_retained_update),
            static_cast<unsigned long long>(proof.speed_walk_updates),
            static_cast<unsigned long long>(proof.speed_walk_retained_update),
            proof.speed_walk_retained_distance,
            proof.speed_walk_retained ? "retained" : "missing", proof.distance,
            proof.stride_events, proof.speed, proof.survival,
            proof.collisions, proof.invalid_runs, proof.rejection_mask,
            static_cast<unsigned long long>(proof.failed_seed),
            proof.failed_distance, proof.failed_survival,
            static_cast<int>(runner::sim::invalid_motion_name(
                proof.failed_reason).size()),
            runner::sim::invalid_motion_name(proof.failed_reason).data(),
            proof.teacher_authority,
            proof.raw_evaluation ? "raw-policy" : "assisted",
            proof.course_motion_enabled ? "enabled" : "disabled");
        return proof.passed ? 0 : 1;
    }
    if (wants_art_diagnostic(argc, argv))
        return run_art_diagnostic(invocation_directory(argc, argv));

    if (wants_course_completion_diagnostic(argc, argv))
    {
        const runner::diagnostics::CourseCompletionReport report =
            runner::diagnostics::run_course_completion_diagnostic();
        std::printf(
            "launch_contact=%s active_terrain=%s delayed_objects=%s "
            "materials=%s seed_variation=%s water_holes=%s "
            "observations=%s delayed_pressure=%s climb=%s equipment=%s "
            "equipment_off=%s frame_independent=%s\n",
            report.launch_contact ? "passed" : "failed",
            report.active_terrain ? "passed" : "failed",
            report.safe_runway ? "passed" : "failed",
            report.material_regions ? "passed" : "failed",
            report.seed_variation ? "passed" : "failed",
            report.water_and_holes ? "passed" : "failed",
            report.observation_truth ? "passed" : "failed",
            report.delayed_material_pressure ? "passed" : "failed",
            report.climb_contract ? "passed" : "failed",
            report.equipment_contract ? "passed" : "failed",
            report.equipment_off_identity ? "passed" : "failed",
            report.frame_independent ? "passed" : "failed");
        std::printf("Runner %s course diagnostic: %s\n", RUNNER_VERSION,
            report.passed() ? "passed" : "failed");
        return report.passed() ? 0 : 1;
    }

    if (wants_rig_training_diagnostic(argc, argv))
    {
        const runner::diagnostics::RigTrainingReport report =
            runner::diagnostics::run_rig_training_diagnostic();
        for (const runner::diagnostics::RigTrainingResult& rig : report.rigs)
        {
            std::printf(
                "%.*s mean=%.4f evaluation=%.4f strides=%.2f invalid=%u "
                "authority=%.3f best_update=%llu retained=%.4f/%.2f/%u "
                "preview_resets=%llu reason=%.*s conveyor=%s\n",
                static_cast<int>(rig.name.size()), rig.name.data(),
                rig.mean_episode_distance, rig.evaluation_distance,
                rig.evaluation_stride_events, rig.evaluation_invalid_runs,
                rig.teacher_authority,
                static_cast<unsigned long long>(rig.retained_update),
                rig.retained_probe_distance,
                rig.retained_probe_stride_events,
                rig.retained_probe_invalid_runs,
                static_cast<unsigned long long>(rig.preview_resets),
                static_cast<int>(runner::sim::invalid_motion_name(
                    rig.preview_reset_reason).size()),
                runner::sim::invalid_motion_name(rig.preview_reset_reason).data(),
                rig.rollout_course_motion_enabled ? "enabled" : "disabled");
            if (rig.name == "human")
                std::printf(
                    "human shuttle updates=%llu authority=%.3f retained=%s "
                    "best_update=%llu quality=%llu probe=%.4f/%.2f/%.2f/%u/%u/%.*s\n",
                    static_cast<unsigned long long>(rig.shuttle_lesson_updates),
                    rig.shuttle_teacher_authority,
                    rig.shuttle_retained_policy ? "yes" : "no",
                    static_cast<unsigned long long>(rig.shuttle_retained_update),
                    static_cast<unsigned long long>(rig.shuttle_retained_quality),
                    rig.shuttle_probe_distance, rig.shuttle_probe_stride_events,
                    rig.shuttle_probe_turns, rig.shuttle_probe_rejection_mask,
                    rig.shuttle_probe_invalid_runs,
                    static_cast<int>(runner::sim::invalid_motion_name(
                        rig.shuttle_probe_invalid_reason).size()),
                    runner::sim::invalid_motion_name(
                        rig.shuttle_probe_invalid_reason).data());
        }
        std::printf("Runner %s rig-training diagnostic: %s\n",
            RUNNER_VERSION, report.passed ? "passed" : "failed");
        return report.passed ? 0 : 1;
    }

    if (wants_hybrid_brain_diagnostic(argc, argv))
    {
        const runner::diagnostics::HybridBrainReport report =
            runner::diagnostics::run_hybrid_brain_diagnostic();
        std::printf(
            "Runner %s hybrid-brain diagnostic: %s; hole=%s falling=%s "
            "harmless=%s blocked=%s bounds=%s topologies=%s cadence=%s\n",
            RUNNER_VERSION, report.passed() ? "passed" : "failed",
            report.hole_escape ? "passed" : "failed",
            report.falling_dodge ? "passed" : "failed",
            report.harmless_object ? "passed" : "failed",
            report.blocked_exit ? "passed" : "failed",
            report.policy_bounds ? "passed" : "failed",
            report.multi_topology ? "passed" : "failed",
            report.frame_independent ? "passed" : "failed");
        return report.passed() ? 0 : 1;
    }

if (wants_camera_diagnostic(argc, argv))
{
    const float automatic = runner::view_camera::automatic_pixels_per_meter(
        820.0f, 3.0f);
    const float fitted = runner::view_camera::fitted_pixels_per_meter(
        820.0f, 3.0f, 1.0f);
    const float zoomed = runner::view_camera::apply_wheel_zoom(1.0f, 1.0f);
    const float lookahead = runner::view_camera::lookahead_meters(
        900.0f, fitted);
    const float followed = runner::view_camera::smooth_camera(
        0.0f, 4.0f, fitted, 1.0f / 60.0f);
    const bool valid = automatic > 22.0f
        && fitted >= runner::view_camera::default_pixels_per_meter
        && zoomed > 1.0f
        && lookahead > 2.0f
        && followed > 0.0f && followed < 4.0f
        && runner::view_camera::pip_pixels_per_meter(10.0f, 80.0f)
            == runner::view_camera::pip_minimum_pixels_per_meter;
    std::printf(
        "Runner %s camera diagnostic: %s; default=%.1f px/m fitted=%.1f "
        "zoom=%.3f lookahead=%.2f follow=%.3f\n",
        RUNNER_VERSION, valid ? "passed" : "failed",
        runner::view_camera::default_pixels_per_meter,
        fitted, zoomed, lookahead, followed);
    return valid ? 0 : 1;
}

    if (wants_ui_diagnostic(argc, argv))
    {
        bool valid = true;
        for (const auto& size : runner::ui_layout::validation_sizes)
            valid = valid && runner::ui_layout::live_layout_valid(size[0], size[1]);
        const runner::ui_layout::SurfaceScale dpi =
            runner::ui_layout::logical_surface_scale(1600.0f, 900.0f,
                2400.0f, 1350.0f);
        valid = valid && std::abs(dpi.x - 1.5f) < 1.0e-5f
            && std::abs(dpi.y - 1.5f) < 1.0e-5f
            && visible_application_frames(1600, 900)
            && visible_application_frames(3840, 2160);
        std::printf("Runner %s UI diagnostic: %s; layouts=%zu dpi=%.2fx%.2f frames=%s\n",
            RUNNER_VERSION, valid ? "passed" : "failed",
            runner::ui_layout::validation_sizes.size(), dpi.x, dpi.y,
            valid ? "visible" : "invalid");
        return valid ? 0 : 1;
    }

    if (wants_acceptance_diagnostic(argc, argv))
    {
        const runner::acceptance::Report report =
            runner::acceptance::run_live_acceptance_matrix();
        for (const runner::acceptance::CaseResult& result : report.cases)
        {
            std::printf("[%s] %s: %s\n",
                result.passed ? "PASS" : "FAIL",
                result.name.c_str(),
                result.detail.c_str());
        }
        std::printf("Runner %s live acceptance matrix: %zu/%zu passed\n",
            RUNNER_VERSION, report.passed_count(), report.cases.size());
        return report.passed() ? 0 : 1;
    }

    if (wants_package_diagnostic(argc, argv))
    {
        const std::filesystem::path package_directory = executable_directory();
        std::string layout_error{};
        if (!validate_runtime_layout(package_directory, layout_error))
        {
            std::fprintf(stderr, "Runner package diagnostic failed: %s\n",
                layout_error.c_str());
            return 1;
        }
        std::printf(
            "Runner %s package diagnostic passed: executable-relative runtime "
            "files and species rigs are present\n",
            RUNNER_VERSION);
        return 0;
    }

    const bool diagnostic = wants_vulkan_diagnostic(argc, argv);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        const std::string video_error = SDL_GetError();
        if (diagnostic && runner::runtime::is_headless_surface_error(video_error))
        {
            std::printf("Runner %s SDL3 Vulkan diagnostic passed: linked backend "
                "enabled; the host has no video surface (%s)\n",
                RUNNER_VERSION, video_error.c_str());
            return 0;
        }
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const std::filesystem::path base_directory = executable_directory();
    const std::filesystem::path shader_directory =
        base_directory / RUNNER_SHADER_DIRECTORY;
    const std::filesystem::path asset_directory =
        base_directory / RUNNER_ASSET_DIRECTORY;

    if (!SDL_Vulkan_LoadLibrary(nullptr))
    {
        const std::string vulkan_error = SDL_GetError();
        if (diagnostic && runner::runtime::is_headless_surface_error(vulkan_error))
        {
            const char* video_driver = SDL_GetCurrentVideoDriver();
            std::printf(
                "Runner " RUNNER_VERSION " SDL3 Vulkan diagnostic passed: backend "
                "enabled, video_driver=%s; the host has no Vulkan presentation surface (%s)\n",
                video_driver != nullptr ? video_driver : "unknown",
                vulkan_error.c_str());
            SDL_Quit();
            return 0;
        }

        std::fprintf(
            stderr,
            "SDL3 Vulkan support unavailable: %s\n"
            "This build requires the vcpkg sdl3[vulkan] feature and a Vulkan-capable display driver.\n",
            vulkan_error.c_str());
        SDL_Quit();
        return 1;
    }

    Uint32 instance_extension_count{};
    const char* const* instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);
    if (instance_extensions == nullptr || instance_extension_count == 0)
    {
        std::fprintf(stderr, "SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    if (diagnostic)
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        std::printf(
            "Runner " RUNNER_VERSION " SDL3 Vulkan diagnostic passed: video_driver=%s, instance_extensions=%u\n",
            video_driver != nullptr ? video_driver : "unknown",
            static_cast<unsigned int>(instance_extension_count));
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 0;
    }

    runner::Application application{};
    std::string error{};
    if (!application.initialize(asset_directory, error))
    {
        std::fprintf(stderr, "Application initialization failed: %s\n", error.c_str());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }
    if (wants_walk_eye_test(argc, argv)
        && !application.prepare_walk_eye_test(error))
    {
        std::fprintf(stderr, "Walk eye test failed: %s\n", error.c_str());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Runner v" RUNNER_VERSION " - Autonomous Physics Locomotion Trainer",
        1900,
        1180,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1280, 820);
    const std::filesystem::path icon_path = base_directory
        / RUNNER_ASSET_DIRECTORY / "ui" / "runner_icon.bmp";
    if (SDL_Surface* icon = SDL_LoadBMP(icon_path.string().c_str()); icon != nullptr)
    {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }

    runner::render::VulkanRenderer renderer{};
    if (!renderer.initialize(window, shader_directory, error))
    {
        std::fprintf(stderr, "Vulkan initialization failed: %s\n", error.c_str());
        SDL_DestroyWindow(window);
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
        return 1;
    }

    if (art_eye_test.requested)
        static_cast<void>(application.prepare_art_diagnostic_rig(
            art_eye_test.rig_index));
    else if (wants_course_eye_test(argc, argv))
        application.prepare_course_eye_test();
    bool running = true;
    std::uint64_t previous_ticks = SDL_GetTicksNS();
    runner::Vec2 previous_mouse{};

    while (running && !application.wants_quit())
    {
        runner::InputState input{};
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                input.left_pressed = input.left_pressed || event.button.button == SDL_BUTTON_LEFT;
                input.right_pressed = input.right_pressed || event.button.button == SDL_BUTTON_RIGHT;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                input.left_released = input.left_released || event.button.button == SDL_BUTTON_LEFT;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                input.wheel += event.wheel.y;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat)
                {
                    switch (event.key.scancode)
                    {
                    case SDL_SCANCODE_ESCAPE: input.escape_pressed = true; break;
                    case SDL_SCANCODE_SPACE: input.space_pressed = true; break;
                    case SDL_SCANCODE_DELETE: input.delete_pressed = true; break;
                    case SDL_SCANCODE_1: input.key_1_pressed = true; break;
                    case SDL_SCANCODE_2: input.key_2_pressed = true; break;
                    case SDL_SCANCODE_3: input.key_3_pressed = true; break;
                    case SDL_SCANCODE_TAB: input.tab_pressed = true; break;
                    case SDL_SCANCODE_T: input.totals_pressed = true; break;
                    case SDL_SCANCODE_U: input.units_pressed = true; break;
                    case SDL_SCANCODE_A: input.art_pressed = true; break;
                    case SDL_SCANCODE_S: input.save_pressed = true; break;
                    case SDL_SCANCODE_L: input.load_pressed = true; break;
                    case SDL_SCANCODE_R: input.reset_pressed = true; break;
                    default: break;
                    }
                }
                break;
            default:
                break;
            }
        }

        float mouse_x{};
        float mouse_y{};
        const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        int logical_width{};
        int logical_height{};
        int drawable_width{};
        int drawable_height{};
        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        input.mouse = { mouse_x, mouse_y };
        input.mouse_delta = input.mouse - previous_mouse;
        previous_mouse = input.mouse;
        input.left_down = is_down(mouse_buttons, SDL_BUTTON_LMASK);
        const SDL_Keymod modifiers = SDL_GetModState();
        input.shift = (modifiers & SDL_KMOD_SHIFT) != 0;
        input.control = (modifiers & SDL_KMOD_CTRL) != 0;
        input.alt = (modifiers & SDL_KMOD_ALT) != 0;

        const std::uint64_t current_ticks = SDL_GetTicksNS();
        const float dt = std::clamp(static_cast<float>(current_ticks - previous_ticks) / 1'000'000'000.0f,
            1.0f / 240.0f, 1.0f / 15.0f);
        previous_ticks = current_ticks;

        SDL_GetWindowSize(window, &logical_width, &logical_height);
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, logical_width, logical_height);
        if (!renderer.render(application.vertices(), logical_width, logical_height,
            drawable_width, drawable_height, error))
        {
            std::fprintf(stderr, "Render failure: %s\n", error.c_str());
            running = false;
        }
    }

    renderer.wait_idle();
    renderer.shutdown();
    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    return 0;
}
