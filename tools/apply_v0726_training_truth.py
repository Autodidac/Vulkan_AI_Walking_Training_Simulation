#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def write(path: str, text: str) -> None:
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(text, encoding="utf-8")

def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one match, got {count}")
    return text.replace(old, new, 1)

def regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one regex match, got {count}")
    return updated

cmake = read("CMakeLists.txt")
cmake = replace_once(cmake, "project(Runner VERSION 0.7.25 LANGUAGES CXX)", "project(Runner VERSION 0.7.26 LANGUAGES CXX)", "project version")
cmake = replace_once(cmake,
    '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"\n        DESTINATION docs)',
    '        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"\n        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0726_TRAINING_TRUTH.md"\n        DESTINATION docs)', "install v0726 doc")
cmake = replace_once(cmake,
    '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"\n',
    '        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0725_ART_LEG_HOTFIX.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0725_ART_LEG_HOTFIX.md"\n        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/docs/RUNNER_V0726_TRAINING_TRUTH.md" "$<TARGET_FILE_DIR:Runner>/docs/RUNNER_V0726_TRAINING_TRUTH.md"\n', "post-build v0726 doc")
needle = '''    add_executable(RunnerLiveAcceptanceTests tests/live_acceptance_tests.cpp)\n'''
test_target = '''    add_executable(RunnerV0726TrainingTruthTests\n        tests/v0726_training_truth_tests.cpp)\n    target_link_libraries(RunnerV0726TrainingTruthTests PRIVATE Runner::Core)\n    target_include_directories(RunnerV0726TrainingTruthTests PRIVATE\n        "${CMAKE_CURRENT_SOURCE_DIR}/src")\n    target_compile_features(RunnerV0726TrainingTruthTests PRIVATE cxx_std_23)\n    runner_enable_warnings(RunnerV0726TrainingTruthTests)\n    add_test(NAME Runner.V0726TrainingTruth\n        COMMAND RunnerV0726TrainingTruthTests)\n    set_tests_properties(Runner.V0726TrainingTruth PROPERTIES TIMEOUT 180)\n\n'''
cmake = replace_once(cmake, needle, test_target + needle, "v0726 test target")
write("CMakeLists.txt", cmake)

sim_h = read("src/simulation.hpp")
sim_h = replace_once(sim_h, '        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }\n',
    '        [[nodiscard]] float course_difficulty() const noexcept { return course_difficulty_; }\n        void set_course_motion_enabled(bool enabled) noexcept\n        {\n            course_motion_enabled_ = enabled;\n        }\n        [[nodiscard]] bool course_motion_enabled() const noexcept\n        {\n            return course_motion_enabled_;\n        }\n', "course motion public contract")
sim_h = replace_once(sim_h, '        [[nodiscard]] float course_speed() const noexcept\n        {\n',
    '        [[nodiscard]] float course_speed() const noexcept\n        {\n            if (!course_motion_enabled_)\n                return 0.0f;\n', "course motion speed gate")
sim_h = replace_once(sim_h, '        CourseStage course_stage_{ CourseStage::balance };\n        float course_difficulty_{ 0.25f };\n',
    '        CourseStage course_stage_{ CourseStage::balance };\n        float course_difficulty_{ 0.25f };\n        bool course_motion_enabled_{ true };\n', "course motion storage")
write("src/simulation.hpp", sim_h)

ppo_h = read("src/ppo.hpp")
ppo_h = replace_once(ppo_h, "inline constexpr std::uint32_t training_semantics_version = 0x0007'2501u;", "inline constexpr std::uint32_t training_semantics_version = 0x0007'2601u;", "training semantics version")
ppo_h = replace_once(ppo_h, "namespace runner::rl\n{\n    inline constexpr std::uint32_t training_semantics_version = 0x0007'2601u;\n",
    "namespace runner::rl\n{\n    inline constexpr std::uint32_t training_semantics_version = 0x0007'2601u;\n\n    [[nodiscard]] inline bool motor_drives_support_branch(\n        const sim::CreatureBlueprint& rig,\n        const sim::MotorConstraint& motor) noexcept;\n", "support role forward declaration")
ppo_h = replace_once(ppo_h, '''        for (std::size_t index = 4; index < rig.active_motor_count; ++index)\n            action[index] *= upper_body_authority;\n''',
    '''        for (std::size_t index = 0; index < rig.active_motor_count; ++index)\n        {\n            if (!motor_drives_support_branch(rig, rig.motors[index]))\n                action[index] *= upper_body_authority;\n        }\n''', "balance topology roles")
old_zero = '''        for (std::size_t index = 4; index < rig.active_motor_count; ++index)\n            action[index] = 0.0f;\n'''
if ppo_h.count(old_zero) != 2:
    raise RuntimeError(f"expected two non-support zero loops, got {ppo_h.count(old_zero)}")
new_zero = '''        for (std::size_t index = 0; index < rig.active_motor_count; ++index)\n        {\n            if (!motor_drives_support_branch(rig, rig.motors[index]))\n                action[index] = 0.0f;\n        }\n'''
ppo_h = ppo_h.replace(old_zero, new_zero)
effective = r'''    [[nodiscard]] inline std::array<float, sim::action_count> effective_policy_action(
        const sim::Environment& environment,
        std::array<float, sim::action_count> policy_action,
        sim::CourseStage stage) noexcept
    {
        const sim::CreatureBlueprint& rig = environment.blueprint();
        const std::size_t active = rig.active_motor_count;
        auto support_motor = [&rig](std::size_t index) noexcept
        {
            return index < rig.active_motor_count
                && motor_drives_support_branch(rig, rig.motors[index]);
        };
        auto blend_teacher = [&](const std::array<float, sim::action_count>& teacher,
            float support_assist, float body_assist) noexcept
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                const float assist = support_motor(index) ? support_assist : body_assist;
                policy_action[index] = lerp(policy_action[index], teacher[index], assist);
            }
        };
        auto neutralize_non_support = [&](float amount) noexcept
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] = lerp(policy_action[index], 0.0f, amount);
            }
        };
        if (stage == sim::CourseStage::balance)
        {
            const auto teacher = balance_teacher_action(environment);
            const bool established = environment.stable_stance_seconds() >= 0.75f;
            blend_teacher(teacher, established ? 0.46f : 0.72f, established ? 0.52f : 0.78f);
        }
        else if (stage == sim::CourseStage::duck_press)
        {
            const auto teacher = duck_teacher_action(environment);
            blend_teacher(teacher, 0.76f + environment.duck_obstacle_weight() * 0.16f, 0.0f);
            neutralize_non_support(0.995f);
        }
        else if (stage == sim::CourseStage::uneven)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float support_assist = movement.intent == locomotion::Intent::recover ? 0.68f : movement.step_up ? 0.56f : 0.34f;
            const float body_assist = movement.intent == locomotion::Intent::recover ? 0.60f : 0.42f;
            blend_teacher(teacher, support_assist, body_assist);
        }
        else if (stage == sim::CourseStage::crouch_walk)
        {
            const auto teacher = crouch_walk_teacher_action(environment);
            blend_teacher(teacher, 0.58f + environment.duck_obstacle_weight() * 0.24f, 0.0f);
            neutralize_non_support(0.98f);
        }
        else if (stage == sim::CourseStage::ramps)
        {
            const auto teacher = balance_teacher_action(environment);
            blend_teacher(teacher, 0.26f, 0.88f);
        }
        else if (stage == sim::CourseStage::hurdles || stage == sim::CourseStage::moving_hazards)
        {
            const auto teacher = walking_teacher_action(environment);
            const locomotion::Plan movement = current_locomotion_plan(environment);
            const float support_assist = movement.intent == locomotion::Intent::crawl ? 0.78f : movement.intent == locomotion::Intent::flee ? 0.52f : movement.intent == locomotion::Intent::recover ? 0.62f : movement.step_up ? 0.48f : 0.24f;
            const float body_assist = movement.intent == locomotion::Intent::crawl ? 0.60f : movement.intent == locomotion::Intent::flee ? 0.34f : 0.20f;
            blend_teacher(teacher, support_assist, body_assist);
        }
        if (environment.longest_stable_stance_seconds() < 1.0f && !sim::stage_allows_controlled_flips(stage))
        {
            for (std::size_t index = 0; index < active; ++index)
            {
                if (!support_motor(index))
                    policy_action[index] *= 0.08f;
            }
        }
        return bilateral_joint_synergy_action(environment, policy_action, stage);
    }

'''
ppo_h = regex_once(ppo_h, r'    \[\[nodiscard\]\] inline std::array<float, sim::action_count> effective_policy_action\(.*?\n    }\n\n    struct TrainingMetrics', effective + '    struct TrainingMetrics', "effective policy topology rewrite")
ppo_h = replace_once(ppo_h, '        void reset_policy(std::uint64_t seed = 0xC0FFEEu);\n', '        void reset_policy(std::uint64_t seed = 0xC0FFEEu,\n            bool clear_totals = false);\n', "reset policy signature")
ppo_h = replace_once(ppo_h, '        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;\n',
    '        void reset_preview(std::uint64_t seed = 0xDEADBEEFu) noexcept;\n        void set_preview_course_motion_enabled(bool enabled) noexcept\n        {\n            preview_.set_course_motion_enabled(enabled);\n        }\n        [[nodiscard]] std::uint64_t preview_reset_count() const noexcept\n        {\n            return preview_reset_sequence_;\n        }\n        [[nodiscard]] sim::InvalidMotion preview_last_reset_reason() const noexcept\n        {\n            return preview_last_reset_reason_;\n        }\n', "preview diagnostics public API")
ppo_h = replace_once(ppo_h, '        void reset_training_state(bool clear_best = true) noexcept;\n', '        void reset_training_state(bool clear_best = true,\n            bool clear_totals = false) noexcept;\n', "reset training state signature")
ppo_h = replace_once(ppo_h, '        std::uint64_t preview_reset_sequence_{};\n', '        std::uint64_t preview_reset_sequence_{};\n        sim::InvalidMotion preview_last_reset_reason_{ sim::InvalidMotion::none };\n', "preview reset reason storage")
write("src/ppo.hpp", ppo_h)

ppo_cpp = read("src/ppo_trainer.cpp")
ppo_cpp = replace_once(ppo_cpp, '''        else\n        {\n            reset_policy();\n        }\n''', '''        else\n        {\n            // A canonical rig switch is a new training subject. Preserve totals\n            // across episode/policy retries, but never carry another rig's totals.\n            reset_policy(0xC0FFEEu, true);\n        }\n''', "fresh rig totals")
ppo_cpp = replace_once(ppo_cpp, '    void PpoTrainer::reset_training_state(bool clear_best) noexcept\n', '    void PpoTrainer::reset_training_state(bool clear_best,\n        bool clear_totals) noexcept\n', "reset training implementation signature")
old_totals = '''        metrics_ = {};\n        metrics_.total_updates = previous_metrics.total_updates;\n        metrics_.total_environment_steps = previous_metrics.total_environment_steps;\n        metrics_.total_episodes = previous_metrics.total_episodes;\n        metrics_.total_valid_episodes = previous_metrics.total_valid_episodes;\n        metrics_.total_invalid_episodes = previous_metrics.total_invalid_episodes;\n        metrics_.total_resets = previous_metrics.total_resets + 1u;\n        metrics_.total_alternating_steps = previous_metrics.total_alternating_steps;\n        metrics_.total_falls = previous_metrics.total_falls;\n        metrics_.total_collisions = previous_metrics.total_collisions;\n        metrics_.total_powered_jumps = previous_metrics.total_powered_jumps;\n        metrics_.total_landed_jumps = previous_metrics.total_landed_jumps;\n        metrics_.total_landed_flips = previous_metrics.total_landed_flips;\n        metrics_.total_obstacles_passed = previous_metrics.total_obstacles_passed;\n        metrics_.total_distance = previous_metrics.total_distance;\n        metrics_.total_training_seconds = previous_metrics.total_training_seconds;\n        metrics_.evaluation_count = previous_metrics.evaluation_count;\n'''
new_totals = '''        metrics_ = {};\n        if (!clear_totals)\n        {\n            metrics_.total_updates = previous_metrics.total_updates;\n            metrics_.total_environment_steps = previous_metrics.total_environment_steps;\n            metrics_.total_episodes = previous_metrics.total_episodes;\n            metrics_.total_valid_episodes = previous_metrics.total_valid_episodes;\n            metrics_.total_invalid_episodes = previous_metrics.total_invalid_episodes;\n            metrics_.total_resets = previous_metrics.total_resets + 1u;\n            metrics_.total_alternating_steps = previous_metrics.total_alternating_steps;\n            metrics_.total_falls = previous_metrics.total_falls;\n            metrics_.total_collisions = previous_metrics.total_collisions;\n            metrics_.total_powered_jumps = previous_metrics.total_powered_jumps;\n            metrics_.total_landed_jumps = previous_metrics.total_landed_jumps;\n            metrics_.total_landed_flips = previous_metrics.total_landed_flips;\n            metrics_.total_obstacles_passed = previous_metrics.total_obstacles_passed;\n            metrics_.total_distance = previous_metrics.total_distance;\n            metrics_.total_training_seconds = previous_metrics.total_training_seconds;\n            metrics_.evaluation_count = previous_metrics.evaluation_count;\n        }\n'''
ppo_cpp = replace_once(ppo_cpp, old_totals, new_totals, "rig-scoped totals")
ppo_cpp = replace_once(ppo_cpp, '    void PpoTrainer::reset_policy(std::uint64_t seed)\n    {\n        policy_ = PolicyNetwork(seed);\n        preview_policy_.parameters() = policy_.parameters();\n        reset_training_state();\n', '    void PpoTrainer::reset_policy(std::uint64_t seed, bool clear_totals)\n    {\n        policy_ = PolicyNetwork(seed);\n        preview_policy_.parameters() = policy_.parameters();\n        reset_training_state(true, clear_totals);\n', "reset policy implementation")
ppo_cpp = replace_once(ppo_cpp, '''        const auto action = effective_policy_action(preview_, raw_action, course_stage_);\n        if (preview_.step(action, dt).terminated)\n        {\n            ++preview_reset_sequence_;\n            preview_.reset(0xDEADBEEFu + metrics_.update\n                + preview_reset_sequence_ * 7919u);\n        }\n''', '''        const auto action = effective_policy_action(preview_, raw_action, course_stage_);\n        const sim::StepResult result = preview_.step(action, dt);\n        if (result.terminated)\n        {\n            preview_last_reset_reason_ = result.invalid_reason;\n            ++preview_reset_sequence_;\n            preview_.reset(0xDEADBEEFu + metrics_.update\n                + preview_reset_sequence_ * 7919u);\n        }\n''', "preview termination reason")
ppo_cpp = replace_once(ppo_cpp, '''    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept\n    {\n        preview_reset_sequence_ = 0u;\n        preview_.reset(seed);\n    }\n''', '''    void PpoTrainer::reset_preview(std::uint64_t seed) noexcept\n    {\n        preview_reset_sequence_ = 0u;\n        preview_last_reset_reason_ = sim::InvalidMotion::none;\n        preview_.reset(seed);\n    }\n''', "manual preview reset state")
write("src/ppo_trainer.cpp", ppo_cpp)

autonomy_h = read("src/autonomy.hpp")
autonomy_h = replace_once(autonomy_h, '        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }\n', '        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }\n        [[nodiscard]] std::uint64_t preview_reset_count() const noexcept\n        {\n            return live_.preview_reset_count();\n        }\n        [[nodiscard]] sim::InvalidMotion preview_last_reset_reason() const noexcept\n        {\n            return live_.preview_last_reset_reason();\n        }\n', "autonomy preview reset diagnostics")
write("src/autonomy.hpp", autonomy_h)

runtime = read("src/autonomy_runtime.cpp")
runtime = replace_once(runtime, '''        worker_.set_course(stage_, difficulty_, false);\n        live_.set_course(stage_, difficulty_, false);\n        publish_locked();\n''', '''        worker_.set_course(stage_, difficulty_, false);\n        live_.set_course(stage_, difficulty_, false);\n        // The large preview is a real locomotion test, not a conveyor-belt\n        // visualization. Training workers may still use moving-course pressure.\n        live_.set_preview_course_motion_enabled(false);\n        publish_locked();\n''', "static live preview")
write("src/autonomy_runtime.cpp", runtime)

commands = read("src/autonomy_commands.cpp").replace("V0.7.20", "V0.7.26")
old_set = '''            worker_.set_blueprint(command.blueprint, command.preserve_policy);\n            worker_.set_course(stage_, difficulty_, false);\n            mastery_streak_ = 0;\n            degradation_streak_ = 0;\n            last_evaluation_count_ = 0;\n            last_saved_best_update_ = 0;\n'''
new_set = '''            worker_.set_blueprint(command.blueprint, command.preserve_policy);\n            worker_.set_course(stage_, difficulty_, false);\n            mastery_streak_ = 0;\n            degradation_streak_ = 0;\n            const TrainingMetrics& reset_metrics = worker_.metrics();\n            last_evaluation_count_ = reset_metrics.evaluation_count;\n            last_saved_best_update_ = 0;\n            stage_entry_total_updates_ = reset_metrics.total_updates;\n            stage_entry_total_episodes_ = reset_metrics.total_episodes;\n            stage_entry_evaluation_count_ = reset_metrics.evaluation_count;\n            stage_entry_baseline_initialized_ = true;\n'''
commands = replace_once(commands, old_set, new_set, "set blueprint fresh baseline")
old_reset = '''            worker_.reset_policy(command.seed);\n            worker_.set_course(stage_, difficulty_, false);\n            mastery_streak_ = 0;\n            degradation_streak_ = 0;\n            last_evaluation_count_ = 0;\n            last_saved_best_update_ = 0;\n'''
new_reset = '''            worker_.reset_policy(command.seed);\n            worker_.set_course(stage_, difficulty_, false);\n            mastery_streak_ = 0;\n            degradation_streak_ = 0;\n            const TrainingMetrics& reset_metrics = worker_.metrics();\n            last_evaluation_count_ = reset_metrics.evaluation_count;\n            last_saved_best_update_ = 0;\n            stage_entry_total_updates_ = reset_metrics.total_updates;\n            stage_entry_total_episodes_ = reset_metrics.total_episodes;\n            stage_entry_evaluation_count_ = reset_metrics.evaluation_count;\n            stage_entry_baseline_initialized_ = true;\n'''
commands = replace_once(commands, old_reset, new_reset, "policy retry baseline")
write("src/autonomy_commands.cpp", commands)

app = read("src/app.cpp")
app = app.replace("runner-v0725-rig-", "runner-v0726-rig-")
app = app.replace('"TOTAL UPDATES {}  LESSON UPDATE {}  DISTANCE {:.1f} M  STEPS {}"', '"TOTAL RIG UPDATES {}  POLICY UPDATE {}  DISTANCE {:.1f} M  STEPS {}"')
app = app.replace('"TOTAL UPDATES {}   COMPLETION {}%"', '"TOTAL RIG UPDATES {}   LESSON COMPLETION {}%"')
app = app.replace('"TOTAL UPDATES {}   SIMULATED RUNS {}   PASSED STAGE CHECKS {}"', '"TOTAL RIG UPDATES {}   SIMULATED RUNS {}   PASSED STAGE CHECKS {}"')
app = app.replace('"MOTION {}   TOTAL UPDATES {}   LESSON UPDATE {}"', '"MOTION {}   TOTAL RIG UPDATES {}   POLICY UPDATE {}"')
torso_anchor = '''            if (optional_art_enabled && optional_helmet_art.loaded()\n                && rig.head_node < particles.size())\n'''
torso_block = '''            if (optional_art_enabled && optional_torso_art.loaded()\n                && rig.active_motor_count >= 8u && rig.paired_leg_chains()\n                && rig.root_node < particles.size()\n                && rig.torso_node < particles.size())\n            {\n                // User-supplied modular armor, bounded to the physical torso.\n                // This is a single chest component, never the old full-sheet overlay.\n                const Vec2 root = point(rig.root_node);\n                const Vec2 torso = point(rig.torso_node);\n                const Vec2 center = (root + torso) * 0.5f;\n                const float body_span = length(torso - root);\n                const float height = std::clamp(body_span * 0.72f, 42.0f, 76.0f);\n                const float width = height\n                    * static_cast<float>(optional_torso_art.width)\n                    / static_cast<float>(optional_torso_art.height);\n                draw_pixel_art(canvas, optional_torso_art,\n                    { center - Vec2{ width * 0.50f, height * 0.54f },\n                      { width, height } }, 0.90f);\n            }\n\n''' + torso_anchor
app = replace_once(app, torso_anchor, torso_block, "bounded torso artwork")
load_anchor = '''        load_optional("weapon_side.ppm", impl_->optional_weapon_art);\n\n        impl_->trainer.set_autosave_paths'''
load_new = '''        load_optional("weapon_side.ppm", impl_->optional_weapon_art);\n        impl_->optional_art_enabled = impl_->optional_foot_art.loaded()\n            || impl_->optional_helmet_art.loaded()\n            || impl_->optional_torso_art.loaded()\n            || impl_->optional_weapon_art.loaded();\n\n        impl_->trainer.set_autosave_paths'''
app = replace_once(app, load_anchor, load_new, "enable supplied runtime art")
preview_line_anchor = '''            line.y += 23.0f;\n            add_text_fit(canvas, line,\n                std::format("MOTION {}   TOTAL RIG UPDATES {}   POLICY UPDATE {}",\n                    sim::invalid_motion_name(environment.invalid_reason()),\n                    trainer.metrics().total_updates, trainer.metrics().update),\n                0.80f, environment.valid_motion() ? accent : danger, text_width);\n\n            draw_training_pip'''
preview_line_new = '''            line.y += 23.0f;\n            add_text_fit(canvas, line,\n                std::format("MOTION {}   TOTAL RIG UPDATES {}   POLICY UPDATE {}",\n                    sim::invalid_motion_name(environment.invalid_reason()),\n                    trainer.metrics().total_updates, trainer.metrics().update),\n                0.80f, environment.valid_motion() ? accent : danger, text_width);\n            line.y += 21.0f;\n            const std::uint64_t preview_restarts = trainer.preview_reset_count();\n            const sim::InvalidMotion preview_reason = trainer.preview_last_reset_reason();\n            add_text_fit(canvas, line,\n                preview_restarts == 0u\n                    ? "PREVIEW RESTARTS 0 - REAL STATIC COURSE"\n                    : std::format("PREVIEW RESTARTS {}   LAST {}",\n                        preview_restarts, sim::invalid_motion_name(preview_reason)),\n                0.72f, preview_restarts == 0u ? muted : yellow, text_width);\n\n            draw_training_pip'''
app = replace_once(app, preview_line_anchor, preview_line_new, "preview restart telemetry")
write("src/app.cpp", app)

explainer = read("src/training_explainer.hpp")
explainer = replace_once(explainer, '''        return "TOTAL UPDATES = every completed controller-learning cycle; this never resets when an attempt fails.";\n''', '''        return "TOTAL RIG UPDATES = completed learning cycles for the selected rig. It survives episode and policy retries, and resets only when a different rig is selected.";\n''', "total updates explanation")
write("src/training_explainer.hpp", explainer)

readme = read("README.md")
readme = readme.replace("Runner 0.7.25", "Runner 0.7.26", 1)
release_section = '''## v0.7.26 rig-scoped training truth and topology-aware locomotion\n\n- Classifies support motors from rig topology instead of assuming motors 0-3 are legs and 4+ are arms. Quadruped, crawler, and hexapod support motors now retain authority through Stand, Crouch, Walk/Run, and later stages.\n- A canonical rig switch starts a genuinely fresh training subject: rig totals, lesson baselines, tests, optimizer state, and policy state reset together. Episode and policy retries within the same rig still preserve that rig's cumulative totals.\n- The large live preview disables course conveyor motion. It must move itself across a static course while the training workers remain free to use moving-course pressure.\n- Preview auto-restarts now retain and display the terminating motion reason and restart count instead of silently snapping back to spawn.\n- Runtime modular armor art is enabled automatically when packaged assets are available. The supplied torso component is bounded to the physical torso instead of drawing an oversized sheet.\n- Autosave names and messages are isolated to v0.7.26.\n\n'''
if release_section not in readme:
    readme = readme.replace("# Runner\n\n", "# Runner\n\n" + release_section, 1)
write("README.md", readme)

changelog = read("CHANGELOG.md")
entry = '''## 0.7.26\n\n- Fixed non-biped training by replacing hard-coded motor-index roles with topology-derived support-motor classification.\n- Reset cumulative training totals and lesson baselines when switching to a different canonical rig while preserving them across same-rig retries.\n- Disabled conveyor motion in the large live preview and surfaced automatic preview restart count/reason.\n- Enabled packaged modular armor art by default when present and bounded the supplied torso component to the physical rig.\n- Updated v0.7.26 autosave isolation and telemetry wording.\n\n'''
if not changelog.startswith("## 0.7.26"):
    changelog = entry + changelog
write("CHANGELOG.md", changelog)

mission = read("missioncache.md")
mission_block = '''# Runner v0.7.26 rig-scoped training truth and multi-rig locomotion\n\n**Release state:** IMPLEMENTED FOR VALIDATION\n\n- **WALK-RIG-ROLE-314:** Remove the humanoid-only assumption that motors 4+ are upper-body. Classify every motor by whether its driven branch reaches a semantic support node.\n- **WALK-RIG-ROLE-315:** Preserve quadruped/crawler/hexapod support authority during Stand, Duck Press, Walk/Run, Crouch Walk, ramps, hurdles, and hazard recovery.\n- **WALK-RIG-RESET-316:** Selecting a different canonical rig starts a fresh training subject and clears that rig's cumulative counters, optimizer/policy state, best state, and lesson baselines.\n- **WALK-RIG-RETRY-317:** Same-rig episode/policy retries preserve cumulative rig totals so failures do not make training history disappear.\n- **WALK-PREVIEW-318:** Large Live preview runs against a static course rather than receiving conveyor progress.\n- **WALK-PREVIEW-319:** Surface preview automatic-restart count and terminating invalid-motion reason.\n- **WALK-TELEMETRY-320:** Label the monotonic counter as TOTAL RIG UPDATES and distinguish it from POLICY UPDATE.\n- **WALK-ART-321:** Enable packaged modular art automatically and attach the supplied torso component to bounded physical rig geometry.\n- **WALK-STATE-322:** Isolate v0.7.26 autosave/checkpoint filenames and update stale v0.7.20 status messages.\n- **WALK-REGRESSION-323:** Add deterministic tests for support-role classification, non-biped motor authority, static preview course, and new-rig counter reset.\n- **WALK-RELEASE-324:** Require Linux GCC14 warnings-as-errors, Windows SDL3/Vulkan build/tests, installed/extracted diagnostics, checksum/manifest, release re-download verification, and clean main-only repository state.\n'''
if "WALK-RIG-ROLE-314" not in mission:
    mission = mission.rstrip() + "\n\n" + mission_block
write("missioncache.md", mission)

doc = '''# Runner v0.7.26 training truth\n\n## Root causes fixed\n\nThe locomotion controller previously treated action slots 0-3 as support legs and action slots 4+ as upper-body. That mapping only matches the humanoid family. Quadruped and crawler rigs use all eight action slots for legs, and hexapod uses six. Several lesson assists therefore damped or zeroed real front/support legs while telemetry still reported normal learning.\n\nMotor role is now derived from the authored rig graph. A motor is a support motor when its driven branch reaches a semantic support seed. The same rule is used by balance, crouch, walking, and effective-policy blending.\n\n## Rig switching\n\n`TOTAL RIG UPDATES` is scoped to the selected training subject. Switching to a different canonical rig clears the new rig's cumulative counters and starts Stand with fresh policy/optimizer/best state. Episode failures, nursery policy retries, and same-rig recalibration keep that rig's totals.\n\nLesson-entry baselines are captured immediately after the command is applied, so progress cannot inherit an old rig's update/evaluation baseline.\n\n## Preview\n\nThe large live preview disables the moving-course conveyor. It must generate its own world displacement. Training workers retain their curriculum pressure independently.\n\nAutomatic preview termination records the invalid-motion reason before reset. The live overlay displays restart count and the last reason, so a fall/overspeed/collapse restart is visible instead of looking like an unexplained teleport.\n\n## Art\n\nThe packaged runtime `foot_side.ppm`, `helmet_side.ppm`, `torso_side.ppm`, and `weapon_side.ppm` are enabled automatically when present. Torso art is bounded to the real root/torso span; the renderer never draws an entire concept sheet over the rig.\n'''
write("docs/RUNNER_V0726_TRAINING_TRUTH.md", doc)

test = r'''#include "autonomy.hpp"
#include "ppo.hpp"
#include "simulation.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
namespace {
int fail(const char* message) { std::cerr << "v0.7.26 training-truth failure: " << message << '\n'; return EXIT_FAILURE; }
bool all_support_motors(const runner::sim::CreatureBlueprint& rig) {
    for (std::size_t i = 0; i < rig.active_motor_count; ++i)
        if (!runner::rl::motor_drives_support_branch(rig, rig.motors[i])) return false;
    return true;
}}
int main() {
    using namespace runner;
    const auto humanoid = sim::CreatureBlueprint::humanoid();
    const auto quadruped = sim::CreatureBlueprint::quadruped();
    const auto crawler = sim::CreatureBlueprint::crawler4();
    const auto hexapod = sim::CreatureBlueprint::hexapod();
    for (std::size_t i = 0; i < 4u; ++i) if (!rl::motor_drives_support_branch(humanoid, humanoid.motors[i])) return fail("humanoid leg was not support");
    for (std::size_t i = 4u; i < humanoid.active_motor_count; ++i) if (rl::motor_drives_support_branch(humanoid, humanoid.motors[i])) return fail("humanoid arm was support");
    if (!all_support_motors(quadruped)) return fail("quadruped support classification");
    if (!all_support_motors(crawler)) return fail("crawler support classification");
    if (!all_support_motors(hexapod)) return fail("hexapod support classification");
    sim::Environment quad{quadruped, 0x726u}; quad.set_course(sim::CourseStage::duck_press, 0.25f);
    std::array<float, sim::action_count> full{}; full.fill(1.0f);
    const auto duck = rl::effective_policy_action(quad, full, sim::CourseStage::duck_press);
    if (std::abs(duck[4]) < 0.02f || std::abs(duck[6]) < 0.02f) return fail("quadruped front legs still zeroed");
    sim::Environment preview{quadruped, 0x727u}; preview.set_course(sim::CourseStage::uneven, 0.30f);
    if (preview.course_speed() <= 0.0f) return fail("training course motion missing");
    preview.set_course_motion_enabled(false);
    if (preview.course_speed() != 0.0f || preview.course_progress() != 0.0f) return fail("preview conveyor still moving");
    rl::PpoTrainer trainer{humanoid, 8u, false}; trainer.train_one_update();
    if (trainer.metrics().total_updates == 0u) return fail("no setup update");
    trainer.reset_policy(0x1234u);
    if (trainer.metrics().total_updates == 0u) return fail("same-rig retry erased totals");
    trainer.set_blueprint(quadruped, false);
    if (trainer.metrics().total_updates != 0u || trainer.metrics().total_episodes != 0u || trainer.metrics().evaluation_count != 0u) return fail("rig switch retained totals");
    rl::AutonomousTrainer autonomous{quadruped, 8u}; autonomous.synchronize();
    if (autonomous.preview().course_motion_enabled()) return fail("large preview conveyor enabled");
    std::cout << "Runner v0.7.26 training-truth checks passed\n";
    return EXIT_SUCCESS;
}
'''
write("tests/v0726_training_truth_tests.cpp", test)

audit = read("tools/repository_audit.cmake")
audit = audit.replace("Runner VERSION 0.7.25", "Runner VERSION 0.7.26").replace("0x0007'2501u", "0x0007'2601u").replace("runner-v0725-rig-autosave.eppo", "runner-v0726-rig-autosave.eppo").replace("Runner v0.7.25 repository hygiene passed", "Runner v0.7.26 repository hygiene passed").replace("CMake v0.7.24 contract missing", "CMake v0.7.26 contract missing").replace("v0.7.24 application contract missing", "v0.7.26 application contract missing").replace("Mission cache v0.7.24 contract missing", "Mission cache v0.7.26 contract missing")
audit = replace_once(audit, '        docs/RUNNER_V0725_ART_LEG_HOTFIX.md\n        tests/v0725_art_leg_hotfix_tests.cpp\n', '        docs/RUNNER_V0725_ART_LEG_HOTFIX.md\n        docs/RUNNER_V0726_TRAINING_TRUTH.md\n        tests/v0725_art_leg_hotfix_tests.cpp\n        tests/v0726_training_truth_tests.cpp\n        assets/optional/runner_armor_concepts/runtime/foot_side.ppm\n        assets/optional/runner_armor_concepts/runtime/helmet_side.ppm\n        assets/optional/runner_armor_concepts/runtime/torso_side.ppm\n        assets/optional/runner_armor_concepts/runtime/weapon_side.ppm\n', "audit required files")
audit = replace_once(audit, '        "RunnerV0725ArtLegHotfixTests"\n        "RUNNER_V0725_ART_LEG_HOTFIX.md"\n', '        "RunnerV0725ArtLegHotfixTests"\n        "RunnerV0726TrainingTruthTests"\n        "RUNNER_V0725_ART_LEG_HOTFIX.md"\n        "RUNNER_V0726_TRAINING_TRUTH.md"\n', "audit cmake refs")
audit = replace_once(audit, '        "WALK-RELEASE-313")', '        "WALK-RELEASE-313"\n        "WALK-RIG-ROLE-314"\n        "WALK-RIG-ROLE-315"\n        "WALK-RIG-RESET-316"\n        "WALK-RIG-RETRY-317"\n        "WALK-PREVIEW-318"\n        "WALK-PREVIEW-319"\n        "WALK-TELEMETRY-320"\n        "WALK-ART-321"\n        "WALK-STATE-322"\n        "WALK-REGRESSION-323"\n        "WALK-RELEASE-324")', "audit missions")
audit = regex_once(audit, r'string\(FIND "\$\{app_text\}" "draw_pixel_art\(canvas, optional_torso_art" torso_bitmap_pos\)\nif\(NOT torso_bitmap_pos EQUAL -1\)\n    message\(FATAL_ERROR "Oversized torso bitmap rendering remains"\)\nendif\(\)\n', 'foreach(reference IN ITEMS\n        "draw_pixel_art(canvas, optional_torso_art"\n        "User-supplied modular armor, bounded to the physical torso"\n        "optional_art_enabled = impl_->optional_foot_art.loaded()")\n    string(FIND "${app_text}" "${reference}" pos)\n    if(pos EQUAL -1)\n        message(FATAL_ERROR "v0.7.26 runtime art contract missing: ${reference}")\n    endif()\nendforeach()\n', "audit torso art")
audit = replace_once(audit, 'file(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")\n', 'foreach(reference IN ITEMS\n        "motor_drives_support_branch"\n        "clear_totals = false"\n        "preview_last_reset_reason")\n    string(FIND "${ppo_text}" "${reference}" pos)\n    if(pos EQUAL -1)\n        message(FATAL_ERROR "v0.7.26 PPO contract missing: ${reference}")\n    endif()\nendforeach()\n\nfile(READ "${RUNNER_SOURCE_DIR}/src/simulation.hpp" simulation_header_text)\nforeach(reference IN ITEMS\n        "course_motion_enabled_"\n        "set_course_motion_enabled")\n    string(FIND "${simulation_header_text}" "${reference}" pos)\n    if(pos EQUAL -1)\n        message(FATAL_ERROR "v0.7.26 static-preview contract missing: ${reference}")\n    endif()\nendforeach()\n\nfile(GLOB release_notes "${RUNNER_SOURCE_DIR}/RELEASE_NOTES*.md")\n', "audit new contracts")
write("tools/repository_audit.cmake", audit)

print("Applied Runner v0.7.26 training-truth patch.")
