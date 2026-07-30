#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>

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
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 20.0f, 20.0f, 0.052f);
        result.calibrate_motor(1, 28.0f, 28.0f, 0.057f);
        result.calibrate_motor(2, 20.0f, 20.0f, 0.052f);
        result.calibrate_motor(3, 28.0f, 28.0f, 0.057f);
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
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 18.0f, 16.0f, 0.052f);
        result.calibrate_motor(1, 22.0f, 24.0f, 0.047f);
        result.calibrate_motor(2, 18.0f, 16.0f, 0.052f);
        result.calibrate_motor(3, 22.0f, 24.0f, 0.047f);
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
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 13.9f, 8.0f, 0.053f);
        result.calibrate_motor(1, 12.4f, 15.8f, 0.043f);
        result.calibrate_motor(2, 11.6f, 8.6f, 0.056f);
        result.calibrate_motor(3, 8.0f, 30.6f, 0.052f);
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
        result.calibrate_motor(0, 22.0f, 22.0f, 0.053f);
        result.calibrate_motor(1, 30.0f, 30.0f, 0.058f);
        result.calibrate_motor(2, 22.0f, 22.0f, 0.053f);
        result.calibrate_motor(3, 30.0f, 30.0f, 0.058f);
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
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 22.0f, 22.0f, 0.052f);
        result.calibrate_motor(1, 28.0f, 28.0f, 0.056f);
        result.calibrate_motor(2, 20.0f, 20.0f, 0.047f);
        result.calibrate_motor(3, 20.0f, 20.0f, 0.047f);
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
                || motor.strength < 0.0f || motor.strength > 0.20f)
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
        std::ofstream output(path, std::ios::trunc);
        if (!output)
        {
            error = "Could not open rig file for writing: " + path.string();
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
        if (!output)
        {
            error = "Failed while writing rig file: " + path.string();
            return false;
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
        const float positive_x = std::max(0.0f, x);
        const float phase = std::fmod(positive_x, 8.0f) / 8.0f;
        const float triangle = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
        float height = triangle * (0.22f + course_difficulty_ * 0.28f);
        if (course_stage_ >= CourseStage::uneven)
        {
            height += std::sin(x * 1.15f) * 0.09f * course_difficulty_;
            height += std::sin(x * 2.75f + 0.7f) * 0.045f * course_difficulty_;
        }
        return std::max(-0.04f, height);
    }

    void Environment::rebuild_course_features() noexcept
    {
        course_features_.clear();
        const float root_x = valid_node(blueprint_.root_node) ? particles_[blueprint_.root_node].position.x : 0.0f;
        if (course_stage_ >= CourseStage::hurdles)
        {
            const int first = std::max(0, static_cast<int>(std::floor((root_x - 6.0f) / 6.0f)));
            for (int index = first; index < first + 7; ++index)
            {
                const float x = 5.0f + static_cast<float>(index) * 6.0f;
                const float height = 0.24f + course_difficulty_ * 0.30f;
                course_features_.push_back({ CourseFeatureKind::hurdle,
                    { x, ground_height_at(x) + height * 0.5f }, { 0.15f, height * 0.5f }, 0.0f, {} });
            }
        }
        if (course_stage_ >= CourseStage::duck_bars)
        {
            const int first = std::max(0, static_cast<int>(std::floor((root_x - 7.0f) / 8.0f)));
            for (int index = first; index < first + 6; ++index)
            {
                const float x = 7.0f + static_cast<float>(index) * 8.0f;
                const float clearance = 3.55f - course_difficulty_ * 0.75f;
                course_features_.push_back({ CourseFeatureKind::overhead_bar,
                    { x, ground_height_at(x) + clearance + 0.12f }, { 1.05f, 0.12f }, 0.0f, {} });
            }
        }
        if (course_stage_ >= CourseStage::moving_hazards)
        {
            const int first = std::max(0, static_cast<int>(std::floor((root_x - 8.0f) / 8.0f)));
            const float speed = 0.8f + course_difficulty_ * 1.0f;
            for (int index = first; index < first + 7; ++index)
            {
                const float lane = 8.0f + static_cast<float>(index) * 8.0f;
                const float travel = std::fmod(elapsed_seconds_ * speed + static_cast<float>(index) * 1.37f, 6.0f);
                const float x = lane + 3.0f - travel;
                const float y = ground_height_at(x) + 1.25f
                    + std::sin(elapsed_seconds_ * 1.8f + static_cast<float>(index)) * 0.40f;
                const float radius = 0.20f + course_difficulty_ * 0.09f;
                course_features_.push_back({ CourseFeatureKind::moving_hazard, { x, y }, {}, radius, { -speed, 0.0f } });
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
        maximum_speed_kmh_ = 0.0f;
        alternating_steps_ = 0;
        last_contact_side_ = 0;
        previous_left_grounded_ = false;
        previous_right_grounded_ = false;
        collided_this_step_ = false;
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
        if (!motor.enabled || motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return;
        Particle& reference = particles_[motor.a];
        Particle& pivot_particle = particles_[motor.pivot];
        Particle& driven = particles_[motor.c];
        const Vec2 reference_arm = reference.position - pivot_particle.position;
        const Vec2 driven_arm = driven.position - pivot_particle.position;
        if (length(reference_arm) <= 1.0e-5f || length(driven_arm) <= 1.0e-5f)
            return;
        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(reference_arm, driven_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.24f, 0.24f) * motor.strength;
        driven.position = pivot_particle.position + rotate(driven_arm, -correction);
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
                if (feature.kind == CourseFeatureKind::moving_hazard)
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
        if (new_left && last_contact_side_ != -1)
        {
            ++alternating_steps_;
            last_contact_side_ = -1;
        }
        if (new_right && last_contact_side_ != 1)
        {
            ++alternating_steps_;
            last_contact_side_ = 1;
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

        progress_window_seconds_ += dt;
        if (progress_window_seconds_ >= 1.0f && valid_node(blueprint_.root_node))
        {
            const float progress = std::abs(particles_[blueprint_.root_node].position.x - progress_window_start_x_);
            if (course_stage_ != CourseStage::balance && action_energy > 0.12f && progress < 0.04f)
                micro_motion_seconds_ += progress_window_seconds_;
            else
                micro_motion_seconds_ = std::max(0.0f, micro_motion_seconds_ - 0.5f);
            progress_window_start_x_ = particles_[blueprint_.root_node].position.x;
            progress_window_seconds_ = 0.0f;
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
                solve_motor(blueprint_.motors[index], actions[index]);
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
            const float effective = blueprint_.motors[index].enabled ? actions[index] : 0.0f;
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
        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor;

        const float allowed_airtime = course_stage_ == CourseStage::hurdles ? 1.30f
            : course_stage_ == CourseStage::moving_hazards ? 1.05f
            : course_stage_ >= CourseStage::ramps ? 0.90f : 0.72f;
        const float gated_upright = elapsed_seconds_ > 0.25f ? upright : 1.0f;
        invalidate(classify_motion_gate(gated_upright, maximum_speed_kmh_, pelvis_position,
            airborne_seconds_, allowed_airtime, micro_motion_seconds_, fallen_));

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

        if (invalid_reason_ != InvalidMotion::none)
            last_reward_ -= 5.0f;
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
        result[17] = 1.0f;
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
            result[22] = nearest->kind == CourseFeatureKind::hurdle ? -1.0f
                : nearest->kind == CourseFeatureKind::overhead_bar ? 0.0f : 1.0f;
            result[23] = clamp((nearest->center.y - root.y) / 4.0f, -2.0f, 2.0f);
            result[24] = nearest->kind == CourseFeatureKind::moving_hazard
                ? nearest->radius : std::max(nearest->half_extent.x, nearest->half_extent.y);
            result[25] = clamp(nearest->velocity.x / 5.0f, -1.0f, 1.0f);
        }
        result[26] = airborne_ratio();
        result[27] = clamp(static_cast<float>(alternating_steps_) / 10.0f, 0.0f, 2.0f);
        result[28] = static_cast<float>(course_stage_) / static_cast<float>(course_stage_count - 1);
        result[29] = course_difficulty_;
        return result;
    }
}
