#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

namespace runner::presentation
{
    enum class StartupMode : std::uint8_t
    {
        interactive,
        isolated_visual_proof
    };

    enum class Session : std::uint8_t
    {
        interactive,
        course_eye,
        art_eye,
        walk_eye,
        equipment_eye
    };

    enum class EquipmentEyeRequest : std::uint8_t
    {
        none,
        exact,
        invalid
    };

    [[nodiscard]] constexpr EquipmentEyeRequest parse_equipment_eye_request(
        std::string_view argument) noexcept
    {
        if (argument == "--equipment-eye-test")
            return EquipmentEyeRequest::exact;
        if (argument.starts_with("--equipment-eye-test"))
            return EquipmentEyeRequest::invalid;
        return EquipmentEyeRequest::none;
    }

    [[nodiscard]] constexpr bool isolated(Session session) noexcept
    {
        return session != Session::interactive;
    }

    [[nodiscard]] constexpr bool course_eye(Session session) noexcept
    {
        return session == Session::course_eye;
    }

    [[nodiscard]] constexpr bool art_eye(Session session) noexcept
    {
        return session == Session::art_eye;
    }

    [[nodiscard]] constexpr bool walk_eye(Session session) noexcept
    {
        return session == Session::walk_eye;
    }

    [[nodiscard]] constexpr bool equipment_eye(Session session) noexcept
    {
        return session == Session::equipment_eye;
    }

    inline constexpr float equipment_eye_minimum_plant_seconds = 0.16f;
    inline constexpr float equipment_eye_minimum_settle_seconds = 0.30f;
    inline constexpr float equipment_eye_maximum_aim_rate = 0.28f;

    [[nodiscard]] inline bool equipment_eye_frame_ready(
        bool physical_ready_state, bool valid_motion,
        bool course_motion_enabled, bool target_visible,
        float plant_seconds, float settle_seconds,
        float aim_error, float aim_tolerance, float aim_rate,
        std::uint32_t shots_fired) noexcept
    {
        return physical_ready_state && valid_motion && !course_motion_enabled
            && target_visible && shots_fired == 0u
            && std::isfinite(plant_seconds)
            && plant_seconds >= equipment_eye_minimum_plant_seconds
            && std::isfinite(settle_seconds)
            && settle_seconds >= equipment_eye_minimum_settle_seconds
            && std::isfinite(aim_error) && std::isfinite(aim_tolerance)
            && aim_tolerance >= 0.0f && aim_error <= aim_tolerance
            && std::isfinite(aim_rate)
            && std::abs(aim_rate) <= equipment_eye_maximum_aim_rate;
    }

    [[nodiscard]] constexpr bool permits_training_controls(Session session) noexcept
    {
        return session == Session::interactive;
    }

    [[nodiscard]] constexpr bool permits_replay_controls(Session session) noexcept
    {
        return session == Session::walk_eye;
    }

    [[nodiscard]] constexpr bool replay_may_advance(Session session,
        bool paused, bool terminal_frozen) noexcept
    {
        return permits_replay_controls(session) && !paused && !terminal_frozen;
    }

    [[nodiscard]] constexpr bool replay_retry_available(Session session,
        bool terminal_frozen) noexcept
    {
        return permits_replay_controls(session) && terminal_frozen;
    }

    [[nodiscard]] constexpr bool tracks_subject(Session session) noexcept
    {
        return session == Session::interactive || session == Session::walk_eye;
    }
}
