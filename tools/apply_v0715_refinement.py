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
    addition = '''    struct PlantedContactLimits
    {
        float separation{};
        float upward_speed{};
    };

    [[nodiscard]] inline PlantedContactLimits planted_contact_limits(
        bool static_support, float radius) noexcept
    {
        if (static_support)
        {
            return {
                clamp(radius * 2.25f, 0.12f, 0.22f),
                3.0f
            };
        }
        return { 0.032f, 0.24f };
    }

    [[nodiscard]] inline bool planted_contact_persists(bool contact_latched,
        bool semantic_support, float separation, float upward_speed,
        bool release_requested, PlantedContactLimits limits) noexcept
    {
        return contact_latched && semantic_support && !release_requested
            && separation > 0.0025f
            && separation <= limits.separation
            && upward_speed <= limits.upward_speed;
    }

''' + anchor
    text = replace_once(text, anchor, addition,
        'persistent contact manifold helpers')

    member_anchor = '''        std::vector<Particle> particles_{};
        std::vector<CourseFeature> course_features_{};
'''
    member_addition = '''        std::vector<Particle> particles_{};
        std::vector<std::uint8_t> support_contact_latch_{};
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
        'remove forced semantic-foot coordinates')

    reset_anchor = '''        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
'''
    reset_addition = '''        support_contact_latch_.assign(particles_.size(), 0u);
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
            const PlantedContactLimits limits = planted_contact_limits(
                static_support, particle.radius);
            const bool actual_contact = separation <= 0.0025f
                && !release_requested;
            const bool persistent_contact = planted_contact_persists(
                contact_latched, semantic_support, separation, velocity.y,
                release_requested, limits);
            if (actual_contact || persistent_contact)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                if (semantic_support)
                    support_contact_latch_[index] = 1u;
                float retention = ground_velocity_retention(traction_contact, velocity.y);
                if (traction_contact)
                {
                    const bool left_toe = blueprint_.additional_left_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_left_contact_nodes[1];
                    const bool right_toe = blueprint_.additional_right_contact_nodes.size() >= 2u
                        && index == blueprint_.additional_right_contact_nodes[1];
                    retention = foot_friction_retention(velocity.x,
                        firmness, looseness, static_support,
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
            else if (semantic_support
                && (release_requested
                    || separation > limits.separation
                    || velocity.y > limits.upward_speed))
            {
                support_contact_latch_[index] = 0u;
            }
        }
    }
'''
    text = replace_once(text, old_ground, new_ground,
        'warm-started bounded contact manifold')

    old_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
'''
    new_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. Only the ground solver may establish
            // support; its warm-started manifold absorbs iterative numerical
            // lift while moving stages still release feet promptly.
            solve_ground(dt);
'''
    text = replace_once(text, old_comment, new_comment,
        'physical contact manifold comment')
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

        static bool planted_contact_manifold_is_bounded(
            Environment& environment) noexcept
        {
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            constexpr float dt = 1.0f / 60.0f;
            const std::size_t node = environment.blueprint_.left_contact_node;
            Particle& support = environment.particles_[node];

            environment.set_course(CourseStage::duck_press, 0.25f);
            float minimum_y = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = minimum_y;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            support.position.y = minimum_y + 0.11f;
            support.previous = support.position - Vec2{ 0.0f, 0.80f * dt };
            support.grounded = false;
            environment.solve_ground(dt);
            const bool static_held = support.grounded
                && std::abs(support.position.y - minimum_y) < 1.0e-6f;

            environment.set_course(CourseStage::uneven, 0.25f);
            minimum_y = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);
            support.position.y = minimum_y;
            support.previous = support.position;
            support.grounded = true;
            environment.solve_ground(dt);
            support.position.y = minimum_y + 0.040f;
            support.previous = support.position - Vec2{ 0.0f, 0.42f * dt };
            support.grounded = false;
            environment.solve_ground(dt);
            const bool moving_released = !support.grounded
                && std::abs(support.position.y - (minimum_y + 0.040f)) < 1.0e-6f;
            return static_held && moving_released;
        }

'''
    text = replace_once(text, anchor, addition,
        'crouch guide and contact manifold test access')

    test_anchor = '''    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

'''
    test_addition = test_anchor + '''    const sim::PlantedContactLimits static_limits =
        sim::planted_contact_limits(true, 0.08f);
    const sim::PlantedContactLimits moving_limits =
        sim::planted_contact_limits(false, 0.08f);
    require(sim::planted_contact_persists(
            true, true, 0.11f, 0.80f, false, static_limits),
        "warm-started static support did not survive iterative solver lift");
    require(!sim::planted_contact_persists(
            true, true, 0.040f, 0.42f, false, moving_limits),
        "moving foot remained magnetically planted");
    require(!sim::planted_contact_persists(
            true, true, 0.018f, 0.08f, true, static_limits),
        "explicit powered release was ignored");

    sim::Environment unpinned_squat(humanoid_rig, 1401);
    require(sim::EnvironmentTestAccess::crouch_guide_preserves_support_dynamics(
            unpinned_squat),
        "crouch curriculum directly pins semantic foot coordinates or support state");
    sim::Environment contact_manifold(humanoid_rig, 1402);
    require(sim::EnvironmentTestAccess::planted_contact_manifold_is_bounded(
            contact_manifold),
        "contact manifold failed static warm-start or moving-foot release");

'''
    text = replace_once(text, test_anchor, test_addition,
        'warm-started physical crouch acceptance')
    write('tests/core_tests.cpp', text)


def patch_documents() -> None:
    text = read('missioncache.md')
    text = replace_regex_once(text,
        r'(### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — WARM-STARTED PHYSICAL CONTACT MANIFOLD; FULL VALIDATION REQUIRED',
        'crouch mission refined status')
    text = replace_regex_once(text,
        r'(### WALK-FEET-142 — Proper forward articulated feet and physical traction\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — STATE-AWARE BOUNDED CONTACT RELEASE; FULL VALIDATION REQUIRED',
        'feet mission refined status')
    audit_anchor = '''Before release, re-evaluate at minimum:
'''
    audit_addition = '''**Contact-manifold implementation:** semantic foot contacts now retain a dedicated warm-start latch independent of the transient `Particle::grounded` flag. Static support permits bounded constraint correction based on foot radius; moving stages use tight separation and upward-speed limits, and powered launch clears contact immediately. The crouch guide remains unable to write support coordinates, velocity history, grounded state, or the contact latch. Tests cover static warm start, moving release, powered release, and curriculum noninterference.

''' + audit_anchor
    text = replace_once(text, audit_anchor, audit_addition,
        'record warm-started contact manifold')
    write('missioncache.md', text)

    text = read('CHANGELOG.md')
    anchor = '## Runner v0.7.15 — active joint growth and state transfer\n'
    addition = '''## Runner v0.7.15 — warm-started physical foot contacts

- Removed all static-crouch writes to semantic foot coordinates and support state.
- Added a dedicated ground-solver contact manifold so repeated constraint passes do not erase a physically planted foot.
- Static support uses radius-bounded correction; moving feet and powered launches release through tighter distance and velocity gates.
- Added adversarial tests for curriculum noninterference, static contact retention, and non-magnetic moving-foot release.

''' + anchor
    text = replace_once(text, anchor, addition,
        'warm-started foot contact changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_header()
    patch_simulation()
    patch_tests()
    patch_documents()


if __name__ == '__main__':
    main()
