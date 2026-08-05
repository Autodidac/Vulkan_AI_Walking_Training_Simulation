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


def patch_simulation_header() -> None:
    text = read('src/simulation.hpp')
    text = replace_once(text,
        '        [[nodiscard]] static CreatureBlueprint chicken();\n',
        '        [[nodiscard]] static CreatureBlueprint scaffold();\n'
        '        [[nodiscard]] static CreatureBlueprint chicken();\n',
        'scaffold declaration')
    write('src/simulation.hpp', text)


def patch_simulation_source() -> None:
    text = read('src/simulation.cpp')
    marker = '    CreatureBlueprint CreatureBlueprint::chicken()\n'
    implementation = r'''    CreatureBlueprint CreatureBlueprint::scaffold()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.00f, 2.62f }, { 0.00f, 3.68f }, { 0.02f, 4.38f },
            { -0.30f, 1.46f }, { -0.38f, 0.26f },
            { 0.30f, 1.46f }, { 0.38f, 0.26f }
        };
        result.radii = { 0.24f, 0.27f, 0.23f, 0.17f, 0.15f, 0.17f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        result.active_motor_count = 4;
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        add_passive_feet(result, 0.16f, 0.28f);
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 56.0f, 0.042f, 0.048f);
        return result;
    }

'''
    text = replace_once(text, marker, implementation + marker,
        'scaffold implementation')
    write('src/simulation.cpp', text)


def patch_autonomy_header() -> None:
    text = read('src/autonomy.hpp')
    marker = '    class AutonomousTrainer\n'
    definitions = r'''    enum class RigMutationKind : std::uint8_t
    {
        motor_strength,
        joint_range,
        support_width,
        torso_height,
        pivot_height,
        split_bone,
        append_leaf,
        duplicate_support,
        remove_leaf,
        node_radius,
        bone_stiffness
    };

    struct RigMutationCandidate
    {
        sim::CreatureBlueprint blueprint{};
        RigMutationKind kind{ RigMutationKind::motor_strength };
        bool changed{};
        bool topology_changed{};
    };

    [[nodiscard]] RigMutationCandidate evolve_rig_candidate(
        const sim::CreatureBlueprint& source, std::uint64_t generation) noexcept;

'''
    text = replace_once(text, marker, definitions + marker,
        'rig mutation public contract')
    text = replace_once(text,
        '        [[nodiscard]] float evaluate_rig_locked(const sim::CreatureBlueprint& candidate) const;\n'
        '        [[nodiscard]] sim::CreatureBlueprint mutate_rig_locked() noexcept;\n',
        '        [[nodiscard]] float evaluate_rig_locked(const sim::CreatureBlueprint& candidate,\n'
        '            const PolicyNetwork& policy) const;\n'
        '        [[nodiscard]] RigMutationCandidate mutate_rig_locked() noexcept;\n',
        'autonomy evolution method declarations')
    write('src/autonomy.hpp', text)


def patch_curriculum() -> None:
    text = read('src/autonomy_curriculum.cpp')
    start = text.index('    float AutonomousTrainer::evaluate_rig_locked(')
    end = text.rindex('\n}')
    replacement = r'''    namespace
    {
        [[nodiscard]] bool same_edge(std::uint16_t a, std::uint16_t b,
            std::uint16_t c, std::uint16_t d) noexcept
        {
            return (a == c && b == d) || (a == d && b == c);
        }

        [[nodiscard]] std::size_t node_degree(const sim::CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            return static_cast<std::size_t>(std::ranges::count_if(
                rig.bones, [node](const sim::DistanceConstraint& bone)
                {
                    return bone.a == node || bone.b == node;
                }));
        }

        [[nodiscard]] bool motor_references_node(const sim::CreatureBlueprint& rig,
            std::size_t node) noexcept
        {
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                const sim::MotorConstraint& motor = rig.motors[index];
                if (motor.enabled && (motor.a == node
                    || motor.pivot == node || motor.c == node))
                    return true;
            }
            return false;
        }

        void recalibrate_after_geometry(sim::CreatureBlueprint& rig,
            const std::array<float, sim::action_count>& negative,
            const std::array<float, sim::action_count>& positive,
            const std::array<float, sim::action_count>& power) noexcept
        {
            rig.rebuild_rest_lengths();
            for (std::size_t index = 0; index < rig.active_motor_count; ++index)
            {
                sim::MotorConstraint& motor = rig.motors[index];
                if (!motor.enabled)
                    continue;
                motor.neutral_angle = rig.rest_joint_angle(index);
                motor.minimum_angle = motor.neutral_angle - negative[index];
                motor.maximum_angle = motor.neutral_angle + positive[index];
                motor.strength = power[index];
            }
        }

        void remove_node(sim::CreatureBlueprint& rig, std::uint16_t removed) noexcept
        {
            rig.nodes.erase(rig.nodes.begin() + removed);
            rig.radii.erase(rig.radii.begin() + removed);
            std::erase_if(rig.bones, [removed](const sim::DistanceConstraint& bone)
            {
                return bone.a == removed || bone.b == removed;
            });
            auto remap = [removed](std::uint16_t& value)
            {
                if (value > removed)
                    --value;
            };
            for (sim::DistanceConstraint& bone : rig.bones)
            {
                remap(bone.a);
                remap(bone.b);
            }
            remap(rig.root_node);
            remap(rig.torso_node);
            remap(rig.head_node);
            remap(rig.left_contact_node);
            remap(rig.right_contact_node);
            for (std::uint16_t& node : rig.additional_left_contact_nodes)
                remap(node);
            for (std::uint16_t& node : rig.additional_right_contact_nodes)
                remap(node);
            for (sim::MotorConstraint& motor : rig.motors)
            {
                remap(motor.a);
                remap(motor.pivot);
                remap(motor.c);
            }
        }

        [[nodiscard]] std::string_view mutation_name(RigMutationKind kind) noexcept
        {
            switch (kind)
            {
            case RigMutationKind::motor_strength: return "MOTOR STRENGTH";
            case RigMutationKind::joint_range: return "JOINT RANGE";
            case RigMutationKind::support_width: return "SUPPORT WIDTH";
            case RigMutationKind::torso_height: return "TORSO HEIGHT";
            case RigMutationKind::pivot_height: return "PIVOT HEIGHT";
            case RigMutationKind::split_bone: return "SPLIT BONE";
            case RigMutationKind::append_leaf: return "APPEND BRANCH";
            case RigMutationKind::duplicate_support: return "DUPLICATE SUPPORT";
            case RigMutationKind::remove_leaf: return "REMOVE LEAF";
            case RigMutationKind::node_radius: return "NODE RADIUS";
            case RigMutationKind::bone_stiffness: return "BONE STIFFNESS";
            }
            return "UNKNOWN";
        }
    }

    RigMutationCandidate evolve_rig_candidate(const sim::CreatureBlueprint& source,
        std::uint64_t generation) noexcept
    {
        RigMutationCandidate result{};
        result.blueprint = source;
        result.kind = static_cast<RigMutationKind>(generation % 11u);
        sim::CreatureBlueprint& candidate = result.blueprint;
        const std::uint64_t original_signature = source.signature();
        const float direction = ((generation / 11u) & 1u) == 0u ? 1.0f : -1.0f;

        std::array<float, sim::action_count> negative{};
        std::array<float, sim::action_count> positive{};
        std::array<float, sim::action_count> power{};
        for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
        {
            negative[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].neutral_angle - candidate.motors[index].minimum_angle);
            positive[index] = std::max(2.0f * pi / 180.0f,
                candidate.motors[index].maximum_angle - candidate.motors[index].neutral_angle);
            power[index] = candidate.motors[index].strength;
        }

        switch (result.kind)
        {
        case RigMutationKind::motor_strength:
        {
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                power[index] = clamp(power[index] + direction * 0.0020f,
                    0.020f, 0.11f);
            }
            break;
        }
        case RigMutationKind::joint_range:
        {
            if (candidate.active_motor_count > 0u)
            {
                const std::size_t index = static_cast<std::size_t>(
                    generation % candidate.active_motor_count);
                const float delta = direction * 1.5f * pi / 180.0f;
                negative[index] = clamp(negative[index] + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
                positive[index] = clamp(positive[index] + delta,
                    2.0f * pi / 180.0f, 120.0f * pi / 180.0f);
            }
            break;
        }
        case RigMutationKind::support_width:
        {
            const float delta = direction * 0.018f;
            if (candidate.left_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.left_contact_node].x -= delta;
            if (candidate.right_contact_node < candidate.nodes.size())
                candidate.nodes[candidate.right_contact_node].x += delta;
            break;
        }
        case RigMutationKind::torso_height:
        {
            const float delta = direction * 0.018f;
            if (candidate.torso_node < candidate.nodes.size())
                candidate.nodes[candidate.torso_node].y = clamp(
                    candidate.nodes[candidate.torso_node].y + delta, 0.40f, 6.0f);
            if (candidate.head_node < candidate.nodes.size())
                candidate.nodes[candidate.head_node].y = clamp(
                    candidate.nodes[candidate.head_node].y + delta, 0.45f, 6.5f);
            break;
        }
        case RigMutationKind::pivot_height:
        {
            const float delta = direction * 0.014f;
            for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
            {
                const std::uint16_t pivot = candidate.motors[index].pivot;
                if (pivot < candidate.nodes.size() && pivot != candidate.root_node)
                    candidate.nodes[pivot].y = clamp(candidate.nodes[pivot].y + delta,
                        0.20f, 5.5f);
            }
            break;
        }
        case RigMutationKind::split_bone:
        {
            if (!candidate.bones.empty() && candidate.nodes.size() < 128u
                && candidate.bones.size() < 256u)
            {
                const std::size_t bone_index = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                const sim::DistanceConstraint original = candidate.bones[bone_index];
                const auto inserted = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back((candidate.nodes[original.a]
                    + candidate.nodes[original.b]) * 0.5f);
                candidate.radii.push_back(clamp(0.5f
                    * (candidate.radii[original.a] + candidate.radii[original.b])
                    * 0.86f, 0.07f, 0.45f));
                candidate.bones[bone_index].b = inserted;
                candidate.bones.push_back({ inserted, original.b, 0.0f,
                    original.stiffness });
                for (std::size_t index = 0; index < candidate.active_motor_count; ++index)
                {
                    sim::MotorConstraint& motor = candidate.motors[index];
                    if (same_edge(motor.a, motor.pivot, original.a, original.b))
                        motor.a = inserted;
                    if (same_edge(motor.pivot, motor.c, original.a, original.b))
                        motor.c = inserted;
                }
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::append_leaf:
        {
            if (candidate.nodes.size() < 128u && candidate.bones.size() < 256u)
            {
                const std::uint16_t parent = candidate.torso_node < candidate.nodes.size()
                    ? candidate.torso_node : candidate.root_node;
                const auto appended = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back(candidate.nodes[parent]
                    + sim::Vec2{ direction * 0.34f, 0.10f });
                candidate.radii.push_back(0.11f);
                candidate.bones.push_back({ parent, appended, 0.0f, 0.72f });
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::duplicate_support:
        {
            const bool left = ((generation / 11u) & 1u) == 0u;
            const std::uint16_t contact = left
                ? candidate.left_contact_node : candidate.right_contact_node;
            if (contact < candidate.nodes.size() && candidate.nodes.size() < 128u
                && candidate.bones.size() < 256u)
            {
                std::uint16_t parent = candidate.root_node;
                for (const sim::DistanceConstraint& bone : candidate.bones)
                {
                    if (bone.a == contact) { parent = bone.b; break; }
                    if (bone.b == contact) { parent = bone.a; break; }
                }
                const auto duplicated = static_cast<std::uint16_t>(candidate.nodes.size());
                candidate.nodes.push_back(candidate.nodes[contact]
                    + sim::Vec2{ left ? -0.12f : 0.12f, 0.015f });
                candidate.radii.push_back(clamp(candidate.radii[contact] * 0.82f,
                    0.06f, 0.24f));
                candidate.bones.push_back({ parent, duplicated, 0.0f, 0.92f });
                if (left)
                    candidate.additional_left_contact_nodes.push_back(duplicated);
                else
                    candidate.additional_right_contact_nodes.push_back(duplicated);
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::remove_leaf:
        {
            std::vector<std::uint16_t> removable{};
            for (std::size_t node = 0; node < candidate.nodes.size(); ++node)
            {
                if (node == candidate.root_node || node == candidate.torso_node
                    || node == candidate.head_node || candidate.is_support_seed(node)
                    || motor_references_node(candidate, node)
                    || node_degree(candidate, node) != 1u)
                    continue;
                removable.push_back(static_cast<std::uint16_t>(node));
            }
            if (!removable.empty() && candidate.nodes.size() > 3u)
            {
                const std::uint16_t removed = removable[static_cast<std::size_t>(
                    generation % removable.size())];
                remove_node(candidate, removed);
                result.topology_changed = true;
            }
            break;
        }
        case RigMutationKind::node_radius:
        {
            if (!candidate.radii.empty())
            {
                const std::size_t node = static_cast<std::size_t>(
                    generation % candidate.radii.size());
                candidate.radii[node] = clamp(candidate.radii[node]
                    + direction * 0.008f, 0.055f, 0.60f);
            }
            break;
        }
        case RigMutationKind::bone_stiffness:
        {
            if (!candidate.bones.empty())
            {
                const std::size_t bone = static_cast<std::size_t>(
                    generation % candidate.bones.size());
                candidate.bones[bone].stiffness = clamp(
                    candidate.bones[bone].stiffness + direction * 0.025f,
                    0.20f, 1.0f);
            }
            break;
        }
        }

        recalibrate_after_geometry(candidate, negative, positive, power);
        result.changed = candidate.valid()
            && candidate.signature() != original_signature;
        if (!result.changed)
        {
            result.blueprint = source;
            result.topology_changed = false;
        }
        return result;
    }

    float AutonomousTrainer::evaluate_rig_locked(
        const sim::CreatureBlueprint& candidate, const PolicyNetwork& policy) const
    {
        if (!candidate.valid())
            return -std::numeric_limits<float>::infinity();

        constexpr std::size_t agents = 4;
        const sim::CourseStage stage = stage_;
        const float difficulty = difficulty_;
        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 1500 : 900;
        std::array<float, agents> scores{};
        std::array<std::jthread, agents> evaluators{};

        for (std::size_t agent = 0; agent < agents; ++agent)
        {
            evaluators[agent] = std::jthread([&candidate, &policy, &scores,
                stage, difficulty, maximum_steps, agent]
            {
                const std::uint64_t seed = 0xA100u
                    + static_cast<std::uint64_t>(agent) * 3253u;
                sim::Environment environment{ candidate, seed };
                environment.set_course(stage, difficulty);
                float reward = 0.0f;
                for (int step = 0; step < maximum_steps; ++step)
                {
                    const auto raw_action = policy.deterministic_action(
                        environment.observation());
                    const auto action = effective_policy_action(
                        environment, raw_action, stage);
                    const sim::StepResult result = environment.step(action);
                    reward += result.reward;
                    if (result.terminated)
                        break;
                }
                const StageMotionQualification qualification =
                    stage_motion_qualification(stage, environment);
                if (!qualification.valid)
                {
                    scores[agent] = -std::numeric_limits<float>::infinity();
                    return;
                }
                scores[agent] = reward + environment.distance_travelled() * 0.75f
                    + environment.elapsed_seconds() * 0.03f
                    + static_cast<float>(environment.gait_cycles()) * 0.03f
                    + environment.duck_seconds() * 0.06f
                    + static_cast<float>(environment.landed_jumps()) * 0.16f
                    + std::min(environment.maximum_spin_turns(), 3.0f) * 0.18f
                    + static_cast<float>(environment.obstacles_passed()) * 0.28f
                    - environment.collision_count() * 0.10f
                    - environment.airborne_ratio() * 0.20f
                    - environment.body_rolling_seconds() * 2.00f;
            });
        }
        for (std::jthread& evaluator : evaluators)
        {
            if (evaluator.joinable())
                evaluator.join();
        }

        float total = 0.0f;
        for (const float score : scores)
        {
            if (!std::isfinite(score))
                return -std::numeric_limits<float>::infinity();
            total += score;
        }
        return total / static_cast<float>(agents);
    }

    RigMutationCandidate AutonomousTrainer::mutate_rig_locked() noexcept
    {
        return evolve_rig_candidate(worker_.blueprint(), rig_generation_);
    }

    void AutonomousTrainer::attempt_rig_evolution_locked()
    {
        const sim::CreatureBlueprint champion_rig = worker_.blueprint();
        const PpoTrainer::CheckpointData champion_checkpoint = worker_.checkpoint_data();
        const float baseline = evaluate_rig_locked(champion_rig, worker_.policy());
        RigMutationCandidate mutation = mutate_rig_locked();
        ++rig_generation_;
        if (!mutation.changed)
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "RIG GENERATION {} REJECTED - INVALID/EMPTY {} MUTATION",
                rig_generation_, mutation_name(mutation.kind));
            return;
        }

        sim::CreatureBlueprint candidate = std::move(mutation.blueprint);
        PpoTrainer nursery(candidate, 16, false);
        PpoTrainer::CheckpointData transfer = champion_checkpoint;
        std::string transfer_error{};
        if (!nursery.apply_checkpoint_data(std::move(transfer), transfer_error, true))
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} REJECTED - POLICY TRANSFER FAILED",
                rig_generation_);
            return;
        }
        nursery.set_course(stage_, difficulty_, false);
        nursery.set_exploration(std::max(0.10f, worker_.exploration()));
        constexpr int nursery_updates = 4;
        for (int update = 0; update < nursery_updates; ++update)
            nursery.train_one_update();

        const float candidate_score = evaluate_rig_locked(candidate, nursery.policy());
        const float required_gain = std::isfinite(baseline)
            ? std::max(0.025f, std::abs(baseline) * 0.01f) : 0.025f;
        if (std::isfinite(candidate_score)
            && (!std::isfinite(baseline) || candidate_score > baseline + required_gain))
        {
            worker_.set_blueprint(candidate, true);
            PpoTrainer::CheckpointData adapted = nursery.checkpoint_data();
            std::string apply_error{};
            if (!worker_.apply_checkpoint_data(std::move(adapted), apply_error, false))
            {
                worker_.set_blueprint(champion_rig, true);
                PpoTrainer::CheckpointData restore = champion_checkpoint;
                std::string restore_error{};
                static_cast<void>(worker_.apply_checkpoint_data(
                    std::move(restore), restore_error, false));
                ++rejected_rig_changes_;
                ++rollback_count_;
                worker_message_ = std::format(
                    "TOPOLOGY NURSERY {} ROLLED BACK - ADAPTED POLICY APPLY FAILED",
                    rig_generation_);
                return;
            }
            ++accepted_rig_changes_;
            mastery_streak_ = 0;
            degradation_streak_ = 0;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} ACCEPTED {}  {:+.3f} VALID SCORE",
                rig_generation_, mutation_name(mutation.kind),
                candidate_score - (std::isfinite(baseline) ? baseline : 0.0f));
            queue_autosave();
        }
        else
        {
            ++rejected_rig_changes_;
            worker_message_ = std::format(
                "TOPOLOGY NURSERY {} REJECTED {} - NO VALID IMPROVEMENT",
                rig_generation_, mutation_name(mutation.kind));
        }
    }
'''
    text = text[:start] + replacement + text[end:]
    write('src/autonomy_curriculum.cpp', text)


def patch_ppo() -> None:
    text = read('src/ppo.hpp')
    text = replace_once(text,
        "    inline constexpr std::uint32_t training_semantics_version = 0x0007'1500u;\n",
        "    inline constexpr std::uint32_t training_semantics_version = 0x0007'1501u;\n",
        'training semantics bump')
    write('src/ppo.hpp', text)


def patch_app() -> None:
    text = read('src/app.cpp')
    text = replace_once(text,
        '        enum class RigPreset : std::uint8_t {\n'
        '            humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom\n'
        '        };\n',
        '        enum class RigPreset : std::uint8_t {\n'
        '            scaffold, humanoid, biped, chicken, quadruped, crawler4, hexapod, monoped, custom\n'
        '        };\n',
        'scaffold preset enum')
    text = replace_once(text,
        '            switch (rig_preset)\n            {\n'
        '            case RigPreset::humanoid: return "HUMANOID";\n',
        '            switch (rig_preset)\n            {\n'
        '            case RigPreset::scaffold: return "SCAFFOLD";\n'
        '            case RigPreset::humanoid: return "HUMANOID";\n',
        'scaffold preset name')
    text = replace_once(text,
        '            case RigPreset::humanoid:\n'
        '                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",\n',
        '            case RigPreset::humanoid:\n'
        '                return { "LEFT HIP", "LEFT KNEE", "RIGHT HIP", "RIGHT KNEE",\n',
        'motor names anchor')
    text = replace_once(text,
        '            case RigPreset::biped:\n'
        '            case RigPreset::chicken:\n',
        '            case RigPreset::scaffold:\n'
        '            case RigPreset::biped:\n'
        '            case RigPreset::chicken:\n',
        'scaffold motor names')
    text = replace_once(text,
        '            switch (preset)\n            {\n'
        '            case RigPreset::humanoid: blueprint = sim::CreatureBlueprint::humanoid(); break;\n',
        '            switch (preset)\n            {\n'
        '            case RigPreset::scaffold: blueprint = sim::CreatureBlueprint::scaffold(); break;\n'
        '            case RigPreset::humanoid: blueprint = sim::CreatureBlueprint::humanoid(); break;\n',
        'scaffold use preset')
    old = '''                const float third = (rect.size.x - 48.0f) / 3.0f;
                if (button({ cursor, { third, 35.0f } }, "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ third + 6.0f, 0.0f }, { third, 35.0f } }, "6-LEG", input,
                    rig_preset == RigPreset::hexapod))
                    use_preset(RigPreset::hexapod);
                if (button({ cursor + Vec2{ (third + 6.0f) * 2.0f, 0.0f }, { third, 35.0f } }, "MONOPED", input,
                    rig_preset == RigPreset::monoped))
                    use_preset(RigPreset::monoped);
'''
    new = '''                const float lower_fourth = (rect.size.x - 54.0f) / 4.0f;
                if (button({ cursor, { lower_fourth, 35.0f } }, "SCAFFOLD", input,
                    rig_preset == RigPreset::scaffold))
                    use_preset(RigPreset::scaffold);
                if (button({ cursor + Vec2{ lower_fourth + 6.0f, 0.0f }, { lower_fourth, 35.0f } },
                    "CHICKEN", input, rig_preset == RigPreset::chicken))
                    use_preset(RigPreset::chicken);
                if (button({ cursor + Vec2{ (lower_fourth + 6.0f) * 2.0f, 0.0f }, { lower_fourth, 35.0f } },
                    "6-LEG", input, rig_preset == RigPreset::hexapod))
                    use_preset(RigPreset::hexapod);
                if (button({ cursor + Vec2{ (lower_fourth + 6.0f) * 3.0f, 0.0f }, { lower_fourth, 35.0f } },
                    "MONOPED", input, rig_preset == RigPreset::monoped))
                    use_preset(RigPreset::monoped);
'''
    text = replace_once(text, old, new, 'scaffold body button')
    write('src/app.cpp', text)


def patch_tests() -> None:
    text = read('tests/core_tests.cpp')
    marker = '    const sim::DuckPressProfile press_clear = sim::duck_press_profile(1.0f, 0.5f, 5.0f);\n'
    tests = r'''    const sim::CreatureBlueprint scaffold = sim::CreatureBlueprint::scaffold();
    require(scaffold.valid() && scaffold.active_motor_count == 4u
            && scaffold.paired_leg_chains(),
        "minimal scaffold is not a valid two-joint-per-side training rig");
    bool topology_mutation_seen = false;
    bool parametric_mutation_seen = false;
    for (std::uint64_t generation = 0; generation < 22u; ++generation)
    {
        const rl::RigMutationCandidate mutation = rl::evolve_rig_candidate(
            scaffold, generation);
        if (!mutation.changed)
            continue;
        require(mutation.blueprint.valid(),
            "rig evolution published a structurally invalid candidate");
        topology_mutation_seen = topology_mutation_seen || mutation.topology_changed;
        parametric_mutation_seen = parametric_mutation_seen || !mutation.topology_changed;
    }
    require(topology_mutation_seen && parametric_mutation_seen,
        "rig evolution does not produce both topology and parameter candidates");

'''
    text = replace_once(text, marker, tests + marker,
        'rig evolution tests')
    write('tests/core_tests.cpp', text)


def patch_missioncache() -> None:
    text = read('missioncache.md')
    text = replace_once(text,
        '**Status:** ACTIVE — PARAMETRIC EVOLUTION EXISTS; TOPOLOGY EVOLUTION NOT YET VERIFIED\n',
        '**Status:** IMPLEMENTED — DETERMINISTIC AND PACKAGE VALIDATION REQUIRED\n',
        'evolution mission implementation status')
    write('missioncache.md', text)


def patch_changelog() -> None:
    text = read('CHANGELOG.md')
    marker = '# Changelog\n'
    addition = r'''# Changelog

## Runner v0.7.15 — structural evolution completion

- Added a selectable minimal scaffold rig with two articulated leg joints per side and proper semantic feet.
- Added deterministic topology mutations for bone splitting, branch growth/removal, and support duplication alongside parameter evolution.
- Added a bounded 16-environment topology nursery that transfers the champion policy, adapts candidates before evaluation, accepts only stage-valid improvement, and restores the exact champion on failed application.
- Isolated the expanded evolution semantics from earlier v0.7.15 checkpoints.
'''
    text = replace_once(text, marker, addition,
        'v0.7.15 evolution changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_simulation_header()
    patch_simulation_source()
    patch_autonomy_header()
    patch_curriculum()
    patch_ppo()
    patch_app()
    patch_tests()
    patch_missioncache()
    patch_changelog()


if __name__ == '__main__':
    main()
