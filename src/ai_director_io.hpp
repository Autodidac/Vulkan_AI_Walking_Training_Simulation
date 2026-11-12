#pragma once

#include "ai_director.hpp"
#include "atomic_file.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string>

namespace runner::director
{
    [[nodiscard]] inline bool save_state(const std::filesystem::path& path,
        const State& state, std::string& error)
    {
        if (path.empty() || !state.valid())
        {
            error = "Director state path or graph is invalid.";
            return false;
        }
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open Director state for writing: "
                + temporary.string();
            return false;
        }
        output.imbue(std::locale::classic());
        output << "EPOCH2DDIRECTOR " << State::schema_version << '\n'
            << static_cast<unsigned>(state.profile) << ' '
            << state.graph.count << ' ' << state.evidence.first_stall << ' '
            << state.evidence.requested_goal << ' '
            << state.current.task_index << ' '
            << static_cast<unsigned>(state.current.reason) << ' '
            << state.current.stable_id << '\n'
            << std::setprecision(std::numeric_limits<float>::max_digits10);
        for (std::size_t index = 0u; index < state.graph.count; ++index)
        {
            const TaskNode& node = state.graph.nodes[index];
            output << node.stable_id << ' '
                << node.stable_challenge_id << ' '
                << static_cast<unsigned>(node.task) << ' '
                << static_cast<unsigned>(node.challenge) << ' '
                << static_cast<unsigned>(node.prerequisite_count) << ' '
                << node.minimum_evidence << ' ' << node.difficulty << ' '
                << static_cast<unsigned>(node.enabled) << ' '
                << state.evidence.mastery[index] << ' '
                << state.evidence.failures[index];
            const PhysicalTrialEvidence& physical =
                state.evidence.physical_mastery[index];
            output << ' ' << physical.schema
                << ' ' << static_cast<unsigned>(physical.kind)
                << ' ' << physical.trial_id
                << ' ' << physical.stable_task_id
                << ' ' << physical.stable_challenge_id
                << ' ' << physical.first_tick
                << ' ' << physical.last_tick
                << ' ' << physical.water.approach_tick
                << ' ' << physical.water.entry_tick
                << ' ' << physical.water.wading_tick
                << ' ' << physical.water.swimming_tick
                << ' ' << physical.water.shore_exit_tick
                << ' ' << physical.water.far_shore_dry_tick
                << ' ' << physical.water.maximum_depth
                << ' ' << physical.water.maximum_submersion
                << ' ' << physical.equipment.two_foot_plant_tick
                << ' ' << physical.equipment.physical_aim_tick
                << ' ' << physical.equipment.miss_tick
                << ' ' << physical.equipment.correction_tick
                << ' ' << physical.equipment.improved_shot_tick
                << ' ' << physical.equipment.maximum_plant_seconds
                << ' ' << physical.equipment.maximum_aim_settle_seconds
                << ' ' << physical.equipment.observed_miss_error
                << ' ' << physical.equipment.applied_correction
                << ' ' << physical.equipment.improved_shot_error
                << ' ' << physical.equipment.shots_fired
                << ' ' << physical.equipment.shots_missed
                << ' ' << physical.equipment.target_hits
                << ' ' << static_cast<unsigned>(
                    physical.equipment.improved_shot_hit);
            for (std::size_t prerequisite = 0u;
                prerequisite < node.prerequisite_count; ++prerequisite)
                output << ' ' << node.prerequisites[prerequisite];
            output << '\n';
        }
        output.close();
        if (!output)
        {
            error = "Failed while writing Director state: " + temporary.string();
            return false;
        }
        if (!persistence::atomic_replace_file(temporary, path, error))
        {
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool load_state(const std::filesystem::path& path,
        State& state, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        input.imbue(std::locale::classic());
        std::string magic{};
        std::uint32_t schema{};
        unsigned profile{};
        unsigned reason{};
        State loaded{};
        input >> magic >> schema >> profile >> loaded.graph.count
            >> loaded.evidence.first_stall >> loaded.evidence.requested_goal
            >> loaded.current.task_index >> reason >> loaded.current.stable_id;
        if (!input || magic != "EPOCH2DDIRECTOR"
            || schema != State::schema_version
            || profile > static_cast<unsigned>(Profile::player_guided)
            || reason > static_cast<unsigned>(SelectionReason::no_eligible_task)
            || loaded.graph.count == 0u
            || loaded.graph.count > maximum_task_nodes)
        {
            error = "Director state header is invalid or incompatible.";
            return false;
        }
        loaded.schema = schema;
        loaded.profile = static_cast<Profile>(profile);
        loaded.current.reason = static_cast<SelectionReason>(reason);
        for (std::size_t index = 0u; index < loaded.graph.count; ++index)
        {
            TaskNode& node = loaded.graph.nodes[index];
            unsigned task{};
            unsigned challenge{};
            unsigned prerequisite_count{};
            unsigned enabled{};
            unsigned physical_kind{};
            unsigned improved_shot_hit{};
            PhysicalTrialEvidence& physical =
                loaded.evidence.physical_mastery[index];
            input >> node.stable_id >> node.stable_challenge_id
                >> task >> challenge >> prerequisite_count
                >> node.minimum_evidence >> node.difficulty >> enabled
                >> loaded.evidence.mastery[index]
                >> loaded.evidence.failures[index]
                >> physical.schema >> physical_kind
                >> physical.trial_id >> physical.stable_task_id
                >> physical.stable_challenge_id
                >> physical.first_tick >> physical.last_tick
                >> physical.water.approach_tick
                >> physical.water.entry_tick
                >> physical.water.wading_tick
                >> physical.water.swimming_tick
                >> physical.water.shore_exit_tick
                >> physical.water.far_shore_dry_tick
                >> physical.water.maximum_depth
                >> physical.water.maximum_submersion
                >> physical.equipment.two_foot_plant_tick
                >> physical.equipment.physical_aim_tick
                >> physical.equipment.miss_tick
                >> physical.equipment.correction_tick
                >> physical.equipment.improved_shot_tick
                >> physical.equipment.maximum_plant_seconds
                >> physical.equipment.maximum_aim_settle_seconds
                >> physical.equipment.observed_miss_error
                >> physical.equipment.applied_correction
                >> physical.equipment.improved_shot_error
                >> physical.equipment.shots_fired
                >> physical.equipment.shots_missed
                >> physical.equipment.target_hits
                >> improved_shot_hit;
            if (!input
                || task > static_cast<unsigned>(TaskKind::tool_traversal)
                || challenge > static_cast<unsigned>(ChallengeKind::construction_anchor)
                || prerequisite_count > maximum_prerequisites
                || enabled > 1u
                || physical_kind
                    > static_cast<unsigned>(PhysicalEvidenceKind::equipment_correction)
                || improved_shot_hit > 1u)
            {
                error = "Director task node is malformed.";
                return false;
            }
            node.task = static_cast<TaskKind>(task);
            node.challenge = static_cast<ChallengeKind>(challenge);
            node.prerequisite_count = static_cast<std::uint8_t>(prerequisite_count);
            node.enabled = enabled != 0u;
            physical.kind = static_cast<PhysicalEvidenceKind>(physical_kind);
            physical.equipment.improved_shot_hit = improved_shot_hit != 0u;
            for (std::size_t prerequisite = 0u;
                prerequisite < node.prerequisite_count; ++prerequisite)
                input >> node.prerequisites[prerequisite];
            if (!input)
            {
                error = "Director prerequisite list is malformed.";
                return false;
            }
        }
        std::string trailing{};
        if (!loaded.valid() || (input >> trailing))
        {
            error = "Director state failed graph validation.";
            return false;
        }
        state = loaded;
        error.clear();
        return true;
    }
}
