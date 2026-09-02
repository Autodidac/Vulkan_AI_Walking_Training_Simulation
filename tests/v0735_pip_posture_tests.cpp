#include "pixel_art.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include "training_explainer.hpp"

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

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static void qualify_walk(Environment& environment,
            float maximum_backward_brace_seconds) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.elapsed_seconds_ = 5.0f;
            environment.stable_stance_seconds_ = 0.0f;
            environment.longest_stable_stance_seconds_ = 0.20f;
            environment.alternating_steps_ = 8u;
            environment.limb_crossings_ = 4u;
            environment.distance_travelled_ = 4.0f;
            environment.maximum_lower_leg_scissor_seconds_ = 0.0f;
            environment.maximum_backward_brace_seconds_ =
                maximum_backward_brace_seconds;
        }
    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.35 PIP/posture failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    bool close(float left, float right, float tolerance = 1.0e-4f) noexcept
    {
        return std::abs(left - right) <= tolerance;
    }

    std::string read_text(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        require(static_cast<bool>(input), "required source text could not be opened");
        return { std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>() };
    }

    float accumulated_seconds(int cadence, float duration)
    {
        const float dt = 1.0f / static_cast<float>(cadence);
        float seconds = 0.0f;
        for (int frame = 0; frame < static_cast<int>(duration * static_cast<float>(cadence)); ++frame)
            seconds = runner::sim::contiguous_condition_seconds(true, seconds, dt);
        return seconds;
    }
}

int main()
{
    namespace art = runner::art;
    namespace rl = runner::rl;
    namespace sim = runner::sim;
    namespace telemetry = runner::telemetry;

    const float pip_scale = art::presentation_pixel_scale(24.0f);
    const float main_scale = art::presentation_pixel_scale(42.0f);
    const float zoom_scale = art::presentation_pixel_scale(84.0f);
    require(close(main_scale, 1.0f) && close(pip_scale, 24.0f / 42.0f)
            && close(zoom_scale, 2.0f),
        "pixels-per-meter presentation scale changed its reference contract");

    const art::HandArtDimensions main_hand =
        art::hand_art_dimensions(32.0f, 40, 28, main_scale);
    const art::HandArtDimensions pip_hand = art::hand_art_dimensions(
        32.0f * pip_scale, 40, 28, pip_scale);
    const art::HandArtDimensions zoom_hand = art::hand_art_dimensions(
        32.0f * zoom_scale, 40, 28, zoom_scale);
    require(close(main_hand.length, pip_hand.length / pip_scale)
            && close(main_hand.thickness, pip_hand.thickness / pip_scale)
            && close(main_hand.length, zoom_hand.length / zoom_scale)
            && close(main_hand.thickness, zoom_hand.thickness / zoom_scale),
        "hand armor changes body proportion between PIP and main view");

    const art::SkinEnvelopeDimensions main_skin =
        art::skin_envelope_dimensions(60.0f, 50.0f, main_scale);
    const art::SkinEnvelopeDimensions pip_skin = art::skin_envelope_dimensions(
        60.0f * pip_scale, 50.0f * pip_scale, pip_scale);
    require(close(main_skin.shoulder_width, pip_skin.shoulder_width / pip_scale)
            && close(main_skin.chest_radius, pip_skin.chest_radius / pip_scale)
            && close(main_skin.pelvis_half_width,
                pip_skin.pelvis_half_width / pip_scale)
            && close(main_skin.joint_overlap, pip_skin.joint_overlap / pip_scale)
            && close(art::assembled_armor_scale(60.0f, 50.0f, main_scale),
                art::assembled_armor_scale(60.0f * pip_scale,
                    50.0f * pip_scale, pip_scale)),
        "assembled armor envelope changes silhouette in the PIP");
    const art::HelmetArtDimensions main_helmet =
        art::helmet_art_dimensions(12.0f, 1.30f, main_scale);
    const art::HelmetArtDimensions pip_helmet =
        art::helmet_art_dimensions(12.0f * pip_scale, 1.30f, pip_scale);
    require(close(main_helmet.height, pip_helmet.height / pip_scale)
            && close(main_helmet.downward_offset,
                pip_helmet.downward_offset / pip_scale)
            && main_helmet.height < 46.0f,
        "helmet remains oversized or changes proportion in the PIP");
    require(!art::presented_limb_transverse_mirror(1.0f)
            && art::presented_limb_transverse_mirror(-1.0f),
        "rear branch art is still independently inverted from whole-rig facing");

    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.nodes[9].x < humanoid.nodes[0].x
            && humanoid.nodes[12].x > humanoid.nodes[0].x
            && std::abs(humanoid.nodes[9].x - humanoid.nodes[0].x) < 0.35f
            && std::abs(humanoid.nodes[12].x - humanoid.nodes[0].x) < 0.35f
            && humanoid.nodes[9].y <= humanoid.nodes[0].y
            && humanoid.nodes[12].y <= humanoid.nodes[0].y
            && std::abs(humanoid.nodes[9].y - humanoid.nodes[0].y) < 0.35f
            && std::abs(humanoid.nodes[12].y - humanoid.nodes[0].y) < 0.35f,
        "authored humanoid hands do not begin at the saved relaxed side rest");

    require(close(art::presentation_pixel_scale(
                std::numeric_limits<float>::quiet_NaN()), 1.0f)
            && close(art::scaled_pixels(-1.0f, pip_scale), 0.0f),
        "invalid presentation scale inputs escaped bounded fallback behavior");

    const runner::Vec2 authored{ 0.0f, 1.0f };
    require(close(sim::directional_backward_brace_ratio(
                authored, { 0.28f, 0.96f }, 1.0f), 0.0f)
            && sim::directional_backward_brace_ratio(
                authored, { -0.28f, 0.96f }, 1.0f)
                > sim::backward_brace_activation_ratio,
        "rightward travel does not distinguish forward balance from backward bracing");
    require(close(sim::directional_backward_brace_ratio(
                authored, { -0.28f, 0.96f }, -1.0f), 0.0f)
            && sim::directional_backward_brace_ratio(
                authored, { 0.28f, 0.96f }, -1.0f)
                > sim::backward_brace_activation_ratio,
        "leftward travel did not mirror the backward-brace contract");
    require(close(sim::directional_backward_brace_ratio(
                {}, { 0.28f, 0.96f }, 1.0f), 0.0f)
            && close(sim::directional_backward_brace_ratio(
                authored, { 0.28f, 0.96f }, 0.0f), 0.0f),
        "degenerate posture or direction produced false brace evidence");

    for (const int cadence : std::array{ 20, 60, 240 })
        require(close(accumulated_seconds(cadence, 1.20f), 1.20f, 2.0e-4f),
            "brace duration changed with render/update cadence");
    float interrupted = 0.0f;
    interrupted = sim::contiguous_condition_seconds(true, interrupted, 0.40f);
    interrupted = sim::contiguous_condition_seconds(false, interrupted, 0.10f);
    require(close(interrupted, 0.0f),
        "non-traverse backing/turning gap did not clear contiguous brace time");

    sim::Environment balanced{ sim::CreatureBlueprint::humanoid(), 0x73501u };
    balanced.set_course(sim::CourseStage::uneven, 0.30f);
    sim::EnvironmentTestAccess::qualify_walk(balanced,
        sim::sustained_backward_brace_limit_seconds);
    require(rl::stage_motion_qualification(
            sim::CourseStage::uneven, balanced).valid
            && rl::incremental_locomotion_candidate(
                sim::CourseStage::uneven, balanced),
        "boundary-safe forward gait was rejected");

    sim::Environment braced{ sim::CreatureBlueprint::humanoid(), 0x73502u };
    braced.set_course(sim::CourseStage::uneven, 0.30f);
    sim::EnvironmentTestAccess::qualify_walk(braced,
        sim::sustained_backward_brace_limit_seconds + 0.01f);
    const rl::StageMotionQualification rejected =
        rl::stage_motion_qualification(sim::CourseStage::uneven, braced);
    require(!rejected.valid
            && (rejected.rejection_mask & rl::evidence_bit(
                rl::MotionEvidenceFailure::backward_brace)) != 0u
            && !rl::incremental_locomotion_candidate(
                sim::CourseStage::uneven, braced)
            && rl::primary_motion_rejection_name(rejected.rejection_mask)
                == "TORSO BRACED AGAINST TRAVEL",
        "sustained pushed-looking walk remained retainable");

    rl::TrainingMetrics lineage{};
    lineage.total_updates = 630u;
    lineage.update = 38u;
    require(telemetry::prior_policy_lineage_updates(lineage) == 592u,
        "manual fresh-controller lineage is still reported as unexplained loss");
    lineage.total_updates = 12u;
    lineage.update = 20u;
    require(telemetry::prior_policy_lineage_updates(lineage) == 0u,
        "lineage display underflowed on adversarial imported metrics");

    const std::filesystem::path root{ RUNNER_SOURCE_ROOT };
    const std::string app = read_text(root / "src" / "app.cpp");
    require(app.find("PRIOR LINEAGE") != std::string::npos
            && app.find("DISCARDED") == std::string::npos
            && app.find("presentation_pixel_scale(scale)") != std::string::npos
            && app.find("draw_authored_pixel_art") != std::string::npos
            && app.find("BACKWARD BRACE PREVIEW") != std::string::npos,
        "packaged renderer/diagnostics source lost the v0.7.35 contracts");

    std::cout << "Runner v0.7.35 PIP proportions and posture truth passed\n";
    return EXIT_SUCCESS;
}