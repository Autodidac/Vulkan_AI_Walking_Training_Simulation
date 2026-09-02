#pragma once

#include "ai_director.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
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
        std::ofstream output(temporary, std::ios::trunc);
        if (!output)
        {
            error = "Could not open Director state for writing: "
                + temporary.string();
            return false;
        }
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
                << static_cast<unsigned>(node.task) << ' '
                << static_cast<unsigned>(node.challenge) << ' '
                << static_cast<unsigned>(node.prerequisite_count) << ' '
                << node.minimum_evidence << ' ' << node.difficulty << ' '
                << static_cast<unsigned>(node.enabled) << ' '
                << state.evidence.mastery[index] << ' '
                << state.evidence.failures[index];
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
        std::error_code filesystem_error{};
        std::filesystem::remove(path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace Director state atomically: "
                + filesystem_error.message();
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool load_state(const std::filesystem::path& path,
        State& state, std::string& error)
    {
        std::ifstream input(path);
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
            input >> node.stable_id >> task >> challenge >> prerequisite_count
                >> node.minimum_evidence >> node.difficulty >> enabled
                >> loaded.evidence.mastery[index]
                >> loaded.evidence.failures[index];
            if (!input
                || task > static_cast<unsigned>(TaskKind::tool_traversal)
                || challenge > static_cast<unsigned>(ChallengeKind::construction_anchor)
                || prerequisite_count > maximum_prerequisites
                || enabled > 1u)
            {
                error = "Director task node is malformed.";
                return false;
            }
            node.task = static_cast<TaskKind>(task);
            node.challenge = static_cast<ChallengeKind>(challenge);
            node.prerequisite_count = static_cast<std::uint8_t>(prerequisite_count);
            node.enabled = enabled != 0u;
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
