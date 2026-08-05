from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding='utf-8')


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace('\r\n', '\n').rstrip() + '\n', encoding='utf-8')


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return text.replace(old, new, 1)


def patch_autonomy_header() -> None:
    text = read('src/autonomy.hpp')
    text = replace_once(text,
        '''        bool changed{};
        bool topology_changed{};
''',
        '''        bool changed{};
        bool topology_changed{};
        std::uint8_t activated_motor_mask{};
''',
        'activated motor mask')
    write('src/autonomy.hpp', text)


def patch_ppo_header() -> None:
    text = read('src/ppo.hpp')
    text = replace_once(text,
        '''        [[nodiscard]] std::array<float, output_size> standard_deviation() const noexcept;
        void set_exploration(float standard_deviation) noexcept;
''',
        '''        [[nodiscard]] std::array<float, output_size> standard_deviation() const noexcept;
        void set_exploration(float standard_deviation) noexcept;
        void neutralize_action_slot(std::size_t slot) noexcept;
''',
        'policy neutral action slot declaration')
    text = replace_once(text,
        '''        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
''',
        '''        void reset_policy(std::uint64_t seed = 0xC0FFEEu);
        void set_exploration(float standard_deviation) noexcept;
        void neutralize_action_slot(std::size_t slot) noexcept
        {
            policy_.neutralize_action_slot(slot);
        }
''',
        'trainer neutral action slot wrapper')
    write('src/ppo.hpp', text)


def patch_ppo_network() -> None:
    text = read('src/ppo_network.cpp')
    anchor = '''    float PolicyNetwork::mean_exploration() const noexcept
'''
    implementation = '''    void PolicyNetwork::neutralize_action_slot(std::size_t slot) noexcept
    {
        if (slot >= output_size)
            return;
        const std::size_t actor_base = layout_.actor_w + slot * hidden_size;
        std::fill(parameters_.begin() + static_cast<std::ptrdiff_t>(actor_base),
            parameters_.begin() + static_cast<std::ptrdiff_t>(actor_base + hidden_size), 0.0f);
        std::fill(gradients_.begin() + static_cast<std::ptrdiff_t>(actor_base),
            gradients_.begin() + static_cast<std::ptrdiff_t>(actor_base + hidden_size), 0.0f);
        parameters_[layout_.actor_b + slot] = 0.0f;
        gradients_[layout_.actor_b + slot] = 0.0f;
        parameters_[layout_.log_std + slot] = std::log(0.08f);
        gradients_[layout_.log_std + slot] = 0.0f;
    }

'''
    text = replace_once(text, anchor, implementation + anchor,
        'policy neutral action slot implementation')
    write('src/ppo_network.cpp', text)


def patch_simulation_serialization() -> None:
    text = read('src/simulation.cpp')
    text = replace_once(text,
        '''            || node_count < 3 || node_count > 128 || bone_count > 256
            || (motor_count != 4 && motor_count != action_count))
''',
        '''            || node_count < 3 || node_count > 128 || bone_count > 256
            || motor_count == 0u || motor_count > action_count)
''',
        'variable active motor count rig loading')
    write('src/simulation.cpp', text)


def patch_curriculum() -> None:
    text = read('src/autonomy_curriculum.cpp')
    old = '''                for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
                {
                    sim::MotorConstraint& motor = candidate.motors[index];
                    if (same_edge(motor.a, motor.pivot, original.a, original.b))
                        motor.a = inserted;
                    if (same_edge(motor.pivot, motor.c, original.a, original.b))
                        motor.c = inserted;
                }
                result.topology_changed = true;
'''
    new = '''                for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
                {
                    sim::MotorConstraint& motor = candidate.motors[index];
                    if (same_edge(motor.a, motor.pivot, original.a, original.b))
                        motor.a = inserted;
                    if (same_edge(motor.pivot, motor.c, original.a, original.b))
                        motor.c = inserted;
                }
                if (candidate.active_motor_count < sim::action_count)
                {
                    const std::size_t slot = candidate.active_motor_count;
                    candidate.motors[slot] = sim::MotorConstraint{
                        original.a, inserted, original.b
                    };
                    candidate.motors[slot].enabled = true;
                    negative[slot] = 24.0f * pi / 180.0f;
                    positive[slot] = 24.0f * pi / 180.0f;
                    power[slot] = 0.034f;
                    ++candidate.active_motor_count;
                    result.activated_motor_mask = static_cast<std::uint8_t>(
                        result.activated_motor_mask | (1u << slot));
                }
                result.topology_changed = true;
'''
    text = replace_once(text, old, new,
        'split bone active motor growth')
    text = replace_once(text,
        '''            result.blueprint = source;
            result.topology_changed = false;
''',
        '''            result.blueprint = source;
            result.topology_changed = false;
            result.activated_motor_mask = 0u;
''',
        'invalid mutation clears active motor mask')
    old = '''        nursery.set_course(stage_, difficulty_, false);
        nursery.set_exploration(std::max(0.10f, worker_.exploration()));
'''
    new = '''        for (std::size_t slot = 0; slot < sim::action_count; ++slot)
        {
            if ((mutation.activated_motor_mask & (1u << slot)) != 0u)
                nursery.neutralize_action_slot(slot);
        }
        nursery.set_course(stage_, difficulty_, false);
        nursery.set_exploration(std::max(0.10f, worker_.exploration()));
'''
    text = replace_once(text, old, new,
        'neutralize newly activated nursery motors')
    text = replace_once(text,
        '''                "TOPOLOGY NURSERY {} ACCEPTED {}  {:+.3f} VALID SCORE",
                rig_generation_, mutation_name(mutation.kind),
''',
        '''                "TOPOLOGY NURSERY {} ACCEPTED {}{}  {:+.3f} VALID SCORE",
                rig_generation_, mutation_name(mutation.kind),
                mutation.activated_motor_mask != 0u ? " + ACTIVE JOINT" : "",
''',
        'active joint acceptance status')
    write('src/autonomy_curriculum.cpp', text)


def patch_checkpoint_transfer() -> None:
    text = read('src/training_checkpoint.cpp')
    text = replace_once(text,
        '''            || !read_value(input, data.training_semantics)
            || data.training_semantics != training_semantics_version
            || !read_value(input, data.rig_signature)
''',
        '''            || !read_value(input, data.training_semantics)
            || !read_value(input, data.rig_signature)
''',
        'parse legacy semantics for explicit transfer')
    text = replace_once(text,
        '''            error = "Invalid or incompatible Runner v0.7.1 training-semantics checkpoint.";
''',
        '''            error = "Invalid or truncated Runner checkpoint.";
''',
        'current checkpoint parse error')
    text = replace_once(text,
        '''        if (data.training_semantics != training_semantics_version)
        {
            error = "INCOMPATIBLE TRAINING SEMANTICS - START FRESH OR IMPORT WEIGHTS EXPLICITLY";
            return false;
        }
        const std::size_t expected = policy_.parameter_count();
        if (data.parameters.size() != expected
            || data.first_moment.size() != expected
            || data.second_moment.size() != expected
            || (!data.best_parameters.empty() && data.best_parameters.size() != expected))
''',
        '''        if (!transfer_only && data.training_semantics != training_semantics_version)
        {
            error = "INCOMPATIBLE TRAINING SEMANTICS - RESUME BLOCKED; USE EXPLICIT WEIGHT TRANSFER";
            return false;
        }
        const std::size_t expected = policy_.parameter_count();
        const bool optimizer_dimensions_valid = data.first_moment.size() == expected
            && data.second_moment.size() == expected
            && (data.best_parameters.empty() || data.best_parameters.size() == expected);
        if (data.parameters.size() != expected
            || (!transfer_only && !optimizer_dimensions_valid))
''',
        'resume reject and transfer-only dimension contract')
    text = replace_once(text,
        '''            error = "WEIGHTS TRANSFERRED - OPTIMIZER AND BEST STATE RESET";
''',
        '''            error = data.training_semantics == training_semantics_version
                ? "WEIGHTS TRANSFERRED - OPTIMIZER AND BEST STATE RESET"
                : "LEGACY WEIGHTS TRANSFERRED - SEMANTICS, OPTIMIZER, BEST, AND MASTERY RESET";
''',
        'explicit legacy transfer status')
    write('src/training_checkpoint.cpp', text)


def patch_messages() -> None:
    text = read('src/autonomy_commands.cpp')
    text = text.replace('AUTOSAVE RESUMED - RUNNER V0.7.6 SEMANTICS',
        'AUTOSAVE RESUMED - RUNNER V0.7.15 GAIT/EVOLUTION SEMANTICS')
    text = text.replace('AUTOSAVE TRANSFERRED - RUNNER V0.7.6 SEMANTICS',
        'AUTOSAVE TRANSFERRED - OPTIMIZER, BEST, AND MASTERY RESET')
    write('src/autonomy_commands.cpp', text)


def patch_acceptance() -> None:
    text = read('src/acceptance.cpp')
    text = replace_once(text,
        '''        const std::array<NamedRig, 7> all_presets{
            NamedRig{ "CHICKEN", sim::CreatureBlueprint::chicken() },
''',
        '''        const std::array<NamedRig, 8> all_presets{
            NamedRig{ "SCAFFOLD", sim::CreatureBlueprint::scaffold() },
            NamedRig{ "CHICKEN", sim::CreatureBlueprint::chicken() },
''',
        'scaffold live acceptance matrix')
    text = text.replace('all seven presets', 'all eight presets')
    write('src/acceptance.cpp', text)


def patch_tests() -> None:
    text = read('tests/core_tests.cpp')
    text = replace_once(text,
        '''    const std::array<sim::CreatureBlueprint, 7> presets{
        sim::CreatureBlueprint::chicken(),
''',
        '''    const std::array<sim::CreatureBlueprint, 8> presets{
        sim::CreatureBlueprint::scaffold(),
        sim::CreatureBlueprint::chicken(),
''',
        'scaffold deterministic preset matrix')

    anchor = '''    require(topology_mutation_seen && parametric_mutation_seen,
        "rig evolution does not produce both topology and parameter candidates");
'''
    addition = anchor + '''    const rl::RigMutationCandidate articulated_growth =
        rl::evolve_rig_candidate(scaffold, 5u);
    require(articulated_growth.changed && articulated_growth.topology_changed
            && articulated_growth.blueprint.active_motor_count
                == scaffold.active_motor_count + 1u
            && articulated_growth.activated_motor_mask
                == static_cast<std::uint8_t>(1u << scaffold.active_motor_count),
        "bone split does not activate one neutral trainable joint slot");
    const std::size_t grown_slot = scaffold.active_motor_count;
    require(articulated_growth.blueprint.motors[grown_slot].enabled
            && articulated_growth.blueprint.motors[grown_slot].a
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].pivot
                < articulated_growth.blueprint.nodes.size()
            && articulated_growth.blueprint.motors[grown_slot].c
                < articulated_growth.blueprint.nodes.size(),
        "newly activated topology motor is not structurally valid");

    rl::PolicyNetwork neutral_policy{ 0xA4710u };
    std::array<float, sim::observation_count> neutral_observation{};
    neutral_observation.fill(0.5f);
    neutral_policy.neutralize_action_slot(grown_slot);
    require(std::abs(neutral_policy.evaluate(neutral_observation).mean[grown_slot])
            < 1.0e-7f,
        "new topology action slot retains stale actor motion after neutralization");
'''
    text = replace_once(text, anchor, addition,
        'active joint and neutral policy tests')

    anchor = '''    require(resumed.course_stage() == trainer.course_stage(), "checkpoint curriculum stage was not restored");

    rl::PpoTrainer wrong_rig{ sim::CreatureBlueprint::quadruped(), 16 };
'''
    addition = '''    require(resumed.course_stage() == trainer.course_stage(), "checkpoint curriculum stage was not restored");

    rl::PpoTrainer::CheckpointData legacy = trainer.checkpoint_data();
    legacy.training_semantics = rl::training_semantics_version - 1u;
    legacy.first_moment.clear();
    legacy.second_moment.clear();
    legacy.best_parameters.clear();
    rl::PpoTrainer blocked_legacy{ humanoid, 16 };
    require(!blocked_legacy.apply_checkpoint_data(legacy, error, false),
        "legacy semantics resumed as valid mastery instead of requiring transfer");
    rl::PpoTrainer transferred_legacy{ humanoid, 16 };
    require(transferred_legacy.apply_checkpoint_data(legacy, error, true),
        "explicit dimension-compatible legacy weight transfer failed: " + error);
    require(transferred_legacy.policy().parameters() == trainer.policy().parameters()
            && transferred_legacy.metrics().update == 0u
            && transferred_legacy.optimizer_step() == 0u
            && transferred_legacy.controller_state() == rl::ControllerState::transferred,
        "legacy transfer retained optimizer, mastery, or non-transfer controller state");
    const std::filesystem::path legacy_path =
        std::filesystem::temp_directory_path() / "runner-v0715-legacy-transfer-test.eppo";
    require(rl::PpoTrainer::write_checkpoint_data(legacy, legacy_path, error),
        "failed to write legacy transfer fixture: " + error);
    rl::PpoTrainer loaded_legacy{ humanoid, 16 };
    require(!loaded_legacy.load_checkpoint(legacy_path, error, false)
            && loaded_legacy.load_checkpoint(legacy_path, error, true),
        "file-based legacy checkpoint is not resume-blocked and transfer-enabled");
    std::filesystem::remove(legacy_path);

    rl::PpoTrainer wrong_rig{ sim::CreatureBlueprint::quadruped(), 16 };
'''
    text = replace_once(text, anchor, addition,
        'legacy semantics transfer tests')
    write('tests/core_tests.cpp', text)


def patch_readme() -> None:
    text = read('README.md')
    text = replace_once(text,
        'Runner 0.7.14 is a combined C++23 SDL3/Vulkan locomotion and SandHybrid live-map laboratory',
        'Runner 0.7.15 is a combined C++23 SDL3/Vulkan locomotion, morphology-evolution, and SandHybrid live-map laboratory',
        'README current version')
    text = replace_once(text,
        '''The built-in presets are chicken, biped, humanoid, quadruped, four-leg crawler, hexapod, and monoped. Every preset has explicit support semantics and a rig-specific control path. The monoped uses a single-leg gait cycle rather than fake alternating biped steps.
''',
        '''The built-in presets are scaffold, chicken, biped, humanoid, quadruped, four-leg crawler, hexapod, and monoped. Every preset has explicit support semantics and a rig-specific control path. The scaffold is the minimal topology-evolution seed; valid bone splitting may activate a previously unused neutral policy slot and train it in a bounded nursery. The monoped uses a single-leg gait cycle rather than fake alternating biped steps.
''',
        'README scaffold and active evolution')
    text = replace_once(text,
        '''- `Delete`: Remove the selected non-required node
- `Shift + click`: Add a node
- `Ctrl + click`: Connect the selected node to another node
''',
        '''- `Delete`: Remove the selected non-required node
- `Shift + click`: Add a node
- `Ctrl + click`: Connect the selected node to another node
- `Alt + click`: Select a bone for stiffness inspection or safe deletion
- Rig Lab buttons: scaffold/presets, near-leg depth, champion restore, fresh policy, crouch/gait-cycle preview, and firm/loose traction diagnostics
''',
        'README editor controls')
    write('README.md', text)


def patch_documents() -> None:
    text = read('missioncache.md')
    text = replace_once(text,
        '''### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation
**Status:** PARTIAL — TOPOLOGY OPERATORS PASS; NEW ACTIVE JOINT SLOT GROWTH REOPENED BEFORE RELEASE
''',
        '''### WALK-EVOLUTION-143 — Structural rig evolution with nursery adaptation
**Status:** IMPLEMENTED — ACTIVE JOINT GROWTH, NEUTRAL SLOT TRANSFER, AND PACKAGE VALIDATION REQUIRED
''',
        'evolution mission implementation status')
    text = replace_once(text,
        '''### WALK-STATE-145 — Isolate corrected locomotion/evolution semantics
**Status:** OPEN
''',
        '''### WALK-STATE-145 — Isolate corrected locomotion/evolution semantics
**Status:** IMPLEMENTED — CROSS-PLATFORM AND PACKAGE VALIDATION REQUIRED
''',
        'state mission implementation status')
    text = text.replace('## Release target\n\n**Target:** Runner v0.7.14',
        '## Historical v0.7.14 release evidence\n\n**Historical target:** Runner v0.7.14', 1)
    write('missioncache.md', text)

    text = read('CHANGELOG.md')
    anchor = '## Runner v0.7.15 — locomotion and evolution editor\n'
    addition = '''## Runner v0.7.15 — active joint growth and state transfer

- Bone-split mutations may now activate one free anatomy action slot as a real articulated joint.
- Newly activated slots have their transferred actor row and bias zeroed before bounded nursery adaptation, preventing stale unused-output motion.
- Normal resume rejects older training semantics, while explicit dimension-compatible transfer imports weights only and clears optimizer, champion, curriculum, and mastery state.
- Expanded deterministic and executable acceptance from seven presets to eight by including the scaffold.

''' + anchor
    text = replace_once(text, anchor, addition,
        'active joint and transfer changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_autonomy_header()
    patch_ppo_header()
    patch_ppo_network()
    patch_simulation_serialization()
    patch_curriculum()
    patch_checkpoint_transfer()
    patch_messages()
    patch_acceptance()
    patch_tests()
    patch_readme()
    patch_documents()


if __name__ == '__main__':
    main()
