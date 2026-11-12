#include "app.hpp"
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

int main(int argc, char** argv)
{
    const bool focused_human_geometry = argc == 2
        && std::string_view{ argv[1] } == "--human-geometry-only";
    require(argc == 1 || focused_human_geometry,
        "unknown modular-art test mode");
    const std::filesystem::path root{ RUNNER_SOURCE_ROOT };
    static_assert(runner::RigPreviewContactEvidence{
        true, true, true, false, false, false, true, 1u }
        .opposing_planted_sole());
    static_assert(runner::RigPreviewContactEvidence{
        false, false, false, true, true, true, true, 1u }
        .opposing_planted_sole());
    static_assert(!runner::RigPreviewContactEvidence{
        true, false, true, false, false, false, true, 1u }
        .opposing_planted_sole());
    static_assert(!runner::RigPreviewContactEvidence{
        true, true, true, false, false, false, false, 1u }
        .opposing_planted_sole());
    static_assert(runner::RigPreviewContactEvidence{
        false, false, false, false, false, false, true, 1u, 2u, 1u }
        .has_physical_support());
    static_assert(!runner::RigPreviewContactEvidence{
        false, false, false, false, false, false, true, 1u, 0u, 0u }
        .has_physical_support());
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
        const float opaque_height_fraction = static_cast<float>(
            maximum_y - minimum_y + 1) / static_cast<float>(sprite.height);
        if (expected_sprite.name == "upper_arm_side.ppm")
            require(std::abs(opaque_height_fraction
                        - runner::art::human_upper_arm_opaque_transverse_fraction)
                    <= 1.0e-6f,
                "upper-arm fit no longer matches the supplied opaque silhouette");
        else if (expected_sprite.name == "forearm_side.ppm")
            require(std::abs(opaque_height_fraction
                        - runner::art::human_forearm_opaque_transverse_fraction)
                    <= 1.0e-6f,
                "forearm fit no longer matches the supplied opaque silhouette");
        else if (expected_sprite.name == "hand_side.ppm")
            require(std::abs(opaque_height_fraction
                        - runner::art::human_hand_opaque_transverse_fraction)
                    <= 1.0e-6f,
                "hand fit no longer matches the supplied opaque silhouette");
        else if (expected_sprite.name == "foot_side.ppm")
            require(std::abs(static_cast<float>(maximum_y + 1)
                        / static_cast<float>(sprite.height)
                        - runner::art::human_boot_opaque_sole_fraction)
                    <= 1.0e-6f,
                "boot attachment no longer matches the supplied opaque sole row");

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
        constexpr runner::Color source{ 0.20f, 0.40f, 0.60f, 0.35f };
        constexpr runner::Color tint{ 0.80f, 0.20f, 0.10f, 0.90f };
        const runner::Color unchanged = runner::art::opaque_depth_tint(
            source, tint, 0.0f);
        const runner::Color biased = runner::art::opaque_depth_tint(
            source, tint, 0.25f);
        const runner::Color saturated = runner::art::opaque_depth_tint(
            source, tint, 9.0f);
        const runner::Color invalid_strength = runner::art::opaque_depth_tint(
            source, tint, std::numeric_limits<float>::quiet_NaN());
        const runner::Color invalid_channels = runner::art::opaque_depth_tint(
            { std::numeric_limits<float>::quiet_NaN(), 0.40f, 0.60f,
                std::numeric_limits<float>::quiet_NaN() },
            { 0.80f, std::numeric_limits<float>::infinity(), 0.10f, 1.0f },
            0.50f);
        const runner::Color repeated = runner::art::opaque_depth_tint(
            source, tint, 0.25f);
        require(same_color(unchanged, source)
                && same_color(invalid_strength, source),
            "zero or invalid depth tint changed authored source pixels");
        require(std::abs(biased.r - 0.35f) < 1.0e-6f
                && std::abs(biased.g - 0.35f) < 1.0e-6f
                && std::abs(biased.b - 0.475f) < 1.0e-6f
                && biased.a == source.a
                && std::abs(saturated.r - tint.r) < 1.0e-6f
                && std::abs(saturated.g - tint.g) < 1.0e-6f
                && std::abs(saturated.b - tint.b) < 1.0e-6f
                && saturated.a == source.a,
            "bounded depth tint changed alpha or escaped its color-only blend");
        require(std::isfinite(invalid_channels.r)
                && std::isfinite(invalid_channels.g)
                && std::isfinite(invalid_channels.b)
                && invalid_channels.a == 0.0f
                && same_color(biased, repeated),
            "adversarial or repeated depth tint is not finite and deterministic");
    }

    {
        runner::art::PixelArt weapon{};
        std::string error{};
        require(runner::art::load_p3_pixel_art(
                runtime / "weapon_side.ppm", weapon, error),
            "weapon source landmark fixture did not load");
        const runner::art::OpaquePixelBounds bounds =
            runner::art::opaque_pixel_bounds(weapon);
        constexpr int grip_x = 12;
        constexpr int grip_y = 16;
        require(bounds.left == 7 && bounds.right == 37
                && bounds.top == 4 && bounds.bottom == 20
                && !weapon.transparent(weapon.pixels[static_cast<std::size_t>(
                    grip_y * weapon.width + grip_x)])
                && std::abs(runner::art::equipment_primary_grip_fraction
                    - static_cast<float>(grip_x - bounds.left)
                        / static_cast<float>(bounds.right - bounds.left))
                    < 1.0e-6f
                && std::abs(runner::art::equipment_primary_grip_normal_fraction
                    - static_cast<float>(grip_y)
                        / static_cast<float>(weapon.height)) < 1.0e-6f,
            "physical hand anchor no longer matches the supplied weapon grip pixel");
    }

    {
        const std::filesystem::path keyed_path =
            std::filesystem::temp_directory_path() / "runner-v0729-keyed.ppm";
        std::ofstream output(keyed_path, std::ios::binary | std::ios::trunc);
        output << "P3\n3 3\n255\n"
            << "255 0 255  255 80 220  255 0 255\n"
            << "255 0 255  1 1 1       253 0 253\n"
            << "255 0 255  255 0 255   255 0 255\n";
        output.close();
        runner::art::PixelArt keyed{};
        std::string error{};
        require(runner::art::load_p3_pixel_art(keyed_path, keyed, error)
                && keyed.chroma_keyed,
            "adversarial keyed fixture did not detect its key");
        require(keyed.transparent(keyed.pixels.front())
                && keyed.transparent(keyed.pixels[5]),
            "adversarial keyed fixture did not hide exact and near-magenta fringe");
        require(!keyed.transparent(keyed.pixels[4]),
            "adversarial keyed fixture erased black subject detail");
        require(!keyed.transparent(keyed.pixels[1]),
            "chroma tolerance erased an authored non-key pink pixel");
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

    // This independently runnable slice keeps the supplied opaque-pixel
    // proportions and physical heel/wrist anchors testable while broader UI
    // or locomotion contracts in this historical aggregate are under repair.
    {
        constexpr runner::Vec2 heel{ 18.0f, -3.0f };
        constexpr runner::Vec2 toe{ -14.0f, 5.0f };
        constexpr float height = 14.0f;
        const float span = runner::length(toe - heel);
        const runner::art::OrientedArtTransform boot =
            runner::art::articulated_boot_transform(
                heel, toe, span, height, 1.0f);
        const runner::art::OrientedArtTransform facing_flipped_boot =
            runner::art::articulated_boot_transform(
                heel, toe, span, height, -1.0f);
        const runner::Vec2 axis = runner::normalized(
            toe - heel, { 1.0f, 0.0f });
        const runner::Vec2 presented_normal{ -axis.y, axis.x };
        const float anchor_offset = height
            * (runner::art::human_boot_opaque_sole_fraction - 0.5f
                - runner::art::human_boot_contact_depth_fraction);
        const float sole_offset = height
            * (runner::art::human_boot_opaque_sole_fraction - 0.5f);
        require(runner::length(boot.beginning
                    + presented_normal * anchor_offset - heel) <= 1.0e-5f
                && runner::length(boot.ending
                    + presented_normal * anchor_offset - toe) <= 1.0e-5f
                && runner::dot(boot.ending - boot.beginning, toe - heel) > 0.0f,
            "rear boot reordered or detached its physical heel/toe plate");
        require(runner::dot(facing_flipped_boot.ending
                    - facing_flipped_boot.beginning, toe - heel) > 0.0f,
            "whole-rig facing flip reordered the real rear heel/toe axis");
        require(runner::length(boot.beginning
                    + presented_normal * sole_offset
                    - (heel + presented_normal * height
                        * runner::art::human_boot_contact_depth_fraction))
                    <= 1.0e-5f
                && runner::length(boot.ending
                    + presented_normal * sole_offset
                    - (toe + presented_normal * height
                        * runner::art::human_boot_contact_depth_fraction))
                    <= 1.0e-5f,
            "authored boot sole detached from the physical contact depth");
        const runner::art::OrientedArtTransform invalid_boot =
            runner::art::articulated_boot_transform(
                { std::numeric_limits<float>::quiet_NaN(), 0.0f },
                toe, span, height, 1.0f);
        require(invalid_boot.thickness == 0.0f,
            "invalid physical foot telemetry manufactured boot art");

        const runner::art::HandArtDimensions hand =
            runner::art::hand_art_dimensions(34.0f, 40, 28, 1.0f, 64.0f);
        const runner::art::HandArtDimensions repeated_hand =
            runner::art::hand_art_dimensions(34.0f, 40, 28, 1.0f, 64.0f);
        const float visible_hand = hand.thickness
            * runner::art::human_hand_opaque_transverse_fraction;
        require(hand.thickness >= 64.0f * 0.75f
                && hand.thickness <= 64.0f * 0.76f + 1.0e-6f
                && visible_hand >= 64.0f * 0.35f
                && visible_hand <= 64.0f * 0.36f
                && hand.length <= 64.0f * 0.38f + 1.0e-6f
                && hand.wrist_overlap >= hand.length * 0.27f,
            "irregular hand remains a tiny hook or escaped its forearm");
        require(hand.length == repeated_hand.length
                && hand.thickness == repeated_hand.thickness
                && hand.wrist_overlap == repeated_hand.wrist_overlap,
            "repeated hand fit changed source-visible geometry");

        const float visible_upper =
            runner::art::human_limb_thickness_ratio(false, true)
            * runner::art::human_upper_arm_opaque_transverse_fraction;
        const float visible_forearm =
            runner::art::human_limb_thickness_ratio(false, false)
            * runner::art::human_forearm_opaque_transverse_fraction;
        require(visible_upper >= 0.56f && visible_upper <= 0.57f
                && visible_forearm >= 0.49f && visible_forearm <= 0.50f
                && visible_upper - visible_forearm >= 0.069f
                && visible_upper - visible_forearm <= 0.071f
                && runner::art::fitted_joint_overlap(
                    64.0f, 34.0f, false) >= 64.0f * 0.08f
                && runner::art::fitted_joint_overlap(
                    64.0f, 34.0f, false) <= 64.0f * 0.09f,
            "source padding restored a uniform or wire-thin arm");

        using runner::art::HumanFootEdgeKind;
        require(runner::art::human_foot_edge_kind(0, 1)
                    == HumanFootEdgeKind::ordered_path
                && runner::art::human_foot_edge_kind(2, 1)
                    == HumanFootEdgeKind::ordered_path
                && runner::art::human_foot_edge_kind(2, 3)
                    == HumanFootEdgeKind::ordered_path
                && runner::art::human_foot_edge_kind(0, 2)
                    == HumanFootEdgeKind::triangle_brace
                && runner::art::human_foot_edge_kind(2, 0)
                    == HumanFootEdgeKind::triangle_brace,
            "mirrored physical foot slots lost the ordered sole or brace role");
        for (const std::array<int, 2> malformed :
            { std::array{ -1, 1 }, std::array{ 0, 0 },
                std::array{ 0, 3 }, std::array{ 2, 4 } })
            require(runner::art::human_foot_edge_kind(
                        malformed[0], malformed[1])
                    == HumanFootEdgeKind::ordinary,
                "malformed or shortcut foot edge gained ordered-path emphasis");
        const runner::art::HumanFootEdgeStyle near_path =
            runner::art::human_foot_edge_style(
                HumanFootEdgeKind::ordered_path, true);
        const runner::art::HumanFootEdgeStyle far_path =
            runner::art::human_foot_edge_style(
                HumanFootEdgeKind::ordered_path, false);
        const runner::art::HumanFootEdgeStyle near_brace =
            runner::art::human_foot_edge_style(
                HumanFootEdgeKind::triangle_brace, true);
        const runner::art::HumanFootEdgeStyle repeated_far_path =
            runner::art::human_foot_edge_style(
                HumanFootEdgeKind::ordered_path, false);
        require(near_path.width > far_path.width
                && far_path.width > near_brace.width * 2.0f
                && near_path.opacity > far_path.opacity
                && far_path.opacity > near_brace.opacity
                && far_path.width == repeated_far_path.width
                && far_path.opacity == repeated_far_path.opacity,
            "rear-foot path is not deterministic, depth-readable, or dominant over its brace");
    }
    if (focused_human_geometry)
    {
        std::cout << "Epoch2DWalkEngine v0.7.51 Human pixel-art geometry contracts passed\n";
        return EXIT_SUCCESS;
    }

    const std::string app = read_text(root / "src" / "app.cpp");
    const std::string main_source = read_text(root / "src" / "main.cpp");
    require(main_source.find(
                "runner::ui_layout::minimum_window_width")
                != std::string::npos
            && main_source.find(
                "runner::ui_layout::minimum_window_height")
                != std::string::npos
            && main_source.find("SDL_SetWindowMinimumSize(window, 1280, 820)")
                == std::string::npos,
        "native SDL window minimum drifted from the tested shared layout contract");
    require(main_source.find("SDL_SCANCODE_F11") != std::string::npos
            && main_source.find("SDL_SetWindowFullscreen(window, requested)")
                != std::string::npos
            && main_source.find("if (fullscreen)") != std::string::npos
            && main_source.find("SDL_SetWindowFullscreen(window, false)")
                != std::string::npos,
        "F11 fullscreen or Escape-first fullscreen exit is not wired through SDL");
    for (std::string_view reference : {
            "draw_oriented_pixel_art", "draw_fitted_armor", "draw_body_segments",
            "optional_upper_arm_art", "optional_forearm_art",
            "optional_thigh_art", "optional_shin_art",
            "articulated_boot_transform", "rig.support_branch_mask(motor)",
            "rig.node_support_mask(index)", "art::oriented_box_transform",
            "art::SkinEnvelopeDimensions", "art::skin_envelope_dimensions",
            "minimum_shoulder", "envelope.chest_radius",
            "assembled_art_scale", "art::scaled_pixels", "presentation_side",
            "art::hand_art_dimensions",
            "art::human_arm_layer_opacity",
            "authored_joint_overlap", "art::fitted_joint_overlap",
            "authored_plate_span", "authored_lower_leg_span",
            "const std::size_t knee_index = support_parent(ankle_index)",
            "is_articulated_foot_motor", "grip_anchored_art_transform",
            "equipment_primary_grip_normal_fraction",
            "equipment_display_position()", "equipment_display_direction()",
            "geometry.butt_to_rear_grip", "draw_held_equipment_layer",
            "constexpr int held_equipment_layer = 2",
            "physically anchored backbone", "stock/receiver/barrel accents",
            "manipulator_outline", "between the rear", "endpoint_radius",
            "no endpoint is displaced to fake depth",
            "opaque_depth_tint", "only inside supplied opaque pixels",
            "physical grip and muzzle",
            "if (!terminal)", "near ? 0.98f : 0.86f",
            "physical_right_leg_near", "chain_tone",
            "CLEAR TO 8-12 M / 2 PHYSICAL GAIT CYCLES",
            "rig_preview_projection(viewport)",
            "view_camera::inspection_pixels_per_meter",
            "fit_rig_preview_projection", "probe_canvas",
            "inspection_envelope_scale_multiplier",
            "inspection_camera_delta_meters",
            "rig_frame_projection.reset()",
            "shared_projection.camera", "shared_projection.scale",
            "draw_art_editor_world(viewport, false, inspect_physical_topology)",
            "L LIVE BALL", "R LIVE TOE",
            "BONE EDITOR",
            "index < particles.size() ? physical_screen(index)",
            "MOTOR EDITOR",
            "TEST VIEW",
            "PHYSICAL PREVIEW / ZOOM {:.2f}X",
            "projection.camera, projection.scale, false",
            "float art_editor_zoom{ ui_layout::rig_lab_default_zoom }",
            "bool live_art_enabled{ false }",
            "bool rig_lab_art_enabled{ false }",
            "return mode == Mode::live ? live_art_enabled : rig_lab_art_enabled",
            "rig_lab_art_enabled ? \"ART ON\" : \"ART OFF\"",
            "impl_->live_art_enabled = impl_->optional_foot_art.loaded()",
            "impl_->rig_lab_art_enabled = false",
            "Epoch2DWalkEngine v", "PHYSICAL LOCOMOTION STUDIO",
            "ui_layout::RigLabWorkspacePage rig_lab_workspace_page",
            "ui_layout::rig_lab_workspace_layout(",
            "ui_layout::rig_lab_dual_setup_button_layout(",
            "ui_layout::trainer_control_group_layout(",
            "ui_layout::trainer_page_button_layout(",
            "ui_layout::trainer_live_page_button_layout(",
            "ui_layout::trainer_summary_layout(",
            "draw_rig_lab_workspace_bar", "START TRAIN", "PAUSE TRAIN",
            "draw_rig_lab_control_drawer", "TRAIN SETUP",
            "RIG ART OFF", "ART EDITOR / RIG ART {}", "CLOSE",
            "AUTOPILOT", "TRAINING EVIDENCE", "RIG INSPECTOR",
            "SESSION", "TRAINING LEVEL {:.0f}%", "STATUS",
            "trainer_quick_action_layout(", "MISSION", "DETAILS",
            "MISSION CONTROLS", "TRAINING CPU BUDGET",
            "if (layout_trainer.width > 0.0f)",
            "if (layout_side.width > 0.0f)",
            "ui_layout::course_reference_marker_visible(",
            "show_pip_challenge_details", "show_challenge_details",
            "RAW TELEMETRY", "LATEST EVALUATION",
            "PHYSICS AUTHORITY", "POLICY + OPTIMIZER",
            "bool live_projection_initialized{}",
            "|| !live_projection_initialized",
            "ui_layout::rig_preview_toolbar_layout(",
            "\"ZOOM OUT\"", "\"FIT\"", "\"ZOOM IN\"",
            "ui_layout::rig_lab_zoom_step(",
            "rig_test_physical_environment",
            "start_physical_rig_test(sim::CourseStage::uneven)",
            "rig_test_terminal.frozen", "physical_pose_signature",
            "RETRY TERMINAL TEST",
            "step_physical_rig_test(dt)",
            "constexpr float authored_authority = 1.0f",
            "TEST / {} / {} / L {} / R {}",
            "DIRECT JOINT INPUT / ASSISTED STABILITY",
            "action[index] = test_input_for_motor(index)",
            "index >= 4 && index < 8",
            "sim::human_left_ankle_action", "LEFT ANKLE", "RIGHT ANKLE",
            "const int motor_rows", "sim::anatomy_action_count) + 1) / 2",
            "editor_weapon_class = sim::WeaponClass::none",
            "retained_guidance_label(",
            "RAW LEARNED",
            "PREVIEW AUTHORITY   RAW POLICY / JOINT OUTPUTS ONLY",
            "const bool optional_guidance_active =",
            "authority.optional_guidance_active()",
            "const std::string authority_path = !optional_guidance_active",
            "ROOT DRIVE NONE / BODY PROJECTORS OFF / COURSE MOTION {}",
            "POSE/CONTACT ASSIST ACTIVE / COURSE MOTION {}",
            "gait_task_indices",
            "1u, 7u, 8u, 9u",
            "DIRECTOR REQUEST QUEUED - {} / {} - PREREQUISITES WILL BE VERIFIED",
            "trainer.set_director_profile(requested_profile)",
            "PREVIOUS", "SELECT", "NEXT",
            "compact_profile_name",
            "DIRECTOR PROFILE REQUEST QUEUED - {} - ASYNC VALIDATION WILL REPORT RESULT",
            "autonomy.director_task_mastery",
            "autonomy.director_task_failures",
            "MASTERY {:.0f}%   FAILURES {}",
            "from replacing this requested species",
            "const bool compact_species", "show_pip_course_annotations",
            "Keep the PIP optically isolated",
            "training_pip_world_box(pip_layout, expanded_footer)",
            "primary_telemetry_display_box(", "TRAINER SNAPSHOT",
            "ui_layout::presentation_surface(width, height)",
            "REPLAY ENDED", "trainer.preview_terminal_cause()",
            "control_layout_scroll", "show_primary_session",
            "const bool focused_pose_view", "show_course_annotations",
            "std::vector<Rect> placed_labels", "label_lane_count = 6u",
            "if (!label_placed)",
            "Modular armor is the exclusive presentation",
            "Graph bones are diagnostic geometry", "rearfoot_brace",
            "art::human_foot_edge_kind", "geometry_pass",
            "point_radii", "second uniform graph pass",
            "canonical 16-byte SandHybrid cells" })
        if (app.find(reference) == std::string::npos)
        {
            std::cerr << "FAILED: topology-bound remade-art renderer contract is missing: "
                      << reference << '\n';
            std::exit(EXIT_FAILURE);
        }
    require(app.find("TAB VIEW  SPACE TRAIN") == std::string::npos
            && app.find("ISOLATED PROOF  WHEEL ZOOM") == std::string::npos,
        "top bar still emits the persistent shortcut microtext wall");
    const std::size_t font_size_begin = app.find(
        "constexpr font::FontSize font_size(");
    const std::size_t font_size_end = app.find("struct Rect", font_size_begin);
    require(font_size_begin != std::string::npos
            && font_size_end != std::string::npos
            && app.find("ui_layout::persistent_text_minimum_scale",
                font_size_begin) < font_size_end,
        "global rendered text can fall below the persistent readable floor");
    require(app.find("\"< TASK\"") == std::string::npos
            && app.find("\"TASK >\"") == std::string::npos
            && app.find("PROFILE {}  >  REQUEST {}") == std::string::npos,
        "Trainer controls still rely on unsupported chevron glyphs");
    require(app.find("Four unique production rigs are exposed.")
                == std::string::npos,
        "Rig Inspector Presets still emits its tiny compatibility paragraph");
    const std::size_t ground_renderer = app.find("void draw_course_ground(");
    const std::size_t open_air_fill = app.find(
        "const Color underground_open_air = rgb(0x09101a)", ground_renderer);
    const std::size_t deformable_cells = app.find(
        "for (int row = first_row; row <= last_row; ++row)", open_air_fill);
    const std::size_t empty_is_open = app.find(
        "material != sandhybrid::Material::empty", deformable_cells);
    const std::size_t atmosphere_is_open = app.find(
        "material != sandhybrid::Material::atmosphere", deformable_cells);
    require(ground_renderer != std::string::npos
            && open_air_fill != std::string::npos
            && deformable_cells != std::string::npos
            && empty_is_open != std::string::npos
            && atmosphere_is_open != std::string::npos
            && ground_renderer < open_air_fill
            && open_air_fill < deformable_cells
            && deformable_cells < empty_is_open
            && empty_is_open < atmosphere_is_open,
        "deformable Empty/Atmosphere cells can reveal the generic brown fallback slab");
    require(app.find("\"SPEED WALK\", \"WALK / RUN\"")
                != std::string::npos
            && app.find("\"SPEED\\nWALK\"") == std::string::npos
            && app.find("\"WALK /\\nRUN\"") == std::string::npos,
        "Trainer gait actions regressed to cramped multiline chips");
    const std::size_t persistent_controls_begin = app.find(
        "\"SESSION\"");
    const std::size_t summary_begin = app.find(
        "if (active_panel_page == LivePanelPage::summary)");
    const std::size_t mission_begin = app.find(
        "else if (active_panel_page == LivePanelPage::mission)",
        summary_begin);
    const std::size_t view_begin = app.find(
        "else if (active_panel_page == LivePanelPage::view)",
        mission_begin);
    const std::size_t totals_begin = app.find(
        "else if (active_panel_page == LivePanelPage::totals)",
        view_begin);
    const std::size_t advanced_heading = app.find(
        "if (!active_raw_telemetry_expanded)", totals_begin);
    const std::size_t trainer_panel_end = app.find(
        "active_content_height =", advanced_heading);
    require(persistent_controls_begin != std::string::npos
            && summary_begin != std::string::npos
            && mission_begin != std::string::npos
            && view_begin != std::string::npos
            && totals_begin != std::string::npos
            && advanced_heading != std::string::npos
            && trainer_panel_end != std::string::npos
            && persistent_controls_begin < summary_begin
            && summary_begin < mission_begin
            && mission_begin < view_begin
            && view_begin < totals_begin
            && totals_begin < advanced_heading
            && advanced_heading < trainer_panel_end,
        "trainer Status/Mission/View/Totals/Advanced source boundaries are missing");
    const std::string_view app_view{ app };
    const std::string_view persistent_controls = app_view.substr(
        persistent_controls_begin, summary_begin - persistent_controls_begin);
    const std::string_view summary_branch = app_view.substr(
        summary_begin, mission_begin - summary_begin);
    const std::string_view mission_branch = app_view.substr(
        mission_begin, view_begin - mission_begin);
    const std::string_view view_branch = app_view.substr(
        view_begin, totals_begin - view_begin);
    const std::string_view totals_branch = app_view.substr(
        totals_begin, advanced_heading - totals_begin);
    const std::string_view advanced_branch = app_view.substr(
        advanced_heading, trainer_panel_end - advanced_heading);
    require(persistent_controls.find("BROWSE {:02}/{:02}")
                == std::string_view::npos
            && persistent_controls.find(
                "MASTERY {:.0f}%  FAILURES {}  FIRST STALL {}")
                == std::string_view::npos
            && persistent_controls.find("format_work_counter(\"UPDATES\"")
                == std::string_view::npos
            && persistent_controls.find("TRAINING SPEED")
                == std::string_view::npos
            && persistent_controls.find("PROFILE:")
                == std::string_view::npos,
        "persistent Trainer controls still emit progressive-detail microtext");
    require(persistent_controls.find(
                "TRAINING LEVEL {:.0f}%")
                != std::string_view::npos,
        "large Session header dropped the current curriculum difficulty");
    require(persistent_controls.find("trainer_quick_action_layout(")
                != std::string_view::npos
            && persistent_controls.find(
                "trainer_live_page_button_layout(")
                != std::string_view::npos
            && persistent_controls.find(
                "\"MISSION\", \"VIEW\", \"DETAILS\"")
                != std::string_view::npos
            && persistent_controls.find("page_index == 2u")
                != std::string_view::npos
            && persistent_controls.find("live_art_enabled ? \"ART ON\"")
                != std::string_view::npos
            && persistent_controls.find("\"MISSION\"")
                != std::string_view::npos
            && persistent_controls.find("\"DETAILS\"")
                != std::string_view::npos,
        "default Live surface lost its independent Art or three-route disclosure hierarchy");
    require(summary_branch.find("TOTAL RIG UPDATES")
                == std::string_view::npos
            && summary_branch.find("LESSON COMPLETION")
                == std::string_view::npos
            && summary_branch.find("telemetry::total_updates_help()")
                == std::string_view::npos
            && summary_branch.find("guidance_summary")
                == std::string_view::npos
            && summary_branch.find("human_status.explanation")
                == std::string_view::npos
            && summary_branch.find("telemetry::latest_test_title(metrics)")
                == std::string_view::npos
            && summary_branch.find(
                "telemetry::latest_test_explanation(metrics, autonomy.stage)")
                == std::string_view::npos
            && summary_branch.find("telemetry::sample_budget_message(progress)")
                == std::string_view::npos,
        "default Summary still emits cumulative, prose, or authority clutter");
    const std::size_t overlay_begin = app.find(
        "const bool details_overlay_page");
    const std::size_t overlay_end = app.find(
        "if (show_primary_telemetry)", overlay_begin);
    require(overlay_begin != std::string::npos
            && overlay_end != std::string::npos
            && overlay_end > overlay_begin,
        "Autopilot overlay ownership branch is missing");
    const std::string_view overlay_branch{ app.data() + overlay_begin,
        overlay_end - overlay_begin };
    require(overlay_branch.find(
                "live_panel_page == LivePanelPage::view")
                != std::string_view::npos
            && overlay_branch.find("LivePanelPage::summary")
                == std::string_view::npos
            && overlay_branch.find("LivePanelPage::mission")
                == std::string_view::npos
            && overlay_branch.find("LivePanelPage::totals")
                == std::string_view::npos
            && overlay_branch.find("LivePanelPage::advanced")
                == std::string_view::npos
            && overlay_branch.find("const bool show_primary_telemetry = rig_lab_snapshot")
                != std::string_view::npos,
        "Summary, Mission, or Details can still duplicate the Autopilot overlays");
    require(app.find("live_default_reference_height_m = 4.0f")
                != std::string::npos
            && app.find(
                "std::max(rig_height, live_default_reference_height_m)")
                != std::string::npos,
        "default Live auto-fit can regress to a distant 4K actor thumbnail");
    for (std::string_view required_summary : {
            "human_status.headline", "evidence", "retained_status",
            "telemetry::stage_goal(autonomy.stage)" })
        require(summary_branch.find(required_summary) != std::string_view::npos,
            "Summary dropped required current status or safety evidence");
    require(mission_branch.find("MISSION CONTROLS")
                != std::string_view::npos
            && mission_branch.find("gait_task_indices")
                != std::string_view::npos
            && mission_branch.find("ASSISTED AUTHORITY")
                != std::string_view::npos
            && mission_branch.find("TASK GRAPH")
                != std::string_view::npos
            && mission_branch.find("PREVIOUS") != std::string_view::npos
            && mission_branch.find("SELECT") != std::string_view::npos
            && mission_branch.find("NEXT") != std::string_view::npos
            && mission_branch.find("compact_profile_name")
                != std::string_view::npos
            && mission_branch.find("MASTERY {:.0f}%   FAILURES {}")
                != std::string_view::npos,
        "Mission page dropped task graph, profile, or mastery controls");
    require(view_branch.find("TRAINING CPU BUDGET") != std::string_view::npos
            && view_branch.find("CAMERA") != std::string_view::npos
            && view_branch.find("UNITS") != std::string_view::npos
            && view_branch.find("trainer_three_choice_layout")
                != std::string_view::npos
            && view_branch.find("ZOOM OUT") != std::string_view::npos
            && view_branch.find("ZOOM IN") != std::string_view::npos
            && view_branch.find("art_toggle_for_mode()")
                == std::string_view::npos
            && view_branch.find("LIVE ART ON") == std::string_view::npos
            && view_branch.find("RIG ART ON") == std::string_view::npos,
        "View page dropped responsive controls or duplicated mode-owned Art");
    require(totals_branch.find("TOTAL RIG UPDATES")
                != std::string_view::npos
            && totals_branch.find("telemetry::total_updates_help()")
                != std::string_view::npos
            && totals_branch.find("const auto totals_line")
                != std::string_view::npos
            && totals_branch.find("persistent_text_minimum_scale")
                != std::string_view::npos,
        "Totals does not expose readable, wrapped cumulative data");
    require(advanced_branch.find("TRAINING HISTORY")
                != std::string_view::npos
            && advanced_branch.find("SHOW RAW TELEMETRY")
                != std::string_view::npos
            && advanced_branch.find("HIDE RAW TELEMETRY")
                != std::string_view::npos
            && advanced_branch.find("PHYSICS")
                != std::string_view::npos
            && advanced_branch.find("CONTACT")
                != std::string_view::npos
            && advanced_branch.find("POLICY")
                != std::string_view::npos
            && advanced_branch.find("OPTIMIZER")
                != std::string_view::npos
            && advanced_branch.find("WEAPON")
                != std::string_view::npos
            && advanced_branch.find("guidance_summary")
                != std::string_view::npos
            && advanced_branch.find("human_status.explanation")
                != std::string_view::npos
            && advanced_branch.find("telemetry::latest_test_title(metrics)")
                != std::string_view::npos
            && advanced_branch.find(
                "telemetry::latest_test_explanation(")
                != std::string_view::npos
            && advanced_branch.find("telemetry::sample_budget_message(progress)")
                != std::string_view::npos
            && advanced_branch.find("BROWSE {:02}/{:02}")
                == std::string_view::npos
            && advanced_branch.find("TRAINING LEVEL {:.0f}%")
                == std::string_view::npos
            && advanced_branch.find("format_work_counter(\"UPDATES\"")
                != std::string_view::npos
            && advanced_branch.find("FAILURES {}")
                != std::string_view::npos
            && advanced_branch.find("INVALID DETAIL {}")
                != std::string_view::npos
            && advanced_branch.find("const auto advanced_line")
                != std::string_view::npos
            && advanced_branch.find("persistent_text_minimum_scale")
                != std::string_view::npos,
        "Advanced lost its concise summary/raw disclosure or required diagnostics");
    require(app.find("rig_trainer_panel_page") != std::string::npos
            && app.find("rig_trainer_scroll_offset") != std::string::npos
            && app.find("rig_raw_telemetry_expanded") != std::string::npos
            && app.find("live_raw_telemetry_expanded") != std::string::npos,
        "Live and Rig Trainer page/scroll/raw disclosure state is not independent");
    const std::size_t drawer_begin = app.find(
        "void draw_rig_lab_control_drawer");
    const std::size_t drawer_end = app.find(
        "void draw_course_ground", drawer_begin);
    require(drawer_begin != std::string::npos
            && drawer_end != std::string::npos
            && app.find("RIG ART ON", drawer_begin) < drawer_end
            && app.find("LIVE ART", drawer_begin) >= drawer_end,
        "Rig Lab drawer exposes Live Art or lost its independent Rig Art switch");
    const std::size_t art_page_begin = app.find(
        "else if (rig_panel_page == RigPanelPage::art)");
    const std::size_t art_page_end = app.find(
        "rig_panel_content_height =", art_page_begin);
    const std::size_t in_context_art = app.find(
        "rig_lab_art_enabled ? \"RIG ART ON\" : \"RIG ART OFF\"",
        art_page_begin);
    const std::size_t hidden_art_guard = app.find(
        "if (!persistent_commands)", art_page_begin);
    require(art_page_begin != std::string::npos
            && art_page_end != std::string::npos
            && in_context_art > art_page_begin
            && in_context_art < art_page_end
            && (hidden_art_guard == std::string::npos
                || hidden_art_guard >= art_page_end),
        "Rig Art toggle is still hidden from an active inspector workspace");
    const std::size_t torso_layer = app.find(
        "if (art_enabled_for_mode()", app.find("draw_body_segments(1)"));
    const std::size_t equipment_layer = app.find(
        "draw_held_equipment_layer(2)", torso_layer);
    const std::size_t near_limb_layer = app.find(
        "draw_fitted_armor(2)", equipment_layer);
    require(torso_layer != std::string::npos
            && equipment_layer > torso_layer
            && near_limb_layer > equipment_layer,
        "held equipment is not painted between the torso and near authored limbs");
    const std::size_t scrollbar_begin = app.find(
        "void draw_panel_scrollbar");
    const std::size_t scrollbar_end = app.find(
        "void draw_top_bar", scrollbar_begin);
    const std::size_t non_scroll_return = app.find(
        "if (!scrollbar.scrollable)", scrollbar_begin);
    require(scrollbar_begin != std::string::npos
            && scrollbar_end != std::string::npos
            && non_scroll_return > scrollbar_begin
            && non_scroll_return < scrollbar_end,
        "non-scrollable panels can still draw decorative full-height rails");
    for (std::string_view expanded_label : {
            "MIRROR VERTICAL", "MIRROR HORIZONTAL", "LAYER -",
            "LAYER +", "RESET MODULE", "SHOW SKELETON",
            "HIDE SKELETON" })
        require(app.find(expanded_label) != std::string::npos,
            "Rig Art lost a descriptive production control label");
    require(app.find("std::abs(raw_feet) < 0.5f")
                != std::string::npos
            && app.find("std::abs(meters) < 0.05f")
                != std::string::npos,
        "distance telemetry can still display a distracting negative zero");
    const std::size_t pip_annotation_begin = app.find(
        "const bool show_pip_challenge_details");
    const std::size_t pip_annotation_end = app.find(
        "if (show_pip_challenge_details)", pip_annotation_begin);
    const std::size_t world_annotation_begin = app.find(
        "const bool show_challenge_details");
    const std::size_t world_annotation_end = app.find(
        "if (show_challenge_details)", world_annotation_begin);
    require(pip_annotation_begin != std::string::npos
            && pip_annotation_end != std::string::npos
            && world_annotation_begin != std::string::npos
            && world_annotation_end != std::string::npos
            && app.find("course_eye_test", pip_annotation_begin)
                >= pip_annotation_end
            && app.find("course_eye_test", world_annotation_begin)
                >= world_annotation_end
            && app.find("course_eye_focus_span_meters")
                != std::string::npos
            && app.find("view_camera::course_eye_camera_center(")
                != std::string::npos,
        "default Course Eye can reopen floating diagnostic tags or whole-map miniaturization");
    const std::size_t install_begin = app.find(
        "bool install_canonical_rig_preview");
    const std::size_t commit_begin = app.find(
        "bool commit_canonical_rig", install_begin);
    const std::size_t use_preset_begin = app.find(
        "void use_preset", commit_begin);
    const std::size_t use_preset_end = app.find(
        "void apply_small_rig_change", use_preset_begin);
    const std::size_t invalidate_preview = app.find(
        "invalidate_rig_preview_snapshots()", install_begin);
    const std::size_t immediate_preview = app.find(
        "trainer.preview_blueprint(candidate)", install_begin);
    const std::size_t local_commit = app.find(
        "blueprint = std::move(candidate)", install_begin);
    const std::size_t queued_commit = app.find(
        "trainer.set_blueprint(blueprint, preserve_policy)", commit_begin);
    const std::size_t preset_commit = app.find(
        "commit_canonical_rig(std::move(candidate), false)", use_preset_begin);
    require(install_begin != std::string::npos
            && commit_begin != std::string::npos
            && use_preset_begin != std::string::npos
            && use_preset_end != std::string::npos
            && invalidate_preview > install_begin
            && invalidate_preview < immediate_preview
            && immediate_preview < local_commit
            && local_commit < commit_begin
            && queued_commit > commit_begin
            && queued_commit < use_preset_begin
            && preset_commit > use_preset_begin
            && preset_commit < use_preset_end,
        "canonical preview commit is not synchronous, centralized, and stale-snapshot safe");
    require(app.find("draw_pixel_art(canvas, optional_foot_art")
                == std::string::npos
            && app.find("if (!optional_art_enabled || !rig.paired_leg_chains())")
                == std::string::npos
            && app.find("draw_segment_art") == std::string::npos
            && app.find("rig.motors[4]") == std::string::npos
            && app.find("neutral connected wrap") == std::string::npos
            && app.find("pelvis_center") == std::string::npos
            && app.find("rgb(0x202a31") == std::string::npos
            && app.find("rgb(0xaeb9c1") == std::string::npos
            && app.find("if (optional_art_enabled && optional_foot_art.loaded())")
                == std::string::npos
            && app.find("const float rear = carried ? 0.46f")
                == std::string::npos
            && app.find("anchor - Vec2{ weapon_length * 0.12f")
                == std::string::npos
            && app.find("TEACHER 0 / REFLEX ASSISTED")
                == std::string::npos
            && app.find("AMBER AUTHORED REST") == std::string::npos
            && app.find("rgb(0x835927, 0.30f)") == std::string::npos
            && app.find("preview_screen(") == std::string::npos
            && app.find("FAINT COMMAND OVERLAY") == std::string::npos
            && app.find("std::optional<sim::Environment>{ trainer.preview() }")
                == std::string::npos
            && app.find("trainer.set_gait_task(") == std::string::npos
            && app.find("art::facing_presented_position") == std::string::npos,
        "axis-aligned boot, raw terrain-cell visual, generated underwrap, bone-strip art, motor-slot, or paired-biped gate remains");
    require(app.find("draw_oriented_envelope") == std::string::npos
            && app.find("manipulator_outline_padding") == std::string::npos,
        "synthetic rectangular limb/foot envelope returned around authored pixels");
    const std::size_t editor_world_begin = app.find(
        "void draw_art_editor_world");
    const std::size_t blueprint_begin = app.find(
        "void draw_blueprint", editor_world_begin);
    require(editor_world_begin != std::string::npos
            && blueprint_begin != std::string::npos
            && app.substr(editor_world_begin,
                blueprint_begin - editor_world_begin).find(
                    "draw_terrain_challenges") == std::string::npos
            && app.substr(editor_world_begin,
                blueprint_begin - editor_world_begin).find(
                    "draw_course_reference") == std::string::npos,
        "Rig Lab reintroduced course placards or distance tethers over physical edit anchors");
    const std::size_t live_world_begin = app.find("void draw_live_world");
    const std::size_t joint_lab_begin = app.find(
        "void draw_joint_lab", live_world_begin);
    require(live_world_begin != std::string::npos
            && joint_lab_begin != std::string::npos,
        "retained replay presentation source boundary is missing");
    const std::string live_world = app.substr(live_world_begin,
        joint_lab_begin - live_world_begin);
    require(live_world.find("presentation::replay_may_advance")
                != std::string::npos
            && live_world.find("walk_eye_terminal.frozen = true")
                != std::string::npos
            && live_world.find("result.terminal_cause") != std::string::npos
            && live_world.find("result.invalid_reason") != std::string::npos
            && live_world.find("EXPLICIT RETRY REQUIRED") != std::string::npos
            && live_world.find("replay = *walk_eye_replay_start")
                == std::string::npos
            && live_world.find("reset_particles") == std::string::npos,
        "Walk proof can silently reset or discard its exact terminal presentation state");
    require(app.find("RETRY RETAINED START") != std::string::npos
            && app.find("TERMINAL FROZEN / RETRY REQUIRED")
                != std::string::npos
            && app.find("retry_walk_eye_replay(guidance)")
                != std::string::npos,
        "Walk proof does not expose an explicit, labelled retained-start retry");
    const std::string_view shared_rig_snapshot{
        ": rig_lab_snapshot ? rig_preview_environment()" };
    const std::size_t shared_snapshot_first = app.find(shared_rig_snapshot);
    const std::size_t shared_snapshot_second = app.find(shared_rig_snapshot,
        shared_snapshot_first == std::string::npos ? 0u
            : shared_snapshot_first + shared_rig_snapshot.size());
    require(shared_snapshot_first != std::string::npos
            && shared_snapshot_second != std::string::npos
            && app.find("RIG LAB PARITY")
                != std::string::npos
            && app.find("rig_preview_title_box")
                != std::string::npos
            && app.find("TEST / ASSISTED PHYSICAL SCRUB FROZEN")
                != std::string::npos,
        "Rig Lab live pane, PIP, and editor do not share one labelled physical snapshot");
    const auto pip_begin = app.find("void draw_training_pip(");
    require(app.find("\"HOLD START\"") != std::string::npos
            && app.find("restart_live_replay(true)") != std::string::npos
            && app.find("REPLAY / START POSE HELD") != std::string::npos
            && app.find("training_preview_is_rollout_sample()") != std::string::npos,
        "Live lacks independent restart/hold or in-rollout sampling");
    require(app.find("impl_->trainer.load_autosave(message, true)") != std::string::npos
            && app.find("AUTOPILOT RUNNING (ECO)") != std::string::npos
            && app.find("live_preview_retry") == std::string::npos
            && app.find("R FOR NEW REPLAY") != std::string::npos
            && app.find("TRAINING CPU BUDGET") != std::string::npos,
        "interactive autopilot and manually reset replay are not independent");
    const auto pip_end = app.find("void draw_visual_session_panel(", pip_begin);
    require(pip_begin != std::string::npos && pip_end != std::string::npos,
        "live inset render boundary is missing");
    const auto live_pip = app.substr(pip_begin, pip_end - pip_begin);
    require(live_pip.find(": trainer.training_preview();") != std::string::npos
            && live_pip.find("trainer.has_training_preview()") != std::string::npos
            && live_pip.find("training_status.training_preview_terminal") != std::string::npos
            && live_pip.find("training_status.training_preview_cause") != std::string::npos
            && live_pip.find("TRAINER SNAPSHOT") != std::string::npos
            && live_pip.find("AWAITING TRAINING SAMPLE") != std::string::npos
            && live_pip.find(": trainer.preview();") == std::string::npos
            && live_pip.find("trainer.preview_terminal()") == std::string::npos,
        "Live inset must show worker snapshots, never a copy of the main replay");
    const std::size_t rig_frame_begin = app.find(
        "// Advance the canonical physical Test Environment before");
    const std::size_t trainer_panel_input = app.find(
        "draw_live_panel(trainer_panel, panel_input, !layout.compact)",
        rig_frame_begin);
    const std::size_t editor_panel_input = app.find(
        "draw_rig_panel(side, panel_input, layout.compact)", rig_frame_begin);
    const std::size_t rig_live_draw = app.find(
        "draw_live_world(live, dt, input,", rig_frame_begin);
    require(rig_frame_begin != std::string::npos
            && rig_live_draw != std::string::npos
            && app.find("step_physical_rig_test(dt)", rig_frame_begin)
                < rig_live_draw,
        "Rig Lab panes can render different fixed-step physical poses in one frame");
    const std::size_t frame_envelope_fit = app.find(
        "rig_frame_projection = fit_rig_preview_projection(world)",
        rig_frame_begin);
    const std::size_t rig_editor_draw = app.find(
        "draw_art_editor_world(world)", rig_frame_begin);
    require(trainer_panel_input != std::string::npos
            && editor_panel_input != std::string::npos
            && frame_envelope_fit != std::string::npos
            && rig_editor_draw != std::string::npos
            && trainer_panel_input < frame_envelope_fit
            && editor_panel_input < frame_envelope_fit
            && frame_envelope_fit < rig_live_draw
            && rig_live_draw < rig_editor_draw
            && app.find("optional_art_enabled = true", frame_envelope_fit)
                == std::string::npos,
        "Rig Lab input can invalidate the shared preview after projection or split same-frame panes");
    require(main_source.find("SDL_GetMouseState(&mouse_x, &mouse_y)")
                != std::string::npos
            && main_source.find("SDL_GetWindowSize(window, &logical_width, &logical_height)")
                != std::string::npos
            && main_source.find("SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height)")
                != std::string::npos
            && main_source.find("application.frame(input, dt, logical_width, logical_height)")
                != std::string::npos
            && main_source.find("normalized_wheel_delta") != std::string::npos
            && main_source.find("SDL_MOUSEWHEEL_FLIPPED") != std::string::npos,
        "high-DPI logical pointer coordinates or normalized wheel routing regressed");
    require(app.find("!rig_lab_snapshot && contains(viewport, input.mouse)")
                != std::string::npos
            && app.find("contains(world, input.mouse)") != std::string::npos
            && app.find("art_editor_zoom = ui_layout::rig_lab_default_zoom")
                != std::string::npos,
        "live/editor wheel ownership or explicit Rig Lab zoom reset is missing");
    const std::size_t physical_test_step_begin = app.find(
        "void step_physical_rig_test");
    const std::size_t physical_test_step_end = app.find(
        "bool has_direct_bone", physical_test_step_begin);
    require(physical_test_step_begin != std::string::npos
            && physical_test_step_end != std::string::npos
            && app.substr(physical_test_step_begin,
                physical_test_step_end - physical_test_step_begin).find(
                    "rig_test_terminal.frozen = true") != std::string::npos
            && app.substr(physical_test_step_begin,
                physical_test_step_end - physical_test_step_begin).find(
                    "start_physical_rig_test(") == std::string::npos,
        "physical Rig Lab Test can still recycle instead of freezing terminal evidence");
    require(live_world.find("render_pixels_per_meter = shared_rig_scale")
                != std::string::npos
            && live_world.find("render_camera_x = shared_rig_camera")
                != std::string::npos
            && live_world.find("\n                camera_x = shared_rig_camera")
                == std::string::npos
            && live_world.find(
                "\n                live_pixels_per_meter = shared_rig_scale")
                == std::string::npos,
        "borrowed Rig Lab projection can still overwrite Live camera state");
    require(app.find("ui_layout::DistanceUnits distance_units{ ui_layout::DistanceUnits::imperial };")
                != std::string::npos,
        "fresh application presentation does not default to imperial units");
    require(app.find("preset(0, 0, \"HUMAN\"") != std::string::npos
            && app.find("preset(0, 1, \"CHICKEN\"") != std::string::npos
            && app.find("preset(1, 0, \"DOG\"") != std::string::npos
            && app.find("preset(1, 1, \"HEXAPOD\"") != std::string::npos
            && app.find("preset(1, 0, \"SCAFFOLD\"") == std::string::npos
            && app.find("Four unique production rigs are exposed")
                == std::string::npos,
        "production preset catalog is not the unique uncluttered Human/Chicken/Dog/Hexapod set");
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
        constexpr float heel_to_toe = 40.0f;
        const runner::art::BootArtDimensions canonical =
            runner::art::articulated_boot_dimensions(
                heel_to_toe, 100.0f, 6.0f);
        const runner::art::BootArtDimensions short_parent_hint =
            runner::art::articulated_boot_dimensions(
                heel_to_toe, 12.0f, 6.0f);
        const runner::art::BootArtDimensions malformed =
            runner::art::articulated_boot_dimensions(
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity(), -1.0f);
        require(std::abs(canonical.width - heel_to_toe) <= 1.0e-5f
                && canonical.height >= heel_to_toe * 0.68f
                && canonical.height <= heel_to_toe * 0.70f,
            "canonical boot opaque span extends beyond the physical heel/toe plate");
        require(std::abs(short_parent_hint.width - heel_to_toe) <= 1.0e-5f
                && short_parent_hint.height >= heel_to_toe * 0.35f,
            "ankle-to-heel parent hint crushed the physical sole");
        require(malformed.width == 0.0f && malformed.height == 0.0f,
            "malformed boot spans escaped deterministic empty bounds");

        const runner::art::OrientedArtTransform fitted =
            runner::art::articulated_boot_transform(
                { 0.0f, 0.0f }, { heel_to_toe, 0.0f },
                canonical.width, canonical.height);
        const float sole_offset = canonical.height
            * (runner::art::human_boot_opaque_sole_fraction - 0.5f
                - runner::art::human_boot_contact_depth_fraction);
        require(std::abs(fitted.beginning.x) <= 1.0e-5f
                && std::abs(fitted.ending.x - heel_to_toe) <= 1.0e-5f
                && std::abs(fitted.beginning.y + sole_offset) <= 1.0e-5f
                && std::abs(fitted.ending.y + sole_offset) <= 1.0e-5f
                && std::abs((fitted.beginning.x + fitted.ending.x) * 0.5f
                    - heel_to_toe * 0.5f) <= 1.0e-5f,
            "boot art no longer fits its opaque sole to the physical heel/toe plate");

        constexpr runner::Vec2 rear_heel{ 18.0f, -3.0f };
        constexpr runner::Vec2 rear_toe{ -14.0f, 5.0f };
        const float rear_span = runner::length(rear_toe - rear_heel);
        const runner::art::OrientedArtTransform rear =
            runner::art::articulated_boot_transform(
                rear_heel, rear_toe, rear_span, 14.0f, 1.0f);
        const runner::art::OrientedArtTransform rear_repeated =
            runner::art::articulated_boot_transform(
                rear_heel, rear_toe, rear_span, 14.0f, 1.0f);
        const runner::Vec2 rear_axis = runner::normalized(
            rear_toe - rear_heel, { 1.0f, 0.0f });
        const runner::Vec2 rear_presented_normal{
            -rear_axis.y, rear_axis.x };
        const runner::Vec2 reconstructed_heel = rear.beginning
            + rear_presented_normal
                * (14.0f
                    * (runner::art::human_boot_opaque_sole_fraction - 0.5f
                        - runner::art::human_boot_contact_depth_fraction));
        const runner::Vec2 reconstructed_toe = rear.ending
            + rear_presented_normal
                * (14.0f
                    * (runner::art::human_boot_opaque_sole_fraction - 0.5f
                        - runner::art::human_boot_contact_depth_fraction));
        require(runner::length(reconstructed_heel - rear_heel) <= 1.0e-5f
                && runner::length(reconstructed_toe - rear_toe) <= 1.0e-5f
                && runner::dot(rear.ending - rear.beginning,
                    rear_toe - rear_heel) > 0.0f,
            "rear-foot presentation reordered the physical heel and toe to match body facing");
        require(runner::length(rear.beginning - rear_repeated.beginning)
                    <= 1.0e-7f
                && runner::length(rear.ending - rear_repeated.ending)
                    <= 1.0e-7f
                && rear.thickness == rear_repeated.thickness,
            "repeated rear-foot transform changed physical bone ordering");
        const runner::art::OrientedArtTransform invalid_transform =
            runner::art::articulated_boot_transform(
                { std::numeric_limits<float>::quiet_NaN(), 0.0f },
                rear_toe, rear_span, 14.0f, 1.0f);
        require(invalid_transform.thickness == 0.0f
                && invalid_transform.beginning.x == 0.0f
                && invalid_transform.beginning.y == 0.0f
                && invalid_transform.ending.x == 0.0f
                && invalid_transform.ending.y == 0.0f,
            "malformed physical foot telemetry manufactured visible boot art");
    }

    {
        const runner::art::HandArtDimensions normal =
            runner::art::hand_art_dimensions(40.0f, 43, 55);
        const runner::art::HandArtDimensions compact =
            runner::art::hand_art_dimensions(2.0f, 1, 1000);
        const runner::art::HandArtDimensions large =
            runner::art::hand_art_dimensions(500.0f, 1000, 1);
        const runner::art::HandArtDimensions invalid =
            runner::art::hand_art_dimensions(
                std::numeric_limits<float>::quiet_NaN(), 0, 0);
        const runner::art::HandArtDimensions fitted =
            runner::art::hand_art_dimensions(34.0f, 40, 28, 1.0f, 64.0f);
        const runner::art::HandArtDimensions fitted_repeated =
            runner::art::hand_art_dimensions(34.0f, 40, 28, 1.0f, 64.0f);
        require(normal.thickness <= 25.0f && normal.length <= 29.0f
                && normal.length >= normal.thickness
                && normal.wrist_overlap >= 3.0f
                && normal.wrist_overlap <= normal.length * 0.18f,
            "fitted side-view hand is oversized or disconnected at the wrist");
        for (const runner::art::HandArtDimensions hand :
            { compact, large, invalid })
            require(std::isfinite(hand.length) && std::isfinite(hand.thickness)
                    && std::isfinite(hand.wrist_overlap)
                    && hand.length >= 17.0f && hand.length <= 38.0f
                    && hand.thickness >= 10.0f && hand.thickness <= 30.0f,
                "hand fit escaped compact/large/malformed bounds");
        const float visible_hand_thickness = fitted.thickness
            * runner::art::human_hand_opaque_transverse_fraction;
        require(fitted.thickness >= 64.0f * 0.75f
                && fitted.thickness <= 64.0f * 0.76f + 1.0e-6f
                && visible_hand_thickness >= 64.0f * 0.35f
                && visible_hand_thickness <= 64.0f * 0.36f
                && fitted.length <= 64.0f * 0.38f + 1.0e-6f
                && fitted.wrist_overlap >= fitted.length * 0.27f,
            "bone-fitted irregular glove is still a tiny hook or escaped the forearm envelope");
        require(fitted.length == fitted_repeated.length
                && fitted.thickness == fitted_repeated.thickness
                && fitted.wrist_overlap == fitted_repeated.wrist_overlap,
            "repeated bone-fitted hand dimensions are not deterministic");
        require(!runner::art::presented_limb_transverse_mirror(1.0f)
                && runner::art::presented_limb_transverse_mirror(-1.0f),
            "limb art does not mirror exactly once with whole-rig facing");
        require(runner::art::human_arm_layer_opacity(true)
                    > runner::art::human_arm_layer_opacity(false)
                && runner::art::human_arm_layer_opacity(false) >= 0.80f
                && runner::art::human_limb_layer_opacity(false)
                    > runner::art::human_arm_layer_opacity(false),
            "near/far hands are not depth-separated without ghosting the rear leg");
        const float upper_arm_ratio =
            runner::art::human_limb_thickness_ratio(false, true);
        const float forearm_ratio =
            runner::art::human_limb_thickness_ratio(false, false);
        const float visible_upper_arm = upper_arm_ratio
            * runner::art::human_upper_arm_opaque_transverse_fraction;
        const float visible_forearm = forearm_ratio
            * runner::art::human_forearm_opaque_transverse_fraction;
        require(upper_arm_ratio > forearm_ratio
                && visible_upper_arm >= 0.56f
                && visible_upper_arm <= 0.57f
                && visible_forearm >= 0.49f
                && visible_forearm <= 0.50f
                && visible_upper_arm - visible_forearm >= 0.069f
                && visible_upper_arm - visible_forearm <= 0.071f,
            "Human arm sources lost their substantial upper-arm to cuff taper");
        const runner::art::OrientedArtTransform left_boot =
            runner::art::support_boot_transform({ 0.0f, 10.0f },
                { 0.0f, 0.0f }, 20.0f, 8.0f, -1.0f);
        require(left_boot.ending.x > left_boot.beginning.x,
            "left-facing vertical boot did not reverse its complete presentation");
        const runner::art::OrientedArtTransform diagonal_right =
            runner::art::support_boot_transform({ -4.0f, 7.0f },
                { 2.0f, 1.0f }, 22.0f, 9.0f, 1.0f);
        const runner::art::OrientedArtTransform diagonal_left =
            runner::art::support_boot_transform({ -4.0f, 7.0f },
                { 2.0f, 1.0f }, 22.0f, 9.0f, -1.0f);
        const runner::Vec2 right_axis = diagonal_right.ending
            - diagonal_right.beginning;
        const runner::Vec2 left_axis = diagonal_left.ending
            - diagonal_left.beginning;
        require(runner::dot(right_axis, left_axis) < -400.0f,
            "arbitrarily rotated boot did not reverse along signed facing");
    }
    {
        constexpr runner::Vec2 grip{ 17.0f, -9.0f };
        const auto reconstructed_source_grip = [](const runner::art::OrientedArtTransform& transform,
            float grip_fraction, bool mirror_vertical)
        {
            const runner::Vec2 delta = transform.ending - transform.beginning;
            const runner::Vec2 axis = runner::normalized(delta, { 1.0f, 0.0f });
            runner::Vec2 normal{ -axis.y, axis.x };
            if (mirror_vertical)
                normal = normal * -1.0f;
            return transform.beginning
                + delta * grip_fraction
                + normal * ((runner::art::equipment_primary_grip_normal_fraction
                    - 0.5f) * transform.thickness);
        };
        const runner::art::OrientedArtTransform weapon =
            runner::art::grip_anchored_art_transform(
                grip, { 3.0f, 4.0f }, 80.0f, 18.0f);
        const runner::Vec2 reconstructed_grip = reconstructed_source_grip(
            weapon, runner::art::equipment_primary_grip_fraction, false);
        require(runner::length(reconstructed_grip - grip) <= 1.0e-5f
                && std::abs(runner::length(weapon.ending - weapon.beginning)
                    - 80.0f) <= 1.0e-5f
                && std::abs(weapon.thickness - 18.0f) <= 1.0e-5f,
            "weapon art no longer keeps its supplied opaque grip on the physical hand");

        const runner::art::OrientedArtTransform mirrored =
            runner::art::grip_anchored_art_transform(
                grip, { -3.0f, -4.0f }, 80.0f, 18.0f,
                runner::art::equipment_primary_grip_fraction,
                runner::art::equipment_primary_grip_normal_fraction, true);
        const runner::Vec2 mirrored_grip = reconstructed_source_grip(
            mirrored, runner::art::equipment_primary_grip_fraction, true);
        require(runner::length(mirrored_grip - grip) <= 1.0e-5f
                && runner::dot(weapon.ending - weapon.beginning,
                    mirrored.ending - mirrored.beginning) < 0.0f,
            "mirrored equipment direction detached the source grip from the hand");

        const runner::art::OrientedArtTransform invalid =
            runner::art::grip_anchored_art_transform(
                grip,
                { std::numeric_limits<float>::quiet_NaN(), 0.0f },
                std::numeric_limits<float>::infinity(), -4.0f,
                std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN(), true);
        require(std::isfinite(invalid.beginning.x)
                && std::isfinite(invalid.beginning.y)
                && std::isfinite(invalid.ending.x)
                && std::isfinite(invalid.ending.y)
                && invalid.thickness == 0.0f,
            "malformed equipment art anchors escaped deterministic finite bounds");

        constexpr std::array<runner::sim::WeaponClass, 3u> classes{
            runner::sim::WeaponClass::sidearm,
            runner::sim::WeaponClass::carbine,
            runner::sim::WeaponClass::launcher
        };
        const runner::Vec2 axis = runner::normalized(
            runner::Vec2{ 3.0f, 4.0f });
        constexpr float scale = 100.0f;
        for (const runner::sim::WeaponClass weapon_class : classes)
        {
            const runner::sim::WeaponGeometry geometry =
                runner::sim::weapon_geometry(weapon_class);
            const float grip_fraction = geometry.butt_to_rear_grip
                / geometry.overall_length;
            const runner::art::OrientedArtTransform transform =
                runner::art::grip_anchored_art_transform(
                    grip, axis, geometry.overall_length * scale, 18.0f,
                    grip_fraction,
                    runner::art::equipment_primary_grip_normal_fraction);
            const runner::Vec2 physical_grip = reconstructed_source_grip(
                transform, grip_fraction, false);
            require(runner::length(physical_grip - grip) <= 1.0e-5f
                    && std::abs(runner::dot(transform.beginning - grip, axis)
                        + geometry.butt_to_rear_grip * scale) <= 1.0e-4f
                    && std::abs(runner::dot(transform.ending - grip, axis)
                        - geometry.rear_to_muzzle * scale) <= 1.0e-4f,
                "rendered stock/rear-grip/muzzle landmarks diverged from WeaponGeometry");
        }
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
                "authored compatibility rig has no topology-classified support motor");
            for (std::size_t node = 0; node < rig.nodes.size(); ++node)
            {
                if (rig.is_support_seed(node))
                    require(rig.node_support_mask(node) != 0u,
                        "authored support node lost its renderer role");
            }
        }
        const std::array<runner::sim::CreatureBlueprint, 4> exposed{
            runner::sim::CreatureBlueprint::humanoid(),
            runner::sim::CreatureBlueprint::chicken(),
            runner::sim::CreatureBlueprint::crawler4(),
            runner::sim::CreatureBlueprint::hexapod() };
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
        require(aspect(exposed[2]) > aspect(exposed[0]) + 0.20f
                && aspect(exposed[3]) > aspect(exposed[0]) + 0.20f,
            "Dog or Hexapod lost its low multi-support silhouette");
    }

    const std::string renderer_header = read_text(root / "src" / "renderer.hpp");
    require(main_source.find("--diagnose-art") != std::string::npos
            && main_source.find("--art-eye-test=human") != std::string::npos
            && main_source.find("--art-eye-test=chicken") != std::string::npos
            && main_source.find("--art-eye-test=dog") != std::string::npos
            && main_source.find("--art-eye-test=hexapod") != std::string::npos
            && main_source.find("Unknown art eye-test subject") != std::string::npos
            && main_source.find("prepare_art_diagnostic_rig") != std::string::npos
            && main_source.find("prepare_art_fallen_eye_test") != std::string::npos
            && main_source.find("equipment_vertices") != std::string::npos
            && main_source.find("rig_vertices") != std::string::npos
            && main_source.find("fallen_vertices") != std::string::npos
            && main_source.find("headroom_limit") != std::string::npos
            && main_source.find("RUNNER_V0729_MODULAR_ART_REMAKE.md")
                != std::string::npos
            && renderer_header.find("maximum_frame_vertex_bytes")
                != std::string::npos,
        "modular-art vertex-budget diagnostic is missing");
    require(app.find("case 3u: rig = sim::CreatureBlueprint::hexapod()")
                != std::string::npos
            && app.find("set_diagnostic_rigid_rotation") != std::string::npos
            && app.find("HORIZONTAL ROTATION EVIDENCE") != std::string::npos,
        "all-rig or horizontal-pose Vulkan art diagnostic is missing");
    require(app.find("PACKAGED ORTHOGRAPHIC ART TEST") != std::string::npos
            && app.find("STRICT SIDE ELEVATION - NO PERSPECTIVE OR FORESHORTENING")
                != std::string::npos
            && app.find("FIXED SIDE PROFILE") != std::string::npos,
        "strict side-profile production eye-test contract is missing");

    const std::size_t equipment_eye_begin = app.find(
        "bool Application::prepare_equipment_eye_test");
    const std::size_t equipment_eye_end = app.find(
        "bool Application::prepare_walk_eye_test", equipment_eye_begin);
    require(equipment_eye_begin != std::string::npos
            && equipment_eye_end != std::string::npos
            && equipment_eye_end > equipment_eye_begin,
        "physical equipment eye-test preparation entry point is missing");
    const std::string_view equipment_eye_scope{ app.data() + equipment_eye_begin,
        equipment_eye_end - equipment_eye_begin };
    require(main_source.find("--equipment-eye-test") != std::string::npos
            && main_source.find("parse_equipment_eye_request")
                != std::string::npos
            && main_source.find("prepare_equipment_eye_test(error)")
                != std::string::npos
            && equipment_eye_scope.find("CourseStage::equipment_targets")
                != std::string_view::npos
            && equipment_eye_scope.find("EquipmentDirective::fire_and_correct")
                != std::string_view::npos
            && equipment_eye_scope.find("GuidanceMode::assisted")
                != std::string_view::npos
            && equipment_eye_scope.find("set_course_motion_enabled(false)")
                != std::string_view::npos
            && equipment_eye_scope.find("effective_policy_action")
                != std::string_view::npos
            && equipment_eye_scope.find("equipment_eye_frame_ready")
                != std::string_view::npos
            && equipment_eye_scope.find("equipment_engagement_ready")
                != std::string_view::npos
            && equipment_eye_scope.find("environment.step(action, fixed_step)")
                != std::string_view::npos
            && equipment_eye_scope.find("environment.shots_fired()")
                != std::string_view::npos
            && equipment_eye_scope.find("ASSISTED TEACHER")
                != std::string_view::npos
            && equipment_eye_scope.find("set_diagnostic_")
                == std::string_view::npos,
        "equipment eye-test is not a fail-closed frozen production-physics aim frame");
    require(app.find("PHYSICAL EQUIPMENT / PLANTED AIM")
                != std::string::npos
            && app.find("ASSISTED TEACHER / NOT RAW")
                != std::string::npos
            && app.find("PACKAGED PHYSICAL EQUIPMENT EYE TEST")
                != std::string::npos,
        "equipment eye-test presentation hides physical state or assisted authority");

    const std::string build_contract = read_text(root / "CMakeLists.txt");
    const std::string capture_header = read_text(
        root / "src" / "software_frame_capture.hpp");
    const std::string capture_tool = read_text(
        root / "tools" / "visual_capture.cpp");
    require(build_contract.find("RunnerVisualCapture EXCLUDE_FROM_ALL")
                != std::string::npos
            && build_contract.find("add_custom_target(RunnerVisualCaptures")
                != std::string::npos
            && build_contract.find("RunnerSoftwareFrameCaptureTests")
                != std::string::npos
            && build_contract.find("install(TARGETS RunnerVisualCapture")
                == std::string::npos,
        "production-vertex capture is missing, implicit, or installed");
    for (std::string_view reference : {
            "ui_layout::default_window_width",
            "ui_layout::default_window_height", "application.vertices()",
            "StartupMode::isolated_visual_proof", "prepare_course_eye_test()",
            "prepare_art_diagnostic_rig", "prepare_equipment_eye_test",
            "equipment-assisted-ready", "prepare_rig_lab_page",
            "application.rig_preview_signature()",
            "canonical_rig_signature(species_index)",
            "Rig Lab request/render species mismatch",
            "RigLabStartupPage::presets", "RigLabStartupPage::structure",
            "RigLabStartupPage::motors", "RigLabStartupPage::art",
            "RigLabStartupPage::test", "\"human\"", "\"chicken\"",
            "\"dog\"", "\"hexapod\"", "toggle.art_pressed = true",
            "zoom.wheel = 1.0f", "validate_zoom_locality",
            "presentation_settle_steps", "settle_presentation(application)",
            "trainer_indicator_changed", "controls_indicator_changed",
            "retained_live_changed", "classified_changed_pixels",
            "maximum_zoom_indicator_changed_pixels",
            "materially_zoomed_subject", "matching_subject_bounds",
            "compact_pip_evidence", "width_fraction < 0.16f",
            "height_fraction < 0.34f", "ground_contact_margin",
            "Measure the clipped production-world viewport",
            "training_pip_world_box(pip)",
            "const bool expects_snapshot = page_index == 1u",
            "pip_subject=",
            "height_fraction < 0.30f", "clear_margin",
            "riglab-test-human-opposing-phase", "opposing_phase_steps",
            "application.rig_preview_contact_evidence()",
            "contact.opposing_planted_sole()",
            "physical_test_generation != initial_generation",
            "terminal recycle cannot produce",
            "capture_species_physical_contact_phase",
            "contact.has_physical_support()",
            "riglab-test-{}-contact-phase",
            "subject_viewport_fraction", "manifest.txt",
            "visual-qa-only-not-native-vulkan-not-raw-learned-proof" })
        require(capture_tool.find(reference) != std::string::npos,
            "visual-capture matrix or evidence boundary is incomplete");
    require(capture_tool.find(
                "constexpr std::size_t opposing_phase_steps = 31u")
                == std::string::npos
            && app.find("++rig_test_physical_generation")
                != std::string::npos,
        "opposing contact evidence still guesses a clock phase or cannot detect recycling");
    for (std::string_view reference : {
            "production_clear_color", "top_left_edge", "accepted_edge",
            "sample_x", "sample_y", "source_over", "linear_to_srgb",
            "ppm_bytes", "fnv1a64", "DifferenceStats", "analyze_region" })
        require(capture_header.find(reference) != std::string::npos,
            "software capture lost its deterministic production-raster contract");

    const std::string simulation = read_text(root / "src" / "simulation.cpp");
    require(simulation.find("runner_armor_concepts") == std::string::npos
            && simulation.find("PixelArt") == std::string::npos,
        "optional art leaked into simulation or training state");
    const std::string art_authoring = read_text(
        root / "src" / "art_authoring.hpp");
    require(art_authoring.find("persistence::atomic_replace_file")
                != std::string::npos
            && art_authoring.find("std::filesystem::remove(path")
                == std::string::npos,
        "art layout save can delete accepted state before replacement succeeds");

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
    require(parallel.find("configure_policy_evaluation_environment(environment,")
                != std::string::npos
            && parallel.find("current_gait_task, current_guidance_mode,")
                != std::string::npos
            && parallel.find("current_physical_guidance_authority);")
                != std::string::npos
            && parallel.find("state.guidance_mode = course_stage_")
                != std::string::npos
            && parallel.find("? sim::GuidanceMode::raw_policy_audit")
                != std::string::npos
            && parallel.find("foundational_policy_observation(")
                != std::string::npos
            && parallel.find("const auto action = current_guidance_mode")
                != std::string::npos
            && parallel.find("? raw_action") != std::string::npos
            && parallel.find(": effective_policy_action(environment,")
                != std::string::npos
            && parallel.find("current_guidance_mode);") != std::string::npos
            && parallel.find("const auto action = raw_action;")
                == std::string::npos,
        "mastery evaluation lost its selected gait, scoped foundational controller, or explicit Assisted branch");

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
    require(app.find("FIT 1.00X") == std::string::npos,
        "Rig Lab still labels every editor zoom level as the fixed 1.00x fit target");

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
