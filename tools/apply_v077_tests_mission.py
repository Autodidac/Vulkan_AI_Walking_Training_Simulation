from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def save(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8", newline="\n")


def replace_once(path: str, old: str, new: str) -> None:
    text = load(path)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"missing target in {path}: {old[:180]!r}")
    save(path, text.replace(old, new, 1))


replace_once("tests/core_tests.cpp",
'''        static float primary_support_gap(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.right_contact_node].position.x
                - environment.particles_[environment.blueprint_.left_contact_node].position.x;
        }
''',
'''        static float primary_support_gap(const Environment& environment) noexcept
        {
            return environment.particles_[environment.blueprint_.right_contact_node].position.x
                - environment.particles_[environment.blueprint_.left_contact_node].position.x;
        }

        static float minimum_semantic_support_clearance(
            const Environment& environment) noexcept
        {
            std::array<std::uint16_t, 32> supports{};
            std::size_t count = 0;
            for (std::size_t index = 0; index < environment.particles_.size(); ++index)
            {
                if (environment.blueprint_.is_support_seed(index))
                    supports[count++] = static_cast<std::uint16_t>(index);
            }
            float minimum = 1000.0f;
            for (std::size_t first = 0; first < count; ++first)
            {
                for (std::size_t second = first + 1; second < count; ++second)
                {
                    const Particle& lhs = environment.particles_[supports[first]];
                    const Particle& rhs = environment.particles_[supports[second]];
                    const float clearance = std::abs(rhs.position.x - lhs.position.x)
                        - lhs.radius - rhs.radius;
                    minimum = std::min(minimum, clearance);
                }
            }
            return minimum;
        }
''')

anchor = '''    sim::Environment fused_feet(sim::CreatureBlueprint::humanoid(), 19);
    sim::EnvironmentTestAccess::force_fused_supports(fused_feet);
    sim::EnvironmentTestAccess::separate_supports(fused_feet);
    require(sim::EnvironmentTestAccess::primary_support_gap(fused_feet) > 0.18f,
        "left and right feet can remain fused into one support blob");'''
addition = anchor + '''

    const std::array<sim::CreatureBlueprint, 7> support_presets{
        sim::CreatureBlueprint::chicken(),
        sim::CreatureBlueprint::biped(),
        sim::CreatureBlueprint::humanoid(),
        sim::CreatureBlueprint::quadruped(),
        sim::CreatureBlueprint::crawler4(),
        sim::CreatureBlueprint::hexapod(),
        sim::CreatureBlueprint::monoped()
    };
    for (std::size_t preset = 0; preset < support_presets.size(); ++preset)
    {
        sim::Environment environment(support_presets[preset], 100u + preset);
        sim::EnvironmentTestAccess::force_fused_supports(environment);
        for (int iteration = 0; iteration < 12; ++iteration)
            sim::EnvironmentTestAccess::separate_supports(environment);
        require(sim::EnvironmentTestAccess::minimum_semantic_support_clearance(environment)
                > -0.005f,
            "a preset can retain fused semantic supports");
    }

    const sim::CreatureBlueprint humanoid_rig = sim::CreatureBlueprint::humanoid();
    const sim::CreatureBlueprint quad_rig = sim::CreatureBlueprint::quadruped();
    require(humanoid_rig.paired_leg_chains() && !quad_rig.paired_leg_chains(),
        "biped-only control path is not separated from quadruped control");

    sim::Environment neutral_humanoid(humanoid_rig, 131);
    neutral_humanoid.set_course(sim::CourseStage::balance, 0.25f);
    const auto standing_teacher = rl::balance_teacher_action(neutral_humanoid);
    require(std::abs(standing_teacher[0]) < 0.20f
            && std::abs(standing_teacher[2]) < 0.20f,
        "standing teacher still forces a jumping-jack hip pose");
    std::array<float, sim::action_count> exploratory{};
    exploratory.fill(0.80f);
    const auto residual_standing = rl::effective_policy_action(
        neutral_humanoid, exploratory, sim::CourseStage::balance);
    require(std::abs(residual_standing[0] - standing_teacher[0]) > 0.08f,
        "standing teacher still erases nearly all PPO exploration");

    sim::Environment neutral_quad(quad_rig, 137);
    neutral_quad.set_course(sim::CourseStage::balance, 0.25f);
    const auto quad_teacher = rl::balance_teacher_action(neutral_quad);
    require(std::ranges::all_of(quad_teacher, [](float value)
            {
                return std::abs(value) < 0.30f;
            }),
        "quadruped standing receives biped-like forced leg motion");

    sim::Environment crouch_humanoid(humanoid_rig, 141);
    crouch_humanoid.set_course(sim::CourseStage::duck_press, 0.30f);
    sim::EnvironmentTestAccess::set_duck_pressure(crouch_humanoid, 1.0f);
    const auto crouch_teacher = rl::duck_teacher_action(crouch_humanoid);
    require(std::abs(crouch_teacher[0]) < std::abs(crouch_teacher[1])
            && std::abs(crouch_teacher[2]) < std::abs(crouch_teacher[3]),
        "static crouch teacher still spreads hips before bending knees");

    const rl::MotorDiscoveryProbe positive_probe = rl::motor_discovery_probe(
        neutral_humanoid, 0u, 0u, 0u);
    const rl::MotorDiscoveryProbe negative_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count, 0u, 0u);
    const rl::MotorDiscoveryProbe synchronized_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u, 0u, 0u);
    const rl::MotorDiscoveryProbe alternating_probe = rl::motor_discovery_probe(
        neutral_humanoid, humanoid_rig.active_motor_count * 2u + 3u, 0u, 0u);
    require(positive_probe.weight > 0.80f && positive_probe.action[0] > 0.0f
            && negative_probe.action[0] < 0.0f,
        "motor discovery does not test one joint in both directions");
    require(std::ranges::all_of(
            std::span(synchronized_probe.action).first(humanoid_rig.active_motor_count),
            [](float value) { return value > 0.0f; }),
        "motor discovery does not test synchronized joint motion");
    require(alternating_probe.action[0] * alternating_probe.action[1] < 0.0f,
        "motor discovery does not test alternating joint motion");
    require(rl::motor_discovery_probe(neutral_humanoid, 0u, 480u, 0u).weight == 0.0f,
        "motor discovery never yields control back to PPO");'''
text = load("tests/core_tests.cpp")
if addition not in text:
    if anchor not in text:
        raise RuntimeError("fused-feet regression anchor missing")
    text = text.replace(anchor, addition, 1)
save("tests/core_tests.cpp", text)

mission = load("missioncache.md")
mission = mission.replace("**Target:** Runner v0.7.6",
                          "**Target:** Runner v0.7.7", 1)
mission = re.sub(r"\*\*Release state:\*\*[^\n]*",
                 "**Release state:** IN PROGRESS - v0.7.6 live screenshots reopened stance, crouch, exploration, and fused-support acceptance",
                 mission, count=1)
entry = '''

## v0.7.7 rig-specific learning and support correction

### WALK-RIGSTANCE-084 — Rig-specific standing controller
**Status:** IN PROGRESS

Standing control and qualification use each preset's authored body orientation and support topology. Quadrupeds, crawlers, hexapods, chickens, monopeds, bipeds, and humanoids may not be forced through one biped hip/knee correction. The quadruped must repeatedly establish a valid stage-one stance without being rearranged by diagonal support separation.

### WALK-CROUCH-085 — Restore leg-driven static crouch learning
**Status:** IN PROGRESS

Static crouch bends biped knees before spreading hips and compacts the authored support geometry for non-bipeds. It must hold beneath the platen, maintain feet-only ground support, retract, and recover to stable stance. The stage does not reward walking after the platen and may not qualify a jumping-jack stance.

### WALK-EXPLORE-086 — Preserve meaningful PPO exploration
**Status:** IN PROGRESS

Teacher guidance remains a bootstrap rather than the controller. Dedicated early rollout lanes test every motor alone in both directions, synchronized groups, and alternating patterns, with neutral recovery intervals. The compounded teacher blend leaves enough residual policy authority for visibly different candidates to branch, and the probes stop after initial motor discovery.

### WALK-FEET-087 — Separate every preset's semantic supports
**Status:** IN PROGRESS

Every pair of semantic support nodes receives non-overlap separation that preserves authored ordering. No preset may show fused feet, and the solver may not reorder a quadruped by assuming every left-channel contact is physically left of every right-channel contact.

### WALK-RELEASE-088 — Publish audited Runner v0.7.7
**Status:** IN PROGRESS

Build and test Linux and the complete Windows Vulkan package, verify the installed executable and run.bat from an unrelated directory, audit ZIP/checksum/manifest and re-downloaded release assets, then remove temporary workflows and branches. Live packaged-runtime screenshots remain the final acceptance authority.
'''
if "### WALK-RIGSTANCE-084" not in mission:
    mission = mission.rstrip() + entry + "\n"
save("missioncache.md", mission)

notes = '''# Runner v0.7.7

- Restores rig-specific standing: quadrupeds and other multi-support presets no longer receive biped-only hip/knee corrections.
- Tests every motor alone in both directions, synchronized groups, and alternating patterns during dedicated early discovery lanes.
- Reduces compounded teacher dominance so PPO retains meaningful residual exploration and can branch beyond one pose.
- Replaces global left/right support pushing with authored-order non-overlap separation for every semantic support node.
- Rejects humanoid and biped jumping-jack support spans during standing and static crouch qualification.
- Changes static crouch to knee-first or authored compact-support control, then rewards the hold and recovery rather than accidental post-platen walking.
- Invalidates v0.7.6 checkpoints and autonomy state so the failed standing/crouch policies cannot be reused as valid progress.
- Keeps deformable sand terrain and falling-material/burial recovery explicitly carried in `missioncache.md` for the subsequent terrain release.
'''
save("RELEASE_NOTES_v0.7.7.md", notes)

Path(__file__).unlink()
print("materialized v0.7.7 tests, release notes, and mission ledger")
