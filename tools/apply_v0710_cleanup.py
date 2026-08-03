from pathlib import Path

root = Path(__file__).resolve().parents[1]
app_path = root / "src/app.cpp"
text = app_path.read_text(encoding="utf-8")

old_pixels = '''                int red{};
                int green{};
                int blue{};
                if (!read_ppm_number(input, red) || !read_ppm_number(input, green)
                    || !read_ppm_number(input, blue)
                    || red < 0 || green < 0 || blue < 0
                    || red > maximum || green > maximum || blue > maximum)
                {
                    error = "Original Runner artwork has incomplete pixel data: "
                        + path.string();
                    return false;
                }
                loaded.pixels.push_back({
                    static_cast<float>(red) * inverse_maximum,
                    static_cast<float>(green) * inverse_maximum,
                    static_cast<float>(blue) * inverse_maximum,
                    1.0f
                });'''
new_pixels = '''                int red_channel{};
                int green_channel{};
                int blue_channel{};
                if (!read_ppm_number(input, red_channel)
                    || !read_ppm_number(input, green_channel)
                    || !read_ppm_number(input, blue_channel)
                    || red_channel < 0 || green_channel < 0 || blue_channel < 0
                    || red_channel > maximum || green_channel > maximum
                    || blue_channel > maximum)
                {
                    error = "Original Runner artwork has incomplete pixel data: "
                        + path.string();
                    return false;
                }
                loaded.pixels.push_back({
                    static_cast<float>(red_channel) * inverse_maximum,
                    static_cast<float>(green_channel) * inverse_maximum,
                    static_cast<float>(blue_channel) * inverse_maximum,
                    1.0f
                });'''
if text.count(old_pixels) != 1:
    raise RuntimeError("PPM channel block not found exactly once")
text = text.replace(old_pixels, new_pixels, 1)

old_autosaves = '''        std::filesystem::path autosave_policy_path{ "runner-v078-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v078-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v078-autonomy.state" };'''
new_autosaves = '''        std::filesystem::path autosave_policy_path{ "runner-v0710-autosave.eppo" };
        std::filesystem::path autosave_rig_path{ "runner-v0710-evolved.rig" };
        std::filesystem::path autosave_state_path{ "runner-v0710-autonomy.state" };'''
if text.count(old_autosaves) != 1:
    raise RuntimeError("old v0.7.8 autosave namespace not found exactly once")
text = text.replace(old_autosaves, new_autosaves, 1)
app_path.write_text(text, encoding="utf-8")

cache_path = root / "missioncache.md"
cache = cache_path.read_text(encoding="utf-8")
if "WALK-AUTOSAVE-113" not in cache:
    cache = cache.rstrip() + '''

### WALK-AUTOSAVE-113 — Isolate corrected training state from v0.7.8
**Status:** IN VALIDATION

Runner v0.7.10 writes and loads only `runner-v0710-*` autosave, evolved-rig, and autonomy-state files. It cannot silently resume the stale v0.7.8 curriculum or evolved rig that reproduced the live regression.
'''
cache_path.write_text(cache, encoding="utf-8")

changelog_path = root / "CHANGELOG.md"
changelog = changelog_path.read_text(encoding="utf-8")
bullet = "- Isolated v0.7.10 autosaves from stale `runner-v078-*` policy, rig, and curriculum-state files."
if bullet not in changelog:
    anchor = "- Loaded and rendered the original Runner pixel artwork from the packaged `assets/chicken.ppm` asset."
    if anchor not in changelog:
        raise RuntimeError("art changelog anchor missing")
    changelog = changelog.replace(anchor, anchor + "\n" + bullet, 1)
changelog_path.write_text(changelog, encoding="utf-8")

Path(__file__).unlink()
