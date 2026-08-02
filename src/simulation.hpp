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
    inline constexpr std::size_t action_count = 8;
    inline constexpr std::size_t observation_count = 40;

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

    [[nodiscard]] inline bool stage_requires_forward_gait(CourseStage stage) noexcept
    {
        return stage == CourseStage::uneven
            || stage == CourseStage::hurdles
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool stage_allows_powered_airtime(CourseStage stage) noexcept
    {
        return stage == CourseStage::ramps
            || stage == CourseStage::hurdles
            || stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool stage_allows_controlled_flips(CourseStage stage) noexcept
    {
        return stage == CourseStage::duck_bars
            || stage == CourseStage::moving_hazards;
    }

    [[nodiscard]] inline bool powered_joint_launch(CourseStage stage, float vertical_speed,
        float action_energy) noexcept
    {
        return stage_allows_powered_airtime(stage)
            && vertical_speed >= 0.85f
            && action_energy >= 0.055f;
    }

    [[nodiscard]] inline float allowed_airtime_for_stage(CourseStage stage,
        bool powered_launch) noexcept
    {
        if (!powered_launch)
            return 0.72f;
        if (stage == CourseStage::ramps)
            return 1.65f;
        if (stage == CourseStage::hurdles)
            return 1.85f;
        if (stage == CourseStage::duck_bars)
            return 2.75f;
        if (stage == CourseStage::moving_hazards)
            return 2.45f;
        return 0.72f;
    }

    [[nodiscard]] inline bool stage_skill_evidence(CourseStage stage,
        std::uint32_t alternating_steps, float duck_seconds,
        std::uint32_t landed_jumps, float maximum_spin_turns,
        std::uint32_t spin_landings, std::uint32_t obstacles_passed) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance:
            return true;
        case CourseStage::walk:
            return duck_seconds >= 0.50f && obstacles_passed >= 1u;
        case CourseStage::ramps:
            return landed_jumps >= 1u;
        case CourseStage::uneven:
            return alternating_steps >= 2u;
        case CourseStage::hurdles:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u);
        case CourseStage::duck_bars:
            return spin_landings >= 1u && maximum_spin_turns >= 0.75f;
        case CourseStage::moving_hazards:
            return alternating_steps >= 2u && obstacles_passed >= 1u
                && (duck_seconds >= 0.25f || landed_jumps >= 1u || spin_landings >= 1u);
        }
        return false;
    }

    [[nodiscard]] inline std::string_view course_stage_name(CourseStage stage) noexcept
    {
        switch (stage)
        {
        case CourseStage::balance: return "1. STAND";
        case CourseStage::walk: return "2. LOW BAR DUCK / RECOVER";
        case CourseStage::ramps: return "3. JUMP / LAND";
        case CourseStage::uneven: return "4. WALK / RUN";
        case CourseStage::hurdles: return "5. MOVING DUCK / JUMP";
        case CourseStage::duck_bars: return "6. CONTROLLED FLIPS";
        case CourseStage::moving_hazards: return "7. MIXED GOAL COURSE";
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

        // Natural stepping often puts a bent knee slightly ahead of the foot.
        // Reject only an obvious body/joint-first shove: the knee must lead well
        // into the obstacle while the foot is both substantially behind it and
        // still below useful clearance. This remains guidance, not a hard gate.
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        const float obstacle_top = course_feature_top(feature);
        const float knee_lead = knee_front_x - feature.center.x;
        const float foot_lag = obstacle_front - foot_front_x;
        const float clearance_deficit = obstacle_top + 0.015f - foot_top_y;
        return knee_lead > 0.24f
            && foot_lag > 0.16f
            && clearance_deficit > 0.08f;
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

    [[nodiscard]] inline float ground_contact_offset(bool traction_contact,
        float particle_radius) noexcept
    {
        return traction_contact ? std::min(particle_radius, 0.065f) : particle_radius;
    }

    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.085f
            && stance_slip_speed < 0.080f
            && maximum_foot_clearance < 0.085f
            && (std::abs(torso_turn_speed) > 0.12f || std::abs(root_speed) > 0.18f);
    }

    inline constexpr float rolling_gate_activation_seconds = 1.35f;
    inline constexpr float rolling_gate_warmup_end_seconds = 2.60f;

    [[nodiscard]] inline bool rolling_gate_active(float elapsed_seconds) noexcept
    {
        return elapsed_seconds >= rolling_gate_activation_seconds;
    }

    [[nodiscard]] inline float body_rolling_limit(CourseStage stage,
        float elapsed_seconds) noexcept
    {
        if (elapsed_seconds < rolling_gate_warmup_end_seconds)
            return stage == CourseStage::balance ? 0.78f : 0.55f;
        return stage == CourseStage::balance ? 0.55f : 0.32f;
    }

    [[nodiscard]] inline float head_contact_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.38f : 0.24f;
    }

    [[nodiscard]] inline float foot_pivot_rolling_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.68f : 0.42f;
    }

    [[nodiscard]] inline bool zero_progress_window(float net_progress,
        std::uint32_t new_steps, float useful_foot_lift, bool recovering) noexcept
    {
        return !recovering && net_progress < 0.045f
            && new_steps == 0u && useful_foot_lift < 0.11f;
    }

    [[nodiscard]] inline float update_zero_progress_seconds(float previous_seconds,
        bool zero_progress, float window_seconds) noexcept
    {
        return zero_progress
            ? previous_seconds + window_seconds
            : std::max(0.0f, previous_seconds - window_seconds * 2.0f);
    }

    inline constexpr float zero_progress_reset_seconds = 1.80f;

    [[nodiscard]] inline bool ground_clearance_hazard(CourseFeatureKind kind) noexcept
    {
        return kind == CourseFeatureKind::rock || kind == CourseFeatureKind::hurdle;
    }

    [[nodiscard]] inline float hazard_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -0.20f || distance_ahead >= 2.60f)
            return 0.0f;
        if (distance_ahead <= 0.45f)
            return 1.0f;
        return clamp((2.60f - distance_ahead) / 2.15f, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float duck_obstacle_approach_weight(float distance_ahead) noexcept
    {
        if (distance_ahead <= -1.25f || distance_ahead >= 4.0f)
            return 0.0f;
        if (distance_ahead <= 1.0f)
            return 1.0f;
        return clamp((4.0f - distance_ahead) / 3.0f, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool hazard_quiver_motion(float distance_ahead, float root_speed,
        float lifted_foot_clearance, float target_clearance, float action_energy) noexcept
    {
        return hazard_approach_weight(distance_ahead) > 0.35f
            && std::abs(root_speed) < 0.16f
            && lifted_foot_clearance < target_clearance * 0.55f
            && action_energy > 0.075f;
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
        body_rolling,
        foot_pivot_rolling,
        zero_progress,
        collapsed_posture,
        excessive_spins,
        hazard_quiver
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
        case InvalidMotion::foot_pivot_rolling: return "FOOT-NODE SKATING / ROLLING";
        case InvalidMotion::zero_progress: return "ZERO MOVEMENT - RESET";
        case InvalidMotion::collapsed_posture: return "COLLAPSED / UNSUPPORTED POSTURE";
        case InvalidMotion::excessive_spins: return "MORE THAN 3 SPINS";
        case InvalidMotion::hazard_quiver: return "HAZARD QUIVER / NO LEG LIFT";
        }
        return "INVALID";
    }

    [[nodiscard]] inline InvalidMotion classify_motion_gate(float uprightness, float speed_kmh,
        Vec2 root_position, float airborne_seconds, float allowed_airtime,
        float micro_motion_seconds, bool fallen,
        CourseStage stage = CourseStage::balance, float airborne_spin_turns = 0.0f) noexcept
    {
        if (std::abs(airborne_spin_turns) > 3.20f)
            return InvalidMotion::excessive_spins;
        if (uprightness < -0.15f && !stage_allows_controlled_flips(stage))
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

    [[nodiscard]] inline bool qualifies_supported_step(int previous_side, int strike_side,
        float seconds_since_previous, float root_displacement,
        float swing_air_seconds, float swing_clearance) noexcept
    {
        return qualifies_alternating_step(previous_side, strike_side,
            seconds_since_previous, root_displacement)
            && std::abs(root_displacement) >= 0.055f
            && swing_air_seconds >= 0.10f
            && swing_clearance >= 0.075f;
    }

    [[nodiscard]] inline float ground_velocity_retention(bool traction_contact,
        float vertical_speed) noexcept
    {
        static_cast<void>(vertical_speed);
        return traction_contact ? 0.0f : 0.985f;
    }

    inline constexpr float course_marker_spacing_m = 8.0f;
    inline constexpr int course_safe_runway_markers = 5;
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
        if (stage == CourseStage::walk)
            return CourseFeatureKind::overhead_bar;
        if (stage == CourseStage::ramps || stage == CourseStage::uneven)
            return CourseFeatureKind::rock;
        if (stage == CourseStage::hurdles)
        {
            if (selector == 0)
                return CourseFeatureKind::rock;
            if (selector == 1)
                return CourseFeatureKind::hurdle;
            return CourseFeatureKind::overhead_bar;
        }
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
        std::size_t active_motor_count{ 4 };

        std::uint16_t root_node{};
        std::uint16_t torso_node{ 1 };
        std::uint16_t head_node{ 2 };
        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };
        std::vector<std::uint16_t> additional_left_contact_nodes{};
        std::vector<std::uint16_t> additional_right_contact_nodes{};

        [[nodiscard]] bool is_left_support_seed(std::size_t node) const noexcept
        {
            if (node == left_contact_node)
                return true;
            return std::ranges::find(additional_left_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_left_contact_nodes.end();
        }
        [[nodiscard]] bool is_right_support_seed(std::size_t node) const noexcept
        {
            if (node == right_contact_node)
                return true;
            return std::ranges::find(additional_right_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_right_contact_nodes.end();
        }
        [[nodiscard]] bool is_support_seed(std::size_t node) const noexcept
        {
            return is_left_support_seed(node) || is_right_support_seed(node);
        }
        [[nodiscard]] std::size_t support_seed_count() const noexcept
        {
            return 2u + additional_left_contact_nodes.size()
                + additional_right_contact_nodes.size();
        }

        [[nodiscard]] static CreatureBlueprint chicken();
        [[nodiscard]] static CreatureBlueprint biped();
        [[nodiscard]] static CreatureBlueprint humanoid();
        [[nodiscard]] static CreatureBlueprint quadruped();
        [[nodiscard]] static CreatureBlueprint crawler4();
        [[nodiscard]] static CreatureBlueprint hexapod();
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

    struct EnvironmentTestAccess;

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
            if (course_stage_ == CourseStage::balance
                || course_stage_ == CourseStage::ramps
                || course_stage_ == CourseStage::duck_bars)
                return 0.0f;
            if (course_stage_ == CourseStage::walk)
                return 0.52f + course_difficulty_ * 0.28f;
            if (course_stage_ == CourseStage::uneven)
                return 0.82f + course_difficulty_ * 0.88f;
            if (course_stage_ == CourseStage::hurdles)
                return 1.05f + course_difficulty_ * 0.95f;
            return 1.20f + course_difficulty_ * 1.05f;
        }
        [[nodiscard]] float course_progress() const noexcept { return elapsed_seconds_ * course_speed(); }
        [[nodiscard]] bool recovering() const noexcept { return recovery_active_; }
        [[nodiscard]] std::uint32_t recovery_events() const noexcept { return recovery_events_; }
        [[nodiscard]] std::uint32_t recovery_successes() const noexcept { return recovery_successes_; }
        [[nodiscard]] float collision_count() const noexcept { return collision_count_; }
        [[nodiscard]] float airborne_ratio() const noexcept;
        [[nodiscard]] std::uint32_t alternating_steps() const noexcept { return alternating_steps_; }
        [[nodiscard]] float duck_seconds() const noexcept { return duck_seconds_; }
        [[nodiscard]] bool duck_active() const noexcept { return duck_active_; }
        [[nodiscard]] float duck_obstacle_weight() const noexcept
        {
            return duck_obstacle_weight_;
        }
        [[nodiscard]] float duck_clearance_margin() const noexcept
        {
            return duck_clearance_margin_;
        }
        [[nodiscard]] std::uint32_t powered_jumps() const noexcept { return powered_jump_count_; }
        [[nodiscard]] std::uint32_t landed_jumps() const noexcept { return landed_jump_count_; }
        [[nodiscard]] float maximum_flip_turns() const noexcept { return maximum_spin_turns_; }
        [[nodiscard]] float maximum_spin_turns() const noexcept { return uncontrolled_spin_turns_; }
        [[nodiscard]] float uncontrolled_spin_turns() const noexcept { return uncontrolled_spin_turns_; }
        [[nodiscard]] std::uint32_t spin_landings() const noexcept { return spin_landing_count_; }
        [[nodiscard]] std::uint32_t obstacles_passed() const noexcept { return obstacles_passed_; }
        [[nodiscard]] std::uint32_t knee_first_faults() const noexcept { return knee_first_faults_; }
        [[nodiscard]] float stance_slip_speed() const noexcept { return stance_slip_speed_; }
        [[nodiscard]] bool non_foot_grounded() const noexcept { return non_foot_grounded_; }
        [[nodiscard]] float body_rolling_seconds() const noexcept { return body_rolling_seconds_; }
        [[nodiscard]] float foot_pivot_rolling_seconds() const noexcept { return foot_pivot_rolling_seconds_; }
        [[nodiscard]] float zero_progress_seconds() const noexcept { return zero_progress_seconds_; }
        [[nodiscard]] float hazard_stall_seconds() const noexcept { return hazard_stall_seconds_; }
        [[nodiscard]] float obstacle_lift_clearance() const noexcept { return obstacle_lift_clearance_; }
        [[nodiscard]] float stable_stance_seconds() const noexcept { return stable_stance_seconds_; }
        [[nodiscard]] float longest_stable_stance_seconds() const noexcept
        {
            return longest_stable_stance_seconds_;
        }
        [[nodiscard]] std::uint32_t duck_recoveries() const noexcept
        {
            return duck_recovery_count_;
        }
        [[nodiscard]] float maximum_joint_speed() const noexcept { return maximum_joint_speed_; }
        [[nodiscard]] float posture_failure_seconds() const noexcept
        {
            return posture_failure_seconds_;
        }
        [[nodiscard]] bool valid_motion() const noexcept { return invalid_reason_ == InvalidMotion::none; }
        [[nodiscard]] InvalidMotion invalid_reason() const noexcept { return invalid_reason_; }
        [[nodiscard]] float uprightness() const noexcept { return torso_uprightness(); }
        [[nodiscard]] bool body_integrity_valid() const noexcept;
        [[nodiscard]] bool current_display_posture_valid() const noexcept;
        [[nodiscard]] bool left_supported() const noexcept
        {
            return contact_supported(blueprint_.left_contact_node);
        }
        [[nodiscard]] bool right_supported() const noexcept
        {
            return contact_supported(blueprint_.right_contact_node);
        }

    private:
        friend struct EnvironmentTestAccess;

        void solve_distance(const DistanceConstraint& constraint) noexcept;
        void stabilize_passive_appendages() noexcept;
        void stabilize_balance_posture() noexcept;
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
        [[nodiscard]] float contact_cluster_clearance(std::uint16_t contact_node) const noexcept;
        [[nodiscard]] bool knee_before_foot_fault() const noexcept;

        CreatureBlueprint blueprint_{};
        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
        std::uint64_t random_state_{ 1 };
        std::array<float, action_count> previous_angles_{};
        std::array<float, action_count> angular_velocities_{};
        std::array<float, action_count> previous_applied_actions_{};
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
        float duck_seconds_{};
        float duck_depth_{};
        float duck_obstacle_weight_{};
        float duck_clearance_margin_{};
        float current_duck_hold_seconds_{};
        float stable_stance_seconds_{};
        float longest_stable_stance_seconds_{};
        float stance_failure_grace_seconds_{};
        float posture_failure_seconds_{};
        float maximum_joint_speed_{};
        std::uint32_t duck_recovery_count_{};
        bool duck_cycle_qualified_{};
        float current_airborne_rotation_{};
        float maximum_spin_turns_{};
        float uncontrolled_spin_turns_{};
        std::uint32_t powered_jump_count_{};
        std::uint32_t landed_jump_count_{};
        std::uint32_t spin_landing_count_{};
        std::uint32_t obstacles_passed_{};
        int last_passed_feature_sequence_{ course_safe_runway_markers - 1 };
        bool duck_active_{};
        bool powered_takeoff_{};
        bool powered_takeoff_this_step_{};
        bool powered_landing_this_step_{};
        bool spin_landing_this_step_{};
        bool passed_obstacle_this_step_{};
        bool collision_contact_active_{};
        bool collision_event_this_step_{};
        float progress_window_seconds_{};
        float progress_window_start_x_{};
        float micro_motion_seconds_{};
        float action_energy_window_{};
        float root_path_window_{};
        Vec2 previous_root_for_path_{};
        float last_step_time_{ -100.0f };
        float last_step_x_{};
        float left_swing_seconds_{};
        float right_swing_seconds_{};
        float left_swing_clearance_{};
        float right_swing_clearance_{};
        float action_change_energy_{};
        bool alternating_step_this_step_{};
        float maximum_speed_kmh_{};
        std::uint32_t alternating_steps_{};
        std::uint32_t progress_window_start_steps_{};
        std::uint32_t knee_first_faults_{};
        float wheel_sliding_seconds_{};
        float body_rolling_seconds_{};
        float foot_pivot_rolling_seconds_{};
        float zero_progress_seconds_{};
        float head_contact_seconds_{};
        float previous_torso_angle_{};
        float torso_turn_speed_{};
        float stance_slip_speed_{};
        float hazard_stall_seconds_{};
        float obstacle_approach_weight_{};
        float obstacle_lift_clearance_{};
        float obstacle_clearance_target_{ 0.20f };
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
