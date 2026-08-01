from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_exact(path: str, old: str, new: str, expected: int = 1) -> None:
    text = read(path)
    count = text.count(old)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} matches, found {count}: {old[:80]!r}")
    write(path, text.replace(old, new))


def replace_regex(path: str, pattern: str, replacement: str, expected: int = 1) -> None:
    text = read(path)
    updated, count = re.subn(pattern, replacement, text, flags=re.DOTALL)
    if count != expected:
        raise RuntimeError(f"{path}: expected {expected} regex matches, found {count}: {pattern[:80]!r}")
    write(path, updated)


# ---------------------------------------------------------------------------
# Version, tests, and sanitizer configuration.
# ---------------------------------------------------------------------------
replace_exact("CMakeLists.txt", "project(EpochRunner VERSION 0.6.6 LANGUAGES CXX)",
              "project(EpochRunner VERSION 0.7.0 LANGUAGES CXX)")
replace_exact(
    "CMakeLists.txt",
    'option(EPOCHRUNNER_BUILD_TESTS "Build deterministic core tests." ON)\n',
    'option(EPOCHRUNNER_BUILD_TESTS "Build deterministic core tests." ON)\n'
    'option(EPOCHRUNNER_ENABLE_TSAN "Enable ThreadSanitizer for CPU-only tests." OFF)\n'
)
replace_exact(
    "CMakeLists.txt",
    "epochrunner_enable_warnings(EpochRunnerCore)\n",
    "epochrunner_enable_warnings(EpochRunnerCore)\n\n"
    "if(EPOCHRUNNER_ENABLE_TSAN AND NOT MSVC)\n"
    "    target_compile_options(EpochRunnerCore PRIVATE -fsanitize=thread -fno-omit-frame-pointer)\n"
    "    target_link_options(EpochRunnerCore PRIVATE -fsanitize=thread)\n"
    "endif()\n"
)
replace_exact(
    "CMakeLists.txt",
    "    add_test(NAME EpochRunner.ConcurrencyBenchmark COMMAND EpochRunnerConcurrencyBenchmark)\n"
    "    set_tests_properties(EpochRunner.ConcurrencyBenchmark PROPERTIES TIMEOUT 30)\n",
    "    add_test(NAME EpochRunner.ConcurrencyBenchmark COMMAND EpochRunnerConcurrencyBenchmark)\n"
    "    set_tests_properties(EpochRunner.ConcurrencyBenchmark PROPERTIES TIMEOUT 45)\n\n"
    "    add_executable(EpochRunnerRuntimePipelineTests tests/runtime_pipeline_tests.cpp)\n"
    "    target_link_libraries(EpochRunnerRuntimePipelineTests PRIVATE EpochRunner::Core)\n"
    "    target_compile_features(EpochRunnerRuntimePipelineTests PRIVATE cxx_std_23)\n"
    "    set_target_properties(EpochRunnerRuntimePipelineTests PROPERTIES\n"
    "        CXX_STANDARD 23\n"
    "        CXX_STANDARD_REQUIRED ON\n"
    "        CXX_EXTENSIONS OFF\n"
    "    )\n"
    "    epochrunner_enable_warnings(EpochRunnerRuntimePipelineTests)\n"
    "    if(EPOCHRUNNER_ENABLE_TSAN AND NOT MSVC)\n"
    "        target_compile_options(EpochRunnerRuntimePipelineTests PRIVATE -fsanitize=thread -fno-omit-frame-pointer)\n"
    "        target_link_options(EpochRunnerRuntimePipelineTests PRIVATE -fsanitize=thread)\n"
    "    endif()\n"
    "    add_test(NAME EpochRunner.RuntimePipeline COMMAND EpochRunnerRuntimePipelineTests)\n"
    "    set_tests_properties(EpochRunner.RuntimePipeline PROPERTIES TIMEOUT 90)\n"
)

# ---------------------------------------------------------------------------
# Eight policy outputs and independently controlled humanoid arms.
# ---------------------------------------------------------------------------
replace_exact("src/simulation.hpp", "inline constexpr std::size_t action_count = 4;",
              "inline constexpr std::size_t action_count = 8;")
replace_exact(
    "src/simulation.hpp",
    "        std::array<MotorConstraint, action_count> motors{};\n\n"
    "        std::uint16_t root_node{};",
    "        std::array<MotorConstraint, action_count> motors{};\n"
    "        std::size_t active_motor_count{ 4 };\n\n"
    "        std::uint16_t root_node{};"
)
replace_exact(
    "src/simulation.cpp",
    "            for (std::size_t index = 0; index < action_count; ++index)\n"
    "            {\n"
    "                const MotorConstraint& motor = rig.motors[index];",
    "            for (std::size_t index = 0; index < rig.active_motor_count; ++index)\n"
    "            {\n"
    "                const MotorConstraint& motor = rig.motors[index];"
)
replace_exact(
    "src/simulation.cpp",
    "            for (std::size_t index = 0; index < action_count; ++index)\n"
    "                rig.calibrate_motor(index, travel, travel, 0.043f);",
    "            for (std::size_t index = 0; index < rig.active_motor_count; ++index)\n"
    "                rig.calibrate_motor(index, travel, travel, 0.043f);"
)
replace_regex(
    "src/simulation.cpp",
    r"    CreatureBlueprint CreatureBlueprint::humanoid\(\)\n    \{.*?\n    \}\n\n    CreatureBlueprint CreatureBlueprint::quadruped",
    '''    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0148f, 4.0173f }, { -0.010f, 4.86f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },
            { -0.42f, 4.06f }, { -0.78f, 3.48f }, { -0.60f, 2.82f },
            { 0.40f, 4.06f }, { 0.76f, 3.48f }, { 0.58f, 2.82f }
        };
        result.radii = {
            0.26f, 0.31f, 0.27f, 0.19f, 0.17f, 0.19f, 0.17f,
            0.16f, 0.15f, 0.14f, 0.16f, 0.15f, 0.14f
        };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f },
            { 1, 7, 0.0f, 0.98f }, { 7, 8, 0.0f, 0.98f }, { 8, 9, 0.0f, 0.96f },
            { 1, 10, 0.0f, 0.98f }, { 10, 11, 0.0f, 0.98f }, { 11, 12, 0.0f, 0.96f },
            { 7, 10, 0.0f, 0.72f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 },
            MotorConstraint{ 1, 7, 8 }, MotorConstraint{ 7, 8, 9 },
            MotorConstraint{ 1, 10, 11 }, MotorConstraint{ 10, 11, 12 }
        };
        result.active_motor_count = 8;
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        for (std::size_t index = 0; index < 4; ++index)
        {
            const bool knee = (index & 1u) != 0u;
            result.calibrate_motor(index, knee ? 58.0f : 36.0f,
                knee ? 58.0f : 36.0f, knee ? 0.051f : 0.045f);
        }
        result.calibrate_motor(4, 95.0f, 95.0f, 0.034f);
        result.calibrate_motor(5, 108.0f, 108.0f, 0.031f);
        result.calibrate_motor(6, 95.0f, 95.0f, 0.034f);
        result.calibrate_motor(7, 108.0f, 108.0f, 0.031f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped'''
)
replace_exact(
    "src/simulation.cpp",
    "        if (motor_index >= motors.size())\n            return 0.0f;",
    "        if (motor_index >= active_motor_count || motor_index >= motors.size())\n            return 0.0f;"
)
replace_exact(
    "src/simulation.cpp",
    "        if (motor_index >= motors.size())\n            return;",
    "        if (motor_index >= active_motor_count || motor_index >= motors.size())\n            return;"
)
replace_exact(
    "src/simulation.cpp",
    "        for (std::size_t index = 0; index < motors.size(); ++index)\n"
    "            calibrate_motor(index, degrees, degrees, power);",
    "        for (std::size_t index = 0; index < active_motor_count; ++index)\n"
    "            calibrate_motor(index, degrees, degrees, power);"
)
replace_exact(
    "src/simulation.cpp",
    "        if (nodes.size() < 3 || radii.size() != nodes.size() || bones.empty())\n"
    "            return false;",
    "        if (nodes.size() < 3 || radii.size() != nodes.size() || bones.empty()\n"
    "            || active_motor_count == 0 || active_motor_count > motors.size())\n"
    "            return false;"
)
replace_exact(
    "src/simulation.cpp",
    "        for (const MotorConstraint& motor : motors)\n"
    "        {\n"
    "            if (!motor.enabled)",
    "        for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)\n"
    "        {\n"
    "            const MotorConstraint& motor = motors[motor_index];\n"
    "            if (!motor.enabled)"
)
replace_exact(
    "src/simulation.cpp",
    "        add_u64(nodes.size()); add_u64(bones.size()); add_u64(motors.size());",
    "        add_u64(nodes.size()); add_u64(bones.size()); add_u64(active_motor_count);"
)
replace_exact(
    "src/simulation.cpp",
    "        for (const MotorConstraint& motor : motors)\n"
    "        {\n"
    "            add_u64(motor.a);",
    "        for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)\n"
    "        {\n"
    "            const MotorConstraint& motor = motors[motor_index];\n"
    "            add_u64(motor.a);"
)
replace_exact("src/simulation.cpp", 'output << "EPOCHRIG 3\\n";',
              'output << "EPOCHRIG 4\\n";')
replace_exact(
    "src/simulation.cpp",
    "            output << nodes.size() << ' ' << bones.size() << ' ' << motors.size() << '\\n';",
    "            output << nodes.size() << ' ' << bones.size() << ' ' << active_motor_count << '\\n';"
)
replace_exact(
    "src/simulation.cpp",
    "            for (const MotorConstraint& motor : motors)\n"
    "            {\n"
    "                output << \"M \"",
    "            for (std::size_t motor_index = 0; motor_index < active_motor_count; ++motor_index)\n"
    "            {\n"
    "                const MotorConstraint& motor = motors[motor_index];\n"
    "                output << \"M \""
)
replace_exact(
    "src/simulation.cpp",
    "        if (!input || magic != \"EPOCHRIG\" || (version != 1 && version != 2 && version != 3)\n"
    "            || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)",
    "        if (!input || magic != \"EPOCHRIG\"\n"
    "            || (version != 1 && version != 2 && version != 3 && version != 4)\n"
    "            || node_count < 3 || node_count > 128 || bone_count > 256\n"
    "            || (motor_count != 4 && motor_count != action_count))"
)
replace_exact(
    "src/simulation.cpp",
    "        CreatureBlueprint result{};\n        if (version >= 2)",
    "        CreatureBlueprint result{};\n"
    "        result.active_motor_count = motor_count;\n"
    "        for (MotorConstraint& motor : result.motors)\n"
    "            motor.enabled = false;\n"
    "        if (version >= 2)"
)
replace_exact(
    "src/simulation.cpp",
    "        for (std::size_t index = 0; index < action_count; ++index)\n"
    "            previous_angles_[index] = joint_angle(blueprint_.motors[index]);",
    "        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n"
    "            previous_angles_[index] = joint_angle(blueprint_.motors[index]);"
)
replace_exact(
    "src/simulation.cpp",
    "        for (std::size_t index = 0; index < action_count; ++index)\n"
    "        {\n"
    "            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;",
    "        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n"
    "        {\n"
    "            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;"
)
replace_exact(
    "src/simulation.cpp",
    "            for (std::size_t index = 0; index < action_count; ++index)\n"
    "                solve_motor(blueprint_.motors[index], applied_actions[index]);",
    "            for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n"
    "                solve_motor(blueprint_.motors[index], applied_actions[index]);"
)
replace_exact(
    "src/simulation.cpp",
    "        for (std::size_t index = 0; index < action_count; ++index)\n"
    "        {\n"
    "            const float effective = blueprint_.motors[index].enabled ? applied_actions[index] : 0.0f;",
    "        for (std::size_t index = 0; index < blueprint_.active_motor_count; ++index)\n"
    "        {\n"
    "            const float effective = blueprint_.motors[index].enabled ? applied_actions[index] : 0.0f;"
)

# Rig lab exposes all active motors and gives humanoid arm names.
replace_exact(
    "src/app.cpp",
    "        [[nodiscard]] std::array<std::string_view, 4> motor_names() const noexcept\n"
    "        {\n"
    "            switch (rig_preset)",
    "        [[nodiscard]] std::array<std::string_view, sim::action_count> motor_names() const noexcept\n"
    "        {\n"
    "            switch (rig_preset)"
)
replace_regex(
    "src/app.cpp",
    r"        \[\[nodiscard\]\] std::array<std::string_view, sim::action_count> motor_names\(\) const noexcept\n        \{.*?\n        \}\n\n        void set_status",
    '''        [[nodiscard]] std::array<std::string_view, sim::action_count> motor_names() const noexcept
        {
            switch (rig_preset)
            {
            case RigPreset::quadruped:
                return { "REAR NEAR LEG", "REAR FAR LEG", "FRONT NEAR LEG", "FRONT FAR LEG",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::crawler4:
                return { "REAR LEG", "MID-REAR LEG", "MID-FRONT LEG", "FRONT LEG",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::hexapod:
                return { "REAR PAIR A", "REAR PAIR B", "MID PAIR", "FRONT PAIR",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::monoped:
                return { "HIP", "KNEE", "LEFT FOOT", "RIGHT FOOT",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::humanoid:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",
                    "LEFT SHOULDER", "LEFT ELBOW", "RIGHT SHOULDER", "RIGHT ELBOW" };
            case RigPreset::biped:
            case RigPreset::chicken:
                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",
                    "UNUSED 5", "UNUSED 6", "UNUSED 7", "UNUSED 8" };
            case RigPreset::custom:
                return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4",
                    "MOTOR 5", "MOTOR 6", "MOTOR 7", "MOTOR 8" };
            }
            return { "MOTOR 1", "MOTOR 2", "MOTOR 3", "MOTOR 4",
                "MOTOR 5", "MOTOR 6", "MOTOR 7", "MOTOR 8" };
        }

        void set_status'''
)
replace_exact(
    "src/app.cpp",
    "            case JointTestGroup::pair_a: return index < 2;\n"
    "            case JointTestGroup::pair_b: return index >= 2;",
    "            case JointTestGroup::pair_a: return index < 4;\n"
    "            case JointTestGroup::pair_b: return index >= 4;"
)
replace_exact(
    "src/app.cpp",
    "        std::filesystem::path autosave_policy_path{ \"epochrunner-v066-skill-autosave.eppo\" };\n"
    "        std::filesystem::path autosave_rig_path{ \"epochrunner-v066-skill-evolved.epochrig\" };\n"
    "        std::filesystem::path autosave_state_path{ \"epochrunner-v066-skill-autonomy.state\" };",
    "        std::filesystem::path autosave_policy_path{ \"epochrunner-v070-autosave.eppo\" };\n"
    "        std::filesystem::path autosave_rig_path{ \"epochrunner-v070-evolved.epochrig\" };\n"
    "        std::filesystem::path autosave_state_path{ \"epochrunner-v070-autonomy.state\" };"
)
replace_exact(
    "src/app.cpp",
    "                const float motor_width = (rect.size.x - 48.0f) * 0.25f;\n"
    "                for (int index = 0; index < 4; ++index)\n"
    "                {\n"
    "                    if (button({ cursor + Vec2{ motor_width * static_cast<float>(index), 0.0f },\n"
    "                        { motor_width - 4.0f, 35.0f } }, std::to_string(index + 1), input, selected_motor == index))\n"
    "                    {\n"
    "                        selected_motor = index;\n"
    "                        joint_test_group = JointTestGroup::selected;\n"
    "                    }\n"
    "                }\n"
    "                cursor.y += 46.0f;",
    "                const float motor_width = (rect.size.x - 48.0f) * 0.25f;\n"
    "                for (int index = 0; index < static_cast<int>(sim::action_count); ++index)\n"
    "                {\n"
    "                    const int column = index % 4;\n"
    "                    const int row = index / 4;\n"
    "                    const bool motor_available = static_cast<std::size_t>(index) < blueprint.active_motor_count;\n"
    "                    if (button({ cursor + Vec2{ motor_width * static_cast<float>(column),\n"
    "                        static_cast<float>(row) * 41.0f }, { motor_width - 4.0f, 35.0f } },\n"
    "                        std::to_string(index + 1), input, selected_motor == index, motor_available))\n"
    "                    {\n"
    "                        selected_motor = index;\n"
    "                        joint_test_group = JointTestGroup::selected;\n"
    "                    }\n"
    "                }\n"
    "                cursor.y += 87.0f;"
)

# ---------------------------------------------------------------------------
# Staged PPO update and immutable checkpoint data.
# ---------------------------------------------------------------------------
replace_exact(
    "src/ppo.hpp",
    "    class PpoTrainer\n    {\n    public:\n",
    '''    class PpoTrainer
    {
    public:
        struct CheckpointData
        {
            std::uint64_t rig_signature{};
            std::vector<float> parameters{};
            std::vector<float> first_moment{};
            std::vector<float> second_moment{};
            std::vector<float> best_parameters{};
            std::vector<float> reward_history{};
            std::vector<float> speed_history{};
            std::uint64_t optimizer_step{};
            std::uint64_t random_state{};
            TrainingMetrics metrics{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            float difficulty{ 0.25f };
        };
'''
)
replace_exact(
    "src/ppo.hpp",
    "        [[nodiscard]] bool restore_best_policy() noexcept;\n"
    "        void train_one_update();",
    "        [[nodiscard]] bool restore_best_policy() noexcept;\n"
    "        void begin_staged_update();\n"
    "        void compute_staged_advantages();\n"
    "        void optimize_staged_update();\n"
    "        void finish_staged_update();\n"
    "        [[nodiscard]] bool staged_update_active() const noexcept { return staged_update_active_; }\n"
    "        void train_one_update();"
)
replace_exact(
    "src/ppo.hpp",
    "        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,\n"
    "            bool transfer_only = false);",
    "        [[nodiscard]] bool load_checkpoint(const std::filesystem::path& path, std::string& error,\n"
    "            bool transfer_only = false);\n"
    "        [[nodiscard]] CheckpointData checkpoint_data() const;\n"
    "        [[nodiscard]] static bool write_checkpoint_data(const CheckpointData& data,\n"
    "            const std::filesystem::path& path, std::string& error);\n"
    "        [[nodiscard]] static bool read_checkpoint_data(const std::filesystem::path& path,\n"
    "            CheckpointData& data, std::string& error);\n"
    "        [[nodiscard]] bool apply_checkpoint_data(CheckpointData data, std::string& error,\n"
    "            bool transfer_only = false);"
)
replace_exact(
    "src/ppo.hpp",
    "        std::shared_ptr<ParallelState> parallel_{};\n",
    "        std::shared_ptr<ParallelState> parallel_{};\n"
    "        RolloutTotals staged_totals_{};\n"
    "        bool staged_update_active_{};\n"
    "        bool staged_advantages_ready_{};\n"
    "        bool staged_optimized_{};\n"
)

replace_regex(
    "src/ppo_trainer.cpp",
    r"    void PpoTrainer::train_one_update\(\)\n    \{.*?\n    \}\n\n    void PpoTrainer::evaluate_policy",
    '''    void PpoTrainer::begin_staged_update()
    {
        if (staged_update_active_)
            return;
        constexpr std::size_t horizon = rollout_horizon;
        const std::size_t environment_count = environments_.size();
        rollout_.clear();
        rollout_.resize(horizon * environment_count);

        const std::uint64_t update_seed = random_state_
            ^ (metrics_.update + 1u) * 0x9E3779B97F4A7C15ULL;
        staged_totals_ = {};
        if (rollout_workers_.empty())
        {
            staged_totals_ = collect_rollout_partition(0, 1, update_seed);
        }
        else
        {
            {
                std::scoped_lock lock(rollout_mutex_);
                rollout_completed_ = 0;
                rollout_update_seed_ = update_seed;
                rollout_active_worker_count_ = std::min(active_worker_count_, rollout_worker_count_);
                ++rollout_generation_;
            }
            rollout_start_cv_.notify_all();
            {
                std::unique_lock lock(rollout_mutex_);
                rollout_done_cv_.wait(lock, [this]
                {
                    return rollout_completed_ == rollout_worker_count_;
                });
            }
            for (const RolloutTotals& worker : rollout_worker_totals_)
            {
                staged_totals_.accumulated_speed += worker.accumulated_speed;
                staged_totals_.completed_reward += worker.completed_reward;
                staged_totals_.completed_distance += worker.completed_distance;
                staged_totals_.completed_episodes += worker.completed_episodes;
            }
        }
        random_state_ ^= update_seed + 0xA0761D6478BD642FULL;
        staged_update_active_ = true;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
    }

    void PpoTrainer::compute_staged_advantages()
    {
        if (!staged_update_active_ || staged_advantages_ready_)
            return;
        constexpr float gamma = 0.995f;
        constexpr float gae_lambda = 0.95f;
        const std::size_t environment_count = environments_.size();
        std::vector<float> next_values(environment_count, 0.0f);
        for (std::size_t environment_index = 0; environment_index < environment_count; ++environment_index)
            next_values[environment_index] = policy_.evaluate(environments_[environment_index].observation()).value;

        for (std::size_t environment_index = 0; environment_index < environment_count; ++environment_index)
        {
            float next_value = next_values[environment_index];
            float next_advantage = 0.0f;
            for (std::size_t reverse = rollout_horizon; reverse-- > 0;)
            {
                Transition& transition = rollout_[reverse * environment_count + environment_index];
                const float continuation = transition.terminal ? 0.0f : 1.0f;
                const float delta = transition.reward + gamma * next_value * continuation - transition.value;
                transition.advantage = delta + gamma * gae_lambda * continuation * next_advantage;
                transition.return_value = transition.advantage + transition.value;
                next_advantage = transition.advantage;
                next_value = transition.value;
            }
        }

        float mean_advantage = 0.0f;
        for (const Transition& transition : rollout_)
            mean_advantage += transition.advantage;
        mean_advantage /= static_cast<float>(rollout_.size());
        float variance = 0.0f;
        for (const Transition& transition : rollout_)
        {
            const float delta = transition.advantage - mean_advantage;
            variance += delta * delta;
        }
        const float inverse_std = 1.0f
            / std::sqrt(variance / static_cast<float>(rollout_.size()) + 1.0e-6f);
        for (Transition& transition : rollout_)
            transition.advantage = (transition.advantage - mean_advantage) * inverse_std;
        staged_advantages_ready_ = true;
    }

    void PpoTrainer::optimize_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || staged_optimized_)
            return;
        update_policy();
        staged_optimized_ = true;
    }

    void PpoTrainer::finish_staged_update()
    {
        if (!staged_update_active_ || !staged_advantages_ready_ || !staged_optimized_)
            return;
        ++metrics_.update;
        metrics_.environment_steps += rollout_.size();
        metrics_.mean_speed = staged_totals_.accumulated_speed
            / static_cast<float>(rollout_.size());
        if (staged_totals_.completed_episodes > 0)
        {
            metrics_.mean_reward = staged_totals_.completed_reward
                / static_cast<float>(staged_totals_.completed_episodes);
            metrics_.mean_episode_distance = staged_totals_.completed_distance
                / static_cast<float>(staged_totals_.completed_episodes);
        }
        else
        {
            const float partial_reward = std::accumulate(
                episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
            metrics_.mean_reward = partial_reward / static_cast<float>(episode_rewards_.size());
        }
        append_history(reward_history_, metrics_.mean_reward);
        append_history(speed_history_, metrics_.mean_speed * 3.6f);
        controller_state_ = ControllerState::training;
        if (metrics_.update == 1 || metrics_.update % 5 == 0)
            evaluate_policy();
        staged_update_active_ = false;
        staged_advantages_ready_ = false;
        staged_optimized_ = false;
        staged_totals_ = {};
    }

    void PpoTrainer::train_one_update()
    {
        begin_staged_update();
        compute_staged_advantages();
        optimize_staged_update();
        finish_staged_update();
    }

    void PpoTrainer::evaluate_policy'''
)

# Replace the checkpoint implementation with immutable data export/import.
write("src/training_checkpoint.cpp", r'''#include "ppo.hpp"

#include <array>
#include <fstream>
#include <format>
#include <type_traits>

namespace epochrunner::rl
{
    namespace
    {
        constexpr std::array<char, 8> checkpoint_magic{ 'E', 'P', 'P', 'O', '2', '7', '\0', '\1' };

        template <typename T>
        bool write_value(std::ofstream& output, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(reinterpret_cast<const char*>(&value), sizeof(T));
            return static_cast<bool>(output);
        }

        template <typename T>
        bool read_value(std::ifstream& input, T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            input.read(reinterpret_cast<char*>(&value), sizeof(T));
            return static_cast<bool>(input);
        }

        bool write_vector(std::ofstream& output, const std::vector<float>& values)
        {
            const std::uint64_t count = values.size();
            if (!write_value(output, count))
                return false;
            if (values.empty())
                return true;
            output.write(reinterpret_cast<const char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(output);
        }

        bool read_vector(std::ifstream& input, std::vector<float>& values, std::size_t maximum)
        {
            std::uint64_t count{};
            if (!read_value(input, count) || count > maximum)
                return false;
            values.resize(static_cast<std::size_t>(count));
            if (values.empty())
                return true;
            input.read(reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(values.size() * sizeof(float)));
            return static_cast<bool>(input);
        }

        bool write_metrics(std::ofstream& output, const TrainingMetrics& value)
        {
            return write_value(output, value.update)
                && write_value(output, value.environment_steps)
                && write_value(output, value.mean_reward)
                && write_value(output, value.mean_episode_distance)
                && write_value(output, value.mean_speed)
                && write_value(output, value.policy_loss)
                && write_value(output, value.value_loss)
                && write_value(output, value.entropy)
                && write_value(output, value.learning_rate)
                && write_value(output, value.evaluation_reward)
                && write_value(output, value.evaluation_distance)
                && write_value(output, value.evaluation_speed)
                && write_value(output, value.evaluation_score)
                && write_value(output, value.evaluation_survival)
                && write_value(output, value.evaluation_collisions)
                && write_value(output, value.evaluation_airborne_ratio)
                && write_value(output, value.evaluation_stride_events)
                && write_value(output, value.evaluation_duck_seconds)
                && write_value(output, value.evaluation_powered_jumps)
                && write_value(output, value.evaluation_jump_landings)
                && write_value(output, value.evaluation_spin_turns)
                && write_value(output, value.evaluation_spin_landings)
                && write_value(output, value.evaluation_obstacles_passed)
                && write_value(output, value.evaluation_invalid_runs)
                && write_value(output, value.evaluation_valid)
                && write_value(output, value.best_evaluation_distance)
                && write_value(output, value.best_evaluation_score)
                && write_value(output, value.best_update)
                && write_value(output, value.evaluation_count)
                && write_value(output, value.imitation_samples)
                && write_value(output, value.imitation_weight)
                && write_value(output, value.imitation_source_score);
        }

        bool read_metrics(std::ifstream& input, TrainingMetrics& value)
        {
            return read_value(input, value.update)
                && read_value(input, value.environment_steps)
                && read_value(input, value.mean_reward)
                && read_value(input, value.mean_episode_distance)
                && read_value(input, value.mean_speed)
                && read_value(input, value.policy_loss)
                && read_value(input, value.value_loss)
                && read_value(input, value.entropy)
                && read_value(input, value.learning_rate)
                && read_value(input, value.evaluation_reward)
                && read_value(input, value.evaluation_distance)
                && read_value(input, value.evaluation_speed)
                && read_value(input, value.evaluation_score)
                && read_value(input, value.evaluation_survival)
                && read_value(input, value.evaluation_collisions)
                && read_value(input, value.evaluation_airborne_ratio)
                && read_value(input, value.evaluation_stride_events)
                && read_value(input, value.evaluation_duck_seconds)
                && read_value(input, value.evaluation_powered_jumps)
                && read_value(input, value.evaluation_jump_landings)
                && read_value(input, value.evaluation_spin_turns)
                && read_value(input, value.evaluation_spin_landings)
                && read_value(input, value.evaluation_obstacles_passed)
                && read_value(input, value.evaluation_invalid_runs)
                && read_value(input, value.evaluation_valid)
                && read_value(input, value.best_evaluation_distance)
                && read_value(input, value.best_evaluation_score)
                && read_value(input, value.best_update)
                && read_value(input, value.evaluation_count)
                && read_value(input, value.imitation_samples)
                && read_value(input, value.imitation_weight)
                && read_value(input, value.imitation_source_score);
        }
    }

    PpoTrainer::CheckpointData PpoTrainer::checkpoint_data() const
    {
        CheckpointData data{};
        data.rig_signature = blueprint_.signature();
        data.parameters = policy_.parameters();
        data.first_moment = adam_.first_moment;
        data.second_moment = adam_.second_moment;
        data.best_parameters = best_parameters_;
        data.reward_history = reward_history_;
        data.speed_history = speed_history_;
        data.optimizer_step = adam_.step;
        data.random_state = random_state_;
        data.metrics = metrics_;
        data.stage = course_stage_;
        data.difficulty = course_difficulty_;
        return data;
    }

    bool PpoTrainer::write_checkpoint_data(const CheckpointData& data,
        const std::filesystem::path& path, std::string& error)
    {
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "Could not open checkpoint for writing: " + temporary.string();
            return false;
        }
        const auto stage = static_cast<std::uint8_t>(data.stage);
        output.write(checkpoint_magic.data(), static_cast<std::streamsize>(checkpoint_magic.size()));
        const bool ok = write_value(output, data.rig_signature)
            && write_value(output, data.optimizer_step)
            && write_value(output, data.random_state)
            && write_value(output, stage)
            && write_value(output, data.difficulty)
            && write_metrics(output, data.metrics)
            && write_vector(output, data.parameters)
            && write_vector(output, data.first_moment)
            && write_vector(output, data.second_moment)
            && write_vector(output, data.best_parameters)
            && write_vector(output, data.reward_history)
            && write_vector(output, data.speed_history);
        output.close();
        if (!ok || !output)
        {
            error = "Failed while writing checkpoint: " + path.string();
            return false;
        }
        std::error_code filesystem_error{};
        std::filesystem::remove(path, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            error = "Could not replace checkpoint atomically: " + filesystem_error.message();
            return false;
        }
        error.clear();
        return true;
    }

    bool PpoTrainer::read_checkpoint_data(const std::filesystem::path& path,
        CheckpointData& data, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        std::array<char, 8> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        std::uint8_t stage{};
        if (!input || magic != checkpoint_magic
            || !read_value(input, data.rig_signature)
            || !read_value(input, data.optimizer_step)
            || !read_value(input, data.random_state)
            || !read_value(input, stage)
            || !read_value(input, data.difficulty)
            || !read_metrics(input, data.metrics)
            || !read_vector(input, data.parameters, 2'000'000)
            || !read_vector(input, data.first_moment, 2'000'000)
            || !read_vector(input, data.second_moment, 2'000'000)
            || !read_vector(input, data.best_parameters, 2'000'000)
            || !read_vector(input, data.reward_history, 10'000)
            || !read_vector(input, data.speed_history, 10'000)
            || stage >= sim::course_stage_count
            || data.difficulty < 0.10f || data.difficulty > 1.0f)
        {
            error = "Invalid or incompatible EpochRunner v0.7 checkpoint.";
            return false;
        }
        data.stage = static_cast<sim::CourseStage>(stage);
        error.clear();
        return true;
    }

    bool PpoTrainer::apply_checkpoint_data(CheckpointData data, std::string& error,
        bool transfer_only)
    {
        const std::size_t expected = policy_.parameter_count();
        if (data.parameters.size() != expected
            || data.first_moment.size() != expected
            || data.second_moment.size() != expected
            || (!data.best_parameters.empty() && data.best_parameters.size() != expected))
        {
            error = "Invalid or incompatible checkpoint dimensions.";
            return false;
        }
        if (!transfer_only && data.rig_signature != blueprint_.signature())
        {
            error = std::format("RIG MISMATCH {:016X} != {:016X}.",
                data.rig_signature, blueprint_.signature());
            return false;
        }
        policy_.parameters() = std::move(data.parameters);
        if (transfer_only)
        {
            reset_training_state();
            controller_state_ = ControllerState::transferred;
            error = "WEIGHTS TRANSFERRED - OPTIMIZER AND BEST STATE RESET";
            return true;
        }
        adam_.first_moment = std::move(data.first_moment);
        adam_.second_moment = std::move(data.second_moment);
        adam_.step = data.optimizer_step;
        random_state_ = data.random_state;
        metrics_ = data.metrics;
        best_parameters_ = std::move(data.best_parameters);
        reward_history_ = std::move(data.reward_history);
        speed_history_ = std::move(data.speed_history);
        course_stage_ = data.stage;
        course_difficulty_ = data.difficulty;
        for (sim::Environment& environment : environments_)
            environment.set_course(course_stage_, course_difficulty_);
        preview_.set_course(course_stage_, course_difficulty_);
        refresh_self_imitation_prior();
        std::fill(episode_rewards_.begin(), episode_rewards_.end(), 0.0f);
        std::fill(episode_distances_.begin(), episode_distances_.end(), 0.0f);
        for (auto& action : rollout_previous_actions_)
            action.fill(0.0f);
        for (std::size_t index = 0; index < environments_.size(); ++index)
            environments_[index].reset(0x1000u + index * 7919u);
        preview_.reset(0xDEADBEEFu + metrics_.update);
        controller_state_ = ControllerState::resumed;
        error.clear();
        return true;
    }

    bool PpoTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error) const
    {
        return write_checkpoint_data(checkpoint_data(), path, error);
    }

    bool PpoTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        CheckpointData data{};
        return read_checkpoint_data(path, data, error)
            && apply_checkpoint_data(std::move(data), error, transfer_only);
    }
}
''')

# ---------------------------------------------------------------------------
# Worker-owned autonomy state, staged coroutine, and asynchronous persistence.
# ---------------------------------------------------------------------------
replace_exact("src/autonomy.hpp", "#include <mutex>\n", "#include <mutex>\n#include <memory>\n")
replace_exact(
    "src/autonomy.hpp",
    "        bool worker_busy{};\n        std::string message",
    "        bool worker_busy{};\n"
    "        std::string pipeline_stage{ \"IDLE\" };\n"
    "        std::uint32_t pipeline_stage_mask{};\n"
    "        std::string message"
)
replace_exact(
    "src/autonomy.hpp",
    "        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error) const;",
    "        [[nodiscard]] bool save_checkpoint(const std::filesystem::path& path, std::string& error);"
)
replace_exact(
    "src/autonomy.hpp",
    "            set_exploration,\n            restore_best",
    "            set_exploration,\n            restore_best,\n            save_checkpoint,\n            apply_checkpoint,\n            apply_autosave"
)
replace_exact(
    "src/autonomy.hpp",
    "            float scalar{};\n        };",
    "            float scalar{};\n"
    "            std::filesystem::path path{};\n"
    "            std::shared_ptr<PpoTrainer::CheckpointData> checkpoint{};\n"
    "            bool transfer_only{};\n"
    "            std::uint64_t rig_generation{};\n"
    "            std::uint64_t accepted_rig_changes{};\n"
    "            std::uint64_t rejected_rig_changes{};\n"
    "            int rollback_count{};\n"
    "        };"
)
replace_exact(
    "src/autonomy.hpp",
    "            commands,\n            trained,\n            published",
    "            commands,\n            rollout,\n            advantages,\n            optimizer,\n            evaluation,\n            published,\n            persistence"
)
replace_exact(
    "src/autonomy.hpp",
    "        void perform_training_update();\n        void perform_post_update();\n",
    "        void consume_persistence_message();\n"
)
replace_exact(
    "src/autonomy.hpp",
    "        void publish_locked();\n        void autosave_locked();\n        void write_state_locked() const;\n        void read_state_locked();",
    "        void publish_locked();\n"
    "        void queue_autosave();\n"
    "        void queue_checkpoint_save(std::filesystem::path path, PpoTrainer::CheckpointData data);\n"
    "        void queue_checkpoint_load(std::filesystem::path path, bool transfer_only);\n"
    "        void queue_autosave_load();\n"
    "        void persistence_main(std::stop_token stop_token);"
)
replace_exact(
    "src/autonomy.hpp",
    "        mutable std::mutex worker_mutex_{};\n        mutable std::mutex snapshot_mutex_{};",
    "        mutable std::mutex snapshot_mutex_{};"
)
replace_exact(
    "src/autonomy.hpp",
    "        std::condition_variable_any wake_cv_{};\n        std::deque<PendingCommand> command_queue_{};",
    '''        std::condition_variable_any wake_cv_{};
        std::deque<PendingCommand> command_queue_{};

        enum class PersistenceKind : std::uint8_t
        {
            save_checkpoint,
            save_autosave,
            load_checkpoint,
            load_autosave
        };

        struct PersistenceJob
        {
            PersistenceKind kind{ PersistenceKind::save_checkpoint };
            std::filesystem::path checkpoint_path{};
            std::filesystem::path rig_path{};
            std::filesystem::path state_path{};
            PpoTrainer::CheckpointData checkpoint{};
            sim::CreatureBlueprint blueprint{};
            bool transfer_only{};
            sim::CourseStage stage{ sim::CourseStage::balance };
            float difficulty{ 0.25f };
            std::uint64_t rig_generation{};
            std::uint64_t accepted_rig_changes{};
            std::uint64_t rejected_rig_changes{};
            int rollback_count{};
        };

        mutable std::mutex persistence_mutex_{};
        std::condition_variable_any persistence_cv_{};
        std::deque<PersistenceJob> persistence_queue_{};
        std::string persistence_message_{};
        std::uint64_t persistence_message_serial_{};
        std::uint64_t consumed_persistence_message_serial_{};'''
)
replace_exact(
    "src/autonomy.hpp",
    "        std::string worker_message_{ \"LEARNING TO BALANCE\" };",
    "        std::string worker_message_{ \"LEARNING TO BALANCE\" };\n"
    "        std::string worker_pipeline_stage_{ \"IDLE\" };\n"
    "        std::uint32_t worker_pipeline_stage_mask_{};"
)
replace_exact(
    "src/autonomy.hpp",
    "        std::jthread worker_thread_{};\n",
    "        std::jthread worker_thread_{};\n        std::jthread persistence_thread_{};\n"
)

# Runtime constructor/destructor, coroutine, and stage ownership.
replace_exact(
    "src/autonomy_runtime.cpp",
    "        publish_locked();\n        synchronize();\n        worker_thread_ = std::jthread([this](std::stop_token stop_token)",
    "        publish_locked();\n"
    "        synchronize();\n"
    "        persistence_thread_ = std::jthread([this](std::stop_token stop_token)\n"
    "        {\n"
    "            persistence_main(stop_token);\n"
    "        });\n"
    "        worker_thread_ = std::jthread([this](std::stop_token stop_token)"
)
replace_exact(
    "src/autonomy_runtime.cpp",
    "        if (worker_thread_.joinable())\n"
    "        {\n"
    "            worker_thread_.request_stop();\n"
    "            wake_cv_.notify_all();\n"
    "        }",
    "        if (worker_thread_.joinable())\n"
    "        {\n"
    "            worker_thread_.request_stop();\n"
    "            wake_cv_.notify_all();\n"
    "        }\n"
    "        if (persistence_thread_.joinable())\n"
    "        {\n"
    "            persistence_thread_.request_stop();\n"
    "            persistence_cv_.notify_all();\n"
    "        }"
)
replace_regex(
    "src/autonomy_runtime.cpp",
    r"    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine\(std::stop_token stop_token\)\n    \{.*?\n    \}\n\n    void AutonomousTrainer::worker_main",
    '''    AutonomousTrainer::TrainingRoutine AutonomousTrainer::training_routine(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            consume_persistence_message();
            worker_pipeline_stage_ = "COMMANDS";
            worker_pipeline_stage_mask_ |= 1u << 0u;
            apply_pending_commands();
            co_yield RoutineStage::commands;

            if (!consume_update_request())
            {
                worker_pipeline_stage_ = "IDLE";
                co_yield RoutineStage::idle;
                continue;
            }

            worker_busy_.store(true, std::memory_order_relaxed);
            const auto started = std::chrono::steady_clock::now();

            worker_pipeline_stage_ = "ROLLOUT COLLECTION";
            worker_pipeline_stage_mask_ |= 1u << 1u;
            worker_.set_cpu_mode(updates_per_cycle_.load(std::memory_order_relaxed));
            worker_.begin_staged_update();
            co_yield RoutineStage::rollout;

            worker_pipeline_stage_ = "ADVANTAGE COMPUTATION";
            worker_pipeline_stage_mask_ |= 1u << 2u;
            worker_.compute_staged_advantages();
            co_yield RoutineStage::advantages;

            worker_pipeline_stage_ = "PARALLEL GRADIENT / OPTIMIZER";
            worker_pipeline_stage_mask_ |= 1u << 3u;
            worker_.optimize_staged_update();
            co_yield RoutineStage::optimizer;

            worker_pipeline_stage_ = "EVALUATION / CURRICULUM";
            worker_pipeline_stage_mask_ |= 1u << 4u;
            worker_.finish_staged_update();
            manage_curriculum_locked();
            co_yield RoutineStage::evaluation;

            const auto finished = std::chrono::steady_clock::now();
            worker_busy_.store(false, std::memory_order_relaxed);
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started);
            last_update_nanoseconds_.store(elapsed.count(), std::memory_order_relaxed);
            ++rate_window_updates_;
            const std::chrono::duration<double> rate_elapsed = finished - rate_window_started_;
            if (rate_elapsed.count() >= 1.0)
            {
                worker_updates_per_second_ = static_cast<double>(rate_window_updates_) / rate_elapsed.count();
                rate_window_updates_ = 0;
                rate_window_started_ = finished;
            }

            worker_pipeline_stage_ = "IMMUTABLE PUBLICATION";
            worker_pipeline_stage_mask_ |= 1u << 5u;
            publish_locked();
            co_yield RoutineStage::published;

            worker_pipeline_stage_ = "ASYNC PERSISTENCE";
            worker_pipeline_stage_mask_ |= 1u << 6u;
            co_yield RoutineStage::persistence;
        }
    }

    void AutonomousTrainer::worker_main'''
)
replace_exact(
    "src/autonomy_runtime.cpp",
    "            const bool post_update_must_finish = stage == RoutineStage::trained;\n"
    "            if (!post_update_must_finish && !has_pending_work())",
    "            const bool pipeline_must_finish = stage == RoutineStage::rollout\n"
    "                || stage == RoutineStage::advantages\n"
    "                || stage == RoutineStage::optimizer\n"
    "                || stage == RoutineStage::evaluation\n"
    "                || stage == RoutineStage::published;\n"
    "            if (!pipeline_must_finish && !has_pending_work())"
)
replace_exact(
    "src/autonomy_runtime.cpp",
    "            if (stage == RoutineStage::published)\n                throttle_after_update();",
    "            if (stage == RoutineStage::persistence)\n                throttle_after_update();"
)
replace_regex(
    "src/autonomy_runtime.cpp",
    r"\n    void AutonomousTrainer::perform_training_update\(\).*?\n    \}\n\n    void AutonomousTrainer::throttle_after_update",
    "\n    void AutonomousTrainer::throttle_after_update"
)

# Replace command handling with nonblocking save/load commands.
write("src/autonomy_commands.cpp", r'''#include "autonomy.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace epochrunner::rl
{
    void AutonomousTrainer::set_autosave_paths(std::filesystem::path checkpoint,
        std::filesystem::path rig, std::filesystem::path state)
    {
        std::scoped_lock lock(persistence_mutex_);
        autosave_checkpoint_ = std::move(checkpoint);
        autosave_rig_ = std::move(rig);
        autosave_state_ = std::move(state);
    }

    bool AutonomousTrainer::load_autosave(std::string& message)
    {
        bool exists = false;
        {
            std::scoped_lock lock(persistence_mutex_);
            exists = std::filesystem::exists(autosave_checkpoint_)
                || std::filesystem::exists(autosave_rig_)
                || std::filesystem::exists(autosave_state_);
        }
        if (!exists)
        {
            message = "NO V0.7 AUTOSAVE FOUND - STARTING WITH STAND TRAINING";
            return false;
        }
        queue_autosave_load();
        message = "AUTOSAVE LOAD QUEUED - TRAINER REMAINS RESPONSIVE";
        return true;
    }

    bool AutonomousTrainer::save_checkpoint(const std::filesystem::path& path, std::string& error)
    {
        if (path.empty())
        {
            error = "Checkpoint path is empty.";
            return false;
        }
        PendingCommand command{};
        command.type = CommandType::save_checkpoint;
        command.path = path;
        enqueue_command(std::move(command));
        error = "CHECKPOINT SAVE QUEUED";
        return true;
    }

    bool AutonomousTrainer::load_checkpoint(const std::filesystem::path& path, std::string& error,
        bool transfer_only)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            error = "Could not open checkpoint: " + path.string();
            return false;
        }
        queue_checkpoint_load(path, transfer_only);
        error = "CHECKPOINT LOAD QUEUED";
        return true;
    }

    void AutonomousTrainer::enqueue_command(PendingCommand command)
    {
        {
            std::scoped_lock lock(command_mutex_);
            std::erase_if(command_queue_, [&command](const PendingCommand& queued)
            {
                return queued.type == command.type;
            });
            command_queue_.push_back(std::move(command));
        }
        wake_cv_.notify_all();
    }

    std::size_t AutonomousTrainer::pending_command_count() const
    {
        std::scoped_lock lock(command_mutex_);
        return command_queue_.size();
    }

    void AutonomousTrainer::apply_pending_commands()
    {
        std::deque<PendingCommand> pending{};
        {
            std::scoped_lock lock(command_mutex_);
            pending.swap(command_queue_);
        }
        if (pending.empty())
            return;

        worker_busy_.store(true, std::memory_order_relaxed);
        for (PendingCommand& command : pending)
            apply_command_locked(std::move(command));
        worker_busy_.store(false, std::memory_order_relaxed);
        publish_locked();
    }

    void AutonomousTrainer::apply_command_locked(PendingCommand&& command)
    {
        switch (command.type)
        {
        case CommandType::set_blueprint:
            if (!command.preserve_policy)
            {
                stage_ = sim::CourseStage::balance;
                difficulty_ = 0.25f;
                rig_generation_ = 0;
                accepted_rig_changes_ = 0;
                rejected_rig_changes_ = 0;
                rollback_count_ = 0;
            }
            worker_.set_blueprint(command.blueprint, command.preserve_policy);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = 0;
            last_saved_best_update_ = 0;
            worker_message_ = command.preserve_policy
                ? "RIG UPDATED WITHOUT BLOCKING THE UI - CONTROLLER RECALIBRATING"
                : "RIG UPDATED WITHOUT BLOCKING THE UI - FRESH STAND LESSON STARTED";
            break;

        case CommandType::reset_policy:
            worker_.reset_policy(command.seed);
            worker_.set_course(stage_, difficulty_, false);
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            last_evaluation_count_ = 0;
            last_saved_best_update_ = 0;
            worker_message_ = "CONTROLLER RESET - CURRENT SKILL RESTARTED";
            break;

        case CommandType::set_exploration:
            worker_.set_exploration(command.scalar);
            worker_message_ = std::format("EXPLORATION SET TO {:.3f}", command.scalar);
            break;

        case CommandType::restore_best:
            if (worker_.restore_best_policy())
            {
                ++rollback_count_;
                worker_message_ = "BEST VERIFIED CONTROLLER RESTORED";
            }
            break;

        case CommandType::save_checkpoint:
            queue_checkpoint_save(std::move(command.path), worker_.checkpoint_data());
            worker_message_ = "IMMUTABLE CHECKPOINT SNAPSHOT QUEUED FOR ASYNC WRITE";
            break;

        case CommandType::apply_checkpoint:
            if (command.checkpoint)
            {
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error,
                    command.transfer_only))
                {
                    stage_ = worker_.course_stage();
                    difficulty_ = worker_.course_difficulty();
                    worker_message_ = command.transfer_only
                        ? "CONTROLLER TRANSFERRED - CURRENT SKILL RECALIBRATING"
                        : "CHECKPOINT RESUMED - BACKGROUND TRAINING CONTINUES";
                }
                else
                {
                    worker_message_ = error;
                }
            }
            break;

        case CommandType::apply_autosave:
            if (command.checkpoint)
            {
                worker_.set_blueprint(command.blueprint, false);
                std::string error{};
                if (worker_.apply_checkpoint_data(std::move(*command.checkpoint), error, false))
                {
                    stage_ = worker_.course_stage();
                    difficulty_ = worker_.course_difficulty();
                    rig_generation_ = command.rig_generation;
                    accepted_rig_changes_ = command.accepted_rig_changes;
                    rejected_rig_changes_ = command.rejected_rig_changes;
                    rollback_count_ = command.rollback_count;
                    worker_message_ = "V0.7 AUTOSAVE RESUMED ASYNCHRONOUSLY";
                }
                else
                {
                    worker_message_ = error;
                }
            }
            break;
        }
    }
}
''')

# Async persistence implementation and immutable publication.
write("src/autonomy_persistence.cpp", r'''#include "autonomy.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace epochrunner::rl
{
    namespace
    {
        bool write_autonomy_state(const std::filesystem::path& path,
            sim::CourseStage stage, float difficulty, std::uint64_t rig_generation,
            std::uint64_t accepted, std::uint64_t rejected, int rollback,
            std::string& error)
        {
            if (path.empty())
                return true;
            const std::filesystem::path temporary = path.string() + ".tmp";
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                error = "Could not open autonomy state for writing: " + temporary.string();
                return false;
            }
            output << "EPOCHAUTONOMY 3\n";
            output << static_cast<int>(stage) << ' ' << difficulty << ' ' << rig_generation << ' '
                << accepted << ' ' << rejected << ' ' << rollback << '\n';
            output.close();
            if (!output)
            {
                error = "Failed while writing autonomy state: " + temporary.string();
                return false;
            }
            std::error_code filesystem_error{};
            std::filesystem::remove(path, filesystem_error);
            filesystem_error.clear();
            std::filesystem::rename(temporary, path, filesystem_error);
            if (filesystem_error)
            {
                error = "Could not replace autonomy state atomically: " + filesystem_error.message();
                return false;
            }
            return true;
        }

        void read_autonomy_state(const std::filesystem::path& path,
            sim::CourseStage& stage, float& difficulty, std::uint64_t& rig_generation,
            std::uint64_t& accepted, std::uint64_t& rejected, int& rollback)
        {
            if (path.empty() || !std::filesystem::exists(path))
                return;
            std::ifstream input(path);
            std::string magic{};
            int version{};
            int stage_value{};
            input >> magic >> version >> stage_value >> difficulty >> rig_generation
                >> accepted >> rejected >> rollback;
            if (!input || magic != "EPOCHAUTONOMY" || version != 3
                || stage_value < 0 || stage_value >= static_cast<int>(sim::course_stage_count))
                return;
            stage = static_cast<sim::CourseStage>(stage_value);
            difficulty = clamp(difficulty, 0.10f, 1.0f);
        }
    }

    void AutonomousTrainer::publish_locked()
    {
        PublishedSnapshot snapshot{};
        snapshot.blueprint = worker_.blueprint();
        snapshot.parameters = worker_.has_best_policy()
            ? worker_.best_policy_parameters()
            : worker_.policy().parameters();
        snapshot.metrics = worker_.metrics();
        snapshot.reward_history = worker_.reward_history();
        snapshot.speed_history = worker_.speed_history();
        snapshot.controller_state = worker_.controller_state();
        snapshot.exploration = worker_.exploration();
        snapshot.optimizer_step = worker_.optimizer_step();
        snapshot.has_best = worker_.has_best_policy();
        const std::span<const sim::Environment> environments = worker_.environments();
        if (!environments.empty())
        {
            const sim::Environment* representative = &environments.front();
            float representative_score = -1.0e9f;
            for (const sim::Environment& environment : environments)
            {
                const float score = (environment.valid_motion() ? 1000.0f : 0.0f)
                    + environment.distance_travelled() * 10.0f + environment.elapsed_seconds();
                if (score > representative_score)
                {
                    representative = &environment;
                    representative_score = score;
                }
            }
            snapshot.training_preview = *representative;
            snapshot.has_training_preview = true;
        }
        snapshot.status.enabled = enabled_.load(std::memory_order_relaxed);
        snapshot.status.stage = stage_;
        snapshot.status.difficulty = difficulty_;
        snapshot.status.rig_generation = rig_generation_;
        snapshot.status.accepted_rig_changes = accepted_rig_changes_;
        snapshot.status.rejected_rig_changes = rejected_rig_changes_;
        snapshot.status.mastery_streak = mastery_streak_;
        snapshot.status.rollback_count = rollback_count_;
        snapshot.status.rollout_threads = worker_.rollout_worker_count();
        snapshot.status.environment_count = worker_.environment_count();
        snapshot.status.pending_commands = pending_command_count();
        snapshot.status.updates_per_second = worker_updates_per_second_;
        snapshot.status.speed_mode = updates_per_cycle_.load(std::memory_order_relaxed);
        snapshot.status.worker_busy = worker_busy_.load(std::memory_order_relaxed);
        snapshot.status.pipeline_stage = worker_pipeline_stage_;
        snapshot.status.pipeline_stage_mask = worker_pipeline_stage_mask_;
        snapshot.status.message = worker_message_;

        std::scoped_lock lock(snapshot_mutex_);
        snapshot.serial = published_.serial + 1u;
        published_ = std::move(snapshot);
    }

    void AutonomousTrainer::queue_checkpoint_save(std::filesystem::path path,
        PpoTrainer::CheckpointData data)
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::save_checkpoint;
        job.checkpoint_path = std::move(path);
        job.checkpoint = std::move(data);
        {
            std::scoped_lock lock(persistence_mutex_);
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_autosave()
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::save_autosave;
        job.checkpoint = worker_.checkpoint_data();
        job.blueprint = worker_.blueprint();
        job.stage = stage_;
        job.difficulty = difficulty_;
        job.rig_generation = rig_generation_;
        job.accepted_rig_changes = accepted_rig_changes_;
        job.rejected_rig_changes = rejected_rig_changes_;
        job.rollback_count = rollback_count_;
        {
            std::scoped_lock lock(persistence_mutex_);
            job.checkpoint_path = autosave_checkpoint_;
            job.rig_path = autosave_rig_;
            job.state_path = autosave_state_;
            std::erase_if(persistence_queue_, [](const PersistenceJob& queued)
            {
                return queued.kind == PersistenceKind::save_autosave;
            });
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_checkpoint_load(std::filesystem::path path, bool transfer_only)
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::load_checkpoint;
        job.checkpoint_path = std::move(path);
        job.transfer_only = transfer_only;
        {
            std::scoped_lock lock(persistence_mutex_);
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::queue_autosave_load()
    {
        PersistenceJob job{};
        job.kind = PersistenceKind::load_autosave;
        job.blueprint = live_blueprint_;
        {
            std::scoped_lock lock(persistence_mutex_);
            job.checkpoint_path = autosave_checkpoint_;
            job.rig_path = autosave_rig_;
            job.state_path = autosave_state_;
            persistence_queue_.push_back(std::move(job));
        }
        persistence_cv_.notify_all();
    }

    void AutonomousTrainer::consume_persistence_message()
    {
        std::scoped_lock lock(persistence_mutex_);
        if (consumed_persistence_message_serial_ == persistence_message_serial_)
            return;
        consumed_persistence_message_serial_ = persistence_message_serial_;
        worker_message_ = persistence_message_;
    }

    void AutonomousTrainer::persistence_main(std::stop_token stop_token)
    {
        while (!stop_token.stop_requested())
        {
            PersistenceJob job{};
            {
                std::unique_lock lock(persistence_mutex_);
                persistence_cv_.wait(lock, stop_token, [this]
                {
                    return !persistence_queue_.empty();
                });
                if (stop_token.stop_requested())
                    break;
                job = std::move(persistence_queue_.front());
                persistence_queue_.pop_front();
            }

            std::string message{};
            if (job.kind == PersistenceKind::save_checkpoint)
            {
                if (PpoTrainer::write_checkpoint_data(job.checkpoint, job.checkpoint_path, message))
                    message = "CHECKPOINT SAVED ASYNCHRONOUSLY";
            }
            else if (job.kind == PersistenceKind::save_autosave)
            {
                bool ok = true;
                if (!job.checkpoint_path.empty())
                    ok = PpoTrainer::write_checkpoint_data(job.checkpoint, job.checkpoint_path, message);
                if (ok && !job.rig_path.empty())
                    ok = job.blueprint.save(job.rig_path, message);
                if (ok)
                    ok = write_autonomy_state(job.state_path, job.stage, job.difficulty,
                        job.rig_generation, job.accepted_rig_changes,
                        job.rejected_rig_changes, job.rollback_count, message);
                if (ok)
                    message = "AUTOSAVE SNAPSHOT PUBLISHED ASYNCHRONOUSLY";
            }
            else if (job.kind == PersistenceKind::load_checkpoint)
            {
                auto data = std::make_shared<PpoTrainer::CheckpointData>();
                if (PpoTrainer::read_checkpoint_data(job.checkpoint_path, *data, message))
                {
                    PendingCommand command{};
                    command.type = CommandType::apply_checkpoint;
                    command.checkpoint = std::move(data);
                    command.transfer_only = job.transfer_only;
                    enqueue_command(std::move(command));
                    message = "CHECKPOINT READ ASYNCHRONOUSLY - APPLY QUEUED";
                }
            }
            else
            {
                if (!job.rig_path.empty() && std::filesystem::exists(job.rig_path))
                {
                    std::string rig_error{};
                    const sim::CreatureBlueprint loaded = sim::CreatureBlueprint::load(job.rig_path, rig_error);
                    if (rig_error.empty())
                        job.blueprint = loaded;
                    else
                        message = rig_error;
                }
                auto data = std::make_shared<PpoTrainer::CheckpointData>();
                if (message.empty()
                    && PpoTrainer::read_checkpoint_data(job.checkpoint_path, *data, message))
                {
                    read_autonomy_state(job.state_path, job.stage, job.difficulty,
                        job.rig_generation, job.accepted_rig_changes,
                        job.rejected_rig_changes, job.rollback_count);
                    PendingCommand command{};
                    command.type = CommandType::apply_autosave;
                    command.blueprint = std::move(job.blueprint);
                    command.checkpoint = std::move(data);
                    command.rig_generation = job.rig_generation;
                    command.accepted_rig_changes = job.accepted_rig_changes;
                    command.rejected_rig_changes = job.rejected_rig_changes;
                    command.rollback_count = job.rollback_count;
                    enqueue_command(std::move(command));
                    message = "AUTOSAVE READ ASYNCHRONOUSLY - APPLY QUEUED";
                }
            }

            {
                std::scoped_lock lock(persistence_mutex_);
                persistence_message_ = std::move(message);
                ++persistence_message_serial_;
            }
            wake_cv_.notify_all();
        }
    }
}
''')

# Curriculum persistence and active motor loops.
replace_exact("src/autonomy_curriculum.cpp", "autosave_locked();", "queue_autosave();", expected=3)
replace_exact(
    "src/autonomy_curriculum.cpp",
    "            for (const sim::MotorConstraint& motor : candidate.motors)\n"
    "            {\n"
    "                if (motor.pivot < candidate.nodes.size()",
    "            for (std::size_t motor_index = 0; motor_index < candidate.active_motor_count; ++motor_index)\n"
    "            {\n"
    "                const sim::MotorConstraint& motor = candidate.motors[motor_index];\n"
    "                if (motor.pivot < candidate.nodes.size()"
)
replace_exact(
    "src/autonomy_curriculum.cpp",
    "        for (std::size_t index = 0; index < candidate.motors.size(); ++index)\n"
    "        {\n"
    "            negative[index]",
    "        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)\n"
    "        {\n"
    "            negative[index]",
    expected=1
)
replace_exact(
    "src/autonomy_curriculum.cpp",
    "        for (std::size_t index = 0; index < candidate.motors.size(); ++index)\n"
    "        {\n"
    "            sim::MotorConstraint& motor",
    "        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)\n"
    "        {\n"
    "            sim::MotorConstraint& motor",
    expected=1
)

print("Integrated v0.7 staged runtime, asynchronous persistence, and humanoid arms.")
