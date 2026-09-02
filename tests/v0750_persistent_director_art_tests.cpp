#include <Epoch2DWalkEngine/Engine.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace runner::sim
{
    struct EnvironmentTestAccess
    {
        struct TractionProbe
        {
            float before_error{};
            float after_error{};
            float used{};
            float available{};
            float root_displacement{};
        };

        [[nodiscard]] static TractionProbe displace_planted_support(
            Environment& environment, float course_x, float slip) noexcept
        {
            const std::uint16_t contact = environment.blueprint_.left_contact_node;
            const float translation = course_x
                - environment.particles_[contact].position.x;
            for (Particle& particle : environment.particles_)
            {
                particle.position.x += translation;
                particle.previous.x += translation;
            }
            Particle& foot = environment.particles_[contact];
            const float firmness = environment.terrain_firmness_at(foot.position.x);
            const float burial = (1.0f - firmness) * 0.055f;
            foot.position.y = environment.ground_height_at(foot.position.x)
                + ground_contact_offset(true, foot.radius) - burial;
            foot.previous = foot.position;
            environment.support_contact_latch_.assign(
                environment.particles_.size(), 0u);
            environment.support_contact_anchor_x_.resize(
                environment.particles_.size());
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
                environment.support_contact_anchor_x_[index]
                    = environment.particles_[index].position.x;
            environment.support_contact_latch_[contact] = 1u;
            foot.grounded = true;
            const float anchor = foot.position.x;
            foot.position.x += slip;
            foot.previous = foot.position;
            const float root_before = environment.particles_[
                environment.blueprint_.root_node].position.x;
            environment.stance_slip_distance_ = 0.0f;
            environment.available_traction_ = 0.0f;
            environment.used_traction_ = 0.0f;
            const float before = std::abs(foot.position.x - anchor);
            environment.solve_ground(1.0f / 60.0f);
            return { before, std::abs(foot.position.x - anchor),
                environment.used_traction_, environment.available_traction_,
                std::abs(environment.particles_[environment.blueprint_.root_node].position.x
                    - root_before) };
        }

        static void set_elapsed_seconds(Environment& environment, float seconds) noexcept
        {
            environment.elapsed_seconds_ = seconds;
        }
    };
}

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void test_training_terminal_preview_is_retained()
    {
        namespace rl = epoch2dwalk::rl;
        namespace sim = epoch2dwalk::sim;
        rl::PpoTrainer trainer{ sim::CreatureBlueprint::humanoid(), 1u, false };
        trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
        bool terminal_seen = false;
        for (int update = 0; update < 48 && !terminal_seen; ++update)
        {
            trainer.train_one_update();
            terminal_seen = trainer.has_training_terminal_preview();
        }
        require(terminal_seen,
            "rollout terminal must be captured before its worker reset");
        const std::uint64_t trial = trainer.training_terminal_trial_id();
        const epoch2dwalk::Vec2 terminal = trainer.training_terminal_position();
        trainer.train_one_update();
        require(trainer.has_training_terminal_preview()
                && trainer.training_terminal_trial_id() >= trial,
            "training publication must retain a stable terminal trial");
        require(std::isfinite(terminal.x) && std::isfinite(terminal.y),
            "retained terminal position must remain finite");
        trainer.set_gait_task(sim::GaitTask::run, true);
        require(!trainer.has_training_terminal_preview(),
            "a declared gait transition starts a new visible training trial");
    }

    void test_all_species_cross_authored_lake()
    {
        namespace rl = epoch2dwalk::rl;
        namespace sim = epoch2dwalk::sim;
        const std::array rigs{
            sim::CreatureBlueprint::humanoid(),
            sim::CreatureBlueprint::chicken(),
            sim::CreatureBlueprint::crawler4(),
            sim::CreatureBlueprint::hexapod()
        };
        for (const sim::CreatureBlueprint& rig : rigs)
        {
            sim::Environment environment{ rig, 0x7500u };
            environment.set_course(sim::CourseStage::uneven, 0.30f);
            environment.set_course_motion_enabled(false);
            environment.set_trial_time_limit(120.0f);
            float shallow_end{};
            for (const sim::TerrainChallenge& challenge :
                environment.terrain().terrain_challenges())
            {
                if (challenge.region == sim::TerrainRegion::shallow_water)
                    shallow_end = challenge.end;
            }
            bool crossed{};
            sim::StepResult result{};
            for (int frame = 0; frame < 7200 && !crossed; ++frame)
            {
                result = environment.step(
                    rl::raw_walking_teacher_action(environment));
                const float root_x = environment.particles()[
                    rig.root_node].position.x;
                crossed = root_x > shallow_end + 0.5f;
                if (result.terminated)
                    break;
            }
            if (!crossed || !environment.valid_motion())
            {
                std::cerr << "lake traversal species="
                    << sim::creature_species_name(rig.presentation_species())
                    << " x=" << environment.particles()[rig.root_node].position.x
                    << " shore=" << shallow_end
                    << " time=" << environment.elapsed_seconds()
                    << " gait=" << environment.gait_cycles()
                    << " region=" << sim::terrain_region_name(
                        environment.terrain().challenge_at(
                            environment.particles()[rig.root_node].position.x).region)
                    << " terminal=" << sim::trial_terminal_cause_name(
                        result.terminal_cause, result.invalid_reason)
                    << '\n';
            }
            require(crossed && environment.valid_motion(),
                "a production morphology cannot physically cross the authored lake");
            require(environment.used_traction()
                    <= environment.available_traction() + 1.0e-5f,
                "lake traversal exceeded its reported finite traction budget");
        }
    }
}

int main()
{
    namespace director = epoch2dwalk::director;
    namespace integration = epoch2dwalk::integration;
    namespace locomotion = epoch2dwalk::locomotion;
    namespace sim = epoch2dwalk::sim;
    namespace rl = epoch2dwalk::rl;
    namespace art = epoch2dwalk::art;

    static_assert(integration::api_version == 3u);
    constexpr director::TaskGraph graph = director::default_training_graph();
    constexpr director::State initial_director = director::make_state(
        director::Profile::curriculum);
    static_assert(initial_director.valid());
    static_assert(!graph.nodes[16].enabled && !graph.nodes[17].enabled);
    static_assert(!director::task_supported(director::TaskKind::construction));
    static_assert(director::task_name(director::TaskKind::gun_stance)
        == std::string_view{ "GUN STANCE" });
    static_assert(integration::desired_task_state(
        director::TaskKind::safe_carry_walk).equipment
        == sim::EquipmentDirective::safe_carry_walk);
    static_assert(integration::desired_task_state(
        director::TaskKind::low_ready_walk).equipment
        == sim::EquipmentDirective::low_ready_walk);
    static_assert(integration::desired_task_state(
        director::TaskKind::fire_and_correct).equipment
        == sim::EquipmentDirective::fire_and_correct);
    static_assert(graph.valid());
    static_assert(graph.count >= 18u);
    director::Evidence evidence{};
    const director::Decision first = director::select(graph, evidence,
        director::Profile::curriculum);
    const director::Decision repeated = director::select(graph, evidence,
        director::Profile::curriculum);
    require(first.selected() && first.task_index == repeated.task_index
            && first.stable_id == repeated.stable_id,
        "AI Director selection is not deterministic");
    evidence.mastery[0] = graph.nodes[0].minimum_evidence;
    evidence.requested_goal = 15u;
    const director::Decision prerequisite = director::select(graph, evidence,
        director::Profile::autonomous_npc);
    require(prerequisite.task_index != 15u,
        "AI Director bypassed physical task prerequisites");
    evidence.first_stall = 1u;
    const director::Decision stall = director::select(graph, evidence,
        director::Profile::curriculum);
    require(stall.task_index == 1u
            && stall.reason == director::SelectionReason::unresolved_stall,
        "AI Director did not return to the first unresolved physical stall");

    sim::Environment human{ sim::CreatureBlueprint::humanoid(), 0x7500u };
    human.set_course(sim::CourseStage::uneven, 0.30f);
    require(human.equipment_directive()
            == sim::EquipmentDirective::passive
            && human.weapon_class() == sim::WeaponClass::carbine
            && std::abs(human.observation()[62]) < 1.0e-6f,
        "default Human did not preserve its neutral gait with a real carbine");
    human.set_equipment_directive(sim::EquipmentDirective::safe_carry_walk);
    std::array<float, sim::action_count> empty_policy{};
    for (std::size_t frame = 0u; frame < 72u && human.valid_motion(); ++frame)
    {
        const auto carry_action = rl::effective_policy_action(human,
            empty_policy, sim::CourseStage::uneven, 1.0f,
            sim::GuidanceMode::assisted);
        static_cast<void>(human.step(carry_action));
    }
    require(human.valid_motion()
            && std::abs(epoch2dwalk::wrap_angle(human.equipment_aim_angle()))
                > 0.30f,
        "selected safe-carry task did not move the physical arm link off vertical");
    require(std::abs(human.observation()[62]
            - 1.0f / static_cast<float>(sim::equipment_directive_count - 1u))
            < 1.0e-6f,
        "policy observation does not identify the selected equipment behavior");
    human.set_equipment_directive(sim::EquipmentDirective::low_ready_walk);
    const auto low_ready_action = rl::effective_policy_action(human,
        empty_policy, sim::CourseStage::uneven, 1.0f,
        sim::GuidanceMode::assisted);
    require(std::abs(low_ready_action[sim::equipment_state_action] - 0.45f)
            < 1.0e-6f
            && low_ready_action[sim::equipment_trigger_action] < -0.99f,
        "low-ready Director task does not own a distinct non-firing physical command");
    static_cast<void>(human.step(low_ready_action));
    require(human.equipment_state() == sim::EquipmentState::low_ready,
        "low-ready command incorrectly advanced into the gun stance");
    human.set_equipment_directive(sim::EquipmentDirective::safe_carry_walk);
    const auto carry_action = rl::effective_policy_action(human, empty_policy,
        sim::CourseStage::uneven, 1.0f, sim::GuidanceMode::assisted);
    static_cast<void>(human.step(carry_action));
    const auto root = human.blueprint().root_node;
    const epoch2dwalk::Vec2 before = human.particles()[root].position;
    static_cast<void>(director::select(graph, evidence,
        director::Profile::autonomous_npc));
    require(epoch2dwalk::length(human.particles()[root].position - before)
            < 1.0e-7f,
        "AI Director selection moved the physical body like a rail");

    director::Evidence gun_evidence{};
    for (std::size_t index = 0u; index < 15u; ++index)
        gun_evidence.mastery[index] = graph.nodes[index].minimum_evidence;
    gun_evidence.requested_goal = 15u;
    const director::Decision gun_decision = director::select(
        graph, gun_evidence, director::Profile::autonomous_npc);
    const integration::TaskCommand gun_command = integration::task_command(
        graph, gun_decision);
    require(gun_decision.task_index == 15u && gun_command.selected()
            && gun_command.supported
            && gun_command.task == director::TaskKind::fire_and_correct,
        "AI Director did not emit the requested physical gun task");
    sim::Environment directed_human{
        sim::CreatureBlueprint::humanoid(), 0x7503u };
    directed_human.set_course(sim::CourseStage::uneven, 0.30f);
    const integration::TaskApplication applied = integration::apply_task(
        directed_human, gun_command);
    require(applied.applied && !applied.rejected
            && directed_human.course_stage()
                == sim::CourseStage::equipment_targets
            && directed_human.equipment_directive()
                == sim::EquipmentDirective::fire_and_correct
            && directed_human.weapon_class() == sim::WeaponClass::carbine,
        "selected gun task did not configure its real physical range and behavior");
    const integration::TaskApplication idempotent = integration::apply_task(
        directed_human, gun_command);
    require(!idempotent.applied && !idempotent.rejected,
        "stable task command reset the physical trial every fixed tick");
    sim::Environment directed_dog{ sim::CreatureBlueprint::crawler4(), 0x7504u };
    const sim::CourseStage dog_stage = directed_dog.course_stage();
    const integration::TaskApplication impossible = integration::apply_task(
        directed_dog, gun_command);
    require(impossible.rejected && directed_dog.course_stage() == dog_stage
            && directed_dog.weapon_class() == sim::WeaponClass::none,
        "Director mutated a non-manipulator before rejecting its gun task");
    const integration::TaskCommand future_tool{
        graph.nodes[16].stable_id, director::TaskKind::construction,
        director::ChallengeKind::construction_anchor, false };
    const sim::CourseStage before_future = directed_human.course_stage();
    require(integration::apply_task(directed_human, future_tool).rejected
            && directed_human.course_stage() == before_future,
        "unimplemented construction task was falsely exposed as physical support");
    director::State persisted_director = initial_director;
    persisted_director.evidence = gun_evidence;
    persisted_director.current = gun_decision;
    require(persisted_director.valid()
            && persisted_director.current.stable_id == gun_decision.stable_id,
        "versioned Director state does not preserve its selected task identity");
    const std::filesystem::path director_path =
        std::filesystem::temp_directory_path()
        / "epoch2dwalk-v0750-director-test.state";
    std::string director_error{};
    director::State loaded_director{};
    require(director::save_state(director_path, persisted_director,
                director_error)
            && director::load_state(director_path, loaded_director,
                director_error)
            && loaded_director.valid()
            && loaded_director.current.stable_id == gun_decision.stable_id
            && loaded_director.evidence.failures == gun_evidence.failures,
        "Director graph/evidence did not survive its external serialization contract");
    director::State invalid_director = persisted_director;
    invalid_director.schema = 99u;
    require(!director::save_state(director_path, invalid_director, director_error),
        "incompatible Director schema was serialized as current");
    std::error_code director_remove_error{};
    std::filesystem::remove(director_path, director_remove_error);
    require(human.weapon_class() == sim::WeaponClass::carbine
            && human.equipment_state() == sim::EquipmentState::safe_carry,
        "physical Human did not receive the default carbine in safe carry");
    sim::Environment dog{ sim::CreatureBlueprint::crawler4(), 0x7501u };
    require(dog.weapon_class() == sim::WeaponClass::none,
        "non-manipulator species received an impossible default gun");

    integration::ControlRequest request{};
    request.mode = integration::ControlMode::player_guided;
    request.selected_task = gun_command;
    const integration::FixedStepResult stepped = integration::fixed_step(
        human, request, 12u, 4u);
    require(stepped.snapshot.valid && stepped.snapshot.trial_id == 12u
            && stepped.snapshot.fixed_tick == 4u
            && std::isfinite(stepped.reward)
            && stepped.task_applied && !stepped.task_rejected
            && stepped.stable_task_id == gun_decision.stable_id
            && stepped.snapshot.equipment_directive
                == sim::EquipmentDirective::fire_and_correct
            && human.course_stage() == sim::CourseStage::equipment_targets
            && stepped.terminal_cause == sim::TrialTerminalCause::none,
        "external host task command did not drive a truthful physical step");
    request.selected_task = {};
    request.retry_trial = true;
    const integration::FixedStepResult retried = integration::fixed_step(
        human, request, 13u, 1u);
    require(retried.retry_applied && retried.snapshot.trial_id == 13u,
        "external host retry request did not start its declared physical trial");
    std::array<float, sim::action_count> zero_action{};
    sim::Environment bounded{ sim::CreatureBlueprint::humanoid(), 0x7502u };
    bounded.set_course(sim::CourseStage::uneven, 0.30f);
    sim::EnvironmentTestAccess::set_elapsed_seconds(bounded,
        sim::default_trial_time_limit_seconds(sim::CourseStage::uneven));
    const sim::StepResult bounded_terminal = bounded.step(zero_action);
    require(bounded_terminal.terminated
            && bounded_terminal.valid_motion
            && bounded_terminal.invalid_reason == sim::InvalidMotion::none
            && bounded_terminal.terminal_cause == sim::TrialTerminalCause::time_limit
            && sim::trial_terminal_cause_name(bounded_terminal.terminal_cause,
                bounded_terminal.invalid_reason) == "TIME LIMIT",
        "valid trial time limit was hidden behind a VALID motion label");
    sim::Environment persistent{ sim::CreatureBlueprint::humanoid(), 0x7502u };
    persistent.set_course(sim::CourseStage::uneven, 0.30f);
    persistent.set_trial_time_limit(96.0f);
    sim::EnvironmentTestAccess::set_elapsed_seconds(persistent, 36.0f);
    const sim::StepResult still_active = persistent.step(zero_action);
    require(!still_active.terminated && persistent.uses_trial_time_limit_override(),
        "persistent preview inherited the short PPO rollout time limit");
    const integration::DiagnosticEvents events = integration::diagnostic_events(
        nullptr, stepped, first.stable_id);
    require(!events.span().empty()
            && stepped.snapshot.stable_challenge_id != 0u,
        "external host did not receive physical challenge diagnostics");
    test_training_terminal_preview_is_retained();

    const auto challenges = human.terrain().terrain_challenges();
    require(challenges.size() == sim::DeformableTerrain::terrain_challenge_count,
        "authored terrain challenge inventory is incomplete");
    bool shallow_seen = false;
    bool hole_seen = false;
    float firm_x = challenges[2].center();
    float shallow_x = firm_x;
    for (std::size_t index = 0; index < challenges.size(); ++index)
    {
        const sim::TerrainChallenge& challenge = challenges[index];
        require(challenge.stable_id != 0u && challenge.end > challenge.begin,
            "terrain challenge identity or extent is invalid");
        if (index > 0u)
            require(std::abs(challenge.begin - challenges[index - 1u].end) < 1.0e-6f,
                "terrain challenge graph contains a hidden seam");
        require(human.terrain().challenge_at(challenge.center()).stable_id
                == challenge.stable_id,
            "terrain challenge lookup does not reproduce its stable identity");
        if (challenge.region == sim::TerrainRegion::firm && index > 0u)
            firm_x = challenge.center();
        if (challenge.region == sim::TerrainRegion::shallow_water)
        {
            shallow_seen = true;
            shallow_x = challenge.center();
        }
        hole_seen = hole_seen || challenge.region == sim::TerrainRegion::hole;
    }
    require(shallow_seen && hole_seen,
        "authored early course does not expose its lake and hole challenges");
    test_all_species_cross_authored_lake();

    const sim::ContactTraction firm = sim::contact_traction(
        1.0f, 0.0f, 0.0f, 0.11f, 1.0f, 1.0f / 60.0f);
    const sim::ContactTraction sand = sim::contact_traction(
        0.35f, 0.80f, 0.0f, 0.11f, 1.0f, 1.0f / 60.0f);
    const sim::ContactTraction wet = sim::contact_traction(
        0.35f, 0.80f, 0.30f, 0.11f, 1.0f, 1.0f / 60.0f);
    require(firm.static_coefficient > sand.static_coefficient
            && sand.static_coefficient > wet.static_coefficient
            && firm.maximum_static_correction > firm.maximum_dynamic_correction
            && firm.maximum_dynamic_correction > wet.maximum_dynamic_correction
            && wet.maximum_dynamic_correction > 0.0f,
        "contact traction ignores material firmness, looseness, or water");

    sim::Environment firm_contact{ sim::CreatureBlueprint::humanoid(), 0x7500u };
    firm_contact.set_course(sim::CourseStage::uneven, 0.30f);
    sim::Environment wet_contact{ sim::CreatureBlueprint::humanoid(), 0x7500u };
    wet_contact.set_course(sim::CourseStage::uneven, 0.30f);
    const auto firm_probe = sim::EnvironmentTestAccess::displace_planted_support(
        firm_contact, firm_x, 0.20f);
    const auto wet_probe = sim::EnvironmentTestAccess::displace_planted_support(
        wet_contact, shallow_x, 0.20f);
    require(firm_probe.after_error < firm_probe.before_error
            && wet_probe.after_error < wet_probe.before_error
            && (firm_probe.before_error - firm_probe.after_error)
                > (wet_probe.before_error - wet_probe.after_error),
        "real planted contacts do not resist slip according to material traction");
    require(firm_probe.used <= firm_probe.available + 1.0e-5f
            && wet_probe.used <= wet_probe.available + 1.0e-5f,
        "traction solver exceeded its reported physical correction budget");
    require(firm_probe.root_displacement < 1.0e-7f
            && wet_probe.root_displacement < 1.0e-7f,
        "planted-contact traction moved the root like a rail");

    locomotion::Signals water{};
    water.uprightness = 0.92f;
    water.left_supported = true;
    water.right_supported = true;
    water.left_support_x = -0.2f;
    water.right_support_x = 0.2f;
    water.water_depth = 0.42f;
    water.water_submersion = 0.35f;
    water.water_depth_ahead = 0.50f;
    const locomotion::Plan wade = locomotion::plan(water);
    require(wade.aquatic && (wade.intent == locomotion::Intent::wade
            || wade.intent == locomotion::Intent::swim),
        "locomotion planner treated the lake as dry walking terrain");
    locomotion::Signals wet_toes = water;
    wet_toes.water_depth = 0.025f;
    wet_toes.water_depth_ahead = 0.025f;
    wet_toes.water_submersion = 0.08f;
    const locomotion::Plan wet_toe_plan = locomotion::plan(wet_toes);
    require(!wet_toe_plan.aquatic
            && wet_toe_plan.intent == locomotion::Intent::walk,
        "wet toes incorrectly replaced ordinary terrain gait with an aquatic gait");
    locomotion::Signals churning = wet_toes;
    churning.terrain_firmness = 0.14f;
    churning.terrain_looseness = 0.96f;
    churning.burial_depth = 0.08f;
    churning.micro_motion_seconds = 0.61f;
    churning.water_depth_ahead = 0.20f;
    churning.gait_cycles = 12u;
    churning.requested_direction = 1.0f;
    const locomotion::Plan extraction = locomotion::plan(churning);
    require(extraction.intent == locomotion::Intent::escape
            && extraction.direction == 1.0f && extraction.step_up,
        "cycle-in-place on yielding terrain did not request bounded "
        "objective-directed physical extraction");
    locomotion::Signals released_extraction = churning;
    released_extraction.zero_progress_seconds = 0.0f;
    released_extraction.hazard_stall_seconds = 0.0f;
    released_extraction.micro_motion_seconds = 2.0f;
    const locomotion::Plan released_plan = locomotion::plan(released_extraction);
    require(released_plan.intent != locomotion::Intent::escape
            && released_plan.direction == 1.0f,
        "micro-motion recovery remained active beyond its upper hysteresis edge");
    locomotion::Signals soft_firm_stall = churning;
    soft_firm_stall.terrain_firmness = 0.92f;
    soft_firm_stall.terrain_looseness = 0.08f;
    soft_firm_stall.water_depth = 0.0f;
    soft_firm_stall.water_depth_ahead = 0.0f;
    soft_firm_stall.water_submersion = 0.0f;
    soft_firm_stall.obstruction_mask = 0x4u;
    soft_firm_stall.left_escape_rise = 0.0f;
    soft_firm_stall.right_escape_rise = 0.0f;
    const locomotion::Plan no_reverse = locomotion::plan(soft_firm_stall);
    require(no_reverse.intent != locomotion::Intent::escape
            && no_reverse.direction == 1.0f,
        "residual burial on clear firm ground triggered a reverse escape");
    locomotion::Signals film_ahead = soft_firm_stall;
    film_ahead.saturated_depth_ahead = 0.025f;
    const locomotion::Plan transition_recovery = locomotion::plan(film_ahead);
    require(transition_recovery.intent == locomotion::Intent::escape
            && transition_recovery.direction == 1.0f,
        "a stalled saturated transition did not preserve objective-directed "
        "physical recovery");
    water.shore_exit_ahead = true;
    locomotion::Signals water_approach = wet_toes;
    water_approach.water_depth = 0.0f;
    water_approach.water_submersion = 0.0f;
    water_approach.water_depth_ahead = 0.20f;
    const locomotion::Plan approach_plan = locomotion::plan(water_approach);
    require(!approach_plan.aquatic
            && approach_plan.intent == locomotion::Intent::walk
            && approach_plan.step_up,
        "water look-ahead preemptively switched to an unplanted aquatic gait");
    water.water_depth_ahead = 0.02f;
    const locomotion::Plan exit = locomotion::plan(water);
    require(exit.intent == locomotion::Intent::shore_exit && exit.aquatic,
        "locomotion planner did not select a physical shore-exit transition");

    require(art::human_limb_layer_opacity(false) >= 0.35f
            && art::human_limb_layer_opacity(false) <= 0.45f,
        "far Human limbs are hidden or read as duplicate foreground chains");
    art::PixelArt padded{};
    padded.width = 5;
    padded.height = 4;
    padded.chroma_keyed = true;
    padded.transparent_key = { 1.0f, 0.0f, 1.0f, 1.0f };
    padded.pixels.assign(20u, padded.transparent_key);
    padded.pixels[6] = { 1.0f, 1.0f, 1.0f, 1.0f };
    padded.pixels[7] = { 0.5f, 0.5f, 0.5f, 1.0f };
    padded.pixels[11] = { 0.4f, 0.4f, 0.4f, 1.0f };
    padded.pixels[12] = { 0.2f, 0.2f, 0.2f, 1.0f };
    const art::OpaquePixelBounds opaque = art::opaque_pixel_bounds(padded);
    require(opaque.valid() && opaque.left == 1 && opaque.right == 3
            && opaque.top == 1 && opaque.bottom == 3,
        "opaque art bounds retained the transparent keyed gutter");
    art::PixelArt empty = padded;
    empty.pixels.assign(20u, empty.transparent_key);
    require(!art::opaque_pixel_bounds(empty).valid(),
        "an empty keyed module manufactured visible fit bounds");
    art::PixelArt malformed = padded;
    malformed.pixels.pop_back();
    require(!art::opaque_pixel_bounds(malformed).valid(),
        "a malformed art payload manufactured visible fit bounds");
    const art::HandArtDimensions hand = art::hand_art_dimensions(
        22.0f, 40, 28, 1.0f, 72.0f);
    require(hand.thickness >= 12.0f && hand.length <= 72.0f * 0.36f,
        "Human hand is detached, undersized, or outside the forearm envelope");
    const auto chicken = art::species_art_profile(sim::CreatureSpecies::chicken);
    const auto dog_art = art::species_art_profile(sim::CreatureSpecies::dog);
    const auto hex = art::species_art_profile(sim::CreatureSpecies::hexapod);
    require(chicken.head_anchor_forward >= 0.07f
            && dog_art.head_anchor_forward > chicken.head_anchor_forward
            && hex.head_anchor_forward >= 0.05f,
        "species head art does not overlap its physical neck attachment");
    require(chicken.foot_length > 0.20f && dog_art.foot_length > 0.25f
            && hex.foot_length > 0.18f,
        "species foot silhouettes do not visibly reach from physical contacts");

    art::Layout authored = art::default_layout(sim::CreatureSpecies::human);
    art::ModuleAdjustment& authored_hand = authored.at(art::Module::hand);
    authored_hand.anchor_along = 0.08f;
    authored_hand.anchor_normal = -0.04f;
    authored_hand.pivot = 0.25f;
    authored_hand.length_scale = 1.15f;
    authored_hand.thickness_scale = 0.90f;
    authored_hand.rotation = 0.20f;
    authored_hand.flip_vertical = true;
    authored_hand.layer = 1;
    require(authored.valid(),
        "bounded module art adjustment was rejected");
    const art::AdjustedTransform fitted = art::adjust_transform(
        { 0.0f, 0.0f }, { 10.0f, 0.0f }, 4.0f, authored_hand,
        true, false);
    require(std::abs(epoch2dwalk::length(fitted.ending - fitted.beginning)
                - 11.5f) < 1.0e-4f
            && std::abs(fitted.thickness - 3.6f) < 1.0e-4f
            && !fitted.flip_vertical && fitted.layer == 1,
        "art editor transform did not preserve pivoted scale/flip/depth semantics");
    const art::AdjustedTransform repeated_fitted = art::adjust_transform(
        { 0.0f, 0.0f }, { 10.0f, 0.0f }, 4.0f, authored_hand,
        true, false);
    require(epoch2dwalk::length(fitted.beginning - repeated_fitted.beginning)
                < 1.0e-7f
            && epoch2dwalk::length(fitted.ending - repeated_fitted.ending)
                < 1.0e-7f,
        "art module transform changed across repeated identical frames");
    art::Layout invalid_art = authored;
    invalid_art.at(art::Module::terminal).length_scale = 4.0f;
    require(!invalid_art.valid(),
        "art editor accepted a transform outside the physical fit envelope");
    art::LayoutHistory history{};
    const art::Layout baseline = art::default_layout(sim::CreatureSpecies::human);
    history.reset(baseline);
    history.commit(authored);
    art::Layout history_value = authored;
    require(history.undo(history_value)
            && history_value.at(art::Module::hand).length_scale == 1.0f
            && history.redo(history_value)
            && std::abs(history_value.at(art::Module::hand).length_scale - 1.15f)
                < 1.0e-6f,
        "art editor undo/redo did not restore exact layout states");
    const std::filesystem::path layout_path =
        std::filesystem::temp_directory_path()
        / "epoch2dwalk-v0750-art-layout-test.artlayout";
    std::string layout_error{};
    art::Layout loaded_layout{};
    require(art::save_layout(layout_path, authored, layout_error)
            && art::load_layout(layout_path, sim::CreatureSpecies::human,
                loaded_layout, layout_error)
            && loaded_layout.valid()
            && !art::load_layout(layout_path, sim::CreatureSpecies::dog,
                loaded_layout, layout_error),
        "art editor save/load did not validate species-owned layout identity");
    std::error_code remove_error{};
    std::filesystem::remove(layout_path, remove_error);

    std::cout << "Runner v0.7.50 persistent director, traction, SDK, and art contracts passed\n";
    return EXIT_SUCCESS;
}
