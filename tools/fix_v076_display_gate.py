from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / 'src/ppo.hpp'
text = path.read_text(encoding='utf-8')

old_gate = '''        if (!qualification.valid
            || !environment.current_display_posture_valid())
            return false;'''
new_gate = '''        if (!qualification.valid || !environment.body_integrity_valid())
            return false;'''
if new_gate not in text:
    if old_gate not in text:
        raise SystemExit('stage display gate target was not found')
    text = text.replace(old_gate, new_gate, 1)

old_balance = '''        if (stage == sim::CourseStage::balance)
        {
            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit
                && (environment.left_supported() || environment.right_supported());
        }'''
new_balance = '''        if (stage == sim::CourseStage::balance)
        {
            // Qualification already proves a sustained neutral supported stance.
            // Keep the current training frame visible through a single solver-frame
            // foot-contact flicker or small raised-shoulder posture oscillation.
            return environment.uprightness() >= 0.60f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit;
        }'''
if new_balance not in text:
    if old_balance not in text:
        raise SystemExit('balance PIP block was not found')
    text = text.replace(old_balance, new_balance, 1)

path.write_text(text, encoding='utf-8', newline='\n')

test_path = root / 'tests/core_tests.cpp'
tests = test_path.read_text(encoding='utf-8')
old_expectation = '''        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, assisted_stance),
            "valid current stance is hidden from the training sample");'''
new_expectation = '''        require(rl::training_preview_priority(
                sim::CourseStage::balance, assisted_stance) > 0,
            "stage-qualified standing environment disappears from the training PIP");'''
if new_expectation not in tests:
    if old_expectation not in tests:
        raise SystemExit('stale standing PIP test expectation was not found')
    tests = tests.replace(old_expectation, new_expectation, 1)

old_neutral = '''        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");'''
new_neutral = '''        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");
        require(rl::stage_display_sample_eligible(
                sim::CourseStage::balance, neutral_stance),
            "current neutral strict stance is not eligible for top-priority PIP display");'''
if new_neutral not in tests:
    if old_neutral not in tests:
        raise SystemExit('neutral stance test anchor was not found')
    tests = tests.replace(old_neutral, new_neutral, 1)

diagnostic_anchor = '''        struct StanceFrame
        {'''
diagnostic_method = '''        static void print_integrity_diagnostics(const Environment& environment) noexcept
        {
            float minimum_bone_ratio = 1000.0f;
            float maximum_bone_ratio = 0.0f;
            std::size_t minimum_bone = 0u;
            std::size_t maximum_bone = 0u;
            for (std::size_t index = 0; index < environment.blueprint_.bones.size(); ++index)
            {
                const DistanceConstraint& bone = environment.blueprint_.bones[index];
                const float ratio = length(environment.particles_[bone.b].position
                    - environment.particles_[bone.a].position)
                    / std::max(bone.rest_length, 1.0e-5f);
                if (ratio < minimum_bone_ratio)
                {
                    minimum_bone_ratio = ratio;
                    minimum_bone = index;
                }
                if (ratio > maximum_bone_ratio)
                {
                    maximum_bone_ratio = ratio;
                    maximum_bone = index;
                }
            }
            const Vec2 root = environment.particles_[environment.blueprint_.root_node].position;
            const Vec2 torso_segment = environment.particles_[environment.blueprint_.torso_node].position - root;
            const Vec2 head_segment = environment.particles_[environment.blueprint_.head_node].position
                - environment.particles_[environment.blueprint_.torso_node].position;
            const Vec2 rest_torso = environment.blueprint_.nodes[environment.blueprint_.torso_node]
                - environment.blueprint_.nodes[environment.blueprint_.root_node];
            const Vec2 rest_head = environment.blueprint_.nodes[environment.blueprint_.head_node]
                - environment.blueprint_.nodes[environment.blueprint_.torso_node];
            const float torso_ratio = length(torso_segment) / std::max(length(rest_torso), 1.0e-5f);
            const float head_ratio = length(head_segment) / std::max(length(rest_head), 1.0e-5f);
            const float alignment = dot(normalized(torso_segment, { 0.0f, 1.0f }),
                normalized(head_segment, { 0.0f, 1.0f }));
            std::cerr << " integrity=" << environment.body_integrity_valid()
                << " bone_min=" << minimum_bone_ratio << '@' << minimum_bone
                << " bone_max=" << maximum_bone_ratio << '@' << maximum_bone
                << " torso_ratio=" << torso_ratio
                << " head_ratio=" << head_ratio
                << " alignment=" << alignment
                << " head_above_torso=" << head_segment.y
                << " upper_dev=" << environment.maximum_upper_body_motor_deviation()
                << std::endl;
        }

        struct StanceFrame
        {'''
if diagnostic_method not in tests:
    if diagnostic_anchor not in tests:
        raise SystemExit('stance diagnostic insertion anchor was not found')
    tests = tests.replace(diagnostic_anchor, diagnostic_method, 1)

old_seed_result = '''            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            valid_agents += qualification.valid ? 1u : 0u;
            if (!qualification.valid)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds() << std::endl;
            }'''
new_seed_result = '''            const rl::StageMotionQualification qualification =
                rl::stage_motion_qualification(sim::CourseStage::balance, environment);
            const bool integrity = environment.body_integrity_valid();
            valid_agents += qualification.valid && integrity ? 1u : 0u;
            if (!qualification.valid || !integrity)
            {
                std::cerr << "evaluation seed " << seed
                    << " rejection=" << qualification.rejection_mask
                    << " invalid=" << static_cast<int>(environment.invalid_reason())
                    << " stance=" << environment.stable_stance_seconds()
                    << " longest=" << environment.longest_stable_stance_seconds()
                    << " max_joint=" << environment.maximum_joint_speed()
                    << " survival=" << environment.elapsed_seconds();
                sim::EnvironmentTestAccess::print_integrity_diagnostics(environment);
            }'''
if new_seed_result not in tests:
    if old_seed_result not in tests:
        raise SystemExit('evaluation seed diagnostics block was not found')
    tests = tests.replace(old_seed_result, new_seed_result, 1)

test_path.write_text(tests, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('aligned standing PIP tests and added raised-shoulder integrity diagnostics')
