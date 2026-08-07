#pragma once

namespace runner::preview_sync
{
    struct Decision
    {
        bool replace_blueprint{};
        bool replace_course{};
        bool reset_episode{};
        bool adopt_controller{ true };
    };

    [[nodiscard]] constexpr Decision decide(bool rig_changed,
        bool course_changed, bool best_changed) noexcept
    {
        return {
            rig_changed,
            course_changed,
            rig_changed || course_changed,
            best_changed || !rig_changed
        };
    }
}
