#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace runner::locomotion
{
    enum class Intent : std::uint8_t
    {
        hold,
        walk,
        run,
        recover,
        crawl,
        flee
    };

    struct Signals
    {
        float uprightness{ 1.0f };
        float root_x{};
        float left_support_x{};
        float right_support_x{};
        bool left_supported{};
        bool right_supported{};
        float near_rise{};
        float mid_rise{};
        float far_rise{};
        float slope{};
        float forward_speed{};
        bool recovering{};
        bool non_foot_grounded{};
        float burial_depth{};
        std::uint8_t obstruction_mask{};
        float free_space_direction{};
        float incoming_velocity_x{};
        float incoming_time_to_impact{ 10.0f };
        float incoming_density{};
        std::uint32_t gait_cycles{};
    };

    struct Plan
    {
        Intent intent{ Intent::hold };
        float direction{ 1.0f };
        float target_speed{};
        float cadence_hz{ 0.75f };
        float stride_scale{ 0.45f };
        float swing_lift{ 0.45f };
        float stance_extension{ 0.35f };
        float balance_reserve{};
        float terrain_demand{};
        bool step_up{};
        bool brake{};
        bool emergency_crawl{};
    };

    [[nodiscard]] inline float support_margin(const Signals& signals) noexcept
    {
        if (signals.left_supported && signals.right_supported)
        {
            const float minimum = std::min(signals.left_support_x,
                signals.right_support_x);
            const float maximum = std::max(signals.left_support_x,
                signals.right_support_x);
            const float midpoint = 0.5f * (minimum + maximum);
            const float half_span = std::max(0.15f, 0.5f * (maximum - minimum));
            return std::clamp(1.0f - std::abs(signals.root_x - midpoint)
                / (half_span + 0.18f), 0.0f, 1.0f);
        }
        if (signals.left_supported || signals.right_supported)
        {
            const float support_x = signals.left_supported
                ? signals.left_support_x : signals.right_support_x;
            return std::clamp(1.0f - std::abs(signals.root_x - support_x) / 0.72f,
                0.0f, 1.0f) * 0.82f;
        }
        return 0.0f;
    }

    [[nodiscard]] inline float balance_reserve(const Signals& signals) noexcept
    {
        const float upright = std::clamp((signals.uprightness - 0.20f) / 0.80f,
            0.0f, 1.0f);
        const float support = signals.left_supported && signals.right_supported ? 1.0f
            : (signals.left_supported || signals.right_supported ? 0.72f : 0.0f);
        const float grounded_penalty = signals.non_foot_grounded ? 0.20f : 1.0f;
        return std::clamp((upright * 0.48f + support * 0.24f
            + support_margin(signals) * 0.28f) * grounded_penalty, 0.0f, 1.0f);
    }

    [[nodiscard]] inline float terrain_demand(const Signals& signals) noexcept
    {
        const float positive_step = std::max({ 0.0f, signals.near_rise,
            signals.mid_rise * 0.85f, signals.far_rise * 0.55f });
        const float drop = std::max({ 0.0f, -signals.near_rise,
            -signals.mid_rise * 0.70f });
        return std::clamp(positive_step / 1.10f
            + drop / 1.45f
            + std::abs(signals.slope) * 0.28f, 0.0f, 1.0f);
    }

    [[nodiscard]] inline bool urgent_threat(const Signals& signals) noexcept
    {
        return signals.incoming_density >= 0.12f
            && signals.incoming_time_to_impact <= 1.80f;
    }

    [[nodiscard]] inline float escape_direction(const Signals& signals) noexcept
    {
        if (std::abs(signals.free_space_direction) >= 0.5f)
            return signals.free_space_direction < 0.0f ? -1.0f : 1.0f;
        if (std::abs(signals.incoming_velocity_x) >= 0.05f)
            return signals.incoming_velocity_x > 0.0f ? -1.0f : 1.0f;
        return 1.0f;
    }

    [[nodiscard]] inline bool emergency_crawl_allowed(const Signals& signals,
        float reserve) noexcept
    {
        const bool constrained = signals.burial_depth >= 0.08f
            || signals.obstruction_mask != 0u;
        return signals.non_foot_grounded && constrained
            && reserve < 0.32f
            && std::abs(signals.free_space_direction) >= 0.5f;
    }

    [[nodiscard]] inline Plan plan(const Signals& signals) noexcept
    {
        Plan result{};
        result.balance_reserve = balance_reserve(signals);
        result.terrain_demand = terrain_demand(signals);
        result.direction = 1.0f;

        const bool threat = urgent_threat(signals);
        if (threat)
            result.direction = escape_direction(signals);

        result.step_up = result.direction > 0.0f
            && signals.near_rise >= 0.14f
            && signals.near_rise <= 1.20f;
        result.emergency_crawl = emergency_crawl_allowed(signals,
            result.balance_reserve);

        if (result.emergency_crawl)
        {
            result.intent = Intent::crawl;
            result.direction = escape_direction(signals);
            result.target_speed = 0.24f;
            result.cadence_hz = 0.58f;
            result.stride_scale = 0.30f;
            result.swing_lift = 0.22f;
            result.stance_extension = 0.20f;
            result.brake = false;
            return result;
        }

        if (signals.recovering || result.balance_reserve < 0.38f)
        {
            result.intent = Intent::recover;
            result.target_speed = 0.10f;
            result.cadence_hz = 0.52f;
            result.stride_scale = 0.24f;
            result.swing_lift = 0.38f;
            result.stance_extension = 0.52f;
            result.brake = true;
            return result;
        }

        if (result.step_up)
        {
            result.intent = Intent::walk;
            result.target_speed = 0.28f + result.balance_reserve * 0.16f;
            result.cadence_hz = 0.62f;
            result.stride_scale = 0.42f;
            result.swing_lift = std::clamp(0.55f + signals.near_rise * 0.42f,
                0.55f, 0.95f);
            result.stance_extension = 0.78f;
            result.brake = std::abs(signals.forward_speed) > result.target_speed * 1.30f;
            return result;
        }

        if (threat)
        {
            result.intent = Intent::flee;
            result.target_speed = 1.35f + result.balance_reserve * 1.35f;
            result.cadence_hz = 1.25f + result.balance_reserve * 0.45f;
            result.stride_scale = 0.78f;
            result.swing_lift = 0.72f;
            result.stance_extension = 0.72f;
            result.brake = signals.forward_speed * result.direction < -0.25f;
            return result;
        }

        const bool gait_established = signals.gait_cycles >= 6u;
        const bool run_ready = signals.gait_cycles >= 10u
            && result.balance_reserve >= 0.70f
            && result.terrain_demand <= 0.28f;
        if (run_ready)
        {
            result.intent = Intent::run;
            result.target_speed = 1.65f + result.balance_reserve * 0.75f;
            result.cadence_hz = 1.35f + result.balance_reserve * 0.30f;
            result.stride_scale = 0.82f;
            result.swing_lift = 0.68f;
            result.stance_extension = 0.68f;
        }
        else
        {
            result.intent = gait_established ? Intent::walk : Intent::hold;
            result.target_speed = gait_established
                ? 0.82f + result.balance_reserve * 0.35f
                : 0.42f + result.balance_reserve * 0.22f;
            result.cadence_hz = gait_established ? 0.96f : 0.72f;
            result.stride_scale = gait_established ? 0.62f : 0.42f;
            result.swing_lift = gait_established ? 0.55f : 0.45f;
            result.stance_extension = gait_established ? 0.56f : 0.48f;
        }
        result.brake = std::abs(signals.forward_speed)
            > result.target_speed * 1.30f
            || result.balance_reserve < 0.52f
            || result.terrain_demand > 0.48f;
        return result;
    }

    [[nodiscard]] inline float target_speed_reward(const Plan& plan_value,
        float signed_speed) noexcept
    {
        const float desired = plan_value.target_speed;
        if (desired <= 0.01f)
            return std::clamp(1.0f - std::abs(signed_speed) / 0.30f, 0.0f, 1.0f);
        const float error = std::abs(signed_speed - desired);
        return std::clamp(1.0f - error / std::max(0.35f, desired), 0.0f, 1.0f);
    }
}
