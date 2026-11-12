#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string_view>

namespace runner::director
{
    inline constexpr std::size_t maximum_task_nodes = 32u;
    inline constexpr std::size_t maximum_prerequisites = 4u;
    inline constexpr std::uint16_t invalid_task_index = 0xffffu;
    inline constexpr std::uint32_t physical_trial_evidence_schema_version = 1u;
    inline constexpr float minimum_physical_plant_seconds = 0.16f;
    inline constexpr float minimum_physical_aim_seconds = 0.30f;
    inline constexpr float maximum_bounded_aim_correction = 0.22f;
    inline constexpr float minimum_water_entry_depth = 0.06f;
    inline constexpr float minimum_water_immersion = 0.05f;
    inline constexpr float minimum_swimming_immersion = 0.58f;

    enum class Profile : std::uint8_t
    {
        curriculum,
        autonomous_npc,
        player_guided
    };

    enum class TaskKind : std::uint8_t
    {
        stand,
        walk,
        speed_walk,
        walk_run_transition,
        run,
        crouch_walk,
        wade,
        swim,
        shore_exit,
        safe_carry_walk,
        low_ready_walk,
        stop_and_plant,
        gun_stance,
        acquire_and_aim,
        fire_and_correct,
        break_contact,
        recover,
        construction,
        tool_traversal
    };

    enum class ChallengeKind : std::uint8_t
    {
        flat,
        loose_cells,
        waterlogged_cells,
        shallow_water,
        shore,
        step,
        gap,
        ramp,
        ledge,
        low_clearance,
        aim_range,
        moving_target,
        construction_anchor
    };

    enum class SelectionReason : std::uint8_t
    {
        requested_goal,
        unresolved_stall,
        repeated_failure,
        prerequisite_ready,
        graph_complete,
        no_eligible_task
    };

    enum class PhysicalEvidenceKind : std::uint8_t
    {
        none,
        water_crossing,
        equipment_correction
    };

    struct WaterExecutionEvidence
    {
        std::uint64_t approach_tick{};
        std::uint64_t entry_tick{};
        std::uint64_t wading_tick{};
        std::uint64_t swimming_tick{};
        std::uint64_t shore_exit_tick{};
        std::uint64_t far_shore_dry_tick{};
        float maximum_depth{};
        float maximum_submersion{};

        constexpr bool operator==(const WaterExecutionEvidence&) const = default;

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return *this == WaterExecutionEvidence{};
        }
    };

    struct EquipmentExecutionEvidence
    {
        std::uint64_t two_foot_plant_tick{};
        std::uint64_t physical_aim_tick{};
        std::uint64_t miss_tick{};
        std::uint64_t correction_tick{};
        std::uint64_t improved_shot_tick{};
        float maximum_plant_seconds{};
        float maximum_aim_settle_seconds{};
        float observed_miss_error{};
        float applied_correction{};
        float improved_shot_error{};
        std::uint32_t shots_fired{};
        std::uint32_t shots_missed{};
        std::uint32_t target_hits{};
        bool improved_shot_hit{};

        constexpr bool operator==(const EquipmentExecutionEvidence&) const = default;

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return *this == EquipmentExecutionEvidence{};
        }
    };

    struct PhysicalTrialEvidence
    {
        std::uint32_t schema{};
        PhysicalEvidenceKind kind{ PhysicalEvidenceKind::none };
        std::uint64_t trial_id{};
        std::uint32_t stable_task_id{};
        std::uint32_t stable_challenge_id{};
        std::uint64_t first_tick{};
        std::uint64_t last_tick{};
        WaterExecutionEvidence water{};
        EquipmentExecutionEvidence equipment{};

        constexpr bool operator==(const PhysicalTrialEvidence&) const = default;

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return *this == PhysicalTrialEvidence{};
        }
    };

    struct TaskNode
    {
        std::uint32_t stable_id{};
        std::uint32_t stable_challenge_id{};
        TaskKind task{ TaskKind::stand };
        ChallengeKind challenge{ ChallengeKind::flat };
        std::array<std::uint16_t, maximum_prerequisites> prerequisites{};
        std::uint8_t prerequisite_count{};
        float minimum_evidence{};
        float difficulty{};
        bool enabled{ true };
    };

    [[nodiscard]] constexpr bool task_supported(TaskKind task) noexcept;

    struct TaskGraph
    {
        std::array<TaskNode, maximum_task_nodes> nodes{};
        std::uint16_t count{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (count == 0u || count > nodes.size())
                return false;
            std::array<bool, maximum_task_nodes> resolved{};
            std::size_t resolved_count{};
            for (std::size_t index = 0; index < count; ++index)
            {
                const TaskNode& node = nodes[index];
                if (node.stable_id == 0u || node.stable_challenge_id == 0u
                    || static_cast<unsigned>(node.task)
                        > static_cast<unsigned>(TaskKind::tool_traversal)
                    || static_cast<unsigned>(node.challenge)
                        > static_cast<unsigned>(ChallengeKind::construction_anchor)
                    || node.prerequisite_count > node.prerequisites.size()
                    || node.minimum_evidence != node.minimum_evidence
                    || node.difficulty != node.difficulty
                    || node.minimum_evidence < 0.0f
                    || node.minimum_evidence > 1.0f
                    || node.difficulty < 0.0f || node.difficulty > 1.0f
                    || (node.enabled && !task_supported(node.task)))
                    return false;
                for (std::size_t other = 0; other < index; ++other)
                {
                    if (nodes[other].stable_id == node.stable_id)
                        return false;
                    if (nodes[other].challenge == node.challenge
                        && nodes[other].stable_challenge_id
                            != node.stable_challenge_id)
                        return false;
                    if (nodes[other].challenge != node.challenge
                        && nodes[other].stable_challenge_id
                            == node.stable_challenge_id)
                        return false;
                }
                for (std::size_t prerequisite = 0;
                    prerequisite < node.prerequisite_count; ++prerequisite)
                {
                    if (node.prerequisites[prerequisite] >= count
                        || node.prerequisites[prerequisite] == index)
                        return false;
                    if (node.enabled
                        && !nodes[node.prerequisites[prerequisite]].enabled)
                        return false;
                    for (std::size_t prior = 0; prior < prerequisite; ++prior)
                    {
                        if (node.prerequisites[prior]
                            == node.prerequisites[prerequisite])
                            return false;
                    }
                }
            }

            // Resolve the dependency DAG without assuming prerequisites are
            // stored before their consumers. A syntactically in-range cycle is
            // still not a schedulable task graph.
            for (std::size_t pass = 0; pass < count; ++pass)
            {
                bool progressed{};
                for (std::size_t index = 0; index < count; ++index)
                {
                    if (resolved[index])
                        continue;
                    bool ready = true;
                    for (std::size_t prerequisite = 0;
                        prerequisite < nodes[index].prerequisite_count;
                        ++prerequisite)
                    {
                        ready = ready
                            && resolved[nodes[index].prerequisites[prerequisite]];
                    }
                    if (!ready)
                        continue;
                    resolved[index] = true;
                    ++resolved_count;
                    progressed = true;
                }
                if (!progressed)
                    break;
            }
            return resolved_count == count;
        }
    };

    struct Evidence
    {
        std::array<float, maximum_task_nodes> mastery{};
        std::array<std::uint16_t, maximum_task_nodes> failures{};
        std::array<PhysicalTrialEvidence, maximum_task_nodes> physical_mastery{};
        std::uint16_t first_stall{ invalid_task_index };
        std::uint16_t requested_goal{ invalid_task_index };
    };

    [[nodiscard]] constexpr bool water_mastery_required(
        const TaskNode& node) noexcept
    {
        return node.challenge == ChallengeKind::waterlogged_cells
            || node.challenge == ChallengeKind::shallow_water
            || node.challenge == ChallengeKind::shore
            || node.task == TaskKind::wade || node.task == TaskKind::swim
            || node.task == TaskKind::shore_exit;
    }

    [[nodiscard]] constexpr bool equipment_correction_mastery_required(
        const TaskNode& node) noexcept
    {
        return node.task == TaskKind::stop_and_plant
            || node.task == TaskKind::gun_stance
            || node.task == TaskKind::acquire_and_aim
            || node.task == TaskKind::fire_and_correct;
    }

    [[nodiscard]] constexpr bool physical_mastery_required(
        const TaskNode& node) noexcept
    {
        return water_mastery_required(node)
            || equipment_correction_mastery_required(node);
    }

    [[nodiscard]] constexpr bool finite_physical_scalar(float value) noexcept
    {
        return value == value
            && value >= -std::numeric_limits<float>::max()
            && value <= std::numeric_limits<float>::max();
    }

    [[nodiscard]] constexpr float physical_abs(float value) noexcept
    {
        return value < 0.0f ? -value : value;
    }

    [[nodiscard]] constexpr bool physical_trial_evidence_valid(
        const TaskNode& node, const PhysicalTrialEvidence& physical) noexcept
    {
        if (!physical_mastery_required(node))
            return physical.empty();
        if (physical.schema != physical_trial_evidence_schema_version
            || physical.trial_id == 0u
            || physical.stable_task_id != node.stable_id
            || physical.stable_challenge_id != node.stable_challenge_id
            || physical.first_tick == 0u
            || physical.last_tick < physical.first_tick)
            return false;

        if (water_mastery_required(node))
        {
            const WaterExecutionEvidence& water = physical.water;
            const bool ordered = water.approach_tick >= physical.first_tick
                && water.approach_tick < water.entry_tick
                && water.entry_tick <= water.wading_tick
                && water.wading_tick <= water.shore_exit_tick
                && water.shore_exit_tick < water.far_shore_dry_tick
                && water.far_shore_dry_tick <= physical.last_tick;
            const bool swim_ordered = water.swimming_tick == 0u
                ? node.task != TaskKind::swim
                : water.swimming_tick >= water.wading_tick
                    && water.swimming_tick <= water.shore_exit_tick;
            return physical.kind == PhysicalEvidenceKind::water_crossing
                && physical.equipment.empty() && ordered && swim_ordered
                && finite_physical_scalar(water.maximum_depth)
                && finite_physical_scalar(water.maximum_submersion)
                && water.maximum_depth > minimum_water_entry_depth
                && water.maximum_submersion >= (node.task == TaskKind::swim
                    ? minimum_swimming_immersion : minimum_water_immersion)
                && water.maximum_submersion <= 1.0f;
        }

        const EquipmentExecutionEvidence& equipment = physical.equipment;
        const float miss_error = physical_abs(equipment.observed_miss_error);
        const float improved_error = physical_abs(equipment.improved_shot_error);
        return physical.kind == PhysicalEvidenceKind::equipment_correction
            && physical.water.empty()
            && equipment.two_foot_plant_tick >= physical.first_tick
            && equipment.two_foot_plant_tick <= equipment.physical_aim_tick
            && equipment.physical_aim_tick < equipment.miss_tick
            && equipment.miss_tick < equipment.correction_tick
            && equipment.correction_tick < equipment.improved_shot_tick
            && equipment.improved_shot_tick <= physical.last_tick
            && finite_physical_scalar(equipment.maximum_plant_seconds)
            && finite_physical_scalar(equipment.maximum_aim_settle_seconds)
            && finite_physical_scalar(equipment.observed_miss_error)
            && finite_physical_scalar(equipment.applied_correction)
            && finite_physical_scalar(equipment.improved_shot_error)
            && equipment.maximum_plant_seconds
                >= minimum_physical_plant_seconds
            && equipment.maximum_aim_settle_seconds
                >= minimum_physical_aim_seconds
            && miss_error > 1.0e-5f
            && physical_abs(equipment.applied_correction) > 1.0e-5f
            && physical_abs(equipment.applied_correction)
                <= maximum_bounded_aim_correction
            && equipment.observed_miss_error * equipment.applied_correction
                < 0.0f
            && improved_error < miss_error
            && equipment.shots_fired >= 2u
            && equipment.shots_missed >= 1u
            && equipment.target_hits >= 1u
            && equipment.shots_missed <= equipment.shots_fired
            && equipment.target_hits
                <= equipment.shots_fired - equipment.shots_missed
            && equipment.improved_shot_hit;
    }

    [[nodiscard]] constexpr bool evidence_valid(const TaskGraph& graph,
        const Evidence& evidence) noexcept
    {
        const auto reference_valid = [&](std::uint16_t index) constexpr noexcept
        {
            return index == invalid_task_index
                || (index < graph.count && graph.nodes[index].enabled
                    && task_supported(graph.nodes[index].task));
        };
        if (!graph.valid() || !reference_valid(evidence.first_stall)
            || !reference_valid(evidence.requested_goal))
            return false;
        for (std::size_t index = 0; index < maximum_task_nodes; ++index)
        {
            const float mastery = evidence.mastery[index];
            if (mastery != mastery || mastery < 0.0f || mastery > 1.0f
                || (index >= graph.count
                    && (mastery != 0.0f || evidence.failures[index] != 0u
                        || !evidence.physical_mastery[index].empty())))
                return false;
            if (index < graph.count)
            {
                const TaskNode& node = graph.nodes[index];
                const bool complete = mastery >= node.minimum_evidence;
                if ((physical_mastery_required(node) && complete
                        && !physical_trial_evidence_valid(node,
                            evidence.physical_mastery[index]))
                    || ((!physical_mastery_required(node) || !complete)
                        && !evidence.physical_mastery[index].empty()))
                    return false;
            }
        }
        return true;
    }

    struct Decision
    {
        std::uint16_t task_index{ invalid_task_index };
        SelectionReason reason{ SelectionReason::no_eligible_task };
        std::uint32_t stable_id{};

        [[nodiscard]] constexpr bool selected() const noexcept
        {
            return task_index != invalid_task_index;
        }
    };

    [[nodiscard]] constexpr Decision select(const TaskGraph& graph,
        const Evidence& evidence, Profile profile) noexcept;

    struct State
    {
        static constexpr std::uint32_t schema_version = 2u;

        std::uint32_t schema{ schema_version };
        Profile profile{ Profile::curriculum };
        TaskGraph graph{};
        Evidence evidence{};
        Decision current{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (schema != schema_version
                || static_cast<unsigned>(profile)
                    > static_cast<unsigned>(Profile::player_guided)
                || !evidence_valid(graph, evidence))
                return false;
            const Decision expected = select(graph, evidence, profile);
            return current.task_index == expected.task_index
                && current.reason == expected.reason
                && current.stable_id == expected.stable_id;
        }
    };

    [[nodiscard]] constexpr bool task_supported(TaskKind task) noexcept
    {
        return task != TaskKind::recover
            && task != TaskKind::construction
            && task != TaskKind::tool_traversal;
    }

    [[nodiscard]] constexpr bool mastery_threshold_met(const TaskNode& node,
        float mastery) noexcept
    {
        return mastery >= node.minimum_evidence;
    }

    [[nodiscard]] constexpr bool task_complete(const TaskGraph& graph,
        const Evidence& evidence, std::size_t index) noexcept
    {
        if (index >= graph.count
            || !mastery_threshold_met(graph.nodes[index],
                evidence.mastery[index]))
            return false;
        return !physical_mastery_required(graph.nodes[index])
            || physical_trial_evidence_valid(graph.nodes[index],
                evidence.physical_mastery[index]);
    }

    [[nodiscard]] constexpr bool prerequisites_met(const TaskGraph& graph,
        const Evidence& evidence, std::size_t index) noexcept
    {
        if (index >= graph.count)
            return false;
        const TaskNode& node = graph.nodes[index];
        for (std::size_t prerequisite = 0;
            prerequisite < node.prerequisite_count; ++prerequisite)
        {
            const std::size_t required = node.prerequisites[prerequisite];
            if (required >= graph.count
                || !task_complete(graph, evidence, required))
                return false;
        }
        return true;
    }

    [[nodiscard]] constexpr Decision select(const TaskGraph& graph,
        const Evidence& evidence, Profile profile) noexcept
    {
        if (static_cast<unsigned>(profile)
                > static_cast<unsigned>(Profile::player_guided)
            || !evidence_valid(graph, evidence))
            return {};
        const auto eligible = [&](std::size_t index) constexpr noexcept
        {
            return index < graph.count && graph.nodes[index].enabled
                && prerequisites_met(graph, evidence, index)
                && !task_complete(graph, evidence, index);
        };

        if (evidence.requested_goal < graph.count
            && eligible(evidence.requested_goal))
        {
            const TaskNode& node = graph.nodes[evidence.requested_goal];
            return { evidence.requested_goal,
                SelectionReason::requested_goal, node.stable_id };
        }
        if (evidence.first_stall < graph.count && eligible(evidence.first_stall))
        {
            const TaskNode& node = graph.nodes[evidence.first_stall];
            return { evidence.first_stall,
                SelectionReason::unresolved_stall, node.stable_id };
        }

        std::uint16_t best = invalid_task_index;
        for (std::uint16_t index = 0u; index < graph.count; ++index)
        {
            if (!eligible(index))
                continue;
            if (best == invalid_task_index)
            {
                best = index;
                continue;
            }
            const bool autonomous = profile == Profile::autonomous_npc;
            const bool player = profile == Profile::player_guided;
            const float candidate_priority = graph.nodes[index].difficulty
                + static_cast<float>(evidence.failures[index])
                    * (autonomous ? -0.08f : 0.10f)
                + (player ? evidence.mastery[index] * 0.05f : 0.0f);
            const float best_priority = graph.nodes[best].difficulty
                + static_cast<float>(evidence.failures[best])
                    * (autonomous ? -0.08f : 0.10f)
                + (player ? evidence.mastery[best] * 0.05f : 0.0f);
            if (candidate_priority < best_priority
                || (candidate_priority == best_priority
                    && graph.nodes[index].stable_id < graph.nodes[best].stable_id))
                best = index;
        }
        if (best == invalid_task_index)
        {
            for (std::size_t index = 0; index < graph.count; ++index)
            {
                if (graph.nodes[index].enabled
                    && !task_complete(graph, evidence, index))
                    return {};
            }
            return { invalid_task_index, SelectionReason::graph_complete, 0u };
        }
        return { best, evidence.failures[best] > 0u
                ? SelectionReason::repeated_failure
                : SelectionReason::prerequisite_ready,
            graph.nodes[best].stable_id };
    }

    enum class TrialOutcome : std::uint8_t
    {
        progress,
        completed,
        failed,
        stalled
    };

    struct TrialEvidence
    {
        std::uint32_t stable_task_id{};
        std::uint16_t task_index{ invalid_task_index };
        TrialOutcome outcome{ TrialOutcome::progress };
        float measured_mastery{};
        PhysicalTrialEvidence physical{};
    };

    struct EvidenceReduction
    {
        Evidence evidence{};
        Decision next{};
        bool accepted{};
    };

    [[nodiscard]] constexpr EvidenceReduction reduce_evidence(
        const TaskGraph& graph, const Evidence& current,
        const TrialEvidence& trial, Profile profile) noexcept
    {
        EvidenceReduction result{ current, {}, false };
        if (!evidence_valid(graph, current)
            || static_cast<unsigned>(profile)
                > static_cast<unsigned>(Profile::player_guided)
            || trial.task_index >= graph.count
            || graph.nodes[trial.task_index].stable_id != trial.stable_task_id
            || static_cast<unsigned>(trial.outcome)
                > static_cast<unsigned>(TrialOutcome::stalled)
            || trial.measured_mastery != trial.measured_mastery
            || trial.measured_mastery < 0.0f
            || trial.measured_mastery > 1.0f)
            return result;

        const TaskNode& node = graph.nodes[trial.task_index];
        if (!node.enabled || !task_supported(node.task)
            || !prerequisites_met(graph, current, trial.task_index)
            || task_complete(graph, current, trial.task_index))
            return result;
        if (trial.outcome == TrialOutcome::completed
            && trial.measured_mastery < node.minimum_evidence)
            return result;
        if (trial.outcome != TrialOutcome::completed
            && trial.measured_mastery >= node.minimum_evidence)
            return result;
        if ((trial.outcome == TrialOutcome::completed
                && !physical_trial_evidence_valid(node, trial.physical))
            || (trial.outcome != TrialOutcome::completed
                && !trial.physical.empty()))
            return result;

        if (trial.outcome == TrialOutcome::completed)
        {
            result.evidence.mastery[trial.task_index] = std::max(
                result.evidence.mastery[trial.task_index], node.minimum_evidence);
            result.evidence.physical_mastery[trial.task_index] = trial.physical;
            if (result.evidence.first_stall == trial.task_index)
                result.evidence.first_stall = invalid_task_index;
        }
        else if (trial.outcome == TrialOutcome::progress)
            result.evidence.mastery[trial.task_index] = std::max(
                result.evidence.mastery[trial.task_index],
                trial.measured_mastery);
        else if (trial.outcome == TrialOutcome::failed
            || trial.outcome == TrialOutcome::stalled)
        {
            std::uint16_t& failures = result.evidence.failures[trial.task_index];
            if (failures != std::numeric_limits<std::uint16_t>::max())
                ++failures;
            if (trial.outcome == TrialOutcome::stalled
                && result.evidence.first_stall == invalid_task_index)
                result.evidence.first_stall = trial.task_index;
        }
        if (result.evidence.requested_goal == trial.task_index
            && trial.outcome == TrialOutcome::completed)
            result.evidence.requested_goal = invalid_task_index;
        result.next = select(graph, result.evidence, profile);
        result.accepted = true;
        return result;
    }

    [[nodiscard]] constexpr std::string_view task_name(TaskKind task) noexcept
    {
        switch (task)
        {
        case TaskKind::stand: return "STAND";
        case TaskKind::walk: return "WALK";
        case TaskKind::speed_walk: return "SPEED WALK";
        case TaskKind::walk_run_transition: return "WALK / RUN TRANSITION";
        case TaskKind::run: return "RUN";
        case TaskKind::crouch_walk: return "CROUCH WALK";
        case TaskKind::wade: return "WADE";
        case TaskKind::swim: return "SWIM";
        case TaskKind::shore_exit: return "SHORE EXIT";
        case TaskKind::safe_carry_walk: return "SAFE CARRY WALK";
        case TaskKind::low_ready_walk: return "LOW READY WALK";
        case TaskKind::stop_and_plant: return "STOP AND PLANT";
        case TaskKind::gun_stance: return "GUN STANCE";
        case TaskKind::acquire_and_aim: return "ACQUIRE AND AIM";
        case TaskKind::fire_and_correct: return "FIRE AND CORRECT";
        case TaskKind::break_contact: return "BREAK CONTACT";
        case TaskKind::recover: return "RECOVER (NOT IMPLEMENTED)";
        case TaskKind::construction: return "CONSTRUCTION (NOT IMPLEMENTED)";
        case TaskKind::tool_traversal: return "TOOL TRAVERSAL (NOT IMPLEMENTED)";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] constexpr std::string_view challenge_name(
        ChallengeKind challenge) noexcept
    {
        switch (challenge)
        {
        case ChallengeKind::flat: return "FLAT";
        case ChallengeKind::loose_cells: return "LOOSE CELLS";
        case ChallengeKind::waterlogged_cells: return "WATERLOGGED CELLS";
        case ChallengeKind::shallow_water: return "SHALLOW WATER";
        case ChallengeKind::shore: return "SHORE";
        case ChallengeKind::step: return "STEP";
        case ChallengeKind::gap: return "GAP";
        case ChallengeKind::ramp: return "RAMP";
        case ChallengeKind::ledge: return "LEDGE";
        case ChallengeKind::low_clearance: return "LOW CLEARANCE";
        case ChallengeKind::aim_range: return "AIM RANGE";
        case ChallengeKind::moving_target: return "MOVING TARGET";
        case ChallengeKind::construction_anchor: return "CONSTRUCTION ANCHOR";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] constexpr std::string_view reason_name(
        SelectionReason reason) noexcept
    {
        switch (reason)
        {
        case SelectionReason::requested_goal: return "REQUESTED GOAL";
        case SelectionReason::unresolved_stall: return "UNRESOLVED FIRST STALL";
        case SelectionReason::repeated_failure: return "RETRY AFTER PHYSICAL FAILURE";
        case SelectionReason::prerequisite_ready: return "PREREQUISITES SATISFIED";
        case SelectionReason::graph_complete: return "TASK GRAPH COMPLETE";
        case SelectionReason::no_eligible_task: return "NO ELIGIBLE PHYSICAL TASK";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] constexpr TaskGraph default_training_graph() noexcept
    {
        TaskGraph graph{};
        const auto add = [&](TaskKind task, ChallengeKind challenge,
            std::uint32_t stable_challenge_id, float evidence, float difficulty,
            std::initializer_list<std::uint16_t> prerequisites,
            bool enabled = true) constexpr
        {
            TaskNode& node = graph.nodes[graph.count];
            node.stable_id = 0xE2500000u + graph.count + 1u;
            node.stable_challenge_id = stable_challenge_id;
            node.task = task;
            node.challenge = challenge;
            node.minimum_evidence = evidence;
            node.difficulty = difficulty;
            node.enabled = enabled;
            for (const std::uint16_t prerequisite : prerequisites)
                node.prerequisites[node.prerequisite_count++] = prerequisite;
            ++graph.count;
        };
        constexpr auto feature_id = [](ChallengeKind challenge) constexpr
        {
            return 0xE2D10000u + static_cast<std::uint32_t>(challenge) + 1u;
        };
        add(TaskKind::stand, ChallengeKind::flat,
            0xE2D00001u, 1.0f, 0.05f, {});
        add(TaskKind::walk, ChallengeKind::flat,
            0xE2D00001u, 1.0f, 0.10f, { 0u });
        add(TaskKind::walk, ChallengeKind::loose_cells,
            0xE2D00002u, 1.0f, 0.20f, { 1u });
        add(TaskKind::walk, ChallengeKind::waterlogged_cells,
            0xE2D00004u, 1.0f, 0.24f, { 2u });
        add(TaskKind::wade, ChallengeKind::shallow_water,
            0xE2D00005u, 1.0f, 0.32f, { 3u });
        add(TaskKind::shore_exit, ChallengeKind::shore,
            0xE2D0000Au, 1.0f, 0.38f, { 4u });
        add(TaskKind::crouch_walk, ChallengeKind::low_clearance,
            feature_id(ChallengeKind::low_clearance),
            1.0f, 0.42f, { 1u });
        add(TaskKind::speed_walk, ChallengeKind::ramp,
            0xE2D0000Cu, 1.0f, 0.48f, { 5u });
        add(TaskKind::walk_run_transition, ChallengeKind::step,
            0xE2D0000Bu, 1.0f, 0.56f, { 7u });
        add(TaskKind::run, ChallengeKind::gap,
            0xE2D00006u, 1.0f, 0.64f, { 8u });
        add(TaskKind::safe_carry_walk, ChallengeKind::flat,
            0xE2D00001u, 1.0f, 0.42f, { 1u });
        add(TaskKind::low_ready_walk, ChallengeKind::flat,
            0xE2D00001u, 1.0f, 0.50f, { 10u });
        add(TaskKind::stop_and_plant, ChallengeKind::aim_range,
            feature_id(ChallengeKind::aim_range),
            1.0f, 0.54f, { 11u });
        add(TaskKind::gun_stance, ChallengeKind::aim_range,
            feature_id(ChallengeKind::aim_range),
            1.0f, 0.58f, { 12u });
        add(TaskKind::acquire_and_aim, ChallengeKind::aim_range,
            feature_id(ChallengeKind::aim_range),
            1.0f, 0.64f, { 13u });
        add(TaskKind::fire_and_correct, ChallengeKind::aim_range,
            feature_id(ChallengeKind::aim_range),
            1.0f, 0.72f, { 14u });
        add(TaskKind::construction, ChallengeKind::construction_anchor,
            feature_id(ChallengeKind::construction_anchor),
            1.0f, 0.82f, { 5u }, false);
        add(TaskKind::tool_traversal, ChallengeKind::ledge,
            feature_id(ChallengeKind::ledge),
            1.0f, 0.90f, { 16u }, false);
        add(TaskKind::swim, ChallengeKind::shallow_water,
            0xE2D00005u, 1.0f, 0.46f, { 4u });
        add(TaskKind::break_contact, ChallengeKind::aim_range,
            feature_id(ChallengeKind::aim_range),
            1.0f, 0.76f, { 13u });
        add(TaskKind::recover, ChallengeKind::ledge,
            feature_id(ChallengeKind::ledge),
            1.0f, 0.30f, { 0u }, false);
        return graph;
    }

    [[nodiscard]] constexpr State make_state(Profile profile) noexcept
    {
        State state{};
        state.profile = profile;
        state.graph = default_training_graph();
        state.current = select(state.graph, state.evidence, state.profile);
        return state;
    }
}
