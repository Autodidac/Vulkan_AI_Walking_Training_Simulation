#pragma once

namespace runner::preview_sync
{
    struct Decision
    {
        bool replace_blueprint{};
        bool start_replay{};
    };

    [[nodiscard]] constexpr Decision decide(bool rig_changed,
        bool first_publication, bool editing_rig, bool explicit_reset) noexcept
    {
        return {
            rig_changed && !editing_rig,
            !editing_rig && (rig_changed || first_publication || explicit_reset)
        };
    }
}
