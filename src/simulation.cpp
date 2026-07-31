#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <system_error>

namespace epochrunner::sim
{
    namespace
    {
        constexpr float degrees_to_radians(float degrees) noexcept
        {
            return degrees * pi / 180.0f;
        }

        [[nodiscard]] bool direct_bone(const CreatureBlueprint& rig, std::uint16_t a, std::uint16_t b) noexcept
        {
            return std::ranges::any_of(rig.bones, [a, b](const DistanceConstraint& bone)
            {
                return (bone.a == a && bone.b == b) || (bone.a == b && bone.b == a);
            });
        }

        void add_passive_feet(CreatureBlueprint& rig, float heel_reach = 0.20f,
            float toe_reach = 0.34f) noexcept
        {
            auto add_foot = [&](std::uint16_t ankle)
            {
                if (ankle >= rig.nodes.size() || rig.nodes.size() > 124)
                    return;
                const Vec2 center = rig.nodes[ankle];
                const float radius = ankle < rig.radii.size()
                    ? clamp(rig.radii[ankle] * 0.72f, 0.10f, 0.15f) : 0.12f;
                const auto heel = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x - heel_reach, center.y - 0.01f });
                rig.radii.push_back(radius);
                const auto toe = static_cast<std::uint16_t>(rig.nodes.size());
                rig.nodes.push_back({ center.x + toe_reach, center.y - 0.015f });
                rig.radii.push_back(radius);
                rig.bones.push_back({ ankle, heel, 0.0f, 0.96f });
                rig.bones.push_back({ ankle, toe, 0.0f, 0.96f });
                rig.bones.push_back({ heel, toe, 0.0f, 0.88f });
            };
            add_foot(rig.left_contact_node);
            add_foot(rig.right_contact_node);
        }

        void calibrate_quadruped_stable_defaults(CreatureBlueprint& rig) noexcept
        {
            // The quadruped is the stable reference because its roughly one-metre
            // driven arms, symmetric travel, and moderate correction speed do not
            // launch the body. Preserve that effective endpoint displacement on
            // every body instead of copying a raw strength onto longer limbs.
            constexpr std::array<float, action_count> travel_degrees{ 22.0f, 30.0f, 22.0f, 30.0f };
            constexpr std::array<float, action_count> reference_linear_gain{ 0.0525f, 0.0575f, 0.0525f, 0.0575f };
            for (std::size_t index = 0; index < action_count; ++index)
            {
                const MotorConstraint& motor = rig.motors[index];
                const float driven_arm = motor.pivot < rig.nodes.size() && motor.c < rig.nodes.size()
                    ? length(rig.nodes[motor.c] - rig.nodes[motor.pivot]) : 1.0f;
                const float normalized_strength = clamp(
                    reference_linear_gain[index] / std::max(0.75f, driven_arm), 0.035f, 0.058f);
                rig.calibrate_motor(index, travel_degrees[index], travel_degrees[index], normalized_strength);
            }
        }
    }

    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.95f }, { 0.05f, 4.05f }, { 0.64f, 4.72f },
            { -0.45f, 1.78f }, { -0.30f, 0.32f },
            { 0.48f, 1.76f }, { 0.66f, 0.32f }, { -1.10f, 3.86f }
        };
        result.radii = { 0.28f, 0.32f, 0.30f, 0.20f, 0.18f, 0.20f, 0.18f, 0.16f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.92f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.74f }, { 0.0f, 3.84f }, { 0.02f, 4.64f },
            { -0.36f, 1.52f }, { -0.46f, 0.26f },
            { 0.36f, 1.52f }, { 0.46f, 0.26f }
        };
        result.radii = { 0.25f, 0.29f, 0.25f, 0.18f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        // Calibrated from the user's most human-like learned rig. Geometry and
        // asymmetric travel are retained; motor power is reduced by about 10%
        // so the useful proportions remain without the flip/fly exploit speed.
        CreatureBlueprint result{};
        result.nodes = {
            { -0.0034f, 2.8127f }, { -0.0148f, 4.0173f }, { -0.010f, 4.86f },
            { -0.3443f, 1.5514f }, { -0.4200f, 0.2500f },
            { 0.3400f, 1.6200f }, { 0.4200f, 0.2500f }
        };
        result.radii = { 0.26f, 0.31f, 0.27f, 0.19f, 0.17f, 0.19f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 1, 0, 5 }, MotorConstraint{ 0, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::quadruped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.05f }, { 1.55f, 2.08f }, { 2.35f, 2.42f },
            { -0.25f, 1.10f }, { -0.48f, 0.24f },
            { 1.72f, 1.08f }, { 1.92f, 0.24f }, { -1.05f, 2.35f }
        };
        result.radii = { 0.29f, 0.30f, 0.25f, 0.18f, 0.16f, 0.18f, 0.16f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 0.95f }, { 0, 7, 0.0f, 0.82f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 1, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.root_node = 0;
        result.torso_node = 1;
        result.head_node = 2;
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 4 },
            MotorConstraint{ 0, 1, 5 }, MotorConstraint{ 1, 5, 6 }
        };
        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.85f }, { 0.0f, 3.86f }, { 0.0f, 4.62f },
            { 0.0f, 1.62f }, { -0.34f, 0.22f },
            { 0.0f, 0.68f }, { 0.34f, 0.22f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.17f, 0.18f, 0.17f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.left_contact_node = 4;
        result.right_contact_node = 6;
        result.motors = {
            MotorConstraint{ 1, 0, 3 }, MotorConstraint{ 0, 3, 5 },
            MotorConstraint{ 3, 5, 4 }, MotorConstraint{ 3, 5, 6 }
        };
        add_passive_feet(result);
        result.rebuild_rest_lengths();
        calibrate_quadruped_stable_defaults(result);
        return result;
    }

    void CreatureBlueprint::rebuild_rest_lengths() noexcept
    {
        for (DistanceConstraint& bone : bones)
        {
            if (bone.a < nodes.size() && bone.b < nodes.size())
                bone.rest_length = std::max(0.05f, length(nodes[bone.b] - nodes[bone.a]));
        }
    }

    float CreatureBlueprint::rest_joint_angle(std::size_t motor_index) const noexcept
    {
        if (motor_index >= motors.size())
            return 0.0f;
        const MotorConstraint& motor = motors[motor_index];
        if (motor.a >= nodes.size() || motor.pivot >= nodes.size() || motor.c >= nodes.size()
            || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c)
            return 0.0f;
        return signed_angle(nodes[motor.a] - nodes[motor.pivot], nodes[motor.c] - nodes[motor.pivot]);
    }

    void CreatureBlueprint::calibrate_motor(std::size_t motor_index, float negative_degrees,
        float positive_degrees, float power) noexcept
    {
        if (motor_index >= motors.size())
            return;
        MotorConstraint& motor = motors[motor_index];
        const bool endpoints = motor.a < nodes.size() && motor.pivot < nodes.size() && motor.c < nodes.size()
            && motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
        motor.enabled = endpoints && direct_bone(*this, motor.a, motor.pivot) && direct_bone(*this, motor.pivot, motor.c);
        if (!endpoints)
            return;
        motor.neutral_angle = rest_joint_angle(motor_index);
        motor.minimum_angle = motor.neutral_angle - degrees_to_radians(clamp(std::abs(negative_degrees), 2.0f, 120.0f));
        motor.maximum_angle = motor.neutral_angle + degrees_to_radians(clamp(std::abs(positive_degrees), 2.0f, 120.0f));
        motor.strength = clamp(power, 0.0f, 0.20f);
    }

    void CreatureBlueprint::calibrate_all_motors(float degrees, float power) noexcept
    {
        for (std::size_t index = 0; index < motors.size(); ++index)
            calibrate_motor(index, degrees, degrees, power);
    }

    bool CreatureBlueprint::valid() const noexcept
    {
        if (nodes.size() < 3 || radii.size() != nodes.size() || bones.empty())
            return false;
        const auto semantic_valid = [this](std::uint16_t node) { return node < nodes.size(); };
        if (!semantic_valid(root_node) || !semantic_valid(torso_node) || !semantic_valid(head_node)
            || !semantic_valid(left_contact_node) || !semantic_valid(right_contact_node))
            return false;
        for (const DistanceConstraint& bone : bones)
        {
            if (bone.a >= nodes.size() || bone.b >= nodes.size() || bone.a == bone.b
                || !std::isfinite(bone.rest_length) || bone.rest_length <= 0.0f)
                return false;
        }
        for (const MotorConstraint& motor : motors)
        {
            if (!motor.enabled)
                continue;
            if (motor.a >= nodes.size() || motor.pivot >= nodes.size() || motor.c >= nodes.size()
                || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c
                || motor.minimum_angle > motor.neutral_angle || motor.neutral_angle > motor.maximum_angle
                || motor.strength < 0.0f || motor.strength > 0.20f
                || !direct_bone(*this, motor.a, motor.pivot)
                || !direct_bone(*this, motor.pivot, motor.c))
                return false;
        }
        return true;
    }

    std::uint64_t CreatureBlueprint::signature() const noexcept
    {
        std::uint64_t hash = 1469598103934665603ULL;
        auto add_u64 = [&](std::uint64_t value)
        {
            for (int byte = 0; byte < 8; ++byte)
            {
                hash ^= (value >> (byte * 8)) & 0xffULL;
                hash *= 1099511628211ULL;
            }
        };
        auto add_float = [&](float value) { add_u64(std::bit_cast<std::uint32_t>(value)); };

        add_u64(nodes.size()); add_u64(bones.size()); add_u64(motors.size());
        add_u64(root_node); add_u64(torso_node); add_u64(head_node);
        add_u64(left_contact_node); add_u64(right_contact_node);
        for (std::size_t index = 0; index < nodes.size(); ++index)
        {
            add_float(nodes[index].x); add_float(nodes[index].y);
            add_float(index < radii.size() ? radii[index] : 0.15f);
        }
        for (const DistanceConstraint& bone : bones)
        {
            add_u64(bone.a); add_u64(bone.b); add_float(bone.rest_length); add_float(bone.stiffness);
        }
        for (const MotorConstraint& motor : motors)
        {
            add_u64(motor.a); add_u64(motor.pivot); add_u64(motor.c); add_u64(motor.enabled ? 1 : 0);
            add_float(motor.minimum_angle); add_float(motor.maximum_angle);
            add_float(motor.neutral_angle); add_float(motor.strength);
        }
        return hash;
    }

    bool CreatureBlueprint::save(const std::filesystem::path& path, std::string& error) const
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

    CreatureBlueprint CreatureBlueprint::load(const std::filesystem::path& path, std::string& error)
    {
        std::ifstream input(path);
        if (!input)
        {
            error = "Could not open rig file: " + path.string();
            return humanoid();
        }

        std::string magic{};
        int version{};
        std::size_t node_count{};
        std::size_t bone_count{};
        std::size_t motor_count{};
        input >> magic >> version >> node_count >> bone_count >> motor_count;
        if (!input || magic != "EPOCHRIG" || (version != 1 && version != 2)
            || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)
        {
            error = "Invalid or unsupported EpochRunner rig file.";
            return humanoid();
        }

        CreatureBlueprint result{};
        if (version >= 2)
        {
            char semantic_tag{};
            input >> semantic_tag >> result.root_node >> result.torso_node >> result.head_node
                >> result.left_contact_node >> result.right_contact_node;
            if (!input || semantic_tag != 'S')
            {
                error = "Invalid rig semantic-node data.";
                return humanoid();
            }
        }

        result.nodes.reserve(node_count);
        result.radii.reserve(node_count);
        result.bones.reserve(bone_count);
        for (std::size_t index = 0; index < node_count; ++index)
        {
            char tag{};
            Vec2 node{};
            float radius{};
            input >> tag >> node.x >> node.y >> radius;
            if (!input || tag != 'N' || !std::isfinite(node.x) || !std::isfinite(node.y)
                || radius <= 0.0f || radius > 4.0f)
            {
                error = "Invalid node data in rig file.";
                return humanoid();
            }
            result.nodes.push_back(node);
            result.radii.push_back(radius);
        }

        if (version == 1)
        {
            result.root_node = 0;
            result.torso_node = static_cast<std::uint16_t>(std::min<std::size_t>(1, node_count - 1));
            result.head_node = static_cast<std::uint16_t>(std::min<std::size_t>(2, node_count - 1));
            result.left_contact_node = static_cast<std::uint16_t>(std::min<std::size_t>(4, node_count - 1));
            result.right_contact_node = static_cast<std::uint16_t>(std::min<std::size_t>(6, node_count - 1));
        }

        for (std::size_t index = 0; index < bone_count; ++index)
        {
            char tag{};
            DistanceConstraint bone{};
            input >> tag >> bone.a >> bone.b >> bone.rest_length >> bone.stiffness;
            if (!input || tag != 'B' || bone.a >= node_count || bone.b >= node_count || bone.a == bone.b
                || bone.rest_length <= 0.0f)
            {
                error = "Invalid bone data in rig file.";
                return humanoid();
            }
            bone.stiffness = clamp(bone.stiffness, 0.05f, 1.0f);
            result.bones.push_back(bone);
        }

        for (std::size_t index = 0; index < motor_count; ++index)
        {
            char tag{};
            MotorConstraint motor{};
            if (version >= 2)
            {
                int enabled{};
                input >> tag >> enabled >> motor.a >> motor.pivot >> motor.c
                    >> motor.minimum_angle >> motor.maximum_angle >> motor.neutral_angle >> motor.strength;
                motor.enabled = enabled != 0;
            }
            else
            {
                input >> tag >> motor.a >> motor.pivot >> motor.c
                    >> motor.minimum_angle >> motor.maximum_angle >> motor.neutral_angle >> motor.strength;
                motor.enabled = true;
            }
            if (!input || tag != 'M' || motor.a >= node_count || motor.pivot >= node_count || motor.c >= node_count
                || motor.a == motor.pivot || motor.pivot == motor.c || motor.a == motor.c
                || !std::isfinite(motor.minimum_angle) || !std::isfinite(motor.maximum_angle)
                || !std::isfinite(motor.neutral_angle) || motor.minimum_angle > motor.maximum_angle)
            {
                error = "Invalid motor data in rig file.";
                return humanoid();
            }
            motor.strength = clamp(motor.strength, 0.0f, 0.20f);
            motor.neutral_angle = clamp(motor.neutral_angle, motor.minimum_angle, motor.maximum_angle);
            result.motors[index] = motor;
        }

        if (!result.valid())
        {
            error = "Rig is structurally invalid or has invalid semantic nodes.";
            return humanoid();
        }
        error.clear();
        return result;
    }

    Environment::Environment()
        : Environment(CreatureBlueprint::humanoid(), 1)
    {
    }

    Environment::Environment(const CreatureBlueprint& blueprint, std::uint64_t seed)
        : blueprint_(blueprint), random_state_(seed == 0 ? 1 : seed)
    {
        reset(seed);
    }

    void Environment::set_blueprint(const CreatureBlueprint& blueprint)
    {
        blueprint_ = blueprint;
        reset(random_state_);
    }

    void Environment::set_course(CourseStage stage, float difficulty)
    {
        course_stage_ = stage;
        course_difficulty_ = clamp(difficulty, 0.10f, 1.0f);
        reset(random_state_);
    }

    float Environment::random_unit() noexcept
    {
        random_state_ ^= random_state_ >> 12;
        random_state_ ^= random_state_ << 25;
        random_state_ ^= random_state_ >> 27;
        const std::uint64_t value = random_state_ * 2685821657736338717ULL;
        return static_cast<float>(value >> 40) * (1.0f / 16777216.0f);
    }

    float Environment::ground_height_at(float x) const noexcept
    {
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return 0.0f;

        constexpr float segment_length = 7.0f;
        const float course_x = std::max(0.0f, x + course_progress());
        const int segment = static_cast<int>(std::floor(course_x / segment_length)) % 5;
        const float local = std::fmod(course_x, segment_length) / segment_length;
        const float smooth = local * local * (3.0f - 2.0f * local);
        const float amplitude = 0.18f + course_difficulty_ * 0.42f;

        float height = 0.0f;
        switch (segment)
        {
        case 0:
            height = 0.0f;
            break;
        case 1:
            height = amplitude * smooth;
            break;
        case 2:
            height = amplitude;
            break;
        case 3:
            height = amplitude * (1.0f - smooth);
            break;
        default:
            height = std::sin(local * pi) * amplitude * 0.78f;
            break;
        }

        if (course_stage_ >= CourseStage::uneven)
        {
            const float roughness = course_difficulty_ * 0.065f;
            height += std::sin(course_x * 0.83f) * roughness;
            height += std::sin(course_x * 2.17f + 0.7f) * roughness * 0.42f;
        }
        return std::max(-0.06f, height);
    }

    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        if (course_stage_ == CourseStage::balance || course_stage_ == CourseStage::walk)
            return;

        const float root_x = valid_node(blueprint_.root_node)
            ? particles_[blueprint_.root_node].position.x : 0.0f;
        constexpr float spacing = 5.5f;
        const float progress = course_progress();
        const int first_sequence = static_cast<int>(std::floor(progress / spacing));
        const float phase = std::fmod(progress, spacing);
        const float treadmill_velocity = -course_speed();

        auto variation_for = [](int sequence) noexcept
        {
            std::uint32_t value = static_cast<std::uint32_t>(sequence) * 747796405u + 2891336453u;
            value ^= value >> 16u;
            value *= 2246822519u;
            value ^= value >> 13u;
            return static_cast<float>(value & 0xffffu) / 65535.0f;
        };

        for (int offset = 0; offset < 10; ++offset)
        {
            const int sequence = first_sequence + offset;
            const int selector = ((sequence % 5) + 5) % 5;
            const float variation = variation_for(sequence);
            const float x = root_x + 4.5f + static_cast<float>(offset) * spacing - phase;
            const float ground = ground_height_at(x);

            CourseFeatureKind kind = CourseFeatureKind::rock;
            if (selector == 1 && course_stage_ >= CourseStage::hurdles)
                kind = CourseFeatureKind::hurdle;
            else if (selector == 2 && course_stage_ >= CourseStage::duck_bars)
                kind = CourseFeatureKind::overhead_bar;
            else if (selector == 3 && course_stage_ >= CourseStage::moving_hazards)
                kind = CourseFeatureKind::moving_hazard;
            else if (selector == 4 && course_stage_ >= CourseStage::moving_hazards)
                kind = CourseFeatureKind::projectile;
            else if (selector >= 3 && course_stage_ >= CourseStage::hurdles)
                kind = CourseFeatureKind::hurdle;

            switch (kind)
            {
            case CourseFeatureKind::rock:
            {
                const float radius = 0.16f + variation * (0.15f + course_difficulty_ * 0.08f);
                course_features_.push_back({
                    kind, { x, ground + radius }, {}, radius, { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::hurdle:
            {
                const float height = 0.24f + course_difficulty_ * 0.34f + variation * 0.12f;
                course_features_.push_back({
                    kind, { x, ground + height * 0.5f }, { 0.14f, height * 0.5f }, 0.0f,
                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::overhead_bar:
            {
                const float clearance = 3.65f - course_difficulty_ * 0.82f - variation * 0.16f;
                course_features_.push_back({
                    kind, { x, ground + clearance + 0.12f }, { 1.05f, 0.12f }, 0.0f,
                    { treadmill_velocity, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::moving_hazard:
            {
                const float oscillation = std::sin(
                    elapsed_seconds_ * (1.7f + course_difficulty_) + static_cast<float>(sequence));
                const float radius = 0.19f + course_difficulty_ * 0.10f;
                course_features_.push_back({
                    kind,
                    { x + oscillation * 0.85f, ground + 1.05f + oscillation * 0.38f },
                    {},
                    radius,
                    { treadmill_velocity + oscillation * 0.35f, 0.0f }
                });
                break;
            }
            case CourseFeatureKind::projectile:
            {
                const float throw_phase = std::fmod(
                    elapsed_seconds_ * (0.72f + course_difficulty_ * 0.28f)
                        + static_cast<float>(sequence) * 0.37f,
                    1.0f);
                const float throw_speed = 2.8f + course_difficulty_ * 2.2f;
                const float arc = 4.0f * throw_phase * (1.0f - throw_phase);
                const float radius = 0.14f + variation * 0.08f;
                course_features_.push_back({
                    kind,
                    { x + 2.2f - throw_phase * 4.4f, ground + 1.15f + arc * 1.55f },
                    {},
                    radius,
                    { treadmill_velocity - throw_speed, (1.0f - throw_phase * 2.0f) * 2.4f }
                });
                break;
            }
            }
        }
    }

    void Environment::reset(std::uint64_t seed)
    {
        if (seed != 0)
            random_state_ = seed;
        if (random_state_ == 0)
            random_state_ = 1;

        particles_.clear();
        particles_.reserve(blueprint_.nodes.size());
        const float phase = (random_unit() - 0.5f) * 0.04f;
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            Vec2 position = blueprint_.nodes[index];
            position.x += (random_unit() - 0.5f) * 0.008f + phase;
            position.y += (random_unit() - 0.5f) * 0.006f;
            const float radius = index < blueprint_.radii.size() ? blueprint_.radii[index] : 0.15f;
            particles_.push_back({ position, position, index == blueprint_.head_node ? 0.65f : 1.0f, radius, false });
        }

        previous_pelvis_ = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position : Vec2{};
        previous_angles_.fill(0.0f);
        angular_velocities_.fill(0.0f);
        for (std::size_t index = 0; index < action_count; ++index)
            previous_angles_[index] = joint_angle(blueprint_.motors[index]);
        elapsed_seconds_ = 0.0f;
        distance_travelled_ = 0.0f;
        forward_speed_ = 0.0f;
        last_reward_ = 0.0f;
        fallen_ = false;
        collision_count_ = 0.0f;
        airborne_seconds_ = 0.0f;
        cumulative_airborne_ = 0.0f;
        progress_window_seconds_ = 0.0f;
        progress_window_start_x_ = previous_pelvis_.x;
        micro_motion_seconds_ = 0.0f;
        action_energy_window_ = 0.0f;
        root_path_window_ = 0.0f;
        previous_root_for_path_ = previous_pelvis_;
        last_step_time_ = -100.0f;
        last_step_x_ = previous_pelvis_.x;
        maximum_speed_kmh_ = 0.0f;
        alternating_steps_ = 0;
        last_contact_side_ = 0;
        previous_left_grounded_ = false;
        previous_right_grounded_ = false;
        collided_this_step_ = false;
        recovery_active_ = false;
        recovery_started_seconds_ = 0.0f;
        recovery_best_upright_ = 1.0f;
        recovery_events_ = 0;
        recovery_successes_ = 0;
        invalid_reason_ = InvalidMotion::none;
        rebuild_course_features();
    }

    void Environment::solve_distance(const DistanceConstraint& constraint) noexcept
    {
        if (constraint.a >= particles_.size() || constraint.b >= particles_.size())
            return;
        Particle& lhs = particles_[constraint.a];
        Particle& rhs = particles_[constraint.b];
        const Vec2 delta = rhs.position - lhs.position;
        const float distance = length(delta);
        if (distance <= 1.0e-6f)
            return;
        const float weight = lhs.inverse_mass + rhs.inverse_mass;
        if (weight <= 1.0e-6f)
            return;
        const Vec2 correction = delta * ((distance - constraint.rest_length) / distance * constraint.stiffness);
        lhs.position += correction * (lhs.inverse_mass / weight);
        rhs.position -= correction * (rhs.inverse_mass / weight);
    }

    void Environment::solve_motor(const MotorConstraint& motor, float action) noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size()
            || motor.c >= particles_.size() || particles_.size() > 128)
            return;
        const Vec2 pivot = particles_[motor.pivot].position;
        const Vec2 reference_arm = particles_[motor.a].position - pivot;
        const Vec2 driven_arm = particles_[motor.c].position - pivot;
        if (length(reference_arm) <= 1.0e-5f || length(driven_arm) <= 1.0e-5f)
            return;
        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(reference_arm, driven_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;

        std::array<bool, 128> visited{};
        std::array<std::uint16_t, 128> stack{};
        std::size_t stack_size = 0;
        visited[motor.pivot] = true;
        visited[motor.a] = true;
        visited[motor.c] = true;
        stack[stack_size++] = motor.c;
        while (stack_size > 0)
        {
            const std::uint16_t node = stack[--stack_size];
            for (const DistanceConstraint& bone : blueprint_.bones)
            {
                std::uint16_t next = std::numeric_limits<std::uint16_t>::max();
                if (bone.a == node)
                    next = bone.b;
                else if (bone.b == node)
                    next = bone.a;
                if (next < particles_.size() && !visited[next])
                {
                    visited[next] = true;
                    stack[stack_size++] = next;
                }
            }
        }
        for (std::size_t index = 0; index < particles_.size(); ++index)
        {
            if (visited[index] && index != motor.a && index != motor.pivot)
                particles_[index].position = pivot + rotate(particles_[index].position - pivot, -correction);
        }
    }

    void Environment::solve_ground(float dt) noexcept
    {
        for (Particle& particle : particles_)
        {
            particle.grounded = false;
            const float minimum_y = ground_height_at(particle.position.x) + particle.radius;
            if (particle.position.y < minimum_y)
            {
                particle.position.y = minimum_y;
                particle.grounded = true;
                const Vec2 velocity = (particle.position - particle.previous) / dt;
                const float friction = std::abs(velocity.y) < 1.5f ? 0.72f : 0.90f;
                particle.previous.x = particle.position.x - velocity.x * dt * friction;
                if (velocity.y < 0.0f)
                    particle.previous.y = particle.position.y + velocity.y * dt * 0.05f;
            }
        }
    }

    void Environment::solve_course() noexcept
    {
        for (Particle& particle : particles_)
        {
            for (const CourseFeature& feature : course_features_)
            {
                if (feature.kind == CourseFeatureKind::moving_hazard
                    || feature.kind == CourseFeatureKind::rock
                    || feature.kind == CourseFeatureKind::projectile)
                {
                    const Vec2 delta = particle.position - feature.center;
                    const float distance = length(delta);
                    const float minimum = particle.radius + feature.radius;
                    if (distance >= minimum)
                        continue;
                    const Vec2 normal = distance > 1.0e-5f ? delta / distance : Vec2{ -1.0f, 0.0f };
                    const Vec2 correction = normal * (minimum - distance);
                    particle.position += correction;
                    particle.previous += correction * 0.25f;
                    if (feature.kind == CourseFeatureKind::projectile)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.34f;
                    else if (feature.kind == CourseFeatureKind::moving_hazard)
                        particle.previous -= feature.velocity * (1.0f / 60.0f) * 0.12f;
                    collided_this_step_ = true;
                    continue;
                }

                const Vec2 minimum = feature.center - feature.half_extent;
                const Vec2 maximum = feature.center + feature.half_extent;
                const Vec2 nearest{
                    clamp(particle.position.x, minimum.x, maximum.x),
                    clamp(particle.position.y, minimum.y, maximum.y)
                };
                Vec2 delta = particle.position - nearest;
                float distance = length(delta);
                if (distance >= particle.radius)
                    continue;
                if (distance <= 1.0e-5f)
                {
                    const float left = std::abs(particle.position.x - minimum.x);
                    const float right = std::abs(maximum.x - particle.position.x);
                    const float bottom = std::abs(particle.position.y - minimum.y);
                    const float top = std::abs(maximum.y - particle.position.y);
                    const float nearest_side = std::min({ left, right, bottom, top });
                    if (nearest_side == left) delta = { -1.0f, 0.0f };
                    else if (nearest_side == right) delta = { 1.0f, 0.0f };
                    else if (nearest_side == bottom) delta = { 0.0f, -1.0f };
                    else delta = { 0.0f, 1.0f };
                    distance = 0.0f;
                }
                const Vec2 normal = normalized(delta, { -1.0f, 0.0f });
                const Vec2 correction = normal * (particle.radius - distance);
                particle.position += correction;
                particle.previous += correction * 0.18f;
                collided_this_step_ = true;
            }
        }
    }

    float Environment::joint_angle(const MotorConstraint& motor) const noexcept
    {
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return motor.neutral_angle;
        return signed_angle(
            particles_[motor.a].position - particles_[motor.pivot].position,
            particles_[motor.c].position - particles_[motor.pivot].position);
    }

    bool Environment::valid_node(std::uint16_t index) const noexcept
    {
        return index < particles_.size() && index < blueprint_.nodes.size();
    }

    float Environment::torso_uprightness() const noexcept
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node))
            return 0.0f;
        const Vec2 current = normalized(
            particles_[blueprint_.torso_node].position - particles_[blueprint_.root_node].position,
            { 0.0f, 1.0f });
        const Vec2 desired = normalized(
            blueprint_.nodes[blueprint_.torso_node] - blueprint_.nodes[blueprint_.root_node],
            { 0.0f, 1.0f });
        return clamp(dot(current, desired), -1.0f, 1.0f);
    }

    void Environment::invalidate(InvalidMotion reason) noexcept
    {
        if (invalid_reason_ == InvalidMotion::none)
            invalid_reason_ = reason;
    }

    float Environment::airborne_ratio() const noexcept
    {
        return elapsed_seconds_ > 1.0e-5f ? clamp(cumulative_airborne_ / elapsed_seconds_, 0.0f, 1.0f) : 0.0f;
    }

    void Environment::update_gait_metrics(float dt, float action_energy) noexcept
    {
        const bool left = valid_node(blueprint_.left_contact_node) && particles_[blueprint_.left_contact_node].grounded;
        const bool right = valid_node(blueprint_.right_contact_node) && particles_[blueprint_.right_contact_node].grounded;
        const bool new_left = left && !previous_left_grounded_;
        const bool new_right = right && !previous_right_grounded_;
        const int strike_side = new_left == new_right ? 0 : (new_left ? -1 : 1);
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        if (strike_side != 0)
        {
            if (last_contact_side_ == 0)
            {
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
            else if (qualifies_alternating_step(last_contact_side_, strike_side,
                elapsed_seconds_ - last_step_time_, root_x - last_step_x_))
            {
                ++alternating_steps_;
                last_contact_side_ = strike_side;
                last_step_time_ = elapsed_seconds_;
                last_step_x_ = root_x;
            }
        }
        previous_left_grounded_ = left;
        previous_right_grounded_ = right;

        if (!left && !right)
        {
            airborne_seconds_ += dt;
            cumulative_airborne_ += dt;
        }
        else
        {
            airborne_seconds_ = 0.0f;
        }

        if (valid_node(blueprint_.root_node))
        {
            const Vec2 root = particles_[blueprint_.root_node].position;
            root_path_window_ += length(root - previous_root_for_path_);
            previous_root_for_path_ = root;
        }
        action_energy_window_ += action_energy * dt;
        progress_window_seconds_ += dt;
        if (progress_window_seconds_ >= 1.0f && valid_node(blueprint_.root_node))
        {
            const float net_progress = std::abs(root_x - progress_window_start_x_);
            const float average_energy = action_energy_window_ / progress_window_seconds_;
            const bool high_energy_stall = average_energy > 0.10f && net_progress < 0.05f;
            const bool inefficient_vibration = average_energy > 0.16f && net_progress < 0.12f
                && root_path_window_ > std::max(0.08f, net_progress * 2.5f);
            if (course_stage_ != CourseStage::balance && (high_energy_stall || inefficient_vibration))
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);
            progress_window_start_x_ = root_x;
            progress_window_seconds_ = 0.0f;
            action_energy_window_ = 0.0f;
            root_path_window_ = 0.0f;
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        if (airborne_seconds_ > allowed_airtime)
            invalidate(InvalidMotion::sustained_flight);
        if (micro_motion_seconds_ >= 3.0f)
            invalidate(InvalidMotion::micro_motion);
    }

    StepResult Environment::step(std::span<const float, action_count> actions, float dt)
    {
        if (!blueprint_.valid() || !valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node) || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return { -5.0f, 0.0f, true, false, InvalidMotion::out_of_bounds };

        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        // Let every body settle onto its feet before the policy can apply a
        // meaningful impulse, then ease control in rather than launching it.
        const float ramp_t = clamp((elapsed_seconds_ - 0.35f) / 1.25f, 0.0f, 1.0f);
        const float control_ramp = ramp_t * ramp_t * (3.0f - 2.0f * ramp_t);
        std::array<float, action_count> applied_actions{};
        for (std::size_t index = 0; index < action_count; ++index)
            applied_actions[index] = clamp(actions[index], -1.0f, 1.0f) * control_ramp;
        constexpr Vec2 gravity{ 0.0f, -22.0f };
        constexpr float damping = 0.996f;
        for (Particle& particle : particles_)
        {
            const Vec2 velocity = (particle.position - particle.previous) * damping;
            particle.previous = particle.position;
            particle.position += velocity + gravity * (dt * dt);
        }

        collided_this_step_ = false;
        rebuild_course_features();
        for (int iteration = 0; iteration < 12; ++iteration)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            for (std::size_t index = 0; index < action_count; ++index)
                solve_motor(blueprint_.motors[index], applied_actions[index]);
            solve_ground(dt);
            solve_course();
        }
        if (collided_this_step_)
            collision_count_ += 1.0f;

        elapsed_seconds_ += dt;
        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        const float frame_progress = pelvis_position.x - previous_pelvis_.x;
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;
        maximum_speed_kmh_ = std::max(maximum_speed_kmh_, std::max(std::abs(raw_speed), std::abs(forward_speed_)) * 3.6f);

        float action_energy = 0.0f;
        for (std::size_t index = 0; index < action_count; ++index)
        {
            const float effective = blueprint_.motors[index].enabled ? applied_actions[index] : 0.0f;
            action_energy += effective * effective;
            const float angle = joint_angle(blueprint_.motors[index]);
            angular_velocities_[index] = wrap_angle(angle - previous_angles_[index]) / dt;
            previous_angles_[index] = angle;
        }
        update_gait_metrics(dt, action_energy);

        const float upright = torso_uprightness();
        const float local_ground = ground_height_at(pelvis_position.x);
        const float torso_floor = local_ground + std::max(0.38f,
            (blueprint_.nodes[blueprint_.torso_node].y - blueprint_.nodes[blueprint_.root_node].y) * 0.28f);
        const float head_floor = local_ground + 0.34f;
        const float torso_y = particles_[blueprint_.torso_node].position.y;
        const float head_y = particles_[blueprint_.head_node].position.y;
        const bool geometric_fall = torso_y < torso_floor || head_y < head_floor;
        const bool hard_fall = torso_y < local_ground + 0.18f || head_y < local_ground + 0.12f;
        fallen_ = geometric_fall;

        float recovery_reward = 0.0f;
        const bool supported = particles_[blueprint_.left_contact_node].grounded
            || particles_[blueprint_.right_contact_node].grounded;
        if (!recovery_active_ && recovery_should_start(
            collided_this_step_, upright, geometric_fall, hard_fall))
        {
            recovery_active_ = true;
            recovery_started_seconds_ = elapsed_seconds_;
            recovery_best_upright_ = upright;
            ++recovery_events_;
        }
        if (recovery_active_)
        {
            const float improvement = upright - recovery_best_upright_;
            if (improvement > 0.0f)
                recovery_reward += improvement * 0.10f;
            recovery_best_upright_ = std::max(recovery_best_upright_, upright);
            const float recovery_time = elapsed_seconds_ - recovery_started_seconds_;
            if (upright >= 0.90f && supported && !geometric_fall && recovery_time >= 0.12f)
            {
                recovery_active_ = false;
                ++recovery_successes_;
                recovery_reward += 0.14f;
            }
            else if (hard_fall || recovery_time > 3.0f)
            {
                recovery_active_ = false;
                recovery_reward -= 0.10f;
            }
        }

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        const bool terminal_fall = recovery_terminal_fall(
            geometric_fall, hard_fall, recovery_active_);
        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, terminal_fall));

        const float left_contact = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        const float right_contact = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
        const float contact = left_contact + right_contact;
        const float gait = clamp(0.25f + static_cast<float>(alternating_steps_) * 0.12f, 0.25f, 1.0f);
        const float safe_progress = clamp(frame_progress, -0.015f, 0.065f);
        const float collision_penalty = collided_this_step_ ? 0.025f : 0.0f;

        if (course_stage_ == CourseStage::balance)
        {
            last_reward_ = std::max(0.0f, upright) * 0.030f
                + contact * 0.0030f
                - std::abs(forward_speed_) * 0.0040f
                - std::abs(distance_travelled_) * 0.0015f
                - action_energy * 0.0012f;
        }
        else
        {
            last_reward_ = std::max(0.0f, safe_progress) * 1.65f * gait
                + std::max(0.0f, upright) * 0.012f
                + contact * 0.0012f
                - std::max(0.0f, -safe_progress) * 0.45f
                - action_energy * 0.0010f
                - collision_penalty;
        }

        last_reward_ += recovery_reward;
        if (invalid_reason_ != InvalidMotion::none)
        {
            recovery_active_ = false;
            last_reward_ -= 5.0f;
        }
        const float timeout = course_stage_ == CourseStage::balance ? 12.0f : 20.0f;
        const bool terminated = invalid_reason_ != InvalidMotion::none || elapsed_seconds_ >= timeout;
        return { last_reward_, forward_speed_, terminated,
            invalid_reason_ == InvalidMotion::none, invalid_reason_ };
    }

    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        const Vec2 root = particles_[blueprint_.root_node].position;
        const Vec2 torso = normalized(particles_[blueprint_.torso_node].position - root, { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position - particles_[blueprint_.root_node].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * 60.0f / 6.0f, -3.0f, 3.0f);
        result[3] = clamp(pelvis_velocity.y * 60.0f / 6.0f, -3.0f, 3.0f);
        for (std::size_t index = 0; index < action_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            if (!motor.enabled)
                continue;
            const float delta = wrap_angle(joint_angle(motor) - motor.neutral_angle);
            const float span = delta < 0.0f
                ? std::max(0.001f, motor.neutral_angle - motor.minimum_angle)
                : std::max(0.001f, motor.maximum_angle - motor.neutral_angle);
            result[4 + index] = clamp(delta / span, -2.0f, 2.0f);
            result[8 + index] = clamp(angular_velocities_[index] / 18.0f, -3.0f, 3.0f);
        }
        result[12] = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        result[13] = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
        result[14] = clamp((particles_[blueprint_.left_contact_node].position.x - root.x) / 2.0f, -2.0f, 2.0f);
        result[15] = clamp((particles_[blueprint_.right_contact_node].position.x - root.x) / 2.0f, -2.0f, 2.0f);
        result[16] = clamp((root.y - ground_height_at(root.x)) / 5.0f, 0.0f, 2.0f);
        result[17] = recovery_active_ ? clamp(torso.y, -1.0f, 1.0f) : 1.0f;
        result[18] = clamp((ground_height_at(root.x + 0.65f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);
        result[19] = clamp((ground_height_at(root.x + 1.50f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);
        result[20] = clamp((ground_height_at(root.x + 3.00f) - ground_height_at(root.x)) / 1.0f, -1.0f, 1.0f);

        const CourseFeature* nearest = nullptr;
        float nearest_dx = std::numeric_limits<float>::max();
        for (const CourseFeature& feature : course_features_)
        {
            const float dx = feature.center.x - root.x;
            if (dx >= -0.3f && dx < nearest_dx)
            {
                nearest_dx = dx;
                nearest = &feature;
            }
        }
        if (nearest != nullptr)
        {
            result[21] = clamp(nearest_dx / 6.0f, -1.0f, 2.0f);
            switch (nearest->kind)
            {
            case CourseFeatureKind::hurdle: result[22] = -1.0f; break;
            case CourseFeatureKind::rock: result[22] = -0.5f; break;
            case CourseFeatureKind::overhead_bar: result[22] = 0.0f; break;
            case CourseFeatureKind::moving_hazard: result[22] = 0.5f; break;
            case CourseFeatureKind::projectile: result[22] = 1.0f; break;
            }
            result[23] = clamp((nearest->center.y - root.y) / 4.0f, -2.0f, 2.0f);
            result[24] = course_feature_observation_size(*nearest);
            result[25] = clamp(nearest->velocity.x / 5.0f, -1.0f, 1.0f);
        }
        result[26] = airborne_ratio();
        result[27] = clamp(static_cast<float>(alternating_steps_) / 10.0f, 0.0f, 2.0f);
        result[28] = static_cast<float>(course_stage_) / static_cast<float>(course_stage_count - 1);
        result[29] = course_difficulty_;
        return result;
    }
}
