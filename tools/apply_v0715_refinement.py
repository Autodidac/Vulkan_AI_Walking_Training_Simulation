from pathlib import Path
import re

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


def replace_regex_once(text: str, pattern: str, replacement: str, label: str) -> str:
    result, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise RuntimeError(f'{label}: expected one match, found {count}')
    return result


def patch_header() -> None:
    text = read('src/simulation.hpp')
    anchor = '''    [[nodiscard]] inline bool qualifies_crossing_step(int previous_side,
'''
    addition = '''    inline constexpr float moving_contact_slop_m = 0.032f;
    inline constexpr float moving_contact_release_speed_mps = 0.24f;

    [[nodiscard]] inline bool planted_contact_persists(bool contact_latched,
        bool semantic_support, bool static_support, float separation,
        float upward_speed, bool release_requested) noexcept
    {
        if (!contact_latched || !semantic_support || release_requested)
            return false;
        if (static_support)
            return true;
        return separation > 0.0025f
            && separation <= moving_contact_slop_m
            && upward_speed <= moving_contact_release_speed_mps;
    }

''' + anchor
    text = replace_once(text, anchor, addition,
        'persistent contact manifold helper')

    member_anchor = '''        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
'''
    member_addition = '''        std::vector<Particle> particles_{};
        std::vector<std::uint8_t> support_contact_latch_{};
        std::vector<float> support_contact_anchor_x_{};
        std::vector<CourseFeature> course_features_{};
'''
    text = replace_once(text, member_anchor, member_addition,
        'support contact manifold storage')
    write('src/simulation.hpp', text)


def patch_simulation() -> None:
    text = read('src/simulation.cpp')
    forced_pin = '''        auto pin_support = [&](std::size_t node)
        {
            if (node >= particles_.size() || node >= blueprint_.nodes.size())
                return;
            Particle& support = particles_[node];
            const float authored_x = blueprint_.nodes[node].x;
            support.position.x = lerp(support.position.x, authored_x, 0.72f);
            support.position.y = ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.previous = support.position;
            support.grounded = true;
        };
        pin_support(blueprint_.left_contact_node);
        pin_support(blueprint_.right_contact_node);
        for (const std::uint16_t node : blueprint_.additional_left_contact_nodes)
            pin_support(node);
        for (const std::uint16_t node : blueprint_.additional_right_contact_nodes)
            pin_support(node);

'''
    text = replace_once(text, forced_pin, '',
        'remove authored semantic-foot pinning')

    reset_anchor = '''        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
'''
    reset_addition = '''        support_contact_latch_.assign(particles_.size(), 0u);
        support_contact_anchor_x_.resize(particles_.size());
        for (std::size_t index = 0; index < particles_.size(); ++index)
            support_contact_anchor_x_[index] = particles_[index].position.x;
        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
'''
    text = replace_once(text, reset_anchor, reset_addition,
        'reset support contact manifold')

    old_ground = '''    void Environment::solve_ground(float dt) noexcept
    {
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            particle.grounded = false;
            const bool traction_contact = contact_cluster_contains(
                blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const float firmness = terrain_firmness_at(particle.position.x);
            const float looseness = terrain_looseness_at(particle.position.x);
            const float burial_allowance = stage_uses_deformable_terrain(course_stage_)
                ? (traction_contact ? (1.0f - firmness) * 0.055f
                    : std::min(particle.radius * 0.78f,
                        (1.0f - firmness + looseness * 0.45f) * 0.18f))
                : 0.0f;
            const float minimum_y = ground_height_at(particle.position.x)
                + ground_contact_offset(traction_contact, particle.radius) - burial_allowance;
            if (particle.position.y <= minimum_y + 0.0025f)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact)
                {
                    const bool left_toe = blueprint_.additional_left_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_left_contact_nodes[1];
                    const bool right_toe = blueprint_.additional_right_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_right_contact_nodes[1];
                    const bool static_lesson = course_stage_ == CourseStage::balance
                        || course_stage_ == CourseStage::duck_press;
                    retention = foot_friction_retention(velocity.x,
                        firmness, looseness, static_lesson,
                        left_toe || right_toe);
                }
                particle.previous.x = particle.position.x - velocity.x * retention * dt;
                if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
        }
    }
'''
    new_ground = '''    void Environment::solve_ground(float dt) noexcept
    {
        const float safe_dt = std::max(dt, 1.0e-5f);
        if (support_contact_latch_.size() != particles_.size())
            support_contact_latch_.assign(particles_.size(), 0u);
        if (support_contact_anchor_x_.size() != particles_.size())
        {
            support_contact_anchor_x_.resize(particles_.size());
            for (std::size_t index = 0; index < particles_.size(); ++index)
                support_contact_anchor_x_[index] = particles_[index].position.x;
        }

        float root_upward_speed = 0.0f;
        if (valid_node(blueprint_.root_node))
        {
            const Particle& root = particles_[blueprint_.root_node];
            root_upward_speed = (root.position.y - root.previous.y) / safe_dt;
        }
        const bool powered_release = powered_joint_launch(
            course_stage_, root_upward_speed, action_change_energy_);
        const bool static_support = course_stage_ == CourseStage::balance
            || course_stage_ == CourseStage::duck_press;

        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            Particle& particle = particles_[index];
            const bool was_grounded = particle.grounded;
            const Vec2 velocity = (particle.position - particle.previous) / safe_dt;
            particle.grounded = false;
            const bool traction_contact = contact_cluster_contains(
                blueprint_.left_contact_node, index)
                || contact_cluster_contains(blueprint_.right_contact_node, index);
            const bool semantic_support = blueprint_.is_support_seed(index);
            const float firmness = terrain_firmness_at(particle.position.x);
            const float looseness = terrain_looseness_at(particle.position.x);
            const float burial_allowance = stage_uses_deformable_terrain(course_stage_)
                ? (traction_contact ? (1.0f - firmness) * 0.055f
                    : std::min(particle.radius * 0.78f,
                        (1.0f - firmness + looseness * 0.45f) * 0.18f))
                : 0.0f;
            const float minimum_y = ground_height_at(particle.position.x)
                + ground_contact_offset(traction_contact, particle.radius) - burial_allowance;
            const float separation = particle.position.y - minimum_y;
            const bool release_requested = semantic_support && powered_release
                && velocity.y > 0.0f;
            const bool contact_latched = semantic_support
                && (was_grounded || support_contact_latch_[index] != 0u);
            const bool actual_contact = separation <= 0.0025f
                && !release_requested;
            const bool persistent_contact = planted_contact_persists(
                contact_latched, semantic_support, static_support,
                separation, velocity.y, release_requested);
            if (actual_contact || persistent_contact)
            {
                if (semantic_support && support_contact_latch_[index] == 0u)
                    support_contact_anchor_x_[index] = particle.position.x;
                if (semantic_support && static_support)
                {
                    particle.position.x = lerp(particle.position.x,
                        support_contact_anchor_x_[index], 0.72f);
                    particle.position.y = ground_height_at(particle.position.x)
                        + ground_contact_offset(true, particle.radius);
                    particle.previous = particle.position;
                    particle.grounded = true;
                    support_contact_latch_[index] = 1u;
                    continue;
                }

                particle.position.y = minimum_y;
                particle.grounded = true;
                if (semantic_support)
                {
                    support_contact_latch_[index] = 1u;
                    support_contact_anchor_x_[index] = particle.position.x;
                }
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact)
                {
                    const bool left_toe = blueprint_.additional_left_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_left_contact_nodes[1];
                    const bool right_toe = blueprint_.additional_right_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_right_contact_nodes[1];
                    retention = foot_friction_retention(velocity.x,
                        firmness, looseness, false,
                        left_toe || right_toe);
                }
                particle.previous.x = particle.position.x
                    - velocity.x * retention * safe_dt;
                if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y
                        + velocity.y * safe_dt * 0.05f;
            }
            else if (semantic_support && !static_support
                && (release_requested
                    || separation > moving_contact_slop_m
                    || velocity.y > moving_contact_release_speed_mps))
            {
                support_contact_latch_[index] = 0u;
            }
        }
    }
'''
    text = replace_once(text, old_ground, new_ground,
        'static-friction contact manifold')

    old_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
'''
    new_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. Only the ground solver may establish
            // support; static friction retains its measured contact anchor while
            // moving stages release through bounded distance and speed gates.
            solve_ground(dt);
'''
    text = replace_once(text, old_comment, new_comment,
        'physical static-friction comment')
    write('src/simulation.cpp', text)


def patch_tests() -> None:
    text = read('tests/core_tests.cpp')
    anchor = '''        static bool guided_squat_is_valid(Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            environment.elapsed_seconds_ = 6.0f;
            environment.duck_press_contact_seen_ = true;
            for (int iteration = 0; iteration < 48; ++iteration)
            {
                environment.stabilize_duck_posture();
                environment.solve_ground(1.0f / 60.0f);
            }
            const CrouchPostureEvidence evidence =
                environment.current_crouch_posture();
            return crouch_posture_qualified(evidence)
                && evidence.pelvis_drop >= 0.30f
                && evidence.left_knee_flex >= 0.16f
                && evidence.right_knee_flex >= 0.16f
                && evidence.torso_pitch <= 0.55f;
        }

'''
    addition = anchor + '''        static bool crouch_guide_preserves_support_dynamics(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.50f);
            environment.elapsed_seconds_ = 6.0f;
            environment.duck_press_contact_seen_ = true;
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            Particle& support = environment.particles_[
                environment.blueprint_.left_contact_node];
            support.position.x += 0.093f;
            support.position.y += 0.017f;
            support.previous.x = support.position.x - 0.041f;
            support.previous.y = support.position.y + 0.006f;
            support.grounded = false;
            const Vec2 position_before = support.position;
            const Vec2 previous_before = support.previous;
            const bool grounded_before = support.grounded;
            environment.stabilize_duck_posture();
            return length(support.position - position_before) < 1.0e-7f
                && length(support.previous - previous_before) < 1.0e-7f
                && support.grounded == grounded_before;
        }

        static bool static_friction_anchor_is_physical(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::duck_press, 0.25f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            constexpr float dt = 1.0f / 60.0f;
            const std::size_t node = environment.blueprint_.left_contact_node;
            Particle& support = environment.particles_[node];
            const float ground = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = ground;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            const float anchor_x = support.position.x;

            support.position += Vec2{ 0.14f, 0.55f };
            support.previous = support.position - Vec2{ 0.05f, 0.20f };
            support.grounded = false;
            environment.solve_ground(dt);
            const bool static_held = support.grounded
                && std::abs(support.position.x - anchor_x) < 0.05f
                && std::abs(support.position.y
                    - (environment.ground_height_at(support.position.x)
                        + ground_contact_offset(true, support.radius))) < 1.0e-6f
                && length(support.position - support.previous) < 1.0e-7f;

            environment.set_course(CourseStage::uneven, 0.25f);
            const float moving_ground = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = moving_ground;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            support.position += Vec2{ 0.0f, 0.040f };
            support.previous = support.position - Vec2{ 0.0f, 0.42f * dt };
            support.grounded = false;
            environment.solve_ground(dt);
            return static_held && !support.grounded;
        }

'''
    text = replace_once(text, anchor, addition,
        'crouch guide and static-friction test access')

    test_anchor = '''    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

'''
    test_addition = test_anchor + '''    require(sim::planted_contact_persists(
            true, true, true, 0.55f, 2.0f, false),
        "static support manifold did not retain a measured ground contact");
    require(!sim::planted_contact_persists(
            true, true, false, 0.040f, 0.42f, false),
        "moving foot remained magnetically planted");
    require(!sim::planted_contact_persists(
            true, true, true, 0.018f, 0.08f, true),
        "explicit powered release was ignored");

    sim::Environment unpinned_squat(humanoid_rig, 1401);
    require(sim::EnvironmentTestAccess::crouch_guide_preserves_support_dynamics(
            unpinned_squat),
        "crouch curriculum directly pins semantic foot coordinates or support state");
    sim::Environment static_anchor(humanoid_rig, 1402);
    require(sim::EnvironmentTestAccess::static_friction_anchor_is_physical(
            static_anchor),
        "ground solver failed measured static-friction anchoring or moving release");

'''
    text = replace_once(text, test_anchor, test_addition,
        'static-friction physical crouch acceptance')
    write('tests/core_tests.cpp', text)


def patch_documents() -> None:
    text = read('missioncache.md')
    text = replace_regex_once(text,
        r'(### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — GROUND-SOLVER STATIC-FRICTION MANIFOLD; FULL VALIDATION REQUIRED',
        'crouch mission refined status')
    text = replace_regex_once(text,
        r'(### WALK-FEET-142 — Proper forward articulated feet and physical traction\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — MEASURED CONTACT ANCHORS WITH MOVING RELEASE; FULL VALIDATION REQUIRED',
        'feet mission refined status')
    audit_anchor = '''Before release, re-evaluate at minimum:
'''
    audit_addition = '''**Static-friction implementation:** the ground solver records each semantic foot's measured contact x when collision first occurs. Static balance/crouch support is resolved against that physical contact anchor and the terrain height, with velocity removed as a zero-slip constraint. The crouch curriculum cannot write feet or contact memory. Moving stages do not use the static anchor and release through tight separation/upward-speed gates; powered launch releases immediately.

''' + audit_anchor
    text = replace_once(text, audit_anchor, audit_addition,
        'record measured static-friction contact')
    write('missioncache.md', text)

    text = read('CHANGELOG.md')
    anchor = '## Runner v0.7.15 — active joint growth and state transfer\n'
    addition = '''## Runner v0.7.15 — measured static-friction contacts

- Removed all authored-coordinate foot pinning from the crouch curriculum.
- Added ground-solver contact anchors captured from actual heel/ball/toe collisions.
- Static support resolves zero-slip friction against the measured contact; moving stages retain bounded release for swing, toe-off, and jumping.
- Added adversarial tests proving the curriculum cannot touch feet and moving contacts are not magnetic.

''' + anchor
    text = replace_once(text, anchor, addition,
        'measured static-friction changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_header()
    patch_simulation()
    patch_tests()
    patch_documents()


if __name__ == '__main__':
    main()
