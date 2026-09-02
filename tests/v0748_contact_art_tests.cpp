#include "pixel_art.hpp"
#include "ppo.hpp"
#include "rig_training_diagnostic.hpp"
#include "simulation.hpp"
#include "species_art_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#ifndef RUNNER_SOURCE_DIR
#error RUNNER_SOURCE_DIR must be defined
#endif

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        static float displaced_human_plant_error(Environment& environment,
            float dt) noexcept
        {
            environment.set_course(CourseStage::shuttle, 0.30f);
            const std::uint16_t contact = environment.blueprint_.left_contact_node;
            Particle& foot = environment.particles_[contact];
            const float anchor = foot.position.x;
            foot.position.y = environment.ground_height_at(anchor)
                + ground_contact_offset(true, foot.radius);
            foot.previous = foot.position;
            foot.grounded = true;
            environment.support_contact_latch_[contact] = 1u;
            environment.support_contact_anchor_x_[contact] = anchor;

            foot.position.x += 0.14f;
            foot.previous.x = foot.position.x - 0.02f;
            environment.solve_ground(dt);
            return std::abs(foot.position.x - anchor);
        }
    };
}

namespace
{
    void require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Runner v0.7.48 contact/art failure: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }

    struct PpmEvidence
    {
        int width{};
        int height{};
        std::size_t content{};
        int min_x{};
        int min_y{};
        int max_x{};
        int max_y{};
    };

    [[nodiscard]] PpmEvidence inspect_ppm(const std::filesystem::path& path)
    {
        std::ifstream input{ path };
        std::string magic{};
        int maximum{};
        PpmEvidence evidence{};
        input >> magic >> evidence.width >> evidence.height >> maximum;
        require(input.good() && magic == "P3" && maximum == 255,
            "species module is not a readable P3 image");
        evidence.min_x = evidence.width;
        evidence.min_y = evidence.height;
        evidence.max_x = -1;
        evidence.max_y = -1;
        for (int y = 0; y < evidence.height; ++y)
        {
            for (int x = 0; x < evidence.width; ++x)
            {
                int red{};
                int green{};
                int blue{};
                input >> red >> green >> blue;
                require(input.good(), "species module payload is truncated");
                if (red == 255 && green == 0 && blue == 255)
                    continue;
                ++evidence.content;
                evidence.min_x = std::min(evidence.min_x, x);
                evidence.min_y = std::min(evidence.min_y, y);
                evidence.max_x = std::max(evidence.max_x, x);
                evidence.max_y = std::max(evidence.max_y, y);
            }
        }
        return evidence;
    }
}

int main()
{
    using namespace runner;
    {
        const auto seeds = diagnostics::walk_eye_candidate_seeds;
        require(seeds.front() == 0x00C0FFEEu && seeds.size() == 4u,
            "walk-eye candidate set changed outside its release contract");
        for (std::size_t left = 0; left < seeds.size(); ++left)
            for (std::size_t right = left + 1u; right < seeds.size(); ++right)
                require(seeds[left] != seeds[right],
                    "walk-eye candidate search contains a duplicate seed");
    }

    const std::filesystem::path root{ RUNNER_SOURCE_DIR };
    constexpr std::array species{ "chicken", "dog", "hexapod" };
    constexpr std::array parts{
        "body", "head", "tail", "upper_leg", "lower_leg", "foot"
    };
    for (const std::string_view animal : species)
    {
        for (const std::string_view part : parts)
        {
            const PpmEvidence evidence = inspect_ppm(root / "assets" / "optional"
                / "species_runtime" / (std::string{ animal } + "_"
                    + std::string{ part } + "_side.ppm"));
            const bool hex_body = animal == "hexapod" && part == "body";
            require(evidence.content >= static_cast<std::size_t>(
                    evidence.width * evidence.height / 12),
                "normalized species module is visually empty");
            require(evidence.min_y <= 2 && evidence.max_y >= evidence.height - 3
                    && evidence.max_x >= evidence.width - 3
                    && evidence.min_x <= (hex_body ? evidence.width / 6 : 2),
                "source-cell whitespace still shortens a species module");
        }
    }

    const art::SpeciesArtTopology chicken = art::species_art_topology(
        sim::CreatureSpecies::chicken);
    const art::SpeciesArtTopology dog = art::species_art_topology(
        sim::CreatureSpecies::dog);
    const art::SpeciesArtTopology hexapod = art::species_art_topology(
        sim::CreatureSpecies::hexapod);
    require(chicken.body_front == chicken.head_attachment
            && chicken.physical_tail && chicken.body_rear == 0u
            && chicken.tail_tip == 5u,
        "Chicken body/head/tail do not share physical attachment landmarks");
    require(dog.body_front == dog.head_attachment && !dog.physical_tail,
        "Dog head does not begin at the physical shoulder attachment");
    require(hexapod.body_front == hexapod.head_attachment,
        "Hexapod head does not begin at its physical body attachment");

    constexpr float amplitude = 0.08f;
    const float left_strike = rl::human_contralateral_arm_swing_offset(
        0.0f, amplitude, 1.0f);
    const float right_strike = rl::human_contralateral_arm_swing_offset(
        pi, amplitude, 1.0f);
    require(left_strike < 0.0f && right_strike > 0.0f
            && std::abs(left_strike + right_strike) < 1.0e-6f,
        "Human arms are not exactly opposed at contralateral heel strikes");
    require(std::abs(rl::human_contralateral_arm_swing_offset(
                0.0f, amplitude, -1.0f) + left_strike) < 1.0e-6f,
        "Human arm opposition does not mirror with travel direction");
    require(rl::human_contralateral_arm_swing_offset(
                std::numeric_limits<float>::quiet_NaN(), amplitude) == 0.0f,
        "non-finite arm phase produced a gait command");

    const sim::CreatureBlueprint persisted_human =
        sim::CreatureBlueprint::humanoid();
    require(persisted_human.human_paired_limb_topology()
            && persisted_human.nodes.size() == 13u
            && persisted_human.additional_left_contact_nodes.empty()
            && persisted_human.additional_right_contact_nodes.empty(),
        "canonical human.rig no longer preserves the user-authored topology");
    const sim::Environment articulated_human{ persisted_human, 0x748F00u };
    require(articulated_human.blueprint().nodes.size() == 17u
            && articulated_human.blueprint().bones.size() == 21u
            && articulated_human.blueprint().additional_left_contact_nodes.size() == 2u
            && articulated_human.blueprint().additional_right_contact_nodes.size() == 2u
            && articulated_human.blueprint().human_casual_gait_plan()
            && !articulated_human.blueprint().horizontal_multi_support_plan(),
        "Human runtime did not add two connected heel/ball/toe plates");

    std::array<float, sim::anatomy_action_count> first_swing{};
    std::array<float, sim::anatomy_action_count> second_swing{};
    for (std::size_t motor = 0; motor < sim::anatomy_action_count; ++motor)
    {
        first_swing[motor] = sim::rig_test_motor_input(
            sim::RigTestPattern::gait, motor, pi * 0.5f, 0.0f);
        second_swing[motor] = sim::rig_test_motor_input(
            sim::RigTestPattern::gait, motor, pi * 1.5f, 0.0f);
    }
    const int first_side = sim::action_requested_swing_side(
        articulated_human.blueprint(), first_swing);
    const int second_side = sim::action_requested_swing_side(
        articulated_human.blueprint(), second_swing);
    if (first_side == 0 || second_side != -first_side)
        std::cerr << "Action contact evidence: sides=" << first_side << ','
            << second_side << " heights="
            << sim::requested_leg_endpoint_height(articulated_human.blueprint(), true, first_swing)
            << ',' << sim::requested_leg_endpoint_height(articulated_human.blueprint(), false, first_swing)
            << " second=" << sim::requested_leg_endpoint_height(articulated_human.blueprint(), true, second_swing)
            << ',' << sim::requested_leg_endpoint_height(articulated_human.blueprint(), false, second_swing)
            << '\n';
    require(first_side != 0 && second_side == -first_side,
        "opposed motor geometry did not request alternating physical feet");
    std::array<float, sim::anatomy_action_count> neutral_actions{};
    require(sim::action_requested_swing_side(articulated_human.blueprint(),
                neutral_actions) == 0,
        "neutral motor geometry manufactured a timed swing-foot selection");
    require(sim::rearm_support_unload_side(-1, 0, true) == 0
            && sim::rearm_support_unload_side(1, 0, true) == 0,
        "neutral geometry did not rearm a completed physical unload");
    require(sim::rearm_support_unload_side(-1, 0, false) == -1
            && sim::rearm_support_unload_side(1, -1, true) == 1,
        "active release or a continuously requested side bypassed unload latching");
    require(sim::rearm_support_unload_side(-1, 1, true) == -1
            && sim::rearm_support_unload_side(1, -1, true) == 1,
        "opposed policy geometry rewrote history instead of earning its release");

    const art::HandArtDimensions fitted_hand = art::hand_art_dimensions(
        32.0f, 40, 28);
    require(fitted_hand.thickness <= 20.0f && fitted_hand.length <= 29.0f,
        "Human hand still scales larger than its fitted forearm");

    const art::OrientedArtTransform planted = art::support_boot_transform(
        { 0.0f, -1.0f }, { 0.2f, 0.0f }, 32.0f, 18.0f, 1.0f, true);
    require(std::abs(planted.beginning.y - planted.ending.y) < 1.0e-6f,
        "a grounded Human boot does not retain a horizontal sole");
    const art::OrientedArtTransform mirrored = art::support_boot_transform(
        { 0.0f, -1.0f }, { 0.2f, 0.0f }, 32.0f, 18.0f, -1.0f, true);
    require(planted.ending.x > planted.beginning.x
            && mirrored.ending.x < mirrored.beginning.x,
        "grounded boot orientation does not mirror with facing");
    const art::OrientedArtTransform physical = art::articulated_boot_transform(
        { 0.0f, 0.0f }, { 24.0f, -6.0f }, 30.0f, 14.0f, 1.0f);
    require(physical.ending.x > physical.beginning.x
            && physical.ending.y < physical.beginning.y,
        "Human boot art ignored the physical heel-to-toe pitch");
    const art::OrientedArtTransform physical_mirror = art::articulated_boot_transform(
        { 0.0f, 0.0f }, { -24.0f, -6.0f }, 30.0f, 14.0f, -1.0f);
    require(physical_mirror.ending.x < physical_mirror.beginning.x,
        "physical Human boot axis did not preserve reverse-facing orientation");

    for (const float hz : std::array{ 20.0f, 60.0f, 240.0f })
    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            static_cast<std::uint64_t>(0x7480u + static_cast<unsigned>(hz)) };
        const float remaining = sim::EnvironmentTestAccess::displaced_human_plant_error(
            environment, 1.0f / hz);
        require(remaining < 0.14f && remaining > 0.005f,
            "a planted Human foot provided no resistance or became a root lock");
    }

    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x748A11u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);
        std::array<float, sim::action_count> zero_policy{};
        const auto assisted = rl::effective_policy_action(environment,
            zero_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::assisted);
        const auto raw_zero = rl::effective_policy_action(environment,
            zero_policy, sim::CourseStage::uneven, 0.0f,
            sim::GuidanceMode::raw_policy_audit);
        require(std::any_of(assisted.begin(), assisted.end(), [](float value)
            {
                return std::abs(value) > 1.0e-5f;
            }),
            "assisted composition did not expose its authored gait authority");
        require(std::all_of(raw_zero.begin(), raw_zero.end(), [](float value)
            {
                return value == 0.0f;
            }),
            "raw policy audit manufactured movement from a zero policy");

        const rl::GuidanceAuthorityReport assisted_report =
            rl::guidance_authority_report(environment,
                sim::CourseStage::uneven, 0.0f);
        require(assisted_report.lesson_teacher == 0.0f
                && assisted_report.topology_support == 1.0f
                && assisted_report.topology_body == 1.0f
                && assisted_report.optional_guidance_active(),
            "teacher-zero assisted replay hid its topology reflex authority");

        environment.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        const rl::GuidanceAuthorityReport raw_report =
            rl::guidance_authority_report(environment,
                sim::CourseStage::uneven, 1.0f);
        require(raw_report.mode == sim::GuidanceMode::raw_policy_audit
                && raw_report.mandatory_joint_cluster == 1.0f
                && raw_report.physical_contact == 1.0f
                && !raw_report.optional_guidance_active(),
            "raw policy audit retained optional teacher or reflex authority");

        std::array<float, sim::action_count> saturated_policy{};
        saturated_policy.fill(2.0f);
        const auto raw_saturated = rl::effective_policy_action(environment,
            saturated_policy, sim::CourseStage::uneven, 1.0f,
            sim::GuidanceMode::raw_policy_audit);
        for (std::size_t index = 0; index < sim::action_count; ++index)
        {
            const bool inactive_anatomy = index >= environment.blueprint().active_motor_count
                && index < sim::anatomy_action_count;
            require(raw_saturated[index] == (inactive_anatomy ? 0.0f : 1.0f),
                "raw policy audit changed a policy output beyond clamp/anatomy masking");
        }
    }

    {
        sim::Environment raw_teacher{ sim::CreatureBlueprint::humanoid(),
            0x748A12u };
        raw_teacher.set_course(sim::CourseStage::uneven, 0.30f);
        raw_teacher.set_course_motion_enabled(false);
        raw_teacher.set_guidance_mode(sim::GuidanceMode::raw_policy_audit);
        for (int frame = 0; frame < 1200; ++frame)
        {
            const auto action = rl::raw_walking_teacher_action(raw_teacher);
            if (raw_teacher.step(action).terminated)
                break;
        }
        if (!raw_teacher.valid_motion()
            || !raw_teacher.body_integrity_valid()
            || raw_teacher.alternating_steps() < 12u
            || raw_teacher.distance_travelled() < 10.0f
            || raw_teacher.gait_cycles() < static_cast<std::uint32_t>(
                rl::casual_walk_teacher_stride_events))
        {
            std::cerr << "raw teacher distance=" << raw_teacher.distance_travelled()
                << " cycles=" << raw_teacher.gait_cycles()
                << " elapsed=" << raw_teacher.elapsed_seconds()
                << " alternating=" << raw_teacher.alternating_steps()
                << " invalid=" << static_cast<int>(raw_teacher.invalid_reason())
                << " upright=" << raw_teacher.uprightness() << '\n';
            const auto& raw_rig = raw_teacher.blueprint();
            const auto& raw_particles = raw_teacher.particles();
            const Vec2 raw_root = raw_particles[raw_rig.root_node].position;
            for (std::size_t node = 0; node < raw_particles.size(); ++node)
            {
                const float rest_radius = length(raw_rig.nodes[node]
                    - raw_rig.nodes[raw_rig.root_node]);
                const float current_radius = length(
                    raw_particles[node].position - raw_root);
                if (current_radius > std::max(1.80f,
                    rest_radius * 3.00f + 0.80f))
                    std::cerr << "raw integrity node=" << node
                        << " radius=" << current_radius << '\n';
            }
            for (std::size_t bone_index = 0;
                bone_index < raw_rig.bones.size(); ++bone_index)
            {
                const auto& bone = raw_rig.bones[bone_index];
                const float ratio = length(raw_particles[bone.b].position
                    - raw_particles[bone.a].position) / bone.rest_length;
                if (ratio < 0.20f || ratio > 2.50f)
                    std::cerr << "raw integrity bone=" << bone_index
                        << " ratio=" << ratio << '\n';
            }
            const Vec2 torso_segment = raw_particles[raw_rig.torso_node].position
                - raw_root;
            const Vec2 head_segment = raw_particles[raw_rig.head_node].position
                - raw_particles[raw_rig.torso_node].position;
            const Vec2 rest_torso = raw_rig.nodes[raw_rig.torso_node]
                - raw_rig.nodes[raw_rig.root_node];
            const Vec2 rest_head = raw_rig.nodes[raw_rig.head_node]
                - raw_rig.nodes[raw_rig.torso_node];
            std::cerr << "raw torso_ratio=" << length(torso_segment) / length(rest_torso)
                << " head_ratio=" << length(head_segment) / length(rest_head)
                << " head_dot=" << dot(normalized(torso_segment),
                    normalized(head_segment)) << '\n';
        }
        require(raw_teacher.valid_motion()
                && raw_teacher.body_integrity_valid()
                && raw_teacher.distance_travelled()
                    >= 10.0f
                && raw_teacher.alternating_steps() >= 12u
                && raw_teacher.gait_cycles()
                    >= static_cast<std::uint32_t>(
                        rl::casual_walk_teacher_stride_events),
            "the raw action-space gait demonstration is not physically learnable "
            "without optional assistance");
    }

    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            0x74848u };
        environment.set_course(sim::CourseStage::uneven, 0.30f);
        environment.set_course_motion_enabled(false);
        double torso_pitch_sum{};
        float maximum_torso_pitch{};
        float maximum_hand_fore_aft{};
        std::size_t observed_frames{};
        std::size_t opposed_arm_frames{};
        std::array<float, 2> maximum_supported_extension{};
        std::array<std::size_t, 2> supported_samples{};
        std::array<std::size_t, 2> straight_support_samples{};
        for (int frame = 0; frame < 1200; ++frame)
        {
            std::array<float, sim::action_count> residual{};
            const auto action = rl::effective_policy_action(environment,
                residual, sim::CourseStage::uneven, 0.0f);
            const sim::StepResult result = environment.step(action);
            require(!result.terminated,
                "contact-led Human teacher terminated during the live gait soak");
            if (frame < 120)
                continue;

            const sim::CreatureBlueprint& rig = environment.blueprint();
            const auto particles = environment.particles();
            Vec2 authored_body = rig.nodes[rig.torso_node]
                - rig.nodes[rig.root_node];
            authored_body.x *= environment.facing_direction();
            const Vec2 current_body = particles[rig.torso_node].position
                - particles[rig.root_node].position;
            const float torso_pitch = std::abs(
                signed_angle(authored_body, current_body));
            torso_pitch_sum += torso_pitch;
            maximum_torso_pitch = std::max(maximum_torso_pitch, torso_pitch);

            const float facing = environment.facing_direction();
            const float left_hand = (particles[9].position.x
                - particles[7].position.x) * facing;
            const float right_hand = (particles[12].position.x
                - particles[10].position.x) * facing;
            maximum_hand_fore_aft = std::max({ maximum_hand_fore_aft,
                std::abs(left_hand), std::abs(right_hand) });
            if (left_hand * right_hand <= 0.0f)
                ++opposed_arm_frames;
            ++observed_frames;

            for (std::size_t side = 0; side < 2u; ++side)
            {
                const bool left = side == 0u;
                if (left ? !environment.left_supported()
                    : !environment.right_supported())
                    continue;
                const std::size_t hip_index = left ? 0u : 2u;
                const std::size_t knee_index = hip_index + 1u;
                const sim::MotorConstraint& hip = rig.motors[hip_index];
                const sim::MotorConstraint& knee = rig.motors[knee_index];
                const float chain_length = length(rig.nodes[hip.c]
                    - rig.nodes[hip.pivot]) + length(rig.nodes[knee.c]
                    - rig.nodes[knee.pivot]);
                const float extension = length(particles[knee.c].position
                    - particles[hip.pivot].position)
                    / std::max(chain_length, 1.0e-5f);
                maximum_supported_extension[side] = std::max(
                    maximum_supported_extension[side], extension);
                ++supported_samples[side];
                if (extension >= 0.90f)
                    ++straight_support_samples[side];
            }
        }
        const float mean_pitch = static_cast<float>(torso_pitch_sum
            / static_cast<double>(std::max<std::size_t>(observed_frames, 1u)));
        if (mean_pitch > 0.12f || maximum_torso_pitch > 0.34f
            || maximum_hand_fore_aft > 0.52f
            || opposed_arm_frames * 4u < observed_frames * 3u)
            std::cerr << "Human live gait evidence: mean_pitch=" << mean_pitch
                << " max_pitch=" << maximum_torso_pitch
                << " max_hand_fore_aft=" << maximum_hand_fore_aft
                << " opposed=" << opposed_arm_frames << '/' << observed_frames
                << " support_extension=" << maximum_supported_extension[0]
                << ',' << maximum_supported_extension[1]
                << " heel/toe=" << environment.heel_strikes()
                << '/' << environment.toe_offs() << '\n';
        require(mean_pitch <= 0.12f && maximum_torso_pitch <= 0.34f,
            "live Human trunk retained a backward rail-slide pitch");
        require(maximum_hand_fore_aft <= 0.52f
                && opposed_arm_frames * 4u >= observed_frames * 3u,
            "live Human arms reached forward together or escaped side-rest");
        for (std::size_t side = 0; side < 2u; ++side)
            require(maximum_supported_extension[side] >= 0.94f
                    && straight_support_samples[side] * 2u
                        >= supported_samples[side],
                "live Human support leg remained chronically flexed");
        require(environment.distance_travelled() >= 8.0f
                && environment.gait_cycles() >= 8u,
            "live Human gait soak advanced without real support transfers");
        require(environment.maximum_lower_leg_scissor_seconds()
                <= sim::sustained_scissor_limit_seconds,
            "articulated heel/toe rollover was misclassified as whole-leg scissor motion");
        require(environment.valid_motion(),
            "articulated Human teacher probe became invalid");
        require(environment.heel_strikes() >= 4u
                && environment.toe_offs() >= 4u,
            "live Human gait never produced physical heel/toe contact phases");
    }

    require(std::abs(art::human_limb_layer_opacity(true) - 0.98f)
                <= 1.0e-6f
            && std::abs(art::human_limb_layer_opacity(false) - 0.44f)
                <= 1.0e-6f,
        "Human near/far limb depth policy regressed into duplicate art");
    require(art::human_limb_thickness_ratio(false, false)
            < art::human_limb_thickness_ratio(false, true)
            && art::human_limb_thickness_ratio(false, true)
                < art::human_limb_thickness_ratio(true, false),
        "Human forearm, upper-arm, and support-limb scale ordering regressed");
    const art::HandArtDimensions compact_hand = art::hand_art_dimensions(
        28.0f, 18, 12, 1.0f);
    require(compact_hand.thickness <= 15.0f && compact_hand.length <= 25.0f,
        "Human hand art escaped the compact forearm-owned envelope");

    // Runner physics is authored for the fixed 60 Hz simulation contract;
    // 30 Hz exercises the supported slow-frame clamp without inventing a
    // variable 240 Hz integration mode that the application never runs.
    for (const float hz : std::array{ 30.0f, 60.0f })
    {
        sim::Environment environment{ sim::CreatureBlueprint::humanoid(),
            static_cast<std::uint64_t>(0x748C0u + static_cast<unsigned>(hz)) };
        environment.set_course(sim::CourseStage::crouch_walk, 0.30f);
        environment.set_course_motion_enabled(false);
        float maximum_pelvis_drop{};
        float minimum_head_clearance = std::numeric_limits<float>::infinity();
        const float requested_dt = 1.0f / hz;
        const float physical_dt = clamp(requested_dt, 1.0f / 240.0f, 1.0f / 30.0f);
        const int frames = static_cast<int>(std::ceil(20.0f / physical_dt));
        for (int frame = 0; frame < frames; ++frame)
        {
            std::array<float, sim::action_count> residual{};
            const auto action = rl::effective_policy_action(environment,
                residual, sim::CourseStage::crouch_walk, 0.0f);
            const sim::StepResult result = environment.step(action, requested_dt);
            if (result.terminated)
                std::cerr << "Crouch terminated " << hz << " Hz frame=" << frame
                    << " reason=" << sim::invalid_motion_name(result.invalid_reason)
                    << " root="
                    << environment.particles()[sim::CreatureBlueprint::humanoid()
                        .root_node].position.x
                    << ','
                    << environment.particles()[sim::CreatureBlueprint::humanoid()
                        .root_node].position.y
                    << " max_kmh=" << environment.maximum_speed_kmh()
                    << " distance=" << environment.crouch_walk_distance()
                    << " cycles=" << environment.gait_cycles()
                    << " duck=" << environment.duck_seconds()
                    << " torso_pitch=" << environment.current_crouch_posture().torso_pitch
                    << " pelvis_drop=" << environment.current_crouch_posture().pelvis_drop
                    << " support_margin=" << environment.current_crouch_posture().support_margin
                    << " feet_supported=" << environment.current_crouch_posture().feet_supported
                    << " landing=" << environment.last_landing_side()
                    << ':' << environment.last_landing_air_seconds()
                    << ':' << environment.last_landing_clearance()
                    << ':' << environment.last_landing_displacement()
                    << ':' << environment.last_landing_qualified()
                    << " nonfoot=" << environment.current_crouch_posture().non_foot_grounded
                    << " rolling_s=" << environment.body_rolling_seconds() << '\n';
            require(!result.terminated,
                "translating Human crouch walk terminated during its physical soak");
            const sim::CrouchPostureEvidence posture =
                environment.current_crouch_posture();
            maximum_pelvis_drop = std::max(maximum_pelvis_drop,
                posture.pelvis_drop);
            minimum_head_clearance = std::min(minimum_head_clearance,
                environment.duck_clearance_margin());
        }
        if (environment.crouch_walk_distance() < 1.50f
            || environment.gait_cycles() < 8u
            || environment.duck_seconds() < 3.5f
            || maximum_pelvis_drop < 0.30f)
            std::cerr << "Crouch walk evidence " << hz << " Hz: distance="
                << environment.crouch_walk_distance() << " cycles="
                << environment.gait_cycles() << " duck="
                << environment.duck_seconds() << " drop=" << maximum_pelvis_drop
                << " clearance=" << minimum_head_clearance << '\n';
        require(environment.crouch_walk_distance() >= 1.50f
                && environment.gait_cycles() >= 8u
                && environment.duck_seconds() >= 3.5f
                && maximum_pelvis_drop >= 0.30f,
            "Human crouch walk did not stay low while alternating and translating");
    }

    std::cout << "Runner v0.7.48 contact-led gait and species art checks passed\n";
    return 0;
}
