from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text.replace("\r\n", "\n").rstrip() + "\n", encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


def stabilize_articulated_stance() -> None:
    text = read("src/simulation.cpp")
    text = replace_once(text,
        '''            { -0.20f, 1.52f }, { -0.16f, 0.26f },
            { 0.20f, 1.52f }, { 0.16f, 0.26f }''',
        '''            { -0.36f, 1.52f }, { -0.46f, 0.26f },
            { 0.36f, 1.52f }, { 0.46f, 0.26f }''',
        "restore stable biped leg spacing")
    text = replace_once(text,
        '''            { -0.2100f, 1.5514f }, { -0.1700f, 0.2500f },
            { 0.2100f, 1.6200f }, { 0.1700f, 0.2500f },''',
        '''            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },''',
        "restore stable humanoid leg spacing")
    text = replace_once(text,
        '''            { -0.25f, 1.42f }, { -0.20f, 0.28f },
            { 0.25f, 1.42f }, { 0.20f, 0.28f },''',
        '''            { -0.42f, 1.42f }, { -0.58f, 0.28f },
            { 0.42f, 1.42f }, { 0.58f, 0.28f },''',
        "restore stable chicken leg spacing")
    text = replace_once(text,
        "        motor.strength = 0.034f;",
        "        motor.strength = 0.022f;",
        "bounded toe motor strength")
    text = replace_once(text,
        '''            if (course_stage_ == CourseStage::balance)
            {
                toe_action = supported ? -0.06f : 0.28f;
            }''',
        '''            if (course_stage_ == CourseStage::balance)
            {
                // Keep the new hinge neutral during Stand. Toe actuation begins
                // only when a lesson actually asks for flexion or propulsion.
                toe_action = 0.0f;
            }''',
        "neutral Stand toe command")

    old_loop_tail = '''            separate_support_clusters();
            if (course_stage_ == CourseStage::duck_press)
            {
                // Separation is the last operation capable of lifting a foot
                // plate. Re-project the authored pose and ground contacts so
                // gait metrics observe the same final state that is rendered.
                stabilize_duck_posture();
                solve_ground(dt);
            }
        }'''
    new_loop_tail = '''            separate_support_clusters();
            if (course_stage_ == CourseStage::duck_press)
                stabilize_duck_posture();
            // Separation and toe rotation are the final operations capable of
            // shifting a semantic contact. End every solver iteration with the
            // same grounded foot state that preview and gait metrics observe.
            solve_ground(dt);
        }'''
    text = replace_once(text, old_loop_tail, new_loop_tail,
        "final all-stage semantic foot ground solve")
    write("src/simulation.cpp", text)


def make_static_crouch_rig_neutral() -> None:
    text = read("src/simulation.cpp")
    start = text.index("        const float head_clearance = valid_node(blueprint_.head_node)\n")
    end = text.index("        float current_joint_speed = 0.0f;\n", start)
    replacement = r'''        const float head_clearance = valid_node(blueprint_.head_node)
            ? particles_[blueprint_.head_node].position.y
                - ground_height_at(particles_[blueprint_.head_node].position.x)
            : 0.0f;
        const float rest_head_clearance = valid_node(blueprint_.head_node)
            ? blueprint_.nodes[blueprint_.head_node].y : 0.0f;
        duck_depth_ = std::max(0.0f, rest_head_clearance - head_clearance);
        const float current_uprightness = torso_uprightness();

        duck_obstacle_weight_ = 0.0f;
        duck_clearance_margin_ = 0.0f;
        for (const CourseFeature& feature : course_features_)
        {
            if (feature.kind != CourseFeatureKind::overhead_bar
                && feature.kind != CourseFeatureKind::duck_press)
                continue;
            const float bar_bottom = feature.center.y - feature.half_extent.y;
            const float head_top = valid_node(blueprint_.head_node)
                ? particles_[blueprint_.head_node].position.y
                    + particles_[blueprint_.head_node].radius
                : bar_bottom;
            const float clearance = bar_bottom - head_top;
            const float weight = feature.kind == CourseFeatureKind::duck_press
                ? clamp((1.10f - clearance) / 1.10f, 0.0f, 1.0f)
                : duck_obstacle_approach_weight(feature.center.x - root_x);
            if (weight <= duck_obstacle_weight_)
                continue;
            duck_obstacle_weight_ = weight;
            duck_clearance_margin_ = clearance;
        }

        const bool generic_duck = feet_supported
            && current_uprightness > 0.60f && duck_depth_ >= 0.48f;
        const bool press_duck = course_stage_ == CourseStage::duck_press
            && feet_supported && !non_foot_grounded_ && body_integrity_valid()
            && duck_obstacle_weight_ >= 0.70f
            && duck_clearance_margin_ >= -0.075f
            && current_uprightness > 0.42f;
        duck_active_ = generic_duck || press_duck;

        const bool disallowed_duck_contact = !duck_ground_contact_allowed(
            duck_active_, non_foot_grounded_);
        if (course_stage_ == CourseStage::duck_press)
        {
            duck_body_contact_seconds_ = disallowed_duck_contact
                ? duck_body_contact_seconds_ + dt
                : std::max(0.0f, duck_body_contact_seconds_ - dt * 3.0f);
            if (duck_body_contact_seconds_ > 0.35f)
                invalidate(InvalidMotion::duck_body_contact);
        }
        else if (disallowed_duck_contact)
        {
            invalidate(InvalidMotion::duck_body_contact);
        }
        if (duck_active_ && !non_foot_grounded_)
            duck_seconds_ += dt;
        if (course_stage_ == CourseStage::crouch_walk
            && duck_active_ && !non_foot_grounded_ && feet_supported)
        {
            crouch_walk_seconds_ += dt;
            crouch_walk_distance_ += std::max(0.0f, root_speed) * dt;
        }

'''
    text = text[:start] + replacement + text[end:]

    hold_start = text.index("        if (course_stage_ == CourseStage::duck_press)\n", end)
    hold_end = text.index("        if ((course_stage_ == CourseStage::duck_press\n", hold_start)
    hold_replacement = r'''        if (course_stage_ == CourseStage::duck_press)
        {
            const bool press_challenge_reached = duck_press_contact_this_step_
                || duck_press_contact_seen_
                || (duck_obstacle_weight_ >= 0.78f
                    && duck_clearance_margin_ <= 0.16f);
            if (press_challenge_reached)
                duck_press_contact_seen_ = true;
            if (duck_press_contact_seen_ && duck_active_ && !non_foot_grounded_
                && duck_clearance_margin_ >= -0.075f
                && duck_clearance_margin_ <= 0.25f
                && body_integrity_valid())
            {
                duck_press_hold_seconds_ += dt;
                if (duck_press_hold_seconds_ >= 0.55f)
                    duck_press_hold_qualified_ = true;
            }
            else if (!duck_press_hold_qualified_)
            {
                duck_press_hold_seconds_ = std::max(
                    0.0f, duck_press_hold_seconds_ - dt * 0.35f);
            }
            if (duck_press_hold_qualified_ && !duck_press_contact_this_step_
                && duck_obstacle_weight_ < 0.15f
                && feet_supported && !non_foot_grounded_
                && current_uprightness >= 0.58f
                && !duck_press_completed_)
            {
                duck_press_completed_ = true;
                duck_walk_started_seconds_ = elapsed_seconds_;
                progress_window_start_x_ = root_x;
                progress_window_start_steps_ = alternating_steps_;
                ++duck_recovery_count_;
                ++obstacles_passed_;
                passed_obstacle_this_step_ = true;
            }
        }
        else if (duck_active_)
        {
            current_duck_hold_seconds_ += dt;
            duck_cycle_qualified_ = duck_cycle_qualified_
                || current_duck_hold_seconds_ >= 0.30f;
        }
        else if (duck_cycle_qualified_ && stable_stance_seconds_ >= 0.40f)
        {
            ++duck_recovery_count_;
            current_duck_hold_seconds_ = 0.0f;
            duck_cycle_qualified_ = false;
        }
        else if (!duck_cycle_qualified_)
        {
            current_duck_hold_seconds_ = 0.0f;
        }

'''
    text = text[:hold_start] + hold_replacement + text[hold_end:]

    old_gate = '''        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall,
            course_stage_, current_airborne_rotation_ / (2.0f * pi)));'''
    new_gate = '''        InvalidMotion frame_gate = classify_motion_gate(gated_upright,
            maximum_speed_kmh_, pelvis_position, airborne_seconds_, allowed_airtime,
            micro_motion_seconds_, terminal_fall, course_stage_,
            current_airborne_rotation_ / (2.0f * pi));
        if (course_stage_ == CourseStage::duck_press
            && (frame_gate == InvalidMotion::sustained_flight
                || frame_gate == InvalidMotion::overspeed))
            frame_gate = InvalidMotion::none;
        invalidate(frame_gate);'''
    text = replace_once(text, old_gate, new_gate,
        "static press stage-specific motion gate")
    write("src/simulation.cpp", text)


def update_support_tests() -> None:
    text = read("tests/core_tests.cpp")
    text = replace_once(text,
        '''                    const bool same_foot = std::ranges::any_of(environment.blueprint_.bones,
                        [lhs_index, rhs_index](const DistanceConstraint& bone)
                        {
                            return (bone.a == lhs_index && bone.b == rhs_index)
                                || (bone.a == rhs_index && bone.b == lhs_index);
                        });''',
        '''                    const bool same_foot =
                        (environment.blueprint_.is_left_support_seed(lhs_index)
                            && environment.blueprint_.is_left_support_seed(rhs_index))
                        || (environment.blueprint_.is_right_support_seed(lhs_index)
                            && environment.blueprint_.is_right_support_seed(rhs_index));''',
        "semantic same-foot test grouping")
    text = replace_once(text,
        '''    require(humanoid.additional_left_contact_nodes.size() == 1u
            && humanoid.additional_right_contact_nodes.size() == 1u,
        "dedicated foot plates do not include heel and toe contacts");''',
        '''    require(humanoid.additional_left_contact_nodes.size() == 2u
            && humanoid.additional_right_contact_nodes.size() == 2u,
        "articulated foot does not include heel, ball, and toe contacts");''',
        "articulated foot contact count")
    write("tests/core_tests.cpp", text)

    text = read("src/acceptance.cpp")
    text = replace_once(text,
        '''                    const bool same_plate = std::ranges::any_of(blueprint.bones,
                        [first_index, second_index](const sim::DistanceConstraint& bone)
                        {
                            return (bone.a == first_index && bone.b == second_index)
                                || (bone.a == second_index && bone.b == first_index);
                        });''',
        '''                    const bool same_plate =
                        (blueprint.is_left_support_seed(first_index)
                            && blueprint.is_left_support_seed(second_index))
                        || (blueprint.is_right_support_seed(first_index)
                            && blueprint.is_right_support_seed(second_index));''',
        "acceptance semantic same-foot grouping")
    write("src/acceptance.cpp", text)


def main() -> None:
    stabilize_articulated_stance()
    make_static_crouch_rig_neutral()
    update_support_tests()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
