from pathlib import Path

root = Path(__file__).resolve().parents[1]

main_path = root / "src/main.cpp"
main = main_path.read_text(encoding="utf-8")
duplicate = '''        int drawable_width{};
        int drawable_height{};
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, drawable_width, drawable_height);
'''
replacement = '''        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        application.frame(input, dt, drawable_width, drawable_height);
'''
if duplicate in main:
    main = main.replace(duplicate, replacement, 1)
    main_path.write_text(main, encoding="utf-8", newline="\n")
elif main.count("int drawable_width{};") != 1:
    raise RuntimeError("Unexpected drawable-width declaration layout")

app_path = root / "src/app.cpp"
app = app_path.read_text(encoding="utf-8")
old = '''            std::vector<Vec2> ground{};
            ground.reserve(65);
            for (int sample = 0; sample <= 64; ++sample)
            {
                const float screen_fraction = static_cast<float>(sample) / 64.0f;
                const float world_x = camera
                    + (screen_fraction - 0.5f) * inner.size.x / scale;
                ground.push_back(world_to_screen({ world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground, 3.0f, rgb(0x51606c));
'''
new = '''            std::vector<Vec2> ground_points{};
            ground_points.reserve(65);
            for (int sample = 0; sample <= 64; ++sample)
            {
                const float screen_fraction = static_cast<float>(sample) / 64.0f;
                const float world_x = camera
                    + (screen_fraction - 0.5f) * inner.size.x / scale;
                ground_points.push_back(world_to_screen(
                    { world_x, environment.ground_height_at(world_x) },
                    inner, camera, scale, 0.82f));
            }
            canvas.polyline(ground_points, 3.0f, rgb(0x51606c));
'''
if old in app:
    app = app.replace(old, new, 1)
    app_path.write_text(app, encoding="utf-8", newline="\n")
elif "std::vector<Vec2> ground_points{};" not in app:
    raise RuntimeError("Unexpected training-preview ground layout")

print("materialized MSVC duplicate-local fixes")
