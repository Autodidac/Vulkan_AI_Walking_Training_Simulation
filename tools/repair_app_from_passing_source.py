from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = subprocess.run(
    ['git', 'show', '35e1d62c7c14169aa3b0b04d7a4dcdb1587c12c5:src/app.cpp'],
    check=True,
    capture_output=True,
    text=True,
).stdout

source = source.replace(
    '            gui::Vec2 measured = font::measure_text(label, scale * ui_font_scale);',
    '            Vec2 measured = font::measure_text(label, scale * ui_font_scale);',
    1,
)

old = '''                add_text(canvas, feature_screen + Vec2{ -58.0f, -42.0f },
                    std::format(feature.kind == sim::CourseFeatureKind::duck_press
                        ? std::format("TRAINER: {}", sim::course_feature_name(feature.kind))
                        : std::format("HAZARD: {}", sim::course_feature_name(feature.kind))), 1.00f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : danger);'''
new = '''                const std::string feature_label = feature.kind == sim::CourseFeatureKind::duck_press
                    ? std::format("TRAINER: {}", sim::course_feature_name(feature.kind))
                    : std::format("HAZARD: {}", sim::course_feature_name(feature.kind));
                add_text(canvas, feature_screen + Vec2{ -58.0f, -42.0f },
                    feature_label, 1.00f,
                    feature.kind == sim::CourseFeatureKind::projectile ? yellow : danger);'''
if old not in source:
    raise RuntimeError('MSVC format target missing from passing app source')
source = source.replace(old, new, 1)

(root / 'src/app.cpp').write_text(source, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('restored complete passing app.cpp and applied only the two MSVC fixes')
