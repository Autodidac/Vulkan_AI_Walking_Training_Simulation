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


def patch_simulation() -> None:
    text = read('src/simulation.cpp')
    old = '''        auto pin_support = [&](std::size_t node)
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
    text = replace_once(text, old, '', 'remove forced semantic-foot coordinates')
    old = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
'''
    new = '''            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. Only the ground solver may establish
            // support; the crouch guide never writes heel/ball/toe coordinates.
            solve_ground(dt);
'''
    text = replace_once(text, old, new, 'physical crouch support comment')
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

'''
    text = replace_once(text, anchor, addition,
        'crouch guide support-dynamics test access')
    anchor = '''    sim::Environment guided_squat(humanoid_rig, 140);
    require(sim::EnvironmentTestAccess::guided_squat_is_valid(guided_squat),
        "authored crouch guide cannot produce a pelvis-down bilateral squat");

'''
    addition = anchor + '''    sim::Environment unpinned_squat(humanoid_rig, 1401);
    require(sim::EnvironmentTestAccess::crouch_guide_preserves_support_dynamics(
            unpinned_squat),
        "crouch curriculum directly pins semantic foot coordinates or support state");

'''
    text = replace_once(text, anchor, addition,
        'unpinned physical crouch acceptance')
    write('tests/core_tests.cpp', text)


def patch_documents() -> None:
    text = read('missioncache.md')
    text = replace_once(text,
        '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** PARTIAL — POSTURE EVIDENCE PASSES; FORCED FOOT-PIN REMOVAL REOPENED BEFORE RELEASE
''',
        '''### WALK-CROUCH-140 — Real squat-shaped crouch, not a forward bow
**Status:** IMPLEMENTED — FORCED FOOT PIN REMOVED; FULL VALIDATION REQUIRED
''',
        'crouch mission refined status')
    text = replace_once(text,
        '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** PARTIAL — GROUND FRICTION PASSES; STATIC CROUCH HARD-PIN REOPENED BEFORE RELEASE
''',
        '''### WALK-FEET-142 — Proper forward articulated feet and physical traction
**Status:** IMPLEMENTED — CROUCH SUPPORT NOW USES ONLY COLLISION/FRICTION; FULL VALIDATION REQUIRED
''',
        'feet mission refined status')
    finding = '''**Second-audit finding:** the static crouch stabilizer still wrote authored x positions, floor y positions, previous positions, and `grounded=true` into every semantic foot contact several times per solver iteration. That invalidated the claimed friction-only support model and could manufacture the very crouch being evaluated. The release remains blocked until those assignments are removed and adversarial tests prove support, squat shape, recovery, and bounded slip still pass through the real ground solver.
'''
    resolution = finding + '''
**Resolution implemented:** the crouch guide now skips every semantic support node. It may shape knees, pelvis, torso, and head, but only `solve_ground` may place heel/ball/toe contacts, alter their retained tangential velocity, or mark them grounded. A dedicated adversarial test perturbs foot position, velocity history, and support state and verifies the guide leaves all three untouched before the real ground solver runs.
'''
    text = replace_once(text, finding, resolution,
        'record physical crouch support resolution')
    write('missioncache.md', text)

    text = read('CHANGELOG.md')
    anchor = '## Runner v0.7.15 — active joint growth and state transfer\n'
    addition = '''## Runner v0.7.15 — physical crouch support refinement

- Removed the static-crouch curriculum's direct writes to heel/ball/toe position, velocity history, and grounded state.
- The posture guide now shapes only non-support anatomy; collision and terrain-aware friction exclusively determine foot support and slip.
- Added an adversarial test proving the guide cannot overwrite perturbed semantic-foot dynamics while the existing squat-shape test still requires pelvis-down bilateral compression.

''' + anchor
    text = replace_once(text, anchor, addition,
        'physical crouch support changelog')
    write('CHANGELOG.md', text)


def main() -> None:
    patch_simulation()
    patch_tests()
    patch_documents()


if __name__ == '__main__':
    main()
