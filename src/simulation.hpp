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
        case CourseStage::balance: return "SPAWN STANCE";
        case CourseStage::walk: return "FLAT SAND PATROL";
        case CourseStage::ramps: return "SAND MOUNDS";
        case CourseStage::uneven: return "LOOSE / DEFORMED SAND";
        case CourseStage::hurdles: return "FLAT DEBRIS";
        case CourseStage::duck_bars: return "LOW-CLEARANCE DEBRIS";
        case CourseStage::moving_hazards: return "COMBAT TRAVERSAL";
        }
        return "UNKNOWN";
    }

    enum class CourseFeatureKind : std::uint8_t
    {
        hurdle,
        overhead_bar,
        moving_hazard,
        rock,
        projectile
    };

    [[nodiscard]] inline std::string_view course_feature_name(CourseFeatureKind kind) noexcept
    {
        switch (kind)
        {
        case CourseFeatureKind::hurdle: return "HURDLE";
        case CourseFeatureKind::overhead_bar: return "LOW BAR";
        case CourseFeatureKind::moving_hazard: return "MOVING HAZARD";
        case CourseFeatureKind::rock: return "ROCK";
        case CourseFeatureKind::projectile: return "THROWN OBJECT";
        }
        return "OBSTACLE";
    }

    struct CourseFeature
    {
        CourseFeatureKind kind{};
        Vec2 center{};
        Vec2 half_extent{};
        float radius{};
        Vec2 velocity{};
        int marker_sequence{ -1 };
    };

    [[nodiscard]] inline float course_feature_half_width(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.half_extent.x;
        }
        return 0.0f;
    }

    [[nodiscard]] inline float course_feature_top(const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.center.y + feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return feature.center.y + feature.half_extent.y;
        }
        return feature.center.y;
    }

    [[nodiscard]] inline bool knee_crosses_before_foot(float knee_front_x,
        float foot_front_x, float foot_top_y, const CourseFeature& feature) noexcept
    {
        if (feature.kind != CourseFeatureKind::rock
            && feature.kind != CourseFeatureKind::hurdle)
            return false;
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        return knee_front_x > feature.center.x
            && foot_front_x < obstacle_front - 0.02f
            && foot_top_y < course_feature_top(feature) + 0.08f;
    }

    [[nodiscard]] inline float gait_progress_multiplier(std::uint32_t alternating_steps,
        bool single_support, float swing_clearance) noexcept
    {
        if (alternating_steps == 0)
            return single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        const float established = clamp(0.30f + static_cast<float>(alternating_steps) * 0.10f,
            0.30f, 1.0f);
        const float swing_bonus = single_support && swing_clearance > 0.10f ? 0.12f : 0.0f;
        return clamp(established + swing_bonus, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool wheel_sliding_motion(float root_speed, bool left_supported,
        bool right_supported, float stance_slip_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.22f && stance_slip_speed > 0.18f;
    }

    [[nodiscard]] inline bool rolling_body_motion(float root_speed, float torso_turn_speed,
        float uprightness, bool feet_supported, bool non_foot_grounded) noexcept
    {
        return non_foot_grounded
            && (std::abs(torso_turn_speed) > 0.45f || uprightness < 0.55f)
            && (!feet_supported || std::abs(root_speed) > 0.08f);
    }

    inline constexpr float terrain_cycle_length_m = 56.0f;

    [[nodiscard]] inline bool course_zone_is_flat(float course_distance) noexcept
    {
        float local = std::fmod(std::max(0.0f, course_distance), terrain_cycle_length_m);
        if (local < 0.0f)
            local += terrain_cycle_length_m;
        return local < 28.0f || local >= 44.0f;
    }

    [[nodiscard]] inline bool obstacles_require_flat_zone(CourseStage stage,
        float difficulty) noexcept
    {
        return stage != CourseStage::moving_hazards || difficulty < 0.70f;
    }

    [[nodiscard]] inline float course_feature_observation_size(
        const CourseFeature& feature) noexcept
    {
        switch (feature.kind)
        {
        case CourseFeatureKind::moving_hazard:
        case CourseFeatureKind::rock:
        case CourseFeatureKind::projectile:
            return feature.radius;
        case CourseFeatureKind::hurdle:
        case CourseFeatureKind::overhead_bar:
            return std::max(feature.half_extent.x, feature.half_extent.y);
        }
        return 0.0f;
    }

    enum class InvalidMotion : std::uint8_t
    {
        none,
        fallen,
        flipped,
        overspeed,
        out_of_bounds,
        sustained_flight,
        micro_motion,
        wheel_sliding,
        body_rolling
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
        case InvalidMotion::wheel_sliding: return "WHEEL-SLIDING EXPLOIT";
        case InvalidMotion::body_rolling: return "HEAD / TAIL / BODY ROLLING";
        }
        return "INVALID";
    }

    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
        Vec2 root_position, float airborne_seconds, float allowed_airtime,
        float micro_motion_seconds, bool fallen) noexcept
    {
        if (uprightness < -0.15f)
            return InvalidMotion::flipped;
        if (speed_kmh >= 50.0f)
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

    [[nodiscard]] inline bool recovery_should_start(bool collided,
        float uprightness, bool geometric_fall, bool hard_fall) noexcept
    {
        static_cast<void>(collided);
        constexpr float independent_recovery_uprightness = 0.72f;
        return !hard_fall
            && (uprightness < independent_recovery_uprightness || geometric_fall);
    }

    [[nodiscard]] inline bool recovery_terminal_fall(bool geometric_fall,
        bool hard_fall, bool recovery_active) noexcept
    {
        return hard_fall || (geometric_fall && !recovery_active);
    }

    [[nodiscard]] inline bool qualifies_alternating_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement) noexcept
    {
        return previous_side != 0 && strike_side != 0 && strike_side != previous_side
            && seconds_since_previous >= 0.12f && std::abs(root_displacement) >= 0.025f;
    }

    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        if (!traction_contact)
            return 0.985f;
        return std::abs(vertical_speed) < 1.5f ? 0.42f : 0.72f;
    }

    inline constexpr float course_marker_spacing_m = 8.0f;
    inline constexpr int course_safe_runway_markers = 3;
    inline constexpr int course_feature_cycle_length = 5;

    [[nodiscard]] inline int first_course_feature_sequence(float root_x, float course_progress,
        float spacing = course_marker_spacing_m, float trailing_distance = 6.0f) noexcept
    {
        return static_cast<int>(std::ceil(
            (root_x + course_progress - trailing_distance) / spacing));
    }

    [[nodiscard]] inline float course_feature_world_x(int sequence, float course_progress,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing - course_progress;
    }

    [[nodiscard]] inline float course_marker_distance_m(int sequence,
        float spacing = course_marker_spacing_m) noexcept
    {
        return static_cast<float>(sequence) * spacing;
    }

    [[nodiscard]] inline CourseFeatureKind scheduled_course_feature(CourseStage stage,
        int marker_sequence) noexcept
    {
        const int relative = std::max(0, marker_sequence - course_safe_runway_markers);
        const int selector = relative % course_feature_cycle_length;
        if (stage == CourseStage::ramps || stage == CourseStage::uneven)
            return CourseFeatureKind::rock;
        if (stage == CourseStage::hurdles)
            return selector == 0 ? CourseFeatureKind::rock : CourseFeatureKind::hurdle;
        if (stage == CourseStage::duck_bars)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }
        switch (selector)
        {
        case 0: return CourseFeatureKind::rock;
        case 1: return CourseFeatureKind::hurdle;
        case 2: return CourseFeatureKind::overhead_bar;
        case 3: return CourseFeatureKind::moving_hazard;
        default: return CourseFeatureKind::projectile;
        }
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
        [[nodiscard]] float course_speed() const noexcept
        {
            if (course_stage_ == CourseStage::balance)
                return 0.0f;
            if (static_cast<std::uint8_t>(course_stage_)
                < static_cast<std::uint8_t>(CourseStage::hurdles))
                return 0.68f + course_difficulty_ * 0.72f;
            return 1.05f + course_difficulty_ * 0.82f;
        }
        [[nodiscard]] float course_progress() const noexcept { return elapsed_seconds_ * course_speed(); }
        [[nodiscard]] bool recovering() const noexcept { return recovery_active_; }
        [[nodiscard]] std::uint32_t recovery_events() const noexcept { return recovery_events_; }
        [[nodiscard]] std::uint32_t recovery_successes() const noexcept { return recovery_successes_; }
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] std::uint32_t knee_first_faults() const noexcept { return knee_first_faults_; }
        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool non_foot_grounded() const noexcept { return non_foot_grounded_; }
        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool left_supported() const noexcept
        {
            return contact_supported(blueprint_.left_contact_node);
        }
        [[nodiscard]] bool right_supported() const noexcept
        {
            return contact_supported(blueprint_.right_contact_node);
        }

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
        [[nodiscard]] bool contact_cluster_contains(std::uint16_t contact_node,
            std::size_t particle_index) const noexcept;
        [[nodiscard]] bool contact_supported(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool non_foot_ground_contact() const noexcept;
        [[nodiscard]] bool head_ground_contact() const noexcept;
        [[nodiscard]] float torso_roll_angle() const noexcept;
        [[nodiscard]] float contact_cluster_front_x(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_top_y(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] float contact_cluster_horizontal_speed(std::uint16_t contact_node,
            float dt) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;

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
        float action_energy_window_{};
        float root_path_window_{};
        Vec2 previous_root_for_path_{};
        float last_step_time_{ -100.0f };
        float last_step_x_{};
        float maximum_speed_kmh_{};
        std::uint32_t alternating_steps_{};
        std::uint32_t knee_first_faults_{};
        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float head_contact_seconds_{};
        float previous_torso_angle_{};
        float torso_turn_speed_{};
        float stance_slip_speed_{};
        bool non_foot_grounded_{};
        bool knee_first_this_step_{};
        int last_contact_side_{};
        bool previous_left_grounded_{};
        bool previous_right_grounded_{};
        bool collided_this_step_{};
        bool recovery_active_{};
        float recovery_started_seconds_{};
        float recovery_best_upright_{ 1.0f };
        std::uint32_t recovery_events_{};
        std::uint32_t recovery_successes_{};
        InvalidMotion invalid_reason_{ InvalidMotion::none };
    };
}
