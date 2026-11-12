#pragma once

#include "presentation_session.hpp"
#include "renderer.hpp"
#include "simulation.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace runner
{
    enum class RigLabStartupPage : std::uint8_t
    {
        presets,
        structure,
        motors,
        art,
        test
    };

    struct RigPreviewContactEvidence
    {
        bool left_heel{};
        bool left_ball{};
        bool left_toe{};
        bool right_heel{};
        bool right_ball{};
        bool right_toe{};
        bool valid_motion{};
        std::uint64_t physical_test_generation{};
        std::uint16_t left_support_contacts{};
        std::uint16_t right_support_contacts{};

        [[nodiscard]] constexpr bool opposing_planted_sole() const noexcept
        {
            const bool left_planted = left_heel && left_ball && left_toe;
            const bool right_planted = right_heel && right_ball && right_toe;
            const bool left_air = !left_heel && !left_ball && !left_toe;
            const bool right_air = !right_heel && !right_ball && !right_toe;
            return valid_motion
                && ((left_planted && right_air)
                    || (right_planted && left_air));
        }

        [[nodiscard]] constexpr bool has_physical_support() const noexcept
        {
            return valid_motion
                && left_support_contacts + right_support_contacts > 0u;
        }
    };

    struct RigPreviewTerminalEvidence
    {
        bool frozen{};
        sim::TrialTerminalCause cause{ sim::TrialTerminalCause::none };
        sim::InvalidMotion reason{ sim::InvalidMotion::none };
        Vec2 position{};
        sim::TerrainRegion terrain{ sim::TerrainRegion::firm };
        float water_depth{};
        std::uint64_t physical_test_generation{};
        std::uint64_t pose_signature{};
    };

    struct LiveCameraEvidence
    {
        float camera_x{};
        float pixels_per_meter{};
        float zoom_factor{};
        bool auto_view{};
        bool projection_initialized{};
    };

    struct InputState
    {
        Vec2 mouse{};
        Vec2 mouse_delta{};
        float wheel{};
        bool left_down{};
        bool left_pressed{};
        bool left_released{};
        bool right_pressed{};
        bool shift{};
        bool control{};
        bool alt{};
        bool delete_pressed{};
        bool escape_pressed{};
        bool space_pressed{};
        bool key_1_pressed{};
        bool key_2_pressed{};
        bool key_3_pressed{};
        bool tab_pressed{};
        bool totals_pressed{};
        bool units_pressed{};
        bool art_pressed{};
        bool save_pressed{};
        bool load_pressed{};
        bool reset_pressed{};
    };

    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        [[nodiscard]] bool initialize(
            const std::filesystem::path& asset_directory,
            std::string& error,
            presentation::StartupMode startup_mode =
                presentation::StartupMode::interactive,
            const std::filesystem::path& persistence_directory = {});
        void prepare_course_eye_test();
        void prepare_art_eye_test();
        [[nodiscard]] bool prepare_art_diagnostic_rig(std::size_t index);
        void prepare_art_fallen_eye_test();
        [[nodiscard]] bool prepare_equipment_eye_test(std::string& error);
        [[nodiscard]] bool prepare_walk_eye_test(std::string& error);
        [[nodiscard]] bool prepare_rig_lab_page(
            RigLabStartupPage page, std::size_t species_index = 0u);
        void frame(const InputState& input, float dt, int width, int height);
        [[nodiscard]] std::span<const render::Vertex> vertices() const noexcept;
        [[nodiscard]] std::uint64_t rig_preview_signature() const noexcept;
        [[nodiscard]] RigPreviewContactEvidence
            rig_preview_contact_evidence() const noexcept;
        [[nodiscard]] RigPreviewTerminalEvidence
            rig_preview_terminal_evidence() const noexcept;
        [[nodiscard]] LiveCameraEvidence live_camera_evidence() const noexcept;
        [[nodiscard]] bool prepare_forced_rig_test_terminal() noexcept;
        [[nodiscard]] bool wants_quit() const noexcept;

    private:
        struct Impl;
        Impl* impl_{};
    };
}
