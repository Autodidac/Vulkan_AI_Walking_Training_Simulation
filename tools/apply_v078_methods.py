from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/app.cpp'
text = path.read_text(encoding='utf-8')
needle = '''        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
'''
overlay = '''        void draw_biomechanical_overlay(const sim::Environment& environment,
            Rect viewport, float camera, float scale)
        {
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty())
                return;

            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            auto ring = [&](Vec2 center, float radius, Color color)
            {
                std::array<Vec2, 33> points{};
                for (std::size_t index = 0; index < points.size(); ++index)
                {
                    const float angle = static_cast<float>(index)
                        / static_cast<float>(points.size() - 1u) * pi * 2.0f;
                    points[index] = center + Vec2{ std::cos(angle), std::sin(angle) } * radius;
                }
                canvas.polyline(points, 1.35f, color);
            };

            const float phase = session_runtime_seconds;
            const Vec2 ghost_offset{ 18.0f, -8.0f };
            for (std::size_t index = 0; index < rig.bones.size(); ++index)
            {
                const sim::DistanceConstraint& bone = rig.bones[index];
                if (bone.a >= particles.size() || bone.b >= particles.size())
                    continue;
                const Vec2 a = point(bone.a);
                const Vec2 b = point(bone.b);
                canvas.line(a, b, 1.25f, with_alpha(accent, 0.22f));
                canvas.line(a + ghost_offset, b + ghost_offset, 0.85f,
                    with_alpha(body_light, 0.10f));
                const float packet_phase = std::fmod(phase * 0.62f
                    + static_cast<float>(index) * 0.173f, 1.0f);
                canvas.circle(a + (b - a) * packet_phase, 2.2f,
                    with_alpha(accent, 0.62f), 12);
            }

            for (std::size_t index = 0; index < particles.size(); ++index)
            {
                const bool semantic = index == rig.root_node || index == rig.torso_node
                    || index == rig.head_node || rig.is_support_seed(index);
                if (!semantic)
                    continue;
                const float radius = (index < rig.radii.size() ? rig.radii[index] : 0.15f)
                    * scale + 7.0f + std::sin(phase * 2.4f
                        + static_cast<float>(index)) * 1.8f;
                ring(point(index), radius, with_alpha(
                    index == rig.head_node ? body_light : accent, 0.42f));
            }

            if (rig.torso_node < particles.size())
            {
                const Vec2 center = point(rig.torso_node) + Vec2{ 24.0f, -28.0f };
                const Rect chip{ center - Vec2{ 18.0f, 12.0f }, { 36.0f, 24.0f } };
                add_rounded_rect(canvas, chip, 4.0f, rgb(0x091923, 0.72f),
                    with_alpha(accent, 0.62f), 1.0f);
                canvas.line(center - Vec2{ 8.0f, 0.0f }, center + Vec2{ 8.0f, 0.0f },
                    1.3f, with_alpha(accent, 0.72f));
                canvas.line(center - Vec2{ 0.0f, 7.0f }, center + Vec2{ 0.0f, 7.0f },
                    1.3f, with_alpha(accent, 0.72f));
                for (int pin = -1; pin <= 1; ++pin)
                {
                    const float y = center.y + static_cast<float>(pin) * 7.0f;
                    canvas.line({ chip.position.x - 5.0f, y }, { chip.position.x, y },
                        1.0f, with_alpha(body_light, 0.52f));
                    canvas.line({ chip.position.x + chip.size.x, y },
                        { chip.position.x + chip.size.x + 5.0f, y },
                        1.0f, with_alpha(body_light, 0.52f));
                }
            }
        }

        void draw_creature(const sim::Environment& environment, Rect viewport, float camera,
            float scale, bool show_nodes = false)
'''
if text.count(needle) != 1:
    raise RuntimeError(f'draw_creature insertion matched {text.count(needle)}')
text = text.replace(needle, overlay, 1)
point_block = '''            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            for (const sim::DistanceConstraint& bone : rig.bones)
'''
point_replacement = '''            auto point = [&](std::size_t index)
            {
                return world_to_screen(particles[index].position, viewport, camera, scale);
            };
            draw_biomechanical_overlay(environment, viewport, camera, scale);
            for (const sim::DistanceConstraint& bone : rig.bones)
'''
if text.count(point_block) != 1:
    raise RuntimeError(f'live overlay call matched {text.count(point_block)}')
text = text.replace(point_block, point_replacement, 1)
lab_block = '''            auto preview_screen = [&](std::size_t index)
            {
                return world_to_screen(preview[index], viewport, 0.0f, scale);
            };
            for (const sim::DistanceConstraint& bone : blueprint.bones)
'''
lab_replacement = '''            auto preview_screen = [&](std::size_t index)
            {
                return world_to_screen(preview[index], viewport, 0.0f, scale);
            };
            for (std::size_t index = 0; index < blueprint.bones.size(); ++index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[index];
                if (bone.a >= preview.size() || bone.b >= preview.size())
                    continue;
                const Vec2 a = preview_screen(bone.a);
                const Vec2 b = preview_screen(bone.b);
                canvas.line(a, b, 1.25f, with_alpha(accent, 0.24f));
                const float packet_phase = std::fmod(session_runtime_seconds * 0.55f
                    + static_cast<float>(index) * 0.137f, 1.0f);
                canvas.circle(a + (b - a) * packet_phase, 2.4f,
                    with_alpha(body_light, 0.68f), 12);
            }
            for (std::size_t index = 0; index < preview.size(); ++index)
            {
                if (index != blueprint.root_node && index != blueprint.torso_node
                    && index != blueprint.head_node && !blueprint.is_support_seed(index))
                    continue;
                const Vec2 center = preview_screen(index);
                const float radius = 13.0f + std::sin(session_runtime_seconds * 2.2f
                    + static_cast<float>(index)) * 2.0f;
                std::array<Vec2, 25> halo{};
                for (std::size_t point_index = 0; point_index < halo.size(); ++point_index)
                {
                    const float angle = static_cast<float>(point_index)
                        / static_cast<float>(halo.size() - 1u) * pi * 2.0f;
                    halo[point_index] = center
                        + Vec2{ std::cos(angle), std::sin(angle) } * radius;
                }
                canvas.polyline(halo, 1.25f, with_alpha(accent, 0.44f));
            }
            for (const sim::DistanceConstraint& bone : blueprint.bones)
'''
if text.count(lab_block) != 1:
    raise RuntimeError(f'rig lab overlay matched {text.count(lab_block)}')
path.write_text(text.replace(lab_block, lab_replacement, 1), encoding='utf-8')
Path(__file__).unlink()
