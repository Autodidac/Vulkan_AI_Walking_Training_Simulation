from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:180]!r}")
    save(path, text.replace(old, new, 1))


# Release identity and clean state boundary.
replace("CMakeLists.txt", "project(Runner VERSION 0.7.5 LANGUAGES CXX)",
        "project(Runner VERSION 0.7.6 LANGUAGES CXX)")
replace("src/ppo.hpp", "inline constexpr std::uint32_t training_semantics_version = 0x0007'0502u;",
        "inline constexpr std::uint32_t training_semantics_version = 0x0007'0600u;")
replace("src/app.cpp", '"runner-v075-autosave.eppo"', '"runner-v076-autosave.eppo"')
replace("src/app.cpp", '"runner-v075-evolved.rig"', '"runner-v076-evolved.rig"')
replace("src/app.cpp", '"runner-v075-autonomy.state"', '"runner-v076-autonomy.state"')
replace("src/autonomy_persistence.cpp", 'output << "RUNAUTONOMY 10\\n";',
        'output << "RUNAUTONOMY 11\\n";')
replace("src/autonomy_persistence.cpp", 'magic != "RUNAUTONOMY" || version != 10',
        'magic != "RUNAUTONOMY" || version != 11')
replace("src/autonomy_commands.cpp", "NO V0.7.5 AUTOSAVE FOUND", "NO V0.7.6 AUTOSAVE FOUND")
replace("src/autonomy_commands.cpp", "V0.7.5 AUTOSAVE RESUMED", "V0.7.6 AUTOSAVE RESUMED")

# Humanoid geometry: central shoulder/chest pivot is intentionally above the
# two lateral shoulder pivots. This restores the better articulated geometry
# Adam observed and gives both shoulder motors a stable downward rest target.
replace("src/simulation.cpp",
'''        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0148f, 4.0173f }, { -0.010f, 4.86f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },
            { -0.42f, 4.06f }, { -0.78f, 3.48f }, { -0.60f, 2.82f },
            { 0.40f, 4.06f }, { 0.76f, 3.48f }, { 0.58f, 2.82f }
        };''',
'''        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0060f, 4.2000f }, { -0.0060f, 4.9800f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f },
            { -0.42f, 4.0200f }, { -0.78f, 3.43f }, { -0.60f, 2.76f },
            { 0.40f, 4.0200f }, { 0.76f, 3.43f }, { 0.58f, 2.76f }
        };''')

# Expose current upper-body joint deviation so standing can reject the arms-up
# exploit rather than merely calling it a valid stance.
replace("src/simulation.hpp",
'''        [[nodiscard]] float maximum_joint_speed() const noexcept { return maximum_joint_speed_; }
        [[nodiscard]] float posture_failure_seconds() const noexcept''',
'''        [[nodiscard]] float maximum_joint_speed() const noexcept { return maximum_joint_speed_; }
        [[nodiscard]] float maximum_upper_body_motor_deviation() const noexcept;
        [[nodiscard]] float posture_failure_seconds() const noexcept''')
replace("src/simulation.cpp",
'''    float Environment::torso_roll_angle() const noexcept
    {''',
'''    float Environment::maximum_upper_body_motor_deviation() const noexcept
    {
        float maximum = 0.0f;
        for (std::size_t index = 4; index < blueprint_.active_motor_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            maximum = std::max(maximum,
                std::abs(wrap_angle(joint_angle(motor) - motor.neutral_angle)));
        }
        return maximum;
    }

    float Environment::torso_roll_angle() const noexcept
    {''')

# Keep the standing teacher centered on calibrated rest geometry and make
# upper-body displacement cost reward, not just joint velocity.
replace("src/ppo.hpp",
'''        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
        {
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.035f * joint_speed, -0.08f, 0.08f);
        }''',
'''        for (std::size_t index = 4; index < environment.blueprint().active_motor_count; ++index)
        {
            const float joint_offset = observation[joint_angle_begin + index];
            const float joint_speed = observation[joint_velocity_begin + index];
            action[index] = clamp(-0.18f * joint_offset - 0.045f * joint_speed,
                -0.22f, 0.22f);
        }''')
replace("src/simulation.cpp",
'''        const float torso_swing_penalty = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;

        switch (course_stage_)''',
'''        const float torso_swing_penalty = (course_stage_ == CourseStage::duck_press
                || course_stage_ == CourseStage::crouch_walk)
            ? std::max(0.0f, std::abs(torso_turn_speed_) - 0.22f) * 0.030f : 0.0f;
        const float upper_body_deviation = maximum_upper_body_motor_deviation();
        const float neutral_upper_body_reward = course_stage_ == CourseStage::balance
            ? clamp(1.0f - upper_body_deviation / 0.70f, 0.0f, 1.0f) * 0.014f
            : 0.0f;
        const float upper_body_posture_penalty = course_stage_ == CourseStage::balance
            ? std::max(0.0f, upper_body_deviation - 0.30f) * 0.035f
            : 0.0f;

        switch (course_stage_)''')
replace("src/simulation.cpp",
'''            last_reward_ = stance_reward
                + std::max(0.0f, upright) * 0.008f
                + contact * 0.0010f
                - std::abs(forward_speed_) * 0.0070f
                - std::abs(distance_travelled_) * 0.0030f
                - action_energy * 0.0018f
                - stance_slip_speed_ * 0.010f
                - posture_failure_seconds_ * 0.020f
                - body_contact_penalty;''',
'''            last_reward_ = stance_reward
                + std::max(0.0f, upright) * 0.008f
                + neutral_upper_body_reward
                + contact * 0.0010f
                - std::abs(forward_speed_) * 0.0070f
                - std::abs(distance_travelled_) * 0.0030f
                - action_energy * 0.0018f
                - stance_slip_speed_ * 0.010f
                - posture_failure_seconds_ * 0.020f
                - upper_body_posture_penalty
                - body_contact_penalty;''')

# One shared source of truth for stage-valid standing, strict mastery, and the
# evaluator stop duration. The previous evaluator stopped at three seconds while
# mastery demanded five/six seconds, making the counter impossible to advance.
replace("src/ppo.hpp",
'''    enum class MotionEvidenceFailure : std::uint32_t
    {''',
'''    inline constexpr float standing_qualification_seconds = 4.0f;
    inline constexpr float standing_mastery_seconds = 6.0f;
    inline constexpr float standing_neutral_arm_limit = 38.0f * pi / 180.0f;
    inline constexpr float standing_qualification_spin_limit = 0.16f;
    inline constexpr float standing_mastery_spin_limit = 0.08f;

    enum class MotionEvidenceFailure : std::uint32_t
    {''')
replace("src/ppo.hpp",
'''        unstable_joints = 1u << 5u,
        body_contact = 1u << 6u
    };''',
'''        unstable_joints = 1u << 5u,
        body_contact = 1u << 6u,
        non_neutral_posture = 1u << 7u,
        excessive_rotation = 1u << 8u
    };''')
replace("src/ppo.hpp",
'''        if ((mask & evidence_bit(MotionEvidenceFailure::no_stable_stance)) != 0u)
            return "NO SUSTAINED STANCE";''',
'''        if ((mask & evidence_bit(MotionEvidenceFailure::non_neutral_posture)) != 0u)
            return "ARMS NOT NEUTRAL";
        if ((mask & evidence_bit(MotionEvidenceFailure::excessive_rotation)) != 0u)
            return "UNCONTROLLED STANDING SPIN";
        if ((mask & evidence_bit(MotionEvidenceFailure::no_stable_stance)) != 0u)
            return "NO SUSTAINED STANCE";''')
replace("src/ppo.hpp",
'''        case sim::CourseStage::balance:
            if (environment.longest_stable_stance_seconds() < 3.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.maximum_joint_speed() > 12.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;''',
'''        case sim::CourseStage::balance:
            if (environment.longest_stable_stance_seconds() < standing_qualification_seconds)
                rejection |= evidence_bit(MotionEvidenceFailure::no_stable_stance);
            if (environment.maximum_upper_body_motor_deviation()
                > standing_neutral_arm_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::non_neutral_posture);
            if (environment.uncontrolled_spin_turns()
                > standing_qualification_spin_limit)
                rejection |= evidence_bit(MotionEvidenceFailure::excessive_rotation);
            if (environment.maximum_joint_speed() > 10.0f)
                rejection |= evidence_bit(MotionEvidenceFailure::unstable_joints);
            break;''')
replace("src/ppo.hpp",
'''                quality_bucket(environment.elapsed_seconds()),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.maximum_joint_speed(), 100.0f)));''',
'''                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.maximum_upper_body_motor_deviation(), 1000.0f)),
                static_cast<std::uint16_t>(65535u
                    - quality_bucket(environment.uncontrolled_spin_turns(), 1000.0f)));''')
replace("src/ppo.hpp",
'''            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && (environment.left_supported() || environment.right_supported());''',
'''            return environment.stable_stance_seconds() >= 1.0f
                && environment.uprightness() >= 0.82f
                && environment.maximum_upper_body_motor_deviation()
                    <= standing_neutral_arm_limit
                && environment.uncontrolled_spin_turns()
                    <= standing_qualification_spin_limit
                && (environment.left_supported() || environment.right_supported());''')

# A finite complete training frame is always publishable for diagnosis. Valid
# stage samples outrank intact failures, but broken attempts remain visible with
# their rejection banner instead of leaving a blank PIP for millions of steps.
replace("src/ppo.hpp",
'''    [[nodiscard]] inline bool policy_candidate_better(std::uint64_t quality,
        float score, std::uint64_t best_quality, float best_score, bool has_best) noexcept''',
'''    [[nodiscard]] inline bool training_preview_frame_renderable(
        const sim::Environment& environment) noexcept
    {
        const auto particles = environment.particles();
        if (!environment.blueprint().valid() || particles.empty()
            || particles.size() != environment.blueprint().nodes.size())
            return false;
        for (const sim::Particle& particle : particles)
        {
            if (!std::isfinite(particle.position.x) || !std::isfinite(particle.position.y)
                || !std::isfinite(particle.previous.x) || !std::isfinite(particle.previous.y))
                return false;
        }
        return true;
    }

    [[nodiscard]] inline int training_preview_priority(sim::CourseStage stage,
        const sim::Environment& environment) noexcept
    {
        if (!training_preview_frame_renderable(environment))
            return 0;
        if (stage_display_sample_eligible(stage, environment))
            return 4;
        if (environment.body_integrity_valid()
            && stage_motion_qualification(stage, environment).valid)
            return 3;
        if (environment.body_integrity_valid())
            return 2;
        return 1;
    }

    [[nodiscard]] inline bool policy_candidate_better(std::uint64_t quality,
        float score, std::uint64_t best_quality, float best_score, bool has_best) noexcept''')

# Strict balance mastery is directly unit-testable and uses the same values as
# the evaluator. No hidden mismatch can make 0/8 permanent again.
replace("src/autonomy.hpp",
'''    inline constexpr int mastery_lock_confirmations = 8;
    struct AutonomyStatus''',
'''    inline constexpr int mastery_lock_confirmations = 8;

    [[nodiscard]] inline bool strict_balance_mastery(
        const TrainingMetrics& metrics) noexcept
    {
        return metrics.evaluation_valid
            && metrics.evaluation_invalid_runs == 0u
            && metrics.evaluation_longest_stance >= standing_mastery_seconds
            && metrics.evaluation_survival >= standing_mastery_seconds
            && metrics.evaluation_spin_turns <= standing_mastery_spin_limit
            && metrics.evaluation_max_joint_speed <= 8.0f;
    }

    struct AutonomyStatus''')
replace("src/autonomy_curriculum.cpp",
'''        case sim::CourseStage::balance:
            return metrics.evaluation_longest_stance >= 5.0f
                && metrics.evaluation_survival >= 6.0f
                && metrics.evaluation_max_joint_speed <= 10.0f;''',
'''        case sim::CourseStage::balance:
            return strict_balance_mastery(metrics);''')
replace("src/autonomy_curriculum.cpp",
'''        else
        {
            worker_message_ = std::format("{} - STRICT MASTERY {}/{}",
                sim::course_stage_name(stage_), mastery_streak_, mastery_lock_confirmations);
        }''',
'''        else
        {
            if (stage_ == sim::CourseStage::balance)
            {
                const std::uint32_t valid_seeds = 6u
                    - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                worker_message_ = std::format(
                    "STAGE VALID {}/6 SEEDS - STRICT STAND {:.1f}/{:.1f}S  SPIN {:.2f}/{:.2f}  MASTERY {}/{}",
                    valid_seeds, metrics.evaluation_longest_stance,
                    standing_mastery_seconds, metrics.evaluation_spin_turns,
                    standing_mastery_spin_limit, mastery_streak_,
                    mastery_lock_confirmations);
            }
            else
            {
                worker_message_ = std::format("{} - STRICT MASTERY {}/{}",
                    sim::course_stage_name(stage_), mastery_streak_,
                    mastery_lock_confirmations);
            }
        }''')

# Let evaluation run long enough to measure strict mastery. Balance spin telemetry
# uses uncontrolled standing rotation rather than flip-only rotation.
replace("src/ppo_parallel.cpp",
'''                                if (current_stage == sim::CourseStage::balance
                                    && environment.valid_motion()
                                    && environment.longest_stable_stance_seconds() >= 3.0f)
                                    break;''',
'''                                if (current_stage == sim::CourseStage::balance
                                    && environment.valid_motion()
                                    && environment.longest_stable_stance_seconds()
                                        >= standing_mastery_seconds)
                                    break;''')
replace("src/ppo_parallel.cpp",
'''                            totals.spin_turns += environment.maximum_flip_turns();''',
'''                            totals.spin_turns += current_stage == sim::CourseStage::balance
                                ? environment.uncontrolled_spin_turns()
                                : environment.maximum_flip_turns();''')

# Honest always-on PIP publication with deterministic priority.
replace("src/autonomy_persistence.cpp",
'''        const sim::Environment* representative = nullptr;
        std::uint64_t representative_quality = 0u;
        float representative_tiebreak = -std::numeric_limits<float>::infinity();''',
'''        const sim::Environment* representative = nullptr;
        int representative_priority = 0;
        std::uint64_t representative_quality = 0u;
        float representative_tiebreak = -std::numeric_limits<float>::infinity();''')
replace("src/autonomy_persistence.cpp",
'''            const bool stage_eligible = stage_display_sample_eligible(stage_, environment);
            const bool structurally_renderable = !environment.particles().empty()
                && environment.body_integrity_valid();
            if (!structurally_renderable)
                continue;
            const std::uint64_t display_quality = qualification.valid
                ? qualification.quality_key
                : pack_quality(
                    static_cast<std::uint16_t>(stage_eligible ? 2u : 1u),''',
'''            const bool stage_eligible = stage_display_sample_eligible(stage_, environment);
            const int display_priority = training_preview_priority(stage_, environment);
            if (display_priority == 0)
                continue;
            const std::uint64_t display_quality = qualification.valid
                ? qualification.quality_key
                : pack_quality(
                    static_cast<std::uint16_t>(stage_eligible ? 3u
                        : environment.body_integrity_valid() ? 2u : 1u),''')
replace("src/autonomy_persistence.cpp",
'''            if (representative == nullptr
                || display_quality > representative_quality
                || (display_quality == representative_quality
                    && tiebreak > representative_tiebreak))''',
'''            if (representative == nullptr
                || display_priority > representative_priority
                || (display_priority == representative_priority
                    && (display_quality > representative_quality
                        || (display_quality == representative_quality
                            && tiebreak > representative_tiebreak))))''')
replace("src/autonomy_persistence.cpp",
'''                representative = &environment;
                representative_quality = display_quality;''',
'''                representative = &environment;
                representative_priority = display_priority;
                representative_quality = display_quality;''')

# PIP status is stage-aware rather than reporting NOT CROUCHING during stand.
replace("src/app.cpp",
'''            const bool moving_crouch = environment.duck_active()
                && environment.crouch_walk_seconds() > 0.0f;
            const Color state_color = qualification.valid ? green
                : intact && foot_only && moving_crouch ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "QUALIFIED"
                : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY TOUCHED GROUND"
                : !environment.duck_active() ? "NOT CROUCHING"
                : "LEARNING";''',
'''            const Color state_color = qualification.valid ? green
                : intact && foot_only ? yellow : danger;
            const std::string_view state_text = qualification.valid
                ? "STAGE VALID"
                : !intact ? "BROKEN RIG"
                : !foot_only ? "BODY TOUCHED GROUND"
                : rl::primary_motion_rejection_name(qualification.rejection_mask);''')
replace("src/app.cpp",
'''            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                    trainer.metrics().update,
                    environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                    environment.gait_cycles(), environment.obstacles_passed()),
                0.72f, state_color, rect.size.x - 24.0f, 0.58f);''',
'''            const std::string pip_metrics = environment.course_stage() == sim::CourseStage::balance
                ? std::format("UPDATE {}  STANCE {:.1f}/{:.1f}S  SPIN {:.2f}  ARMS {:.0f} DEG",
                    trainer.metrics().update,
                    environment.longest_stable_stance_seconds(),
                    rl::standing_mastery_seconds,
                    environment.uncontrolled_spin_turns(),
                    environment.maximum_upper_body_motor_deviation() * 180.0f / pi)
                : std::format("UPDATE {}  CROUCH {:.1f}S  {:.2f}M  STEPS {}  PASSED {}",
                    trainer.metrics().update,
                    environment.crouch_walk_seconds(), environment.crouch_walk_distance(),
                    environment.gait_cycles(), environment.obstacles_passed());
            add_text_fit(canvas, rect.position + Vec2{ 12.0f, rect.size.y - 23.0f },
                pip_metrics, 0.72f, state_color, rect.size.x - 24.0f, 0.58f);''')
replace("src/app.cpp",
'''                add_text_fit(canvas, cursor,
                    std::format("STANCE {:.1f}/{:.1f} S   DUCK REC {:.1f}",
                        metrics.evaluation_stable_stance,
                        metrics.evaluation_longest_stance,
                        metrics.evaluation_duck_recoveries),
                    1.05f, metrics.evaluation_valid ? green : danger, usable_width);
                cursor.y += 29.0f;
                add_text_fit(canvas, cursor, std::format("QUALITY {:016X}   {}",
                    metrics.evaluation_quality_key,
                    rl::primary_motion_rejection_name(metrics.evaluation_rejection_mask)),
                    0.98f, metrics.evaluation_valid ? accent : danger,
                    usable_width, 0.82f);''',
'''                if (autonomy.stage == sim::CourseStage::balance)
                {
                    add_text_fit(canvas, cursor,
                        std::format("STANCE CUR {:.1f} LONG {:.1f} TARGET {:.1f} S",
                            metrics.evaluation_stable_stance,
                            metrics.evaluation_longest_stance,
                            rl::standing_mastery_seconds),
                        1.00f, metrics.evaluation_valid ? green : danger, usable_width);
                    cursor.y += 29.0f;
                    const std::uint32_t valid_seeds = 6u
                        - std::min<std::uint32_t>(metrics.evaluation_invalid_runs, 6u);
                    add_text_fit(canvas, cursor,
                        std::format("SPIN {:.2f}/{:.2f}   VALID SEEDS {}/6",
                            metrics.evaluation_spin_turns,
                            rl::standing_mastery_spin_limit, valid_seeds),
                        0.98f, rl::strict_balance_mastery(metrics) ? green : yellow,
                        usable_width, 0.82f);
                }
                else
                {
                    add_text_fit(canvas, cursor,
                        std::format("STANCE {:.1f}/{:.1f} S   DUCK REC {:.1f}",
                            metrics.evaluation_stable_stance,
                            metrics.evaluation_longest_stance,
                            metrics.evaluation_duck_recoveries),
                        1.05f, metrics.evaluation_valid ? green : danger, usable_width);
                    cursor.y += 29.0f;
                    add_text_fit(canvas, cursor, std::format("QUALITY {:016X}   {}",
                        metrics.evaluation_quality_key,
                        rl::primary_motion_rejection_name(metrics.evaluation_rejection_mask)),
                        0.98f, metrics.evaluation_valid ? accent : danger,
                        usable_width, 0.82f);
                }''')

# Deterministic acceptance coverage for geometry, strict values, arms/spin, and
# nonblank diagnostic PIP fallback.
replace("tests/core_tests.cpp",
'''        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.stable_stance_seconds_ = 3.5f;
            environment.longest_stable_stance_seconds_ = 3.5f;
            environment.maximum_joint_speed_ = 0.5f;
        }''',
'''        static void qualify_stable_stance(Environment& environment) noexcept
        {
            environment.invalid_reason_ = InvalidMotion::none;
            environment.non_foot_grounded_ = false;
            environment.elapsed_seconds_ = 6.5f;
            environment.stable_stance_seconds_ = 6.5f;
            environment.longest_stable_stance_seconds_ = 6.5f;
            environment.maximum_joint_speed_ = 0.5f;
            environment.uncontrolled_spin_turns_ = 0.0f;
        }

        static void force_standing_spin(Environment& environment, float turns) noexcept
        {
            environment.uncontrolled_spin_turns_ = turns;
        }

        static void force_arms_overhead(Environment& environment) noexcept
        {
            if (environment.particles_.size() < 13u)
                return;
            const Vec2 left = environment.particles_[7].position;
            const Vec2 right = environment.particles_[10].position;
            environment.particles_[8].position = left + Vec2{ -0.08f, 0.70f };
            environment.particles_[9].position = left + Vec2{ -0.02f, 1.34f };
            environment.particles_[11].position = right + Vec2{ 0.08f, 0.70f };
            environment.particles_[12].position = right + Vec2{ 0.02f, 1.34f };
            for (const std::size_t index : { 8u, 9u, 11u, 12u })
                environment.particles_[index].previous = environment.particles_[index].position;
        }''')
replace("tests/core_tests.cpp",
'''    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.nodes.size() >= 17,''',
'''    const sim::CreatureBlueprint humanoid = sim::CreatureBlueprint::humanoid();
    require(humanoid.torso_node < humanoid.nodes.size()
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[7].y + 0.10f
            && humanoid.nodes[humanoid.torso_node].y > humanoid.nodes[10].y + 0.10f,
        "humanoid central shoulder pivot is not above both lateral shoulder pivots");
    require(humanoid.nodes[8].y < humanoid.nodes[7].y
            && humanoid.nodes[11].y < humanoid.nodes[10].y,
        "humanoid rest arms do not hang below the shoulder pivots");
    require(humanoid.nodes.size() >= 17,''')
replace("tests/core_tests.cpp",
'''            const bool lesson_complete = assisted_stance.valid_motion()
                && assisted_stance.longest_stable_stance_seconds() >= 3.0f;''',
'''            const bool lesson_complete = assisted_stance.valid_motion()
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds;''')
replace("tests/core_tests.cpp",
'''        require(qualification.valid,
            "shared balance controller cannot sustain a stage-valid physics stance");''',
'''        require(qualification.valid
                && assisted_stance.longest_stable_stance_seconds()
                    >= rl::standing_mastery_seconds,
            "shared balance controller cannot sustain a strict neutral physics stance");''')
replace("tests/core_tests.cpp",
'''                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds() >= 3.0f)
                    break;''',
'''                if (environment.valid_motion()
                    && environment.longest_stable_stance_seconds()
                        >= rl::standing_mastery_seconds)
                    break;''')
replace("tests/core_tests.cpp",
'''        require(valid_agents >= 4u,
            "shared balance controller fails the robust four-of-six PPO seed gate");''',
'''        require(valid_agents == evaluation_agents,
            "shared balance controller fails the strict six-of-six PPO seed gate");''')
replace("tests/core_tests.cpp",
'''        require(!rl::stage_display_sample_eligible(sim::CourseStage::balance, intact),
            "detached feet can still publish into the training preview");
    }''',
'''        require(!rl::stage_display_sample_eligible(sim::CourseStage::balance, intact),
            "detached feet can still publish as a qualified training preview");
        require(rl::training_preview_frame_renderable(intact)
                && rl::training_preview_priority(sim::CourseStage::balance, intact) == 1,
            "finite rejected training attempts disappear instead of remaining diagnosable in PIP");
    }''')
# Add explicit arms/spin and mastery metric tests after authority test.
anchor = '''        require(effective_arms < effective_legs * 0.40f + 0.02f,
            "early balance still grants more authority to arms than legs");
    }
'''
addition = anchor + '''
    {
        sim::Environment neutral_stance{ humanoid, 0x576A6Eu };
        sim::EnvironmentTestAccess::qualify_stable_stance(neutral_stance);
        require(rl::stage_motion_qualification(
                sim::CourseStage::balance, neutral_stance).valid,
            "neutral strict standing evidence is rejected");
        sim::EnvironmentTestAccess::force_arms_overhead(neutral_stance);
        const auto raised = rl::stage_motion_qualification(
            sim::CourseStage::balance, neutral_stance);
        require(!raised.valid
                && (raised.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::non_neutral_posture)) != 0u,
            "arms-up standing exploit is still stage-valid");

        sim::Environment spinning_stance{ humanoid, 0x5A1E7u };
        sim::EnvironmentTestAccess::qualify_stable_stance(spinning_stance);
        sim::EnvironmentTestAccess::force_standing_spin(
            spinning_stance, rl::standing_qualification_spin_limit + 0.01f);
        const auto spinning = rl::stage_motion_qualification(
            sim::CourseStage::balance, spinning_stance);
        require(!spinning.valid
                && (spinning.rejection_mask & rl::evidence_bit(
                    rl::MotionEvidenceFailure::excessive_rotation)) != 0u,
            "rotating standing exploit is still stage-valid");

        rl::TrainingMetrics strict{};
        strict.evaluation_valid = true;
        strict.evaluation_invalid_runs = 0u;
        strict.evaluation_longest_stance = rl::standing_mastery_seconds;
        strict.evaluation_survival = rl::standing_mastery_seconds;
        strict.evaluation_spin_turns = rl::standing_mastery_spin_limit;
        strict.evaluation_max_joint_speed = 7.5f;
        require(rl::strict_balance_mastery(strict),
            "strict standing values cannot advance mastery");
        strict.evaluation_invalid_runs = 1u;
        require(!rl::strict_balance_mastery(strict),
            "partial seed success still advances strict standing mastery");
    }
'''
if addition not in load("tests/core_tests.cpp"):
    text = load("tests/core_tests.cpp")
    if anchor not in text:
        raise RuntimeError("authority test anchor missing")
    save("tests/core_tests.cpp", text.replace(anchor, addition, 1))

# Update the stale test success label.
text = load("tests/core_tests.cpp")
text = text.replace("Runner v0.7.4 obstacle, duck-press, integrity, telemetry, concurrency, gait, and rig-edit tests passed",
                    "Runner v0.7.6 standing, PIP, obstacle, integrity, telemetry, concurrency, gait, and rig-edit tests passed")
save("tests/core_tests.cpp", text)

# Ledger and release notes.
mission = load("missioncache.md")
mission = mission.replace("**Target:** Runner v0.7.5",
                          "**Target:** Runner v0.7.6", 1)
mission = mission.replace(
    "**Release state:** PUBLISHED - v0.7.5 assets independently audited; awaiting Adam's live packaged-runtime confirmation",
    "**Release state:** IN PROGRESS - v0.7.5 live screenshot reopened standing mastery and PIP acceptance",
    1,
)
entry = '''

## v0.7.6 standing mastery and live-PIP correction

### WALK-STAND-080 — Make strict standing mastery attainable and honest
**Status:** IN PROGRESS

The v0.7.5 evaluator stopped each standing trial at three seconds while strict mastery required a longer result, leaving the display at STAGE VALID but STRICT MASTERY 0/8 indefinitely. Evaluation now continues through the same six-second strict target used by mastery. Strict success requires all six seeded evaluations, six seconds of neutral stable stance, low joint speed, and near-zero uncontrolled rotation. The UI exposes the exact target, seed count, spin threshold, and failure reason.

### WALK-SHOULDER-081 — Raise the humanoid central shoulder pivot
**Status:** IN PROGRESS

The humanoid central chest/shoulder pivot sits above both lateral shoulder pivots. The calibrated rest arms hang below the shoulders, the standing teacher returns upper-body motors toward neutral, and arms-overhead standing is rejected rather than promoted.

### WALK-PIP-082 — Never hide the active training result
**Status:** IN PROGRESS

The PIP always publishes the best current finite full-rig training environment. A qualified intact sample has highest priority, but rejected or broken finite attempts remain visible with their exact rejection banner instead of leaving a blank WAITING frame. Standing PIP telemetry shows stance versus target, uncontrolled spin, and upper-body angle.

### WALK-RELEASE-083 — Publish audited Runner v0.7.6
**Status:** IN PROGRESS

Build and test Linux and the complete Windows Vulkan application; verify all deterministic suites, installed executable, executable-relative run.bat, package diagnostics, ZIP, checksum, manifest, and re-downloaded release assets. Remove temporary workflows and branches after publication. Live screenshot acceptance remains pending and contradictory behavior reopens the exact mission.
'''
if "### WALK-STAND-080" not in mission:
    mission += entry
save("missioncache.md", mission)

notes = '''# Runner v0.7.6

- Fixes the impossible standing-mastery loop: evaluation now reaches the same six-second target required by strict mastery.
- Requires six-of-six seeded strict standing results before one of eight mastery confirmations is counted.
- Rejects arms-overhead standing, uncontrolled standing rotation, non-foot contact, violent joints, and short stance results.
- Raises the humanoid central shoulder/chest pivot above both lateral shoulder pivots and restores hanging neutral arm geometry.
- Keeps the training PIP populated with the best current finite training environment, including rejected attempts and exact failure reasons.
- Shows standing target time, valid evaluation seeds, spin threshold, and upper-body angle directly in the UI.
- Invalidates v0.7.5 standing checkpoints and autosaves so the accepted arms-up/spinning controller cannot resume as progress.
- Carries deformable sand terrain and falling-material/burial recovery missions forward unchanged.
'''
save("RELEASE_NOTES_v0.7.6.md", notes)

Path(__file__).unlink()
print("materialized Runner v0.7.6 standing and PIP correction")
