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
app = app_path.read_text(encoding="utf-8")n