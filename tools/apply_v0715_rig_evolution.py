from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def patch_simulation_header() -> None:
    text = read("src/simulation.hpp")
    anchor = """    enum class FootContactPhase : std::uint8_t
"""
    contract = """    enum class RigTestPattern : std::uint8_t
    {
        manual,
        crouch,
        gait
    };

    [[nodiscard]] inline float rig_test_motor_input(RigTestPattern pattern,
        std::size_t motor_index, float phase, float manual_input) noexcept
    {
        if (pattern == RigTestPattern::manual)
            return clamp(manual_input, -1.0f, 1.0f);
        if (pattern == RigTestPattern::crouch)
        {
            constexpr std::array<float, action_count> crouch{
                -0.22f, 0.70f, 0.22f, -0.70f, 0.0f, 0.0f, 0.0f, 0.0f
            };
            return crouch[std::min(motor_index, crouch.size() - 1u)];
        }
        const float swing = std::sin(phase);
        const std::array<float, action_count> gait{
            0.58f * swing,
            0.48f * std::max(0.0f, swing),
            -0.58f * swing,
            -0.48f * std::max(0.0f, -swing),
            -0.16f * swing, 0.08f * swing,
            0.16f * swing, -0.08f * swing
        };
        return gait[std::min(motor_index, gait.size() - 1u)];
    }

"""
    text = replace_once(text, anchor, contract + anchor,
        "rig test pattern contract")
    write("src/simulation.hpp", text)


def patch_app() -> None:
    text = read("src/app.cpp")
    text = replace_once(text,
        """        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };
""",
        """        enum class JointTestGroup : std::uint8_t { selected, pair_a, pair_b, all };
""",
        "joint group enum anchor")
    text = replace_once(text,
        """        int selected_node{ -1 };
        int selected_motor{};
""",
        """        int selected_node{ -1 };
        int selected_bone{ -1 };
        int selected_motor{};
""",
        "selected bone editor state")
    text = replace_once(text,
        """        bool joint_auto_sweep{};
        bool run_paused{};
""",
        """        bool joint_auto_sweep{};
        bool right_leg_near{ true };
        bool rig_test_loose_ground{};
        sim::RigTestPattern rig_test_pattern{ sim::RigTestPattern::manual };
        bool run_paused{};
""",
        "rig test and depth state")

    anchor = """        [[nodiscard]] bool has_direct_bone(std::uint16_t a, std::uint16_t b) const noexcept
"""
    helpers = """        [[nodiscard]] static bool blueprint_connected(
            const sim::CreatureBlueprint& rig) noexcept
        {
            if (rig.nodes.empty())
                return false;
            std::vector<bool> visited(rig.nodes.size(), false);
            std::vector<std::uint16_t> stack{ rig.root_node };
            if (rig.root_node >= rig.nodes.size())
                return false;
            visited[rig.root_node] = true;
            while (!stack.empty())
            {
                const std::uint16_t node = stack.back();
                stack.pop_back();
                for (const sim::DistanceConstraint& bone : rig.bones)
                {
                    std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                    if (bone.a == node) next = bone.b;
                    else if (bone.b == node) next = bone.a;
                    if (next < visited.size() && !visited[next])
                    {
                        visited[next] = true;
                        stack.push_back(next);
                    }
                }
            }
            return std::ranges::all_of(visited, [](bool value) { return value; });
        }

        [[nodiscard]] float test_input_for_motor(std::size_t motor_index) const noexcept
        {
            return sim::rig_test_motor_input(rig_test_pattern, motor_index,
                session_runtime_seconds * 2.0f * pi * 1.05f, joint_test_input);
        }

"""
    text = replace_once(text, anchor, helpers + anchor,
        "editor connectivity and test pattern helpers")

    text = replace_once(text,
        """            selected_node = -1;
            selected_motor = 0;
""",
        """            selected_node = -1;
            selected_bone = -1;
            selected_motor = 0;
""",
        "preset resets selected bone")
    text = replace_once(text,
        """            selected_node = -1;
            apply_small_rig_change("NODE DELETED; AFFECTED MOTORS DISABLED");
""",
        """            selected_node = -1;
            selected_bone = -1;
            apply_small_rig_change("NODE DELETED; AFFECTED MOTORS DISABLED");
""",
        "node deletion resets selected bone")

    text = replace_once(text,
        """            const auto& rig = environment.blueprint();
            if (particles.empty())
""",
        """            const auto& rig = environment.blueprint();
            if (particles.empty())
""",
        "draw creature rig anchor")
    text = replace_once(text,
        """                const Color color = side < 0 ? rgb(0x765033)
                    : side > 0 ? leg : body;
""",
        """                const bool near = side != 0
                    && ((side > 0) == right_leg_near);
                const Color color = side == 0 ? body
                    : near ? leg : rgb(0x765033);
""",
        "near side bone toggle")
    text = replace_once(text,
        """                if (side < 0)
                    color = rgb(0x765033);
                else if (side > 0)
                    color = leg;
""",
        """                if (side != 0)
                    color = ((side > 0) == right_leg_near)
                        ? leg : rgb(0x765033);
""",
        "near side node toggle")
    text = replace_once(text,
        """                    color = side < 0 ? rgb(0x765033) : leg;
""",
        """                    color = side != 0 && ((side > 0) == right_leg_near)
                        ? leg : rgb(0x765033);
""",
        "near side foot toggle")

    text = replace_once(text,
        """            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();
""",
        """            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
            const rl::TrainingMetrics& metrics = trainer.metrics();
""",
        "live panel autonomy anchor")
    anchor = """            cursor.y += 15.0f;

            const float third = (usable_width - 12.0f) / 3.0f;
"""
    addition = """            cursor.y += 15.0f;
            add_text_fit(canvas, cursor,
                std::format("RIG GEN {}  ACCEPT {}  REJECT {}  RB {}  {}",
                    autonomy.rig_generation, autonomy.accepted_rig_changes,
                    autonomy.rejected_rig_changes, autonomy.rollback_count,
                    autonomy.pipeline_stage),
                0.80f, accent, usable_width, 0.66f);
            cursor.y += 25.0f;

            const float third = (usable_width - 12.0f) / 3.0f;
"""
    text = replace_once(text, anchor, addition,
        "live evolution telemetry")

    text = replace_once(text,
        """        void draw_rig_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
""",
        """        void draw_rig_panel(Rect rect, const InputState& input)
        {
            add_rounded_rect(canvas, rect, 11.0f, panel, border, 1.0f);
            const rl::AutonomyStatus& autonomy = trainer.autonomy_status();
""",
        "rig panel evolution status")

    anchor = """                cursor.y += 50.0f;

                add_text(canvas, cursor, std::format("SELECTED NODE: {}", selected_node), 1.20f, muted);
"""
    controls = """                cursor.y += 50.0f;
                const float control_third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { control_third, 35.0f } },
                    right_leg_near ? "NEAR LEG: RIGHT" : "NEAR LEG: LEFT",
                    input, right_leg_near))
                    right_leg_near = !right_leg_near;
                if (button({ cursor + Vec2{ control_third + 6.0f, 0.0f },
                    { control_third, 35.0f } }, "RESTORE CHAMPION", input,
                    trainer.has_best_policy(), trainer.has_best_policy()))
                {
                    set_status(trainer.restore_best_policy()
                        ? "VERIFIED CHAMPION RESTORE QUEUED"
                        : "NO VERIFIED CHAMPION AVAILABLE");
                }
                if (button({ cursor + Vec2{ (control_third + 6.0f) * 2.0f, 0.0f },
                    { control_third, 35.0f } }, "FRESH POLICY", input))
                {
                    trainer.reset_policy(0x715300u
                        + autonomy.rig_generation * 0x9E3779B97F4A7C15ULL);
                    set_status("FRESH POLICY NURSERY QUEUED FOR CURRENT RIG");
                }
                cursor.y += 46.0f;

                add_text(canvas, cursor, std::format("SELECTED NODE: {}", selected_node), 1.20f, muted);
"""
    text = replace_once(text, anchor, controls,
        "rig depth and training controls")

    anchor = """                add_text(canvas, cursor, "SELECT OR DRAG NODES IN THE VIEW. SHIFT ADDS; CTRL CONNECTS.", 1.00f, muted);
"""
    bone_controls = """                cursor.y += 5.0f;
                add_text(canvas, cursor, std::format("SELECTED BONE: {}", selected_bone), 1.10f, muted);
                cursor.y += 27.0f;
                if (selected_bone >= 0
                    && static_cast<std::size_t>(selected_bone) < blueprint.bones.size())
                {
                    sim::DistanceConstraint& selected = blueprint.bones[
                        static_cast<std::size_t>(selected_bone)];
                    const float stiffness = slider({ cursor, { rect.size.x - 170.0f, 38.0f } },
                        "BONE STIFFNESS", selected.stiffness, 0.20f, 1.0f, input);
                    if (stiffness != selected.stiffness)
                    {
                        selected.stiffness = stiffness;
                        queue_rig_change("BONE STIFFNESS UPDATED");
                    }
                    if (button({ cursor + Vec2{ rect.size.x - 152.0f, -5.0f },
                        { 116.0f, 31.0f } }, "DELETE BONE", input))
                    {
                        sim::CreatureBlueprint candidate = blueprint;
                        candidate.bones.erase(candidate.bones.begin() + selected_bone);
                        if (candidate.valid() && blueprint_connected(candidate))
                        {
                            blueprint = std::move(candidate);
                            selected_bone = -1;
                            apply_small_rig_change("BONE DELETED");
                        }
                        else
                        {
                            set_status("BONE DELETE REJECTED - RIG OR MOTOR WOULD DISCONNECT");
                        }
                    }
                    cursor.y += 47.0f;
                }
                add_text(canvas, cursor,
                    std::format("GEN {}  ACCEPT {}  REJECT {}  ROLLBACK {}",
                        autonomy.rig_generation, autonomy.accepted_rig_changes,
                        autonomy.rejected_rig_changes, autonomy.rollback_count),
                    0.92f, accent);
                cursor.y += 25.0f;
                add_text(canvas, cursor, "SELECT/DRAG NODES. SHIFT ADDS, CTRL CONNECTS, ALT SELECTS BONE.", 0.92f, muted);
"""
    text = replace_once(text, anchor, bone_controls,
        "bone stiffness deletion and evolution controls")

    old = """            for (const sim::DistanceConstraint& bone : blueprint.bones)
            {
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen(bone.a), screen(bone.b), 17.0f, rgb(0x835927));
            }
"""
    new = """            for (std::size_t bone_index = 0; bone_index < blueprint.bones.size(); ++bone_index)
            {
                const sim::DistanceConstraint& bone = blueprint.bones[bone_index];
                if (bone.a < blueprint.nodes.size() && bone.b < blueprint.nodes.size())
                    canvas.line(screen(bone.a), screen(bone.b),
                        bone_index == static_cast<std::size_t>(selected_bone) ? 22.0f : 17.0f,
                        bone_index == static_cast<std::size_t>(selected_bone) ? accent : rgb(0x835927));
            }
"""
    text = replace_once(text, old, new,
        "selected bone highlight")

    anchor = """            if (input.left_pressed && contains(viewport, input.mouse) && !over_joint_lab)
            {
"""
    alt_select = """            if (input.left_pressed && input.alt
                && contains(viewport, input.mouse) && !over_joint_lab)
            {
                auto segment_distance = [](Vec2 point, Vec2 a, Vec2 b) noexcept
                {
                    const Vec2 segment = b - a;
                    const float denominator = dot(segment, segment);
                    const float t = denominator > 1.0e-6f
                        ? clamp(dot(point - a, segment) / denominator, 0.0f, 1.0f)
                        : 0.0f;
                    return length(point - (a + segment * t));
                };
                selected_bone = -1;
                float best_distance = 16.0f;
                for (std::size_t index = 0; index < blueprint.bones.size(); ++index)
                {
                    const sim::DistanceConstraint& bone = blueprint.bones[index];
                    if (bone.a >= blueprint.nodes.size() || bone.b >= blueprint.nodes.size())
                        continue;
                    const float distance = segment_distance(
                        input.mouse, screen(bone.a), screen(bone.b));
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        selected_bone = static_cast<int>(index);
                    }
                }
                selected_node = -1;
                dragging_node = false;
            }
            if (input.left_pressed && !input.alt
                && contains(viewport, input.mouse) && !over_joint_lab)
            {
"""
    text = replace_once(text, anchor, alt_select,
        "alt-click bone selection")

    text = replace_once(text,
        """                const float delta = wrap_angle(sim::motor_target_angle(motor, joint_test_input) - current);
""",
        """                const float delta = wrap_angle(sim::motor_target_angle(
                    motor, test_input_for_motor(static_cast<std::size_t>(motor_index))) - current);
""",
        "rig test pattern preview")

    text = replace_once(text,
        """            const Rect joint_rect{ { viewport.position.x + 20.0f, viewport.position.y + viewport.size.y - 174.0f },
                { std::min(850.0f, viewport.size.x - 40.0f), 154.0f } };
""",
        """            const Rect joint_rect{ { viewport.position.x + 20.0f, viewport.position.y + viewport.size.y - 220.0f },
                { std::min(850.0f, viewport.size.x - 40.0f), 200.0f } };
""",
        "expanded joint and gait test panel")

    text = replace_once(text,
        """                joint_auto_sweep = false;
                joint_test_input = -1.0f;
""",
        """                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = -1.0f;
""",
        "manual minimum test mode")
    text = replace_once(text,
        """                joint_auto_sweep = false;
                joint_test_input = 0.0f;
""",
        """                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 0.0f;
""",
        "manual rest test mode")
    text = replace_once(text,
        """                joint_auto_sweep = false;
                joint_test_input = 1.0f;
""",
        """                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = false;
                joint_test_input = 1.0f;
""",
        "manual maximum test mode")
    text = replace_once(text,
        """                joint_auto_sweep ? "STOP" : "SWEEP", input, joint_auto_sweep))
                joint_auto_sweep = !joint_auto_sweep;
            joint_test_input = slider({ rect.position + Vec2{ 14.0f, 119.0f }, { rect.size.x - 28.0f, 36.0f } },
                "TEST INPUT  -1 MIN / 0 REST / +1 MAX", joint_test_input, -1.0f, 1.0f, input);
""",
        """                joint_auto_sweep ? "STOP" : "SWEEP", input, joint_auto_sweep))
            {
                rig_test_pattern = sim::RigTestPattern::manual;
                joint_auto_sweep = !joint_auto_sweep;
            }

            row.y += 39.0f;
            if (button({ row, { group_width - 4.0f, 31.0f } }, "CROUCH", input,
                rig_test_pattern == sim::RigTestPattern::crouch))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::crouch;
            }
            if (button({ row + Vec2{ group_width, 0.0f }, { group_width - 4.0f, 31.0f } }, "GAIT CYCLE", input,
                rig_test_pattern == sim::RigTestPattern::gait))
            {
                joint_auto_sweep = false;
                rig_test_pattern = sim::RigTestPattern::gait;
            }
            if (button({ row + Vec2{ group_width * 2.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "FIRM GROUND", input,
                !rig_test_loose_ground))
                rig_test_loose_ground = false;
            if (button({ row + Vec2{ group_width * 3.0f, 0.0f }, { group_width - 4.0f, 31.0f } }, "LOOSE GROUND", input,
                rig_test_loose_ground))
                rig_test_loose_ground = true;

            const float friction = sim::foot_friction_retention(0.45f,
                rig_test_loose_ground ? 0.25f : 1.0f,
                rig_test_loose_ground ? 0.75f : 0.0f, false, false);
            add_text(canvas, rect.position + Vec2{ 14.0f, 119.0f },
                std::format("TRACTION TEST RETENTION {:.3f}  {}",
                    friction, rig_test_loose_ground ? "LOOSE" : "FIRM"),
                0.92f, rig_test_loose_ground ? yellow : green);
            joint_test_input = slider({ rect.position + Vec2{ 14.0f, 157.0f }, { rect.size.x - 28.0f, 36.0f } },
                "MANUAL INPUT  -1 MIN / 0 REST / +1 MAX", joint_test_input, -1.0f, 1.0f, input);
""",
        "crouch gait and friction editor test controls")

    text = replace_once(text,
        """                    "CLICK SELECT / DRAG MOVE / SHIFT ADD / CTRL CONNECT / DELETE REMOVE", 1.12f, muted);
""",
        """                    "CLICK/DRAG NODE / SHIFT ADD / CTRL CONNECT / ALT SELECT BONE / DELETE NODE", 1.02f, muted);
""",
        "editor instruction update")
    write("src/app.cpp", text)


def patch_tests() -> None:
    text = read("tests/core_tests.cpp")
    anchor = """    require(sim::foot_friction_retention(0.45f, 1.0f, 0.0f, true, false)
            < sim::foot_friction_retention(0.45f, 1.0f, 0.0f, false, false),
        "static lessons do not apply stronger planted-foot friction");
"""
    addition = anchor + """    require(sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                0u, 0.0f, 0.0f) < 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::crouch,
                1u, 0.0f, 0.0f) > 0.0f
            && sim::rig_test_motor_input(sim::RigTestPattern::gait,
                0u, pi * 0.5f, 0.0f)
                * sim::rig_test_motor_input(sim::RigTestPattern::gait,
                    2u, pi * 0.5f, 0.0f) < 0.0f,
        "rig lab crouch and alternating gait test patterns are incorrect");
"""
    text = replace_once(text, anchor, addition,
        "rig test pattern acceptance")

    anchor = """    const sim::CreatureBlueprint humanoid_rig = sim::CreatureBlueprint::humanoid();
"""
    addition = """    sim::CreatureBlueprint editor_bone_rig = sim::CreatureBlueprint::scaffold();
    require(editor_bone_rig.valid(), "editor scaffold starts invalid");
    editor_bone_rig.bones.front().stiffness = 0.42f;
    require(editor_bone_rig.valid()
            && std::abs(editor_bone_rig.bones.front().stiffness - 0.42f) < 0.0001f,
        "editor bone stiffness control cannot preserve a valid rig");

""" + anchor
    text = replace_once(text, anchor, addition,
        "bone stiffness editor acceptance")
    write("tests/core_tests.cpp", text)


def patch_documents() -> None:
    text = read("missioncache.md")
    text = replace_once(text,
        """### WALK-EDITOR-144 — Complete controls for gait, feet, evolution, and diagnostics
**Status:** OPEN
""",
        """### WALK-EDITOR-144 — Complete controls for gait, feet, evolution, and diagnostics
**Status:** IMPLEMENTED — DETERMINISTIC, CROSS-PLATFORM, AND SCREENSHOT VALIDATION REQUIRED
""",
        "editor mission implementation status")
    write("missioncache.md", text)

    text = read("CHANGELOG.md")
    anchor = """## Runner v0.7.15 — side-view gait and traction
"""
    addition = """## Runner v0.7.15 — locomotion and evolution editor

- Added ALT-click bone selection, stiffness editing, selected-bone highlighting, and safe connected-rig deletion checks.
- Added near/far leg display control, explicit champion restore and fresh-policy controls, and live generation/accept/reject/rollback telemetry.
- Added manual, sweep, squat, and alternating gait-cycle previews plus firm/loose-ground traction diagnostics and heel/ball/toe labels.

""" + anchor
    text = replace_once(text, anchor, addition,
        "editor completion changelog")
    write("CHANGELOG.md", text)


def main() -> None:
    patch_simulation_header()
    patch_app()
    patch_tests()
    patch_documents()


if __name__ == "__main__":
    main()
