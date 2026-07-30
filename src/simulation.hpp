#pragma once

#include "math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace epochrunner::sim
{
    inline constexpr std::size_t action_count = 4;
    inline constexpr std::size_t observation_count = 18;

    struct Particle
    {
        Vec2 position{};
        Vec2 previous{};
        float inverse_mass{ 1.0f };
        float radius{ 0.12f };
        bool grounded{};
    };

    struct DistanceConstraint
    {
        std::uint16_t a{};
        std::uint16_t b{};
        float rest_length{};
        float stiffness{ 1.0f };
    };

    struct MotorConstraint
    {
        std::uint16_t a{};
        std::uint16_t pivot{};
        std::uint16_t c{};
        float minimum_angle{ -1.2f };
        float maximum_angle{ 1.2f };
        float neutral_angle{};
        float strength{ 0.55f };
    };

    struct CreatureBlueprint
    {
        std::vector<Vec2> nodes{};
        std::vector<float> radii{};
        std::vector<DistanceConstraint> bones{};
        std::array<MotorConstraint, action_count> motors{};

        [[nodiscard]] static CreatureBlueprint chicken();
        void rebuild_rest_lengths() noexcept;
        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] static CreatureBlueprint load(const std::filesystem::path& path, std::string& error);
    };

    struct StepResult
    {
        float reward{};
        float forward_speed{};
        bool terminated{};
    };

    class Environment
    {
    public:
        Environment();
        explicit Environment(const CreatureBlueprint& blueprint, std::uint64_t seed = 1);

        void set_blueprint(const CreatureBlueprint& blueprint);
        void reset(std::uint64_t seed = 0);
        [[nodiscard]] StepResult step(std::span<const float, action_count> actions, float dt = 1.0f / 60.0f);
        [[nodiscard]] std::array<float, observation_count> observation() const noexcept;

        [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
        [[nodiscard]] const CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] float elapsed_seconds() const noexcept { return elapsed_seconds_; }
        [[nodiscard]] float distance_travelled() const noexcept { return distance_travelled_; }
        [[nodiscard]] float forward_speed() const noexcept { return forward_speed_; }
        [[nodiscard]] bool fallen() const noexcept { return fallen_; }
        [[nodiscard]] float ground_height() const noexcept { return ground_height_; }

    private:
        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
        void solve_ground(float dt) noexcept;
        [[nodiscard]] float joint_angle(const MotorConstraint& motor) const noexcept;
        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float random_unit() noexcept;

        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::uint64_t random_state_{ 1 };
        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        Vec2 previous_pelvis_{};
        float ground_height_{};
        float elapsed_seconds_{};
        float distance_travelled_{};
        float forward_speed_{};
        float last_reward_{};
        bool fallen_{};
    };
}
