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


replace_once(
    "src/simulation.hpp",
    '''    [[nodiscard]] inline bool knee_crosses_before_foot(float knee_front_x,
        float foot_front_x, float foot_top_y, const CourseFeature& feature) noexcept
    {
        if (feature.kind != CourseFeatureKind::rock
            && feature.kind != CourseFeatureKind::hurdle)
            return false;
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        return knee_front_x > feature.center.x
            && foot_front_x < obstacle_front - 0.02f
            && foot_top_y < course_feature_top(feature) + 0.08f;
    }
''',
    '''    [[nodiscard]] inline bool knee_crosses_before_foot(float knee_front_x,
        float foot_front_x, float foot_top_y, const CourseFeature& feature) noexcept
    {
        if (feature.kind != CourseFeatureKind::rock
            && feature.kind != CourseFeatureKind::hurdle)
            return false;

        // Natural stepping often puts a bent knee slightly ahead of the foot.
        // Reject only an obvious body/joint-first shove: the knee must lead well
        // into the obstacle while the foot is both substantially behind it and
        // still below useful clearance. This remains guidance, not a hard gate.
        const float obstacle_front = feature.center.x + course_feature_half_width(feature);
        const float obstacle_top = course_feature_top(feature);
        const float knee_lead = knee_front_x - feature.center.x;
        const float foot_lag = obstacle_front - foot_front_x;
        const float clearance_deficit = obstacle_top + 0.015f - foot_top_y;
        return knee_lead > 0.24f
            && foot_lag > 0.16f
            && clearance_deficit > 0.08f;
    }
''')

replace_once(
    "src/simulation.cpp",
    '''        const float knee_first_penalty = knee_first_this_step_ ? 0.11f : 0.0f;
''',
    '''        // This is intentionally a mild shaping penalty. Natural knee lead
        // is now tolerated; only a large low-foot body-first obstacle shove reaches here.
        const float knee_first_penalty = knee_first_this_step_ ? 0.028f : 0.0f;
''')

replace_once(
    "tests/core_tests.cpp",
    '''    require(sim::knee_crosses_before_foot(1.12f, 0.92f, 0.34f, rock_order),
        "knee-first rock traversal is not detected");
    require(!sim::knee_crosses_before_foot(1.12f, 1.32f, 0.34f, rock_order),
        "foot-first rock traversal is incorrectly penalized");
''',
    '''    require(!sim::knee_crosses_before_foot(1.12f, 0.92f, 0.34f, rock_order),
        "normal bent-knee lead is still over-constrained");
    require(sim::knee_crosses_before_foot(1.42f, 0.82f, 0.20f, rock_order),
        "egregious low-foot body-first rock shove is not detected");
    require(!sim::knee_crosses_before_foot(1.42f, 1.32f, 0.34f, rock_order),
        "foot-first rock traversal is incorrectly penalized");
    require(!sim::knee_crosses_before_foot(1.42f, 0.82f, 0.58f, rock_order),
        "useful foot clearance is incorrectly rejected because the knee leads");
''')

replace_once(
    "MISSIONS.md",
    '''Forward reward must represent foot-led alternating walking, not a body sliding across planted contacts. A knee may not clear a rock or hurdle before its corresponding foot. Sustained double-supported sliding is an invalid gait exploit.
''',
    '''Forward reward must represent alternating supported locomotion, not a body sliding across planted contacts. Natural knee lead and bent-leg clearance are allowed; only an egregious low-foot body/joint-first shove into a rock or hurdle receives a mild shaping penalty. Sustained double-supported sliding remains an invalid gait exploit.
''')

replace_once(
    "MISSIONS.md",
    '''- Knee-before-foot traversal over rocks or hurdles receives a strong per-step penalty and increments telemetry.
- Foot-first traversal is not penalized.
''',
    '''- A knee may lead naturally while the foot is rising, close to the obstacle, or already above useful clearance.
- Only a large knee lead with a substantially trailing, low foot receives a mild shaping penalty and increments telemetry.
- The joint-clearance rule never terminates an otherwise valid episode and never overrides learned get-up or obstacle strategies.
- Foot-first and useful-clearance traversal are not penalized.
''')

replace_once(
    "MISSIONS.md",
    '''- Biped and humanoid hips/knees have bounded travel sufficient to clear configured rocks and hurdles.
''',
    '''- Biped and humanoid hips/knees have bounded travel sufficient to clear configured rocks and hurdles without requiring an artificial foot-before-knee ordering.
''')

print("Applied relaxed joint-clearance guidance")
