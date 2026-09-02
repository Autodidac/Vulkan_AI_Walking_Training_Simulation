#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

namespace runner::director
{
    inline constexpr std::size_t maximum_task_nodes = 32u;
    inline constexpr std::size_t maximum_prerequisites = 4u;
    inline constexpr std::uint16_t invalid_task_index = 0xffffu;

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

    struct TaskNode
    {
        std::uint32_t stable_id{};
        TaskKind task{ TaskKind::stand };
        ChallengeKind challenge{ ChallengeKind::flat };
        std::array<std::uint16_t, maximum_prerequisites> prerequisites{};
        std::uint8_t prerequisite_count{};
        float minimum_evidence{};
        float difficulty{};
        bool enabled{ true };
    };

    struct TaskGraph
    {
        std::array<TaskNode, maximum_task_nodes> nodes{};
        std::uint16_t count{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (count == 0u || count > nodes.size())
                return false;
            for (std::size_t index = 0; index < count; ++index)
            {
                const TaskNode& node = nodes[index];
                if (node.stable_id == 0u
                    || node.prerequisite_count > node.prerequisites.size())
                    return false;
                for (std::size_t prerequisite = 0;
                    prerequisite < node.prerequisite_count; ++prerequisite)
                {
                    if (node.prerequisites[prerequisite] >= count
                        || node.prerequisites[prerequisite] == index)
                        return false;
                }
            }
            return true;
        }
    };

    struct Evidence
    {
        std::array<float, maximum_task_nodes> mastery{};
        std::array<std::uint16_t, maximum_task_nodes> failures{};
        std::uint16_t first_stall{ invalid_task_index };
        std::uint16_t requested_goal{ invalid_task_index };
    };

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

    struct State
    {
        static constexpr std::uint32_t schema_version = 1u;

        std::uint32_t schema{ schema_version };
        Profile profile{ Profile::curriculum };
        TaskGraph graph{};
        Evidence evidence{};
        Decision current{};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            if (schema != schema_version || !graph.valid())
                return false;
            if (!current.selected())
                return current.stable_id == 0u;
            return current.task_index < graph.count
                && graph.nodes[current.task_index].enabled
                && current.stable_id == graph.nodes[current.task_index].stable_id;
        }
    };

    [[nodiscard]] constexpr bool task_supported(TaskKind task) noexcept
    {
        return task != TaskKind::construction && task != TaskKind::tool_traversal;
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
                || evidence.mastery[required]
                    < graph.nodes[required].minimum_evidence)
                return false;
        }
        return true;
    }

    [[nodiscard]] constexpr bool task_complete(const TaskNode& node,
        float mastery) noexcept
    {
        return mastery >= node.minimum_evidence;
    }

    [[nodiscard]] constexpr Decision select(const TaskGraph& graph,
        const Evidence& evidence, Profile profile) noexcept
    {
        if (!graph.valid())
            return {};
        const auto eligible = [&](std::size_t index) constexpr noexcept
        {
            return index < graph.count && graph.nodes[index].enabled
                && prerequisites_met(graph, evidence, index)
                && !task_complete(graph.nodes[index], evidence.mastery[index]);
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
                    && !task_complete(graph.nodes[index], evidence.mastery[index]))
                    return {};
            }
            return { invalid_task_index, SelectionReason::graph_complete, 0u };
        }
        return { best, evidence.failures[best] > 0u
                ? SelectionReason::repeated_failure
                : SelectionReason::prerequisite_ready,
            graph.nodes[best].stable_id };
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
        case TaskKind::recover: return "RECOVER";
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
            float evidence, float difficulty,
            std::initializer_list<std::uint16_t> prerequisites,
            bool enabled = true) constexpr
        {
            TaskNode& node = graph.nodes[graph.count];
            node.stable_id = 0xE2500000u + graph.count + 1u;
            node.task = task;
            node.challenge = challenge;
            node.minimum_evidence = evidence;
            node.difficulty = difficulty;
            node.enabled = enabled;
            for (const std::uint16_t prerequisite : prerequisites)
                node.prerequisites[node.prerequisite_count++] = prerequisite;
            ++graph.count;
        };
        add(TaskKind::stand, ChallengeKind::flat, 1.0f, 0.05f, {});
        add(TaskKind::walk, ChallengeKind::flat, 1.0f, 0.10f, { 0u });
        add(TaskKind::walk, ChallengeKind::loose_cells, 1.0f, 0.20f, { 1u });
        add(TaskKind::walk, ChallengeKind::waterlogged_cells, 1.0f, 0.24f, { 2u });
        add(TaskKind::wade, ChallengeKind::shallow_water, 1.0f, 0.32f, { 3u });
        add(TaskKind::shore_exit, ChallengeKind::shore, 1.0f, 0.38f, { 4u });
        add(TaskKind::crouch_walk, ChallengeKind::low_clearance,
            1.0f, 0.42f, { 1u });
        add(TaskKind::speed_walk, ChallengeKind::ramp, 1.0f, 0.48f, { 5u });
        add(TaskKind::walk_run_transition, ChallengeKind::step,
            1.0f, 0.56f, { 7u });
        add(TaskKind::run, ChallengeKind::gap, 1.0f, 0.64f, { 8u });
        add(TaskKind::safe_carry_walk, ChallengeKind::flat,
            1.0f, 0.42f, { 1u });
        add(TaskKind::low_ready_walk, ChallengeKind::aim_range,
            1.0f, 0.50f, { 10u });
        add(TaskKind::stop_and_plant, ChallengeKind::aim_range,
            1.0f, 0.54f, { 11u });
        add(TaskKind::gun_stance, ChallengeKind::aim_range,
            1.0f, 0.58f, { 12u });
        add(TaskKind::acquire_and_aim, ChallengeKind::moving_target,
            1.0f, 0.64f, { 13u });
        add(TaskKind::fire_and_correct, ChallengeKind::moving_target,
            1.0f, 0.72f, { 14u });
        add(TaskKind::construction, ChallengeKind::construction_anchor,
            1.0f, 0.82f, { 5u }, false);
        add(TaskKind::tool_traversal, ChallengeKind::ledge,
            1.0f, 0.90f, { 16u }, false);
        add(TaskKind::swim, ChallengeKind::shallow_water,
            1.0f, 0.46f, { 4u });
        add(TaskKind::break_contact, ChallengeKind::moving_target,
            1.0f, 0.76f, { 13u });
        add(TaskKind::recover, ChallengeKind::ledge,
            1.0f, 0.30f, { 0u });
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
