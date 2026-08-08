#include "rig_training_diagnostic.hpp"

#include "ppo.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace runner::diagnostics
{
    namespace
    {
        struct RigCase
        {
            std::string_view name{};
            sim::CreatureBlueprint blueprint{};
        };
    }

    RigTrainingReport run_rig_training_diagnostic(std::uint64_t updates)
    {
        updates = std::max<std::uint64_t>(1u, updates);
        const std::array cases{
            RigCase{ "biped", sim::CreatureBlueprint::biped() },
            RigCase{ "quadruped", sim::CreatureBlueprint::quadruped() },
            RigCase{ "crawler4", sim::CreatureBlueprint::crawler4() },
            RigCase{ "hexapod", sim::CreatureBlueprint::hexapod() }
        };

        RigTrainingReport report{};
        report.updates = updates;
        for (std::size_t index = 0; index < cases.size(); ++index)
        {
            const RigCase& rig = cases[index];
            rl::PpoTrainer trainer{ rig.blueprint, 8u, true };
            trainer.set_course(sim::CourseStage::uneven, 0.30f, false);
            for (std::uint64_t update = 0; update < updates; ++update)
            {
                trainer.train_one_update();
                for (std::uint32_t frame = 0; frame < 60u; ++frame)
                    trainer.step_preview();
            }

            const rl::TrainingMetrics& metrics = trainer.metrics();
            bool course_motion_enabled{};
            for (const sim::Environment& environment : trainer.environments())
                course_motion_enabled = course_motion_enabled
                    || environment.course_motion_enabled();
            report.rigs[index] = {
                rig.name,
                metrics.mean_episode_distance,
                metrics.evaluation_distance,
                metrics.evaluation_stride_events,
                metrics.evaluation_invalid_runs,
                trainer.preview_reset_count(),
                trainer.preview_last_reset_reason(),
                course_motion_enabled
            };
        }

        const RigTrainingResult& baseline = report.rigs.front();
        report.passed = std::ranges::all_of(report.rigs,
            [](const RigTrainingResult& result)
            {
                return std::isfinite(result.mean_episode_distance)
                    && std::isfinite(result.evaluation_distance)
                    && !result.rollout_course_motion_enabled
                    && result.preview_resets <= 24u;
            });
        for (std::size_t index = 1; index < report.rigs.size(); ++index)
        {
            const RigTrainingResult& result = report.rigs[index];
            report.passed = report.passed
                && result.evaluation_distance >= baseline.evaluation_distance - 0.25f
                && result.evaluation_stride_events >= 2.0f;
        }
        return report;
    }
}
