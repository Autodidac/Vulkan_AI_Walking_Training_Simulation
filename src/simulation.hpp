#pragma once

#include "math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace epochrunner::sim
{
    inline constexpr std::size_t action_count = 4;
    inline constexpr std::size_t observation_count = 30;

    enum class CourseStage : std::uint8_t
    {
        balance,
        walk,
        ramps,
        uneven,
        hurdles,
        duck_bars,
        moving_hazards
    };

    inline constexpr std::size_t course_stage_count = 7;

    [[nodiscard]] inline std::string_view course_stage_name(CourseStage stage) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance: return "BALANCE";
        case CourseStage::walk: return "FLAT WALK";
        case CourseStage::ramps: return "RAMPS";
        case CourseStage::uneven: return "UNEVEN TERRAIN";
        case CourseStage::hurdles: return "HURDLES";
        case CourseStage::duck_bars: return "DUCK UNDER BARS";
        case CourseStage::moving_hazards: return "MOVING HAZARDS";
        }
        return "UNKNOWN";
    }

    enum class CourseFeatureKind : std::uint8_t
    {
        hurdle,
        overhead_bar,
        moving_hazard
    };

    struct CourseFeature
    {
        CourseFeatureKind kind{};
        Vec2 center{};
        Vec2 half_extent{};
        float radius{};
        Vec2 velocity{};
    };

    enum class InvalidMotion : std::uint8_t
    {
        none,
        fallen,
        flipped,
        overspeed,
        out_of_bounds,
        sustained_flight,
        micro_motion
    };

    [[nodiscard]] inline std::string_view invalid_motion_name(InvalidMotion reason) noexcept
    {
        switch (reason)
        {
        case InvalidMotion::none: return "VALID";
        case InvalidMotion::fallen: return "FALLEN";
        case InvalidMotion::flipped: return "FLIPPED";
        case InvalidMotion::overspeed: return "OVER 50 KM/H";
        case InvalidMotion::out_of_bounds: return "OUT OF BOUNDS";
        case InvalidMotion::sustained_flight: return "FLYING";
        case InvalidMotion::micro_motion: return "MICRO-MOTION EXPLOIT";
        }
        return "INVALID";
    }

    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
        Vec2 root_position, float airborne_seconds, float allowed_airtime,
        float micro_motion_seconds, bool fallen) noexcept
    {
        if (uprightness < -0.15f)
            return InvalidMotion::flipped;
        if (speed_kmh > 50.0f)
            return InvalidMotion::overspeed;
        if (root_position.x < -8.0f || root_position.x > 300.0f || root_position.y > 14.0f)
            return InvalidMotion::out_of_bounds;
        if (airborne_seconds > allowed_airtime)
            return InvalidMotion::sustained_flight;
        if (micro_motion_seconds >= 3.0f)
            return InvalidMotion::micro_motion;
        if (fallen)
            return InvalidMotion::fallen;
        return InvalidMotion::none;
    }

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
        float strength{ 0.055f };
        bool enabled{ true };
    };

    [[nodiscard]] inline float motor_target_angle(const MotorConstraint& motor, float action) noexcept
    {
        action = clamp(action, -1.0f, 1.0f);
        const float negative_span = std::max(0.0f, motor.neutral_angle - motor.minimum_angle);
        const float positive_span = std::max(0.0f, motor.maximum_angle - motor.neutral_angle);
        const float target = action < 0.0f
            ? motor.neutral_angle + action * negative_span
            : motor.neutral_angle + action * positive_span;
        return clamp(target, motor.minimum_angle, motor.maximum_angle);
    }

    struct CreatureBlueprint
    {
        std::vector<Vec2> nodes{};
        std::vector<float> radii{};
        std::vector<DistanceConstraint> bones{};
        std::array<MotorConstraint, action_count> motors{};

        std::uint16_t root_node{};
        std::uint16_t torso_node{ 1 };
        std::uint16_t head_node{ 2 };
        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };

        [[nodiscard]] static CreatureBlueprint chicken();
        [[nodiscard]] static CreatureBlueprint biped();
        [[nodiscard]] static CreatureBlueprint humanoid();
        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint monoped();
        void rebuild_rest_lengths() noexcept;
        void calibrate_motor(std::size_t motor_index, float negative_degrees = 30.0f,
            float positive_degrees = 30.0f, float power = 0.055f) noexcept;
        void calibrate_all_motors(float degrees = 30.0f, float power = 0.055f) noexcept;
        [[nodiscard]] float rest_joint_angle(std::size_t motor_index) const noexcept;
        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] std::uint64_t signature() const noexcept;
        [[nodiscard]] bool save(const std::filesystem::path& path, std::string& error) const;
        [[nodiscard]] static CreatureBlueprint load(const std::filesystem::path& path, std::string& error);
    };

    struct StepResult
    {
        float reward{};
        float forward_speed{};
        bool terminated{};
        bool valid_motion{ true };
        InvalidMotion invalid_reason{ InvalidMotion::none };
    };

    class Environment
    {
    public:
        Environment();
        explicit Environment(const CreatureBlueprint& blueprint, std::uint64_t seed = 1);

        void set_blueprint(const CreatureBlueprint& blueprint);
        void set_course(CourseStage stage, float difficulty = 0.25f);
        void reset(std::uint64_t seed = 0);
        [[nodiscard]] StepResult step(std::span<const float, action_count> actions, float dt = 1.0f / 60.0f);
        [[nodiscard]] std::array<float, observation_count> observation() const noexcept;

        [[nodiscard]] const std::vector<Particle>& particles() const noexcept { return particles_; }
        [[nodiscard]] const CreatureBlueprint& blueprint() const noexcept { return blueprint_; }
        [[nodiscard]] std::span<const CourseFeature> course_features() const noexcept { return course_features_; }
        [[nodiscard]] CourseStage course_stage() const noexcept { return course_stage_; }
        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }
        [[nodiscard]] float elapsed_seconds() const noexcept { return elapsed_seconds_; }
        [[nodiscard]] float distance_travelled() const noexcept { return distance_travelled_; }
        [[nodiscard]] float forward_speed() const noexcept { return forward_speed_; }
        [[nodiscard]] bool fallen() const noexcept { return fallen_; }
        [[nodiscard]] float ground_height() const noexcept { return 0.0f; }
        [[nodiscard]] float ground_height_at(float x) const noexcept;
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }

    private:
        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void solve_motor(const MotorConstraint& motor, float action) noexcept;
        void solve_ground(float dt) noexcept;
        void solve_course() noexcept;
        void rebuild_course_features() noexcept;
        void update_gait_metrics(float dt, float action_energy) noexcept;
        void invalidate(InvalidMotion reason) noexcept;
        [[nodiscard]] float joint_angle(const MotorConstraint& motor) const noexcept;
        [[nodiscard]] float torso_uprightness() const noexcept;
        [[nodiscard]] float random_unit() noexcept;
        [[nodiscard]] bool valid_node(std::uint16_t index) const noexcept;

        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
        std::uint64_t random_state_{ 1 };
        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        Vec2 previous_pelvis_{};
        float elapsed_seconds_{};
        float distance_travelled_{};
        float forward_speed_{};
        float last_reward_{};
        bool fallen_{};

        CourseStage course_stage_{ CourseStage::balance };
        float course_difficulty_{ 0.25f };
        float collision_count_{};
        float airborne_seconds_{};
        float cumulative_airborne_{};
        float progress_window_seconds_{};
        float progress_window_start_x_{};
        float micro_motion_seconds_{};
        float maximum_speed_kmh_{};
        std::uint32_t alternating_steps_{};
        int last_contact_side_{};
        bool previous_left_grounded_{};
        bool previous_right_grounded_{};
        bool collided_this_step_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
    };
}
