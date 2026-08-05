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
    addition = '''    inline constexpr float planted_contact_slop_m = 0.032f;
    inline constexpr float planted_contact_release_speed_mps = 0.24f;

    [[nodiscard]] inline bool planted_contact_persists(bool was_grounded,
        bool semantic_support, float separation, float upward_speed,
        bool release_requested) noexcept
    {
        return was_grounded && semantic_support && !release_requested
            && separation > 0.0025f
            && separation <= planted_contact_slop_m
            && upward_speed <= planted_contact_release_speed_mps;
    }

''' + anchor
    text = replace_once(text, anchor, addition,
        'planted-contact persistence helper')
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
        float root_upward_speed = 0.0f;
        if (valid_node(blueprint_.root_node))
        {
            const Particle& root = particles_[blueprint_.root_node];
            root_upward_speed = (root.position.y - root.previous.y) / safe_dt;
        }
        const bool powered_release = powered_joint_launch(
            course_stage_, root_upward_speed, action_change_energy_);

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
            const bool actual_contact = separation <= 0.0025f
                && !release_requested;
            const bool persistent_contact = planted_contact_persists(
                was_grounded, semantic_support, separation, velocity.y,
                release_requested);
            if (actual_contact || persistent_contact)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
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
                particle.previous.x = particle.position.x
                    - velocity.x * retention * safe_dt;
                if (traction_contact)
                    particle.previous.y = particle.position.y;
                else if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y
                        + velocity.y * safe_dt * 0.05f;
            }
        }
    }
'''
    text = replace_once(text, old_ground, new_ground,
        'bounded planted-contact persistence')

    old_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
'''
    new_comment = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. Only the ground solver may establish
            // support; bounded persistence absorbs numerical lift without
            // preventing a real swing, toe-off, or powered launch.
            solve_ground(dt);
'''
    text = replace_once(text, old_comment, new_comment,
        'physical crouch support comment')
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

        static bool planted_contact_persistence_is_bounded(
            Environment& environment) noexcept
        {
            environment.set_course(CourseStage::balance, 0.25f);
            if (!environment.valid_node(environment.blueprint_.left_contact_node))
                return false;
            constexpr float dt = 1.0f / 60.0f;
            Particle& support = environment.particles_[
                environment.blueprint_.left_contact_node];
            const float minimum_y = environment.ground_height_at(support.position.x)
                + ground_contact_offset(true, support.radius);

            support.position.y = minimum_y + 0.018f;
            support.previous = support.position - Vec2{ 0.0f, 0.08f * dt };
            support.grounded = true;
            environment.solve_ground(dt);
            const bool held = support.grounded
                && std::abs(support.position.y - minimum_y) < 1.0e-6f;

            support.position.y = minimum_y + 0.018f;
            support.previous = support.position - Vec2{ 0.0f, 0.42f * dt };
            support.grounded = true;
            environment.solve_ground(dt);
            const bool released = !support.grounded
                && std::abs(support.position.y - (minimum_y + 0.018f)) < 1.0e-6f;
            return held && released;
        }

'''
    text = replace_once(text, anchor, addition,
        'crouch and contact persistence test access')

    test_anchor = '''    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

'''
    test_addition = test_anchor + '''    require(sim::planted_contact_persists(
            true, true, 0.018f, 0.08f, false),
        "previously planted semantic support did not survive bounded solver lift");
    require(!sim::planted_contact_persists(
            true, true, 0.018f, 0.42f, false),
        "fast upward foot motion remained magnetically planted");
    require(!sim::planted_contact_persists(
            true, true, 0.050f, 0.08f, false),
        "support persistence exceeded its bounded contact slop");
    require(!sim::planted_contact_persists(
            true, true, 0.018f, 0.08f, true),
        "explicit powered release was ignored");

    sim::Environment unpinned_squat(humanoid_rig, 1401);
    require(sim::EnvironmentTestAccess::crouch_guide_preserves_support_dynamics(
            unpinned_squat),
        "crouch curriculum directly pins semantic foot coordinates or support state");
    sim::Environment contact_slop(humanoid_rig, 1402);
    require(sim::EnvironmentTestAccess::planted_contact_persistence_is_bounded(
            contact_slop),
        "ground solver did not distinguish numerical contact lift from deliberate foot lift");

'''
    text = replace_once(text, test_anchor, test_addition,
        'bounded physical crouch acceptance')
    write('tests/core_tests.cpp', text)


def patch_documents() -> None:
    text = read('missioncache.md')
    text = replace_regex_once(text,
        r'(### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — BOUNDED PHYSICAL CONTACT PERSISTENCE; FULL VALIDATION REQUIRED',
        'crouch mission refined status')
    text = replace_regex_once(text,
        r'(### WALK-FEET-142 — Proper forward articulated feet and physical traction\n)\*\*Status:\*\*[^\n]*',
        r'\1**Status:** IMPLEMENTED — BOUNDED PLANTED CONTACT WITH PROMPT RELEASE; FULL VALIDATION REQUIRED',
        'feet mission refined status')
    audit_anchor = '''Before release, re-evaluate at minimum:
'''
    audit_addition = '''**Contact-persistence implementation:** `solve_ground` now remembers the immediately previous physical support result and tolerates only 0.032 m of semantic-foot numerical separation at no more than 0.24 m/s upward speed. Larger separation, faster lift, or an explicit powered launch releases the contact. The crouch guide no longer writes support coordinates, velocity history, or grounded state. Positive, negative, and adversarial tests cover bounded persistence, deliberate release, and curriculum noninterference.

''' + audit_anchor
    text = replace_once(text, audit_anchor, audit_addition,
        'record bounded planted-contact implementation')
    write('missioncache.md', text)

    text = read('CHANGELOG.md')
    anchor = '## Runner v0.7.15 — active joint growth and state transfer\n'
    addition = '''## Runner v0.7.15 — physical planted-contact persistence

- Removed the static-crouch curriculum's direct writes to heel/ball/toe position, velocity history, and grounded state.
- Added a bounded ground-solver persistence rule for previously planted semantic foot contacts so iterative constraints cannot create false unsupported frames.
- Contacts release on excessive separation, deliberate upward motion, or powered launch; adversarial tests reject magnetic feet and curriculum pinning.

''' + anchor
    text = replace_once(text, anchor, addition,
        'physical planted-contact changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_header()
    patch_simulation()
    patch_tests()
    patch_documents()


if __name__ == '__main__':
    main()
