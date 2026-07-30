from pathlib import Path
import re


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8", newline="\n")


def replace_between(source: str, start_token: str, end_token: str, replacement: str) -> str:
    start = source.find(start_token)
    if start < 0:
        raise SystemExit(f"start token not found: {start_token}")
    end = source.find(end_token, start)
    if end < 0:
        raise SystemExit(f"end token not found: {end_token}")
    return source[:start] + replacement + source[end:]


sim_path = "src/simulation.cpp"
sim = read(sim_path)
if "#include <system_error>" not in sim:
    sim = sim.replace("#include <string>\n", "#include <string>\n#include <system_error>\n", 1)

save_function = r'''    bool CreatureBlueprint::save(const std::filesystem::path& path, std::string& error) const
    {
        std::filesystem::path temporary = path;
        temporary += ".tmp";
        std::filesystem::path backup = path;
        backup += ".bak";

        std::error_code filesystem_error{};
        std::filesystem::remove(temporary, filesystem_error);
        filesystem_error.clear();
        {
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
            {
                error = "Could not open temporary rig file for writing: " + temporary.string();
                return false;
            }
            output << "EPOCHRIG 2\n";
            output << nodes.size() << ' ' << bones.size() << ' ' << motors.size() << '\n';
            output << "S " << root_node << ' ' << torso_node << ' ' << head_node << ' '
                << left_contact_node << ' ' << right_contact_node << '\n';
            output << std::setprecision(9);
            for (std::size_t index = 0; index < nodes.size(); ++index)
                output << "N " << nodes[index].x << ' ' << nodes[index].y << ' ' << radii[index] << '\n';
            for (const DistanceConstraint& bone : bones)
                output << "B " << bone.a << ' ' << bone.b << ' ' << bone.rest_length << ' ' << bone.stiffness << '\n';
            for (const MotorConstraint& motor : motors)
            {
                output << "M " << (motor.enabled ? 1 : 0) << ' ' << motor.a << ' ' << motor.pivot << ' ' << motor.c << ' '
                    << motor.minimum_angle << ' ' << motor.maximum_angle << ' '
                    << motor.neutral_angle << ' ' << motor.strength << '\n';
            }
            output.flush();
            if (!output)
            {
                output.close();
                std::filesystem::remove(temporary, filesystem_error);
                error = "Failed while writing temporary rig file: " + temporary.string();
                return false;
            }
        }

        std::filesystem::remove(backup, filesystem_error);
        filesystem_error.clear();
        bool moved_original = false;
        if (std::filesystem::exists(path, filesystem_error) && !filesystem_error)
        {
            std::filesystem::rename(path, backup, filesystem_error);
            if (filesystem_error)
            {
                std::filesystem::remove(temporary, filesystem_error);
                error = "Could not prepare existing rig for replacement: " + path.string();
                return false;
            }
            moved_original = true;
        }

        filesystem_error.clear();
        std::filesystem::rename(temporary, path, filesystem_error);
        if (filesystem_error)
        {
            std::error_code cleanup_error{};
            std::filesystem::remove(temporary, cleanup_error);
            if (moved_original)
            {
                cleanup_error.clear();
                std::filesystem::rename(backup, path, cleanup_error);
            }
            error = "Could not publish saved rig: " + filesystem_error.message();
            return false;
        }
        if (moved_original)
        {
            filesystem_error.clear();
            std::filesystem::remove(backup, filesystem_error);
        }
        error.clear();
        return true;
    }

'''
sim = replace_between(
    sim,
    "    bool CreatureBlueprint::save(",
    "    CreatureBlueprint CreatureBlueprint::load(",
    save_function,
)
write(sim_path, sim)

# A generated v0.4.1 staged-save patch accidentally embedded literal newlines
# inside a few normal C++ string/character literals. Repair any remaining
# instances without touching valid adjacent strings or raw strings.
for source_path in Path("src").glob("*.cpp"):
    source = read(str(source_path))
    previous = None
    while source != previous:
        previous = source
        source = re.sub(
            r'"([^"\r\n]*)\r?\n"',
            lambda match: '"' + match.group(1) + r'\n"',
            source,
        )
        source = source.replace("'\n'", r"'\n'")
    write(str(source_path), source)

# The pre-feet test still expected the old seven-node humanoid. The new body
# retains the original seven articulated nodes and adds two heel/toe nodes per
# foot, for eleven total.
test_path = "tests/core_tests.cpp"
tests = read(test_path)
old_test = '    require(humanoid.nodes.size() == 7, "human-calibrated rig should have pelvis, torso, head, knees, and feet");\n'
new_test = '    require(humanoid.nodes.size() == 11, "human-calibrated rig should include passive heel/toe feet");\n'
if old_test in tests:
    tests = tests.replace(old_test, new_test, 1)
elif new_test not in tests:
    raise SystemExit("humanoid node-count assertion was not found")
write(test_path, tests)

print("Repaired v0.4.1 generated source and stale feet assertion")
