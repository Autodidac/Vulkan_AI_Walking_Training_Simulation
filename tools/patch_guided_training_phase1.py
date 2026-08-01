from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = read(path)
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one match, found {count}: {old[:220]!r}")
    write(path, text.replace(old, new, 1))


def insert_before(path: str, marker: str, addition: str) -> None:
    text = read(path)
    count = text.count(marker)
    if count != 1:
        raise RuntimeError(f"{path}: expected one marker, found {count}: {marker[:220]!r}")
    write(path, text.replace(marker, addition + marker, 1))


# ---- Rolling gate warm-up and longer runway ---------------------------------
replace_once(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.10f
            && stance_slip_speed < 0.065f
            && maximum_foot_clearance < 0.075f
            && std::abs(torso_turn_speed) > 0.20f;
    }
''',
    '''    [[nodiscard]] inline bool foot_pivot_rolling_motion(float root_speed,
        bool left_supported, bool right_supported, float stance_slip_speed,
        float maximum_foot_clearance, float torso_turn_speed) noexcept
    {
        return left_supported && right_supported
            && std::abs(root_speed) > 0.10f
            && stance_slip_speed < 0.065f
            && maximum_foot_clearance < 0.075f
            && std::abs(torso_turn_speed) > 0.20f;
    }

    inline constexpr float rolling_gate_activation_seconds = 1.35f;
    inline constexpr float rolling_gate_warmup_end_seconds = 2.60f;

    [[nodiscard]] inline bool rolling_gate_active(float elapsed_seconds) noexcept
    {
        return elapsed_seconds >= rolling_gate_activation_seconds;
    }

    [[nodiscard]] inline float body_rolling_limit(CourseStage stage,
        float elapsed_seconds) noexcept
    {
        if (elapsed_seconds < rolling_gate_warmup_end_seconds)
            return stage == CourseStage::balance ? 0.78f : 0.55f;
        return stage == CourseStage::balance ? 0.55f : 0.32f;
    }

    [[nodiscard]] inline float head_contact_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.38f : 0.24f;
    }

    [[nodiscard]] inline float foot_pivot_rolling_limit(float elapsed_seconds) noexcept
    {
        return elapsed_seconds < rolling_gate_warmup_end_seconds ? 0.68f : 0.42f;
    }
''')

replace_once(
    "src/simulation.hpp",
    '''    inline constexpr int course_safe_runway_markers = 3;
''',
    '''    inline constexpr int course_safe_runway_markers = 5;
''')

# ---- Multi-foot semantic support groups -------------------------------------
replace_once(
    "src/simulation.hpp",
    '''        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };

        [[nodiscard]] static CreatureBlueprint chicken();
''',
    '''        std::uint16_t left_contact_node{ 4 };
        std::uint16_t right_contact_node{ 6 };
        std::vector<std::uint16_t> additional_left_contact_nodes{};
        std::vector<std::uint16_t> additional_right_contact_nodes{};

        [[nodiscard]] bool is_left_support_seed(std::size_t node) const noexcept
        {
            if (node == left_contact_node)
                return true;
            return std::ranges::find(additional_left_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_left_contact_nodes.end();
        }
        [[nodiscard]] bool is_right_support_seed(std::size_t node) const noexcept
        {
            if (node == right_contact_node)
                return true;
            return std::ranges::find(additional_right_contact_nodes,
                static_cast<std::uint16_t>(node)) != additional_right_contact_nodes.end();
        }
        [[nodiscard]] bool is_support_seed(std::size_t node) const noexcept
        {
            return is_left_support_seed(node) || is_right_support_seed(node);
        }
        [[nodiscard]] std::size_t support_seed_count() const noexcept
        {
            return 2u + additional_left_contact_nodes.size()
                + additional_right_contact_nodes.size();
        }

        [[nodiscard]] static CreatureBlueprint chicken();
''')

# Replace the fake two-legged quadruped with four independently driven legs.
old_quadruped = '''    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.05f }, { 1.55f, 2.08f }, { 2.35f, 2.42f },
            { -0.25f, 1.10f }, { -0.48f, 0.24f },
            { 1.72f, 1.08f }, { 1.92f, 0.24f }, { -1.05f, 2.35f }
        };
        result.radii = { 0.29f, 0.30f, 0.25f, 0.18f, 0.16f, 0.18f, 0.16f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.95f }, { 0, 7, 0.0f, 0.82f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 1, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 0, 1, 5 }, MotorConstraint{ 1, 5, 6 }
        };
        result.rebuild_rest_lengths();
        calibrate_grounded_defaults(result, 34.0f, 50.0f, 0.046f, 0.052f);
        return result;
    }
'''
new_quadruped = '''    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        // A real planar quadruped: four separate legs, slightly staggered in x
        // so the near/far pairs remain visible in a side view. The two support
        // channels are diagonal pairs, allowing a stable trot with four policy
        // outputs instead of pretending that two articulated legs are four.
        result.nodes = {
            { 0.0f, 1.58f }, { 1.48f, 1.62f }, { 2.22f, 1.88f }, { -0.88f, 1.78f },
            { -0.46f, 0.24f }, { 0.08f, 0.28f },
            { 1.42f, 0.24f }, { 1.96f, 0.28f }
        };
        result.radii = { 0.30f, 0.31f, 0.25f, 0.15f, 0.16f, 0.15f, 0.16f, 0.15f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.95f }, { 0, 3, 0.0f, 0.84f },
            { 0, 4, 0.0f, 1.0f }, { 0, 5, 0.0f, 0.98f },
            { 1, 6, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.98f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.additional_left_contact_nodes = { 7 };
        result.additional_right_contact_nodes = { 5 };
        result.motors = {
            MotorConstraint{ 1, 0, 4 }, MotorConstraint{ 1, 0, 5 },
            MotorConstraint{ 0, 1, 6 }, MotorConstraint{ 0, 1, 7 }
        };
        result.rebuild_rest_lengths();
        calibrate_obstacle_legs(result, 52.0f);
        return result;
    }
'''
replace_once("src/simulation.cpp", old_quadruped, new_quadruped)

replace_once(
    "src/simulation.cpp",
    '''        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.motors = {
''',
    '''        result.left_contact_node = 3;
        result.right_contact_node = 5;
        result.additional_left_contact_nodes = { 6 };
        result.additional_right_contact_nodes = { 4 };
        result.motors = {
''')

replace_once(
    "src/simulation.cpp",
    '''        result.left_contact_node = 3;
        result.right_contact_node = 6;
        result.motors = {
''',
    '''        result.left_contact_node = 3;
        result.right_contact_node = 6;
        result.additional_left_contact_nodes = { 6, 7 };
        result.additional_right_contact_nodes = { 4, 5, 8 };
        result.motors = {
''')

# Structural validation and signature must include every support seed.
replace_once(
    "src/simulation.cpp",
    '''        if (!semantic_valid(root_node) || !semantic_valid(torso_node) || !semantic_valid(head_node)
            || !semantic_valid(left_contact_node) || !semantic_valid(right_contact_node))
            return false;
''',
    '''        if (!semantic_valid(root_node) || !semantic_valid(torso_node) || !semantic_valid(head_node)
            || !semantic_valid(left_contact_node) || !semantic_valid(right_contact_node))
            return false;
        for (const std::uint16_t node : additional_left_contact_nodes)
        {
            if (!semantic_valid(node) || node == left_contact_node)
                return false;
        }
        for (const std::uint16_t node : additional_right_contact_nodes)
        {
            if (!semantic_valid(node) || node == right_contact_node)
                return false;
        }
''')

replace_once(
    "src/simulation.cpp",
    '''        add_u64(left_contact_node); add_u64(right_contact_node);
        for (std::size_t index = 0; index < nodes.size(); ++index)
''',
    '''        add_u64(left_contact_node); add_u64(right_contact_node);
        add_u64(additional_left_contact_nodes.size());
        for (const std::uint16_t node : additional_left_contact_nodes) add_u64(node);
        add_u64(additional_right_contact_nodes.size());
        for (const std::uint16_t node : additional_right_contact_nodes) add_u64(node);
        for (std::size_t index = 0; index < nodes.size(); ++index)
''')

# EPOCHRIG 3 persists multi-foot semantics while still loading versions 1 and 2.
replace_once("src/simulation.cpp", '            output << "EPOCHRIG 2\\n";\n',
             '            output << "EPOCHRIG 3\\n";\n')
replace_once(
    "src/simulation.cpp",
    '''            output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
                << left_contact_node << ' ' << right_contact_node << '\n';
            output << std::setprecision(9);
''',
    '''            output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
                << left_contact_node << ' ' << right_contact_node << '\n';
            output << "L " << additional_left_contact_nodes.size();
            for (const std::uint16_t node : additional_left_contact_nodes) output << ' ' << node;
            output << '\n';
            output << "R " << additional_right_contact_nodes.size();
            for (const std::uint16_t node : additional_right_contact_nodes) output << ' ' << node;
            output << '\n';
            output << std::setprecision(9);
''')

replace_once(
    "src/simulation.cpp",
    '''        if (!input || magic != "EPOCHRIG" || (version != 1 && version != 2)
            || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)
''',
    '''        if (!input || magic != "EPOCHRIG" || (version != 1 && version != 2 && version != 3)
            || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)
''')

replace_once(
    "src/simulation.cpp",
    '''        if (version >= 2)
        {
            char semantic_tag{};
            input >> semantic_tag >> result.root_node >> result.torso_node >> result.head_node
                >> result.left_contact_node >> result.right_contact_node;
            if (!input || semantic_tag != 'S')
            {
                error = "Invalid rig semantic-node data.";
                return humanoid();
            }
        }

        result.nodes.reserve(node_count);
''',
    '''        if (version >= 2)
        {
            char semantic_tag{};
            input >> semantic_tag >> result.root_node >> result.torso_node >> result.head_node
                >> result.left_contact_node >> result.right_contact_node;
            if (!input || semantic_tag != 'S')
            {
                error = "Invalid rig semantic-node data.";
                return humanoid();
            }
        }
        if (version >= 3)
        {
            auto read_supports = [&](char expected, std::vector<std::uint16_t>& nodes)
            {
                char tag{};
                std::size_t count{};
                input >> tag >> count;
                if (!input || tag != expected || count > node_count)
                    return false;
                nodes.resize(count);
                for (std::uint16_t& node : nodes)
                {
                    input >> node;
                    if (!input || node >= node_count)
                        return false;
                }
                return true;
            };
            if (!read_supports('L', result.additional_left_contact_nodes)
                || !read_supports('R', result.additional_right_contact_nodes))
            {
                error = "Invalid multi-foot support data.";
                return humanoid();
            }
        }

        result.nodes.reserve(node_count);
''')

# Seed the support-cluster search from all feet in a gait phase.
old_cluster_start = '''        const float contact_height = blueprint_.nodes[contact_node].y;
        if (blueprint_.nodes[particle_index].y > contact_height + 0.18f)
            return false;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> queue{};
        std::size_t head = 0;
        std::size_t tail = 0;
        visited[contact_node] = true;
        queue[tail++] = contact_node;
'''
new_cluster_start = '''        float contact_height = blueprint_.nodes[contact_node].y;
        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> queue{};
        std::size_t head = 0;
        std::size_t tail = 0;
        auto add_seed = [&](std::uint16_t seed)
        {
            if (seed >= blueprint_.nodes.size() || visited[seed])
                return;
            visited[seed] = true;
            queue[tail++] = seed;
            contact_height = std::max(contact_height, blueprint_.nodes[seed].y);
        };
        add_seed(contact_node);
        if (contact_node == blueprint_.left_contact_node)
        {
            for (const std::uint16_t seed : blueprint_.additional_left_contact_nodes)
                add_seed(seed);
        }
        else if (contact_node == blueprint_.right_contact_node)
        {
            for (const std::uint16_t seed : blueprint_.additional_right_contact_nodes)
                add_seed(seed);
        }
        if (blueprint_.nodes[particle_index].y > contact_height + 0.18f)
            return false;
'''
replace_once("src/simulation.cpp", old_cluster_start, new_cluster_start)

# Slight start-of-episode grace: penalties remain active, but hard rejection starts
# after the body has settled and control has begun ramping in.
replace_once(
    "src/simulation.cpp",
    '''        const float rolling_limit = course_stage_ == CourseStage::balance ? 0.55f : 0.32f;
        if (body_rolling_seconds_ > rolling_limit || head_contact_seconds_ > 0.24f)
            invalidate(InvalidMotion::body_rolling);
''',
    '''        if (!rolling_gate_active(elapsed_seconds_))
        {
            body_rolling_seconds_ = 0.0f;
            head_contact_seconds_ = 0.0f;
        }
        else if (body_rolling_seconds_ > body_rolling_limit(course_stage_, elapsed_seconds_)
            || head_contact_seconds_ > head_contact_limit(elapsed_seconds_))
        {
            invalidate(InvalidMotion::body_rolling);
        }
''')

replace_once(
    "src/simulation.cpp",
    '''        if (foot_pivot_rolling_seconds_ > 0.42f)
            invalidate(InvalidMotion::foot_pivot_rolling);
''',
    '''        if (!rolling_gate_active(elapsed_seconds_))
            foot_pivot_rolling_seconds_ = 0.0f;
        else if (foot_pivot_rolling_seconds_ > foot_pivot_rolling_limit(elapsed_seconds_))
            invalidate(InvalidMotion::foot_pivot_rolling);
''')

# Give the longer runway enough evaluation and episode time to reach hazards.
replace_once(
    "src/ppo_parallel.cpp",
    '''                    constexpr std::size_t evaluation_agents = 6;
                    constexpr int maximum_steps = 900;
''',
    '''                    constexpr std::size_t evaluation_agents = 6;
                    const int maximum_steps = static_cast<std::uint8_t>(current_stage)
                        >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 2400 : 1200;
''')

replace_once(
    "src/autonomy_curriculum.cpp",
    '''        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 1500 : 900;
''',
    '''        const int maximum_steps = static_cast<std::uint8_t>(stage)
            >= static_cast<std::uint8_t>(sim::CourseStage::hurdles) ? 2400 : 1200;
''')

replace_once(
    "src/simulation.cpp",
    '''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : static_cast<std::uint8_t>(course_stage_) >= static_cast<std::uint8_t>(CourseStage::hurdles)
                ? 32.0f : 24.0f;
''',
    '''        const float timeout = course_stage_ == CourseStage::balance ? 12.0f
            : static_cast<std::uint8_t>(course_stage_) >= static_cast<std::uint8_t>(CourseStage::hurdles)
                ? 48.0f : 30.0f;
''')

# ---- Publish one real worker environment for picture-in-picture -------------
replace_once(
    "src/autonomy.hpp",
    '''        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }
        [[nodiscard]] const sim::CreatureBlueprint& blueprint() const noexcept { return live_blueprint_; }
''',
    '''        [[nodiscard]] const sim::Environment& preview() const noexcept { return live_.preview(); }
        [[nodiscard]] const sim::Environment& training_preview() const noexcept
        {
            return cached_training_preview_;
        }
        [[nodiscard]] bool has_training_preview() const noexcept { return cached_has_training_preview_; }
        [[nodiscard]] const sim::CreatureBlueprint& blueprint() const noexcept { return live_blueprint_; }
''')

replace_once(
    "src/autonomy.hpp",
    '''            bool has_best{};
            std::uint64_t serial{};
''',
    '''            bool has_best{};
            sim::Environment training_preview{};
            bool has_training_preview{};
            std::uint64_t serial{};
''')

replace_once(
    "src/autonomy.hpp",
    '''        PpoTrainer live_;
        sim::CreatureBlueprint live_blueprint_{};
        PublishedSnapshot published_{};
''',
    '''        PpoTrainer live_;
        sim::CreatureBlueprint live_blueprint_{};
        sim::Environment cached_training_preview_{};
        bool cached_has_training_preview_{};
        PublishedSnapshot published_{};
''')

replace_once(
    "src/autonomy_runtime.cpp",
    '''        cached_has_best_ = snapshot.has_best;
        applied_serial_ = snapshot.serial;
''',
    '''        cached_has_best_ = snapshot.has_best;
        cached_training_preview_ = std::move(snapshot.training_preview);
        cached_has_training_preview_ = snapshot.has_training_preview;
        applied_serial_ = snapshot.serial;
''')

replace_once(
    "src/autonomy_persistence.cpp",
    '''        snapshot.has_best = worker_.has_best_policy();
        snapshot.status.enabled = enabled_.load(std::memory_order_relaxed);
''',
    '''        snapshot.has_best = worker_.has_best_policy();
        const std::span<const sim::Environment> environments = worker_.environments();
        if (!environments.empty())
        {
            const sim::Environment* representative = &environments.front();
            float representative_score = -1.0e9f;
            for (const sim::Environment& environment : environments)
            {
                const float score = (environment.valid_motion() ? 1000.0f : 0.0f)
                    + environment.distance_travelled() * 10.0f + environment.elapsed_seconds();
                if (score > representative_score)
                {
                    representative = &environment;
                    representative_score = score;
                }
            }
            snapshot.training_preview = *representative;
            snapshot.has_training_preview = true;
        }
        snapshot.status.enabled = enabled_.load(std::memory_order_relaxed);
''')

# ---- UI: correct motor names, semantic feet, and actual-training PIP ---------
replace_once(
    "src/app.cpp",
    '''            case RigPreset::quadruped:
                return { "REAR HIP", "REAR KNEE", "FRONT SHOULDER", "FRONT KNEE" };
''',
    '''            case RigPreset::quadruped:
                return { "REAR NEAR LEG", "REAR FAR LEG", "FRONT NEAR LEG", "FRONT FAR LEG" };
''')

# Both live and Rig Lab render every support seed as a foot.
replace_once(
    "src/app.cpp",
    '''                const bool primary_foot = index == rig.left_contact_node || index == rig.right_contact_node;
                if (primary_foot)
''',
    '''                const bool primary_foot = rig.is_support_seed(index);
                if (primary_foot)
''')

replace_once(
    "src/app.cpp",
    '''                if (index == blueprint.left_contact_node || index == blueprint.right_contact_node)
                    color = leg;
''',
    '''                if (blueprint.is_support_seed(index))
                    color = leg;
''')

# Preserve support lists when deleting/remapping custom rig nodes.
replace_once(
    "src/app.cpp",
    '''            remap(blueprint.root_node); remap(blueprint.torso_node); remap(blueprint.head_node);
            remap(blueprint.left_contact_node); remap(blueprint.right_contact_node);
            for (sim::MotorConstraint& item : blueprint.motors)
''',
    '''            remap(blueprint.root_node); remap(blueprint.torso_node); remap(blueprint.head_node);
            remap(blueprint.left_contact_node); remap(blueprint.right_contact_node);
            auto remap_supports = [removed](std::vector<std::uint16_t>& nodes)
            {
                std::erase(nodes, removed);
                for (std::uint16_t& node : nodes)
                {
                    if (node > removed)
                        --node;
                }
            };
            remap_supports(blueprint.additional_left_contact_nodes);
            remap_supports(blueprint.additional_right_contact_nodes);
            for (sim::MotorConstraint& item : blueprint.motors)
''')

# New autosave namespace is required because support semantics and rolling rules changed.
replace_once("src/app.cpp", '        std::filesystem::path autosave_policy_path{ "epochrunner-v064-autosave.eppo" };\n',
             '        std::filesystem::path autosave_policy_path{ "epochrunner-v065-autosave.eppo" };\n')
replace_once("src/app.cpp", '        std::filesystem::path autosave_rig_path{ "epochrunner-v064-evolved.epochrig" };\n',
             '        std::filesystem::path autosave_rig_path{ "epochrunner-v065-evolved.epochrig" };\n')
replace_once("src/app.cpp", '        std::filesystem::path autosave_state_path{ "epochrunner-v064-autonomy.state" };\n',
             '        std::filesystem::path autosave_state_path{ "epochrunner-v065-autonomy.state" };\n')

insert_before(
    "src/app.cpp",
    '''        void draw_live_panel(Rect rect, const InputState& input)
''',
    '''        void draw_training_pip(Rect rect)
        {
            add_rounded_rect(canvas, rect, 10.0f, rgb(0x071019, 0.98f), accent_dim, 1.5f);
            add_text(canvas, rect.position + Vec2{ 13.0f, 10.0f },
                "RAW TRAINING SAMPLE", 1.03f, accent);
            if (!trainer.has_training_preview())
            {
                add_text_fit(canvas, rect.position + Vec2{ 13.0f, 44.0f },
                    "WAITING FOR FIRST ROLLOUT", 1.02f, muted, rect.size.x - 26.0f);
                return;
            }

            const sim::Environment& environment = trainer.training_preview();
            const Rect inner{ rect.position + Vec2{ 9.0f, 35.0f },
                { rect.size.x - 18.0f, rect.size.y - 44.0f } };
            const auto& particles = environment.particles();
            const auto& rig = environment.blueprint();
            if (particles.empty() || rig.root_node >= particles.size())
                return;
            const float camera = particles[rig.root_node].position.x + 0.55f;
            const float scale = std::clamp(inner.size.y / 5.7f, 28.0f, 43.0f);

            std::vector<Vec2> ground{};
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

            for (const sim::CourseFeature& feature : environment.course_features())
            {
                const Vec2 point = world_to_screen(feature.center, inner, camera, scale, 0.82f);
                if (point.x < inner.position.x - 20.0f
                    || point.x > inner.position.x + inner.size.x + 20.0f)
                    continue;
                if (feature.kind == sim::CourseFeatureKind::rock
                    || feature.kind == sim::CourseFeatureKind::moving_hazard
                    || feature.kind == sim::CourseFeatureKind::projectile)
                {
                    canvas.circle(point, feature.radius * scale,
                        feature.kind == sim::CourseFeatureKind::projectile ? danger : yellow, 18);
                }
                else
                {
                    const Vec2 minimum = world_to_screen(feature.center - feature.half_extent,
                        inner, camera, scale, 0.82f);
                    const Vec2 maximum = world_to_screen(feature.center + feature.half_extent,
                        inner, camera, scale, 0.82f);
                    canvas.quad({ minimum.x, maximum.y }, { maximum.x, minimum.y }, yellow);
                }
            }
            draw_creature(environment, inner, camera, scale);
            add_text_fit(canvas, rect.position + Vec2{ 13.0f, rect.size.y - 25.0f },
                std::format("{:.2f} M  /  {}", environment.distance_travelled(),
                    sim::invalid_motion_name(environment.invalid_reason())),
                0.88f, environment.valid_motion() ? green : danger, rect.size.x - 26.0f, 0.72f);
        }

''')

replace_once(
    "src/app.cpp",
    '''            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },
                "LIVE SAND-SIM ENEMY CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE",
                1.05f, muted, overlay_width, 1.00f);
''',
    '''            add_text_fit(canvas, viewport.position + Vec2{ 24.0f, viewport.size.y - 38.0f },
                "LIVE SAND-SIM ENEMY CONTROLLER   v" EPOCHRUNNER_VERSION "   BACKGROUND TRAINING ACTIVE",
                1.05f, muted, overlay_width, 1.00f);

            const float pip_width = std::clamp(viewport.size.x * 0.34f, 300.0f, 390.0f);
            const float pip_height = std::clamp(viewport.size.y * 0.27f, 190.0f, 245.0f);
            draw_training_pip({
                { viewport.position.x + viewport.size.x - pip_width - 18.0f,
                  viewport.position.y + 18.0f },
                { pip_width, pip_height }
            });
''')

# ---- Deterministic tests -----------------------------------------------------
replace_once(
    "tests/core_tests.cpp",
    '''    require(std::abs(sim::course_marker_distance_m(4) - 32.0f) < 0.0001f,
        "course mile-marker spacing is not shared with obstacle scheduling");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 3)
            == sim::CourseFeatureKind::rock
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 4)
            == sim::CourseFeatureKind::hurdle
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::overhead_bar
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 6)
            == sim::CourseFeatureKind::moving_hazard
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 7)
            == sim::CourseFeatureKind::projectile,
        "moving-hazard lesson does not schedule every obstacle class on consecutive markers");
    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 24.0f,
        "course does not provide enough safe runway before the first obstacle marker");
''',
    '''    require(std::abs(sim::course_marker_distance_m(4) - 32.0f) < 0.0001f,
        "course mile-marker spacing is not shared with obstacle scheduling");
    require(sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 5)
            == sim::CourseFeatureKind::rock
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 6)
            == sim::CourseFeatureKind::hurdle
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 7)
            == sim::CourseFeatureKind::overhead_bar
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 8)
            == sim::CourseFeatureKind::moving_hazard
        && sim::scheduled_course_feature(sim::CourseStage::moving_hazards, 9)
            == sim::CourseFeatureKind::projectile,
        "moving-hazard lesson does not schedule every obstacle class on consecutive markers");
    require(sim::course_marker_distance_m(sim::course_safe_runway_markers) >= 40.0f,
        "course does not provide the requested longer learning runway");
''')

insert_before(
    "tests/core_tests.cpp",
    '''    require(sim::hazard_approach_weight(0.40f) == 1.0f,
''',
    '''    require(!sim::rolling_gate_active(1.0f)
            && sim::rolling_gate_active(sim::rolling_gate_activation_seconds),
        "rolling hard gate does not provide a bounded startup settle window");
    require(sim::body_rolling_limit(sim::CourseStage::walk, 1.8f)
            > sim::body_rolling_limit(sim::CourseStage::walk, 4.0f),
        "rolling gate does not become strict after startup");
    require(sim::foot_pivot_rolling_limit(1.8f) > sim::foot_pivot_rolling_limit(4.0f),
        "orange-foot rolling gate does not become strict after startup");
''')

replace_once(
    "tests/core_tests.cpp",
    '''    const sim::CreatureBlueprint crawler4 = sim::CreatureBlueprint::crawler4();
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    require(crawler4.nodes.size() >= 9 && crawler4.bones.size() >= 10,
        "four-legged crawler geometry is incomplete");
    require(hexapod.nodes.size() >= 12 && hexapod.bones.size() >= 16,
        "six-legged hexapod geometry is incomplete");
''',
    '''    const sim::CreatureBlueprint quadruped = sim::CreatureBlueprint::quadruped();
    const sim::CreatureBlueprint crawler4 = sim::CreatureBlueprint::crawler4();
    const sim::CreatureBlueprint hexapod = sim::CreatureBlueprint::hexapod();
    require(quadruped.support_seed_count() == 4,
        "quadruped is still semantically a two-foot biped");
    require(quadruped.additional_left_contact_nodes.size() == 1
            && quadruped.additional_right_contact_nodes.size() == 1,
        "quadruped diagonal support pairs are missing");
    require(std::abs(quadruped.nodes[4].x - quadruped.nodes[5].x) > 0.30f
            && std::abs(quadruped.nodes[6].x - quadruped.nodes[7].x) > 0.30f,
        "quadruped near/far legs overlap in the side-view geometry");
    require(crawler4.nodes.size() >= 9 && crawler4.bones.size() >= 10
            && crawler4.support_seed_count() == 4,
        "four-legged crawler geometry or support semantics are incomplete");
    require(hexapod.nodes.size() >= 12 && hexapod.bones.size() >= 16
            && hexapod.support_seed_count() == 6,
        "six-legged hexapod geometry or support semantics are incomplete");
''')

print("Applied guided-training phase 1")
