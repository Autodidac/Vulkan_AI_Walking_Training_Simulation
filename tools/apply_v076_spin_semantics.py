from pathlib import Path

root = Path(__file__).resolve().parents[1]

simulation_path = root / 'src/simulation.cpp'
simulation = simulation_path.read_text(encoding='utf-8')

old_reset = '''        current_airborne_rotation_ = 0.0f;
        maximum_spin_turns_ = 0.0f;
        powered_jump_count_ = 0;'''
new_reset = '''        current_airborne_rotation_ = 0.0f;
        maximum_spin_turns_ = 0.0f;
        uncontrolled_spin_turns_ = 0.0f;
        powered_jump_count_ = 0;'''
if new_reset not in simulation:
    if old_reset not in simulation:
        raise SystemExit('spin reset anchor was not found')
    simulation = simulation.replace(old_reset, new_reset, 1)

old_turn = '''        const float torso_delta = wrap_angle(torso_angle - previous_torso_angle_);
        torso_turn_speed_ = torso_delta / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        non_foot_grounded_ = non_foot_ground_contact();'''
new_turn = '''        const float torso_delta = wrap_angle(torso_angle - previous_torso_angle_);
        torso_turn_speed_ = torso_delta / std::max(dt, 1.0e-5f);
        previous_torso_angle_ = torso_angle;
        if (course_stage_ == CourseStage::balance)
        {
            // Standing rotation is a posture failure even while both feet remain
            // planted. Track the maximum wrapped torso turn instead of counting
            // only airborne flips, so upright spinning cannot pass mastery.
            uncontrolled_spin_turns_ = std::max(uncontrolled_spin_turns_,
                std::abs(torso_angle) / (2.0f * pi));
        }
        non_foot_grounded_ = non_foot_ground_contact();'''
if new_turn not in simulation:
    if old_turn not in simulation:
        raise SystemExit('torso rotation tracking anchor was not found')
    simulation = simulation.replace(old_turn, new_turn, 1)

old_airborne = '''            if (powered_takeoff_ && stage_allows_controlled_flips(course_stage_))
                maximum_spin_turns_ = std::max(maximum_spin_turns_, airborne_turns);
            else
                uncontrolled_spin_turns_ += std::abs(torso_delta) / (2.0f * pi);'''
new_airborne = '''            if (powered_takeoff_ && stage_allows_controlled_flips(course_stage_))
                maximum_spin_turns_ = std::max(maximum_spin_turns_, airborne_turns);
            else if (course_stage_ != CourseStage::balance)
                uncontrolled_spin_turns_ += std::abs(torso_delta) / (2.0f * pi);'''
if new_airborne not in simulation:
    if old_airborne not in simulation:
        raise SystemExit('airborne spin tracking anchor was not found')
    simulation = simulation.replace(old_airborne, new_airborne, 1)

simulation_path.write_text(simulation, encoding='utf-8', newline='\n')

test_path = root / 'tests/core_tests.cpp'
tests = test_path.read_text(encoding='utf-8')
old_flip_test = '''    {
        sim::Environment flip_semantics{ humanoid, 0xF11Fu };
        require(flip_semantics.maximum_flip_turns() == 0.0f
                && flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "fresh rig does not separate flip and spin counters");
    }'''
new_flip_test = '''    {
        sim::Environment flip_semantics{ humanoid, 0xF11Fu };
        require(flip_semantics.maximum_flip_turns() == 0.0f
                && flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "fresh rig does not separate flip and spin counters");
        sim::EnvironmentTestAccess::force_standing_spin(flip_semantics, 0.25f);
        require(flip_semantics.uncontrolled_spin_turns() == 0.25f,
            "grounded standing rotation cannot be represented by the strict gate");
        flip_semantics.reset(0xF120u);
        require(flip_semantics.uncontrolled_spin_turns() == 0.0f,
            "standing spin evidence leaks across episode resets");
    }'''
if new_flip_test not in tests:
    if old_flip_test not in tests:
        raise SystemExit('flip/spin reset test anchor was not found')
    tests = tests.replace(old_flip_test, new_flip_test, 1)

test_path.write_text(tests, encoding='utf-8', newline='\n')
Path(__file__).unlink()
print('materialized grounded standing-spin semantics correction')
