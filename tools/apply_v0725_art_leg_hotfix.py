#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    target = ROOT / path
    target.write_text(text, encoding="utf-8", newline="\n")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_between(text: str, start: str, end: str, replacement: str,
    label: str) -> str:
    first = text.find(start)
    if first < 0:
        raise RuntimeError(f"{label}: start marker not found")
    last = text.find(end, first)
    if last < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:first] + replacement + text[last:]


def patch_application() -> None:
    path = "src/app.cpp"
    text = read(path)
    text = text.replace("runner-v0724-rig-autosave.eppo",
        "runner-v0725-rig-autosave.eppo")
    text = text.replace("runner-v0724-rig-evolved.rig",
        "runner-v0725-rig-evolved.rig")
    text = text.replace("runner-v0724-rig-autonomy.state",
        "runner-v0725-rig-autonomy.state")

    compact_armor = r'''            if (optional_art_enabled
                && rig.root_node < particles.size()
                && rig.torso_node < particles.size())
            {
                // COMPACT SEGMENTED BODY ARMOR: keep the approved helmet and
                // feet, but replace the oversized translucent bitmap sheet with
                // node-attached geometry that follows the real body and arms.
                const Vec2 root = point(rig.root_node);
                const Vec2 torso = point(rig.torso_node);
                const Vec2 body_axis = normalized(torso - root, { 0.0f, -1.0f });
                const Vec2 body_right{ -body_axis.y, body_axis.x };
                const float torso_length = std::max(24.0f, length(torso - root));

                float shoulder_span = torso_length * 0.82f;
                Vec2 left_shoulder = torso - body_right * shoulder_span * 0.5f;
                Vec2 right_shoulder = torso + body_right * shoulder_span * 0.5f;
                if (rig.active_motor_count >= 8u
                    && rig.motors[4].pivot < particles.size()
                    && rig.motors[6].pivot < particles.size())
                {
                    left_shoulder = point(rig.motors[4].pivot);
                    right_shoulder = point(rig.motors[6].pivot);
                    shoulder_span = std::max(24.0f,
                        length(right_shoulder - left_shoulder));
                }

                const Vec2 chest_bottom = root + body_axis * (torso_length * 0.20f);
                const Vec2 chest_top = torso - body_axis * (torso_length * 0.10f);
                const float chest_radius = std::clamp(
                    std::min(shoulder_span * 0.25f, torso_length * 0.28f),
                    10.0f, 27.0f);
                canvas.capsule(chest_bottom, chest_top, chest_radius + 2.0f,
                    rgb(0x33414c, 0.96f), 20);
                canvas.capsule(chest_bottom, chest_top, chest_radius,
                    rgb(0x8c9aa5, 0.94f), 20);
                canvas.capsule(chest_bottom + body_axis * 2.0f,
                    chest_top - body_axis * 3.0f,
                    std::max(7.0f, chest_radius * 0.62f),
                    rgb(0xaeb9c1, 0.78f), 18);

                const Vec2 indicator_center =
                    chest_bottom + (chest_top - chest_bottom) * 0.55f;
                const float indicator_half = std::clamp(
                    chest_radius * 0.56f, 6.0f, 14.0f);
                canvas.capsule(indicator_center - body_right * indicator_half,
                    indicator_center + body_right * indicator_half,
                    3.2f, rgb(0x0ed7e9), 12);

                const float shoulder_cap_radius = std::clamp(
                    shoulder_span * 0.15f, 7.0f, 14.0f);
                auto shoulder_cap = [&](Vec2 center)
                {
                    canvas.circle(center, shoulder_cap_radius + 1.5f,
                        rgb(0x34434f, 0.96f), 20);
                    canvas.circle(center, shoulder_cap_radius,
                        rgb(0x8b99a5, 0.92f), 20);
                };
                shoulder_cap(left_shoulder);
                shoulder_cap(right_shoulder);

                auto forearm_guard = [&](std::size_t motor_index)
                {
                    if (motor_index >= rig.active_motor_count)
                        return;
                    const sim::MotorConstraint& motor = rig.motors[motor_index];
                    if (!motor.enabled || motor.pivot >= particles.size()
                        || motor.c >= particles.size())
                        return;
                    const Vec2 elbow = point(motor.pivot);
                    const Vec2 hand = point(motor.c);
                    const Vec2 start = elbow + (hand - elbow) * 0.22f;
                    const Vec2 finish = elbow + (hand - elbow) * 0.78f;
                    const float guard_radius = std::clamp(
                        length(hand - elbow) * 0.10f, 3.8f, 7.0f);
                    canvas.capsule(start, finish, guard_radius + 1.0f,
                        rgb(0x34434f, 0.94f), 14);
                    canvas.capsule(start, finish, guard_radius,
                        rgb(0x7f8e9a, 0.88f), 14);
                };
                forearm_guard(5u);
                forearm_guard(7u);
            }
'''
    text = replace_between(text,
        "            if (optional_art_enabled && optional_torso_art.loaded()",
        "            if (optional_art_enabled && optional_helmet_art.loaded()",
        compact_armor,
        "compact body armor")
    write(path, text)


def patch_simulation() -> None:
    path = "src/simulation.cpp"
    text = read(path)

    projection = r'''    void Environment::project_structure_rigid(float dt) noexcept
    {
        static_cast<void>(dt);
        const bool upright_walking_stage = course_stage_ == CourseStage::uneven
            || course_stage_ == CourseStage::hurdles
            || course_stage_ == CourseStage::moving_hazards;
        if (!upright_walking_stage
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan()
            || recovery_active_ || non_foot_grounded_)
            return;

        struct LegChain
        {
            std::uint16_t hip{};
            std::uint16_t knee{};
            std::uint16_t foot{};
            float upper_length{};
            float lower_length{};
            bool supported{};
        };

        auto make_chain = [&](std::size_t hip_motor_index,
            std::size_t knee_motor_index) noexcept
        {
            const MotorConstraint& hip_motor = blueprint_.motors[hip_motor_index];
            const MotorConstraint& knee_motor = blueprint_.motors[knee_motor_index];
            return LegChain{
                hip_motor.pivot,
                hip_motor.c,
                knee_motor.c,
                length(blueprint_.nodes[hip_motor.c]
                    - blueprint_.nodes[hip_motor.pivot]),
                length(blueprint_.nodes[knee_motor.c]
                    - blueprint_.nodes[hip_motor.c]),
                contact_supported(knee_motor.c)
            };
        };

        std::array<LegChain, 2> legs{
            make_chain(0u, 1u), make_chain(2u, 3u)
        };
        for (const LegChain& leg : legs)
        {
            if (!valid_node(leg.hip) || !valid_node(leg.knee)
                || !valid_node(leg.foot)
                || leg.upper_length <= 1.0e-5f
                || leg.lower_length <= 1.0e-5f)
                return;
        }

        const std::size_t supported_count = static_cast<std::size_t>(legs[0].supported)
            + static_cast<std::size_t>(legs[1].supported);
        const float single_support_ratio = course_stage_ == CourseStage::hurdles
            ? 0.72f : course_stage_ == CourseStage::moving_hazards
                ? 0.76f : 0.80f;
        const float minimum_stance_ratio = supported_count >= 2u
            ? std::max(0.84f, single_support_ratio) : single_support_ratio;

        // A two-link leg can keep both bone lengths yet still fold until the
        // knee appears to telescope into the pelvis. Lift the complete upper
        // body just enough to restore reserve above each planted support.
        float required_upper_body_lift = 0.0f;
        for (const LegChain& leg : legs)
        {
            if (!leg.supported)
                continue;
            const Vec2 hip = particles_[leg.hip].position;
            const Vec2 foot = particles_[leg.foot].position;
            const float maximum_extension = leg.upper_length + leg.lower_length;
            const float target_extension = maximum_extension * minimum_stance_ratio;
            const float horizontal = hip.x - foot.x;
            const float horizontal_squared = std::min(
                horizontal * horizontal,
                target_extension * target_extension);
            const float target_vertical = std::sqrt(std::max(0.0f,
                target_extension * target_extension - horizontal_squared));
            required_upper_body_lift = std::max(required_upper_body_lift,
                target_vertical - (hip.y - foot.y));
        }

        if (required_upper_body_lift > 0.0f)
        {
            const Vec2 lift{ 0.0f, required_upper_body_lift };
            for (std::size_t index = 0; index < particles_.size(); ++index)
            {
                const bool knee = index == legs[0].knee || index == legs[1].knee;
                if (knee || blueprint_.is_support_seed(index))
                    continue;
                particles_[index].position += lift;
                particles_[index].previous += lift;
            }
        }

        auto solve_chain_ik = [&](const LegChain& leg)
        {
            Particle& hip_particle = particles_[leg.hip];
            Particle& knee_particle = particles_[leg.knee];
            Particle& foot_particle = particles_[leg.foot];
            Vec2 hip = hip_particle.position;
            Vec2 foot = foot_particle.position;
            Vec2 hip_to_foot = foot - hip;
            float distance = length(hip_to_foot);
            const float minimum_reach = std::abs(
                leg.upper_length - leg.lower_length) + 0.0001f;
            const float maximum_reach = leg.upper_length + leg.lower_length - 0.0001f;

            if (distance > maximum_reach)
            {
                const Vec2 direction = normalized(hip_to_foot, { 0.0f, -1.0f });
                const Vec2 correction = direction * (distance - maximum_reach);
                if (leg.supported)
                {
                    for (std::size_t index = 0; index < particles_.size(); ++index)
                    {
                        const bool knee = index == legs[0].knee || index == legs[1].knee;
                        if (knee || blueprint_.is_support_seed(index))
                            continue;
                        particles_[index].position += correction;
                        particles_[index].previous += correction;
                    }
                    hip = hip_particle.position;
                }
                else
                {
                    foot_particle.position -= correction;
                    foot_particle.previous -= correction;
                    foot = foot_particle.position;
                }
                hip_to_foot = foot - hip;
                distance = length(hip_to_foot);
            }
            if (distance < minimum_reach)
            {
                const Vec2 direction = normalized(hip_to_foot, { 0.0f, -1.0f });
                const Vec2 correction = direction * (minimum_reach - distance);
                if (leg.supported)
                {
                    const Vec2 lift{ 0.0f, minimum_reach - distance };
                    for (std::size_t index = 0; index < particles_.size(); ++index)
                    {
                        const bool knee = index == legs[0].knee || index == legs[1].knee;
                        if (knee || blueprint_.is_support_seed(index))
                            continue;
                        particles_[index].position += lift;
                        particles_[index].previous += lift;
                    }
                    hip = hip_particle.position;
                }
                else
                {
                    foot_particle.position += correction;
                    foot_particle.previous += correction;
                    foot = foot_particle.position;
                }
                hip_to_foot = foot - hip;
                distance = length(hip_to_foot);
            }

            const Vec2 axis = normalized(hip_to_foot, { 0.0f, -1.0f });
            const Vec2 perpendicular{ -axis.y, axis.x };
            const float safe_distance = std::clamp(distance,
                minimum_reach, maximum_reach);
            const float along = (
                leg.upper_length * leg.upper_length
                - leg.lower_length * leg.lower_length
                + safe_distance * safe_distance)
                / (2.0f * safe_distance);
            const float height = std::sqrt(std::max(0.0f,
                leg.upper_length * leg.upper_length - along * along));
            const Vec2 base = hip + axis * along;

            float side = dot(knee_particle.position - base, perpendicular);
            if (std::abs(side) <= 0.001f)
            {
                const Vec2 rest_axis = normalized(
                    blueprint_.nodes[leg.foot] - blueprint_.nodes[leg.hip], axis);
                const Vec2 rest_perpendicular{ -rest_axis.y, rest_axis.x };
                side = dot(blueprint_.nodes[leg.knee]
                    - blueprint_.nodes[leg.hip], rest_perpendicular);
            }
            const float bend_sign = side < 0.0f ? -1.0f : 1.0f;
            const Vec2 target_knee = base + perpendicular * (height * bend_sign);
            const Vec2 knee_delta = target_knee - knee_particle.position;
            knee_particle.position = target_knee;
            knee_particle.previous += knee_delta;
        };

        // Reconstruct both knees from exact two-link geometry. This preserves
        // natural swing-leg bend while preventing a planted stance chain from
        // folding into a visually compressed telescoping leg.
        solve_chain_ik(legs[0]);
        solve_chain_ik(legs[1]);
    }

'''
    text = replace_between(text,
        "    void Environment::project_structure_rigid(float dt) noexcept",
        "    void Environment::separate_support_clusters() noexcept",
        projection,
        "stance-leg projection")

    text = replace_once(text,
        '''        if (elapsed_seconds_ < 0.75f
            || !stage_requires_forward_gait(course_stage_)
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return 0.0f;
''',
        '''        if (!stage_requires_forward_gait(course_stage_)
            || !blueprint_.paired_leg_chains()
            || blueprint_.horizontal_multi_support_plan())
            return 0.0f;
''',
        "early leg error measurement")
    text = replace_once(text,
        '''        if (elapsed_seconds_ >= 0.75f
            && (!std::isfinite(structural_error) || structural_error > 0.040f))
            invalidate(InvalidMotion::structural_compression);
''',
        '''        if (elapsed_seconds_ >= 0.10f
            && (!std::isfinite(structural_error) || structural_error > 0.020f))
            invalidate(InvalidMotion::structural_compression);
''',
        "early structural integrity gate")
    write(path, text)


def patch_version_and_build() -> None:
    path = "CMakeLists.txt"
    text = read(path)
    text = text.replace("project(Runner VERSION 0.7.24 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.25 LANGUAGES CXX)")
    text = text.replace("Runner v0.7.24 screenshot icon generation failed",
        "Runner v0.7.25 screenshot icon generation failed")
    text = replace_once(text,
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        '''        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${runner_sandhybrid_SOURCE_DIR}/missioncache.md"''',
        "post-build v0725 document")
    text = replace_once(text,
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        DESTINATION docs)''',
        '''        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"
        DESTINATION docs)''',
        "install v0725 document")
    test_target = r'''    add_executable(RunnerV0725ArtLegHotfixTests
        tests/v0725_art_leg_hotfix_tests.cpp)
    target_link_libraries(RunnerV0725ArtLegHotfixTests PRIVATE Runner::Core)
    target_include_directories(RunnerV0725ArtLegHotfixTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(RunnerV0725ArtLegHotfixTests PRIVATE cxx_std_23)
    target_compile_definitions(RunnerV0725ArtLegHotfixTests PRIVATE
        RUNNER_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    runner_enable_warnings(RunnerV0725ArtLegHotfixTests)
    add_test(NAME Runner.V0725ArtLegHotfix
        COMMAND RunnerV0725ArtLegHotfixTests)
    set_tests_properties(Runner.V0725ArtLegHotfix PROPERTIES TIMEOUT 180)

'''
    text = replace_once(text,
        "    add_executable(RunnerLiveAcceptanceTests tests/live_acceptance_tests.cpp)\n",
        test_target
        + "    add_executable(RunnerLiveAcceptanceTests tests/live_acceptance_tests.cpp)\n",
        "v0725 test target")
    write(path, text)


def patch_training_semantics() -> None:
    path = "src/ppo.hpp"
    text = read(path)
    text = replace_once(text,
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'2401u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'2501u;",
        "v0725 training semantics")
    write(path, text)


def patch_docs() -> None:
    path = "README.md"
    text = read(path)
    text = text.replace("Runner 0.7.24 is", "Runner 0.7.25 is", 1)
    text = text.replace(
        "`A`: toggle optional torso/helmet/weapon overlays; foot sprites remain independent",
        "`A`: toggle compact body armor and the approved helmet/foot presentation; weapon preview remains Rig-Lab-only")
    text = replace_once(text,
        "- [`docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md`](docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md) documents rigid bones, stage-qualified totals, mastery-aware completion, and the exact screenshot icon source.\n",
        "- [`docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md`](docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md) documents rigid bones, stage-qualified totals, mastery-aware completion, and the exact screenshot icon source.\n"
        "- [`docs/RUNNER_V0725_ART_LEG_HOTFIX.md`](docs/RUNNER_V0725_ART_LEG_HOTFIX.md) documents compact node-attached armor and supported stance-leg extension.\n",
        "README v0725 document link")
    section = '''## v0.7.25 compact armor and stance-leg hotfix\n\n- Keeps the approved helmet and foot artwork while removing the oversized translucent torso bitmap.\n- Builds a compact chest plate, shoulder caps, forearm guards, and cyan indicator from the real body joints.\n- Prevents supported walking legs from folding until the knee appears to telescope into the pelvis.\n- Reconstructs each paired leg with exact two-link geometry after restoring supported stance extension.\n- Applies walking-chain integrity from startup instead of waiting through the visible first interval.\n- Preserves swing-leg bend, static crouch, crouch-walk, monoped, quadruped, crawler, and hexapod motion paths.\n- Isolates corrected v0.7.25 controller state and autosaves.\n\n'''
    text = replace_once(text,
        "## v0.7.24 structural integrity and truthful telemetry\n",
        section + "## v0.7.24 structural integrity and truthful telemetry\n",
        "README v0725 section")
    write(path, text)

    path = "CHANGELOG.md"
    text = read(path)
    if not text.startswith("## 0.7.25\n"):
        text = '''## 0.7.25\n\n- Retained the approved helmet and foot artwork while replacing the oversized torso bitmap with compact node-attached armor.\n- Added bounded shoulder caps, forearm guards, a narrow chest plate, and a compact cyan status indicator.\n- Added supported stance-leg extension so fixed-length legs cannot fold into a telescoping-looking vertical stack.\n- Reconstructed paired knees analytically from authored upper/lower leg lengths after each upright walking solve.\n- Applied walking-chain integrity during startup and tightened early structural error rejection.\n- Added forced-compression, natural walking, segment-length, and armor-source regression tests.\n- Bumped training semantics and isolated v0.7.25 autosaves.\n\n''' + text
    write(path, text)


def patch_mission_cache() -> None:
    path = "missioncache.md"
    text = read(path)
    if "# Runner v0.7.25 compact armor and stance-leg integrity\n" not in text:
        section = '''# Runner v0.7.25 compact armor and stance-leg integrity\n\n**Release state:** CACHED BEFORE IMPLEMENTATION — RELEASE BLOCKING.\n\nDirect packaged v0.7.24 eye testing confirms that the approved helmet and foot assets are usable, but the translucent torso sheet, circular shoulder masses, and duplicate ghost arms obscure the actual gait. Fixed segment lengths also remain insufficient: a two-link leg can preserve both bone lengths while folding until the knee appears to telescope into the pelvis.\n\n### WALK-COMPACT-ARMOR-307 — Replace the oversized torso overlay\n**Status:** OPEN — RELEASE BLOCKING\n\nKeep the approved helmet and foot presentation. Replace only the torso, shoulder, and forearm overlay with compact geometry attached to the real body nodes. No rectangular sprite sheet, giant shoulder circles, duplicate arms, or physics changes are allowed.\n\n### WALK-STANCE-EXTENSION-308 — Preserve supported leg extension\n**Status:** OPEN — RELEASE BLOCKING\n\nA supported walking leg must retain enough hip-to-foot extension to remain a usable stance chain. Fixed upper/lower lengths may not be satisfied by folding the knee into the pelvis.\n\n### WALK-CHAIN-IK-309 — Reconstruct paired legs from authored lengths\n**Status:** OPEN — RELEASE BLOCKING\n\nAfter stance reserve is restored, reconstruct each knee from exact two-link geometry, preserve its bend side, pin supported feet, and retain natural swing-leg flexion.\n\n### WALK-STARTUP-310 — Remove the visible startup compression window\n**Status:** OPEN — RELEASE BLOCKING\n\nWalking-chain projection and error measurement begin during startup rather than waiting 0.75 seconds while the visible preview collapses.\n\n### WALK-STATE-311 — Isolate v0.7.25 locomotion semantics\n**Status:** OPEN — RELEASE BLOCKING\n\nBump training semantics and use v0.7.25 autosave paths so older controllers cannot silently resume against the corrected stance-chain behavior.\n\n### WALK-REGRESSION-312 — Lock art and stance-chain behavior\n**Status:** OPEN — RELEASE BLOCKING\n\nAdd forced-compression recovery, exact segment-length, natural walking soak, compact-art source, approved helmet/foot retention, complete Linux, complete Windows SDL3/Vulkan, installed/extracted package, and runtime diagnostic tests.\n\n### WALK-RELEASE-313 — Publish and clean Runner v0.7.25\n**Status:** OPEN — RELEASE BLOCKING\n\nMerge only validated source, publish `v0.7.25`, re-download and byte-verify every asset, record evidence, close temporary PRs, and delete temporary branches/workflows.\n\n'''
        text = replace_once(text, "# Carried open work\n",
            section + "# Carried open work\n", "v0725 mission section")
    text = text.replace("# Runner v0.7.25 equipment, carry, and target curriculum",
        "# Runner v0.7.26 equipment, carry, and target curriculum")
    write(path, text)


def patch_repository_audit() -> None:
    path = "tools/repository_audit.cmake"
    text = read(path)
    text = replace_once(text,
        "        docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md\n",
        "        docs/RUNNER_V0724_STRUCTURAL_METRICS_ICON.md\n"
        "        docs/RUNNER_V0725_ART_LEG_HOTFIX.md\n"
        "        tests/v0725_art_leg_hotfix_tests.cpp\n",
        "repository required v0725 files")
    text = text.replace("project(Runner VERSION 0.7.24 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.25 LANGUAGES CXX)")
    text = replace_once(text,
        "        \"RunnerV0724StructuralMetricsIconTests\"\n",
        "        \"RunnerV0724StructuralMetricsIconTests\"\n"
        "        \"RunnerV0725ArtLegHotfixTests\"\n"
        "        \"RUNNER_V0725_ART_LEG_HOTFIX.md\"\n",
        "repository CMake v0725 contracts")
    text = replace_once(text,
        "        \"WALK-RELEASE-306\")\n",
        "        \"WALK-RELEASE-306\"\n"
        "        \"WALK-COMPACT-ARMOR-307\"\n"
        "        \"WALK-STANCE-EXTENSION-308\"\n"
        "        \"WALK-CHAIN-IK-309\"\n"
        "        \"WALK-STARTUP-310\"\n"
        "        \"WALK-STATE-311\"\n"
        "        \"WALK-REGRESSION-312\"\n"
        "        \"WALK-RELEASE-313\")\n",
        "repository mission v0725 contracts")
    text = text.replace("training_semantics_version = 0x0007'2401u",
        "training_semantics_version = 0x0007'2501u")
    text = replace_once(text,
        "        \"runner-v0724-rig-autosave.eppo\")\n",
        "        \"runner-v0725-rig-autosave.eppo\"\n"
        "        \"COMPACT SEGMENTED BODY ARMOR\"\n"
        "        \"shoulder_cap_radius\")\n",
        "repository app v0725 contracts")
    text = replace_once(text,
        "file(GLOB release_notes",
        '''string(FIND "${app_text}" "draw_pixel_art(canvas, optional_torso_art" torso_bitmap_pos)
if(NOT torso_bitmap_pos EQUAL -1)
    message(FATAL_ERROR "Oversized torso bitmap rendering remains")
endif()
string(FIND "${simulation_text}" "minimum_stance_ratio" stance_ratio_pos)
string(FIND "${simulation_text}" "solve_chain_ik" chain_ik_pos)
if(stance_ratio_pos EQUAL -1 OR chain_ik_pos EQUAL -1)
    message(FATAL_ERROR "v0.7.25 stance-chain correction is missing")
endif()

file(GLOB release_notes''',
        "repository v0725 implementation audit")
    text = replace_once(text,
        "        tools/v0724-rescue-trigger.txt)\n",
        "        tools/v0724-rescue-trigger.txt\n"
        "        tools/apply_v0725_art_leg_hotfix.py)\n",
        "repository stale v0725 migration")
    text = text.replace("Runner v0.7.24 repository hygiene passed",
        "Runner v0.7.25 repository hygiene passed")
    write(path, text)


def main() -> int:
    patch_application()
    patch_simulation()
    patch_version_and_build()
    patch_training_semantics()
    patch_docs()
    patch_mission_cache()
    patch_repository_audit()
    print("Runner v0.7.25 art and stance-leg hotfix applied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
