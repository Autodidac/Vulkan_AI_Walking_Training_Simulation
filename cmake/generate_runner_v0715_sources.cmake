if(NOT DEFINED RUNNER_SOURCE_DIR OR NOT DEFINED RUNNER_OUTPUT_DIR)
    message(FATAL_ERROR "RUNNER_SOURCE_DIR and RUNNER_OUTPUT_DIR are required")
endif()

function(runner_replace_once variable old_text new_text label)
    set(value "${${variable}}")
    string(FIND "${value}" "${old_text}" match_offset)
    if(match_offset EQUAL -1)
        message(FATAL_ERROR "Runner v0.7.15 source generation could not find ${label}")
    endif()
    string(REPLACE "${old_text}" "${new_text}" value "${value}")
    set(${variable} "${value}" PARENT_SCOPE)
endfunction()

file(MAKE_DIRECTORY "${RUNNER_OUTPUT_DIR}")

file(READ "${RUNNER_SOURCE_DIR}/src/app.cpp" app_source)
runner_replace_once(app_source
    [=[float pixels_per_meter, float ground_fraction = 0.84f) noexcept]=]
    [=[float pixels_per_meter, float ground_fraction = 0.72f) noexcept]=]
    "live ground framing default")
runner_replace_once(app_source
    [=[const float ground_y = viewport.position.y + viewport.size.y * 0.84f;]=]
    [=[const float ground_y = viewport.position.y + viewport.size.y * 0.72f;]=]
    "screen-to-world ground framing")

runner_replace_once(app_source
[=[                    if (tile.macro_ready)
                    {
                        draw_world_cell(macro_x0, macro_y0,
                            macro_x0 + sim::DeformableTerrain::macro_tile_size,
                            macro_y0 + sim::DeformableTerrain::macro_tile_size,
                            tile.uniform_material);
                        continue;
                    }
]=]
[=[                    const float macro_y1 = macro_y0
                        + sim::DeformableTerrain::macro_tile_size;
                    bool near_surface = false;
                    for (std::size_t local_x = 0;
                        local_x < sim::DeformableTerrain::macro_cell_side; ++local_x)
                    {
                        const float sample_x = macro_x0
                            + (static_cast<float>(local_x) + 0.5f)
                                * sim::DeformableTerrain::fine_cell_spacing;
                        if (macro_y1 >= environment.ground_height_at(sample_x)
                            - sim::DeformableTerrain::fine_cell_spacing * 3.0f)
                        {
                            near_surface = true;
                            break;
                        }
                    }
                    if (tile.macro_ready && !tile.active && !near_surface)
                    {
                        draw_world_cell(macro_x0, macro_y0,
                            macro_x0 + sim::DeformableTerrain::macro_tile_size,
                            macro_y1, tile.uniform_material);
                        continue;
                    }
]=]
    "surface-aware macro-tile renderer")

runner_replace_once(app_source
[=[
            std::vector<Vec2> surface{};
            const int first_column = static_cast<int>(std::floor(
                left / sim::DeformableTerrain::fine_cell_spacing));
            const int last_column = static_cast<int>(std::ceil(
                right / sim::DeformableTerrain::fine_cell_spacing));
            surface.reserve(static_cast<std::size_t>(std::max(0,
                last_column - first_column + 1)));
            for (int column = first_column; column <= last_column; ++column)
            {
                const float x = static_cast<float>(column)
                    * sim::DeformableTerrain::fine_cell_spacing;
                surface.push_back(world_to_screen({ x, environment.ground_height_at(x) },
                    viewport, camera, scale));
            }
            if (surface.size() >= 2u)
                canvas.polyline(surface, 1.5f, rgb(0x5d6870, 0.72f));
]=]
[=[
]=]
    "duplicate terrain surface polyline")

runner_replace_once(app_source
[=[
            constexpr float dash_spacing = 1.6f;
            const int first_dash = static_cast<int>(std::floor((left + progress) / dash_spacing));
            const int last_dash = static_cast<int>(std::ceil((right + progress) / dash_spacing));
            for (int index = first_dash; index <= last_dash; ++index)
            {
                const float x0 = static_cast<float>(index) * dash_spacing - progress;
                const float x1 = x0 + 0.72f;
                const Vec2 start = world_to_screen(
                    { x0, environment.ground_height_at(x0) + 0.035f }, viewport, camera, scale);
                const Vec2 end = world_to_screen(
                    { x1, environment.ground_height_at(x1) + 0.035f }, viewport, camera, scale);
                canvas.line(start, end, 3.0f, rgb(0xd6d9c4, 0.82f));
            }
]=]
[=[
]=]
    "moving course-reference dashes")
runner_replace_once(app_source
    [=[                if (index < 0)]=]
    [=[                if (index <= 0)]=]
    "zero-distance marker suppression")

runner_replace_once(app_source
[=[            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 1.8f, 0.045f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, 90.0f);
            draw_course_reference(environment, viewport, camera_x, 90.0f);
            draw_course_features(environment, viewport, camera_x, 90.0f);
            draw_creature(environment, viewport, camera_x, 90.0f);
]=]
[=[            constexpr float live_pixels_per_meter = 22.0f;
            if (!environment.particles().empty())
                camera_x = lerp(camera_x,
                    environment.particles()[environment.blueprint().root_node].position.x + 5.5f, 0.035f);
            add_rounded_rect(canvas, viewport, 11.0f, rgb(0x09101a), border, 1.0f);
            draw_course_ground(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_reference(environment, viewport, camera_x, live_pixels_per_meter);
            draw_course_features(environment, viewport, camera_x, live_pixels_per_meter);
            draw_creature(environment, viewport, camera_x, live_pixels_per_meter);
]=]
    "live-world camera scale")

runner_replace_once(app_source
    [=[runner-v0714-autosave.eppo]=]
    [=[runner-v0715-autosave.eppo]=]
    "v0.7.15 policy autosave")
runner_replace_once(app_source
    [=[runner-v0714-evolved.rig]=]
    [=[runner-v0715-evolved.rig]=]
    "v0.7.15 rig autosave")
runner_replace_once(app_source
    [=[runner-v0714-autonomy.state]=]
    [=[runner-v0715-autonomy.state]=]
    "v0.7.15 autonomy autosave")
file(WRITE "${RUNNER_OUTPUT_DIR}/app.cpp" "${app_source}")

file(READ "${RUNNER_SOURCE_DIR}/src/autonomy_curriculum.cpp" curriculum_source)
runner_replace_once(curriculum_source
[=[        if (!metrics.evaluation_valid)
        {
            worker_message_ = std::format("INVALID RUN REJECTED - {} / {}",
                metrics.evaluation_invalid_runs,
                primary_motion_rejection_name(metrics.evaluation_rejection_mask));
        }
]=]
[=[        if (!metrics.evaluation_valid)
        {
            const bool catastrophic_invalid = metrics.evaluation_quality_key == 0u
                || metrics.evaluation_distance < -0.25f
                || metrics.evaluation_invalid_runs >= 3u;
            if (catastrophic_invalid && worker_.has_best_policy()
                && worker_.restore_best_policy())
            {
                ++rollback_count_;
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_.set_course(stage_, difficulty_, false);
                worker_message_ = "INVALID/BACKWARD GENERATION - RESTORED CHAMPION AND RESTARTED LESSON";
                queue_autosave();
                return;
            }
            if (catastrophic_invalid && !worker_.has_best_policy()
                && metrics.evaluation_count % 3u == 0u)
            {
                worker_.set_blueprint(worker_.blueprint(), false);
                worker_.set_course(stage_, difficulty_, false);
                mastery_streak_ = 0;
                degradation_streak_ = 0;
                worker_message_ = "NO VALID CHAMPION AFTER THREE EVALUATIONS - RESET POLICY NURSERY";
                queue_autosave();
                return;
            }
            worker_message_ = std::format("INVALID RUN REJECTED - {} / {}",
                metrics.evaluation_invalid_runs,
                primary_motion_rejection_name(metrics.evaluation_rejection_mask));
        }
]=]
    "catastrophic invalid-policy recovery")
file(WRITE "${RUNNER_OUTPUT_DIR}/autonomy_curriculum.cpp" "${curriculum_source}")
