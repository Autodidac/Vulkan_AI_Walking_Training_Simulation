#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <string_view>

namespace epochrunner::sim
{
    namespace
    {
        constexpr std::uint16_t pelvis = 0;
        constexpr std::uint16_t chest = 1;
        constexpr std::uint16_t head = 2;
        constexpr std::uint16_t left_knee = 3;
        constexpr std::uint16_t left_foot = 4;
        constexpr std::uint16_t right_knee = 5;
        constexpr std::uint16_t right_foot = 6;
        constexpr std::uint16_t tail = 7;

    }

    CreatureBlueprint CreatureBlueprint::chicken()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.95f },
            { 0.05f, 4.05f },
            { 0.64f, 4.72f },
            { -0.45f, 1.78f },
            { -0.30f, 0.32f },
            { 0.48f, 1.76f },
            { 0.66f, 0.32f },
            { -1.10f, 3.86f }
        };
        result.radii = { 0.28f, 0.32f, 0.30f, 0.20f, 0.18f, 0.20f, 0.18f, 0.16f };
        result.bones = {
            { pelvis, chest, 0.0f, 1.0f },
            { chest, head, 0.0f, 1.0f },
            { chest, tail, 0.0f, 0.92f },
            { pelvis, left_knee, 0.0f, 1.0f },
            { left_knee, left_foot, 0.0f, 1.0f },
            { pelvis, right_knee, 0.0f, 1.0f },
            { right_knee, right_foot, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ chest, pelvis, left_knee, -1.25f, 0.55f, -0.35f, 0.32f, true },
            MotorConstraint{ pelvis, left_knee, left_foot, 0.15f, 2.35f, 1.10f, 0.38f, true },
            MotorConstraint{ chest, pelvis, right_knee, -0.55f, 1.25f, 0.35f, 0.32f, true },
            MotorConstraint{ pelvis, right_knee, right_foot, -2.35f, -0.15f, -1.10f, 0.38f, true }
        };
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 22.0f, 22.0f, 0.060f);
        result.calibrate_motor(1, 30.0f, 30.0f, 0.065f);
        result.calibrate_motor(2, 22.0f, 22.0f, 0.060f);
        result.calibrate_motor(3, 30.0f, 30.0f, 0.065f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::biped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.70f }, { 0.0f, 3.72f }, { 0.05f, 4.48f },
            { -0.38f, 1.48f }, { -0.48f, 0.26f },
            { 0.38f, 1.48f }, { 0.48f, 0.26f }, { -0.72f, 3.55f }
        };
        result.radii = { 0.25f, 0.28f, 0.24f, 0.18f, 0.17f, 0.18f, 0.17f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.85f },
            { 0, 3, 0.0f, 1.0f }, { 3, 4, 0.0f, 1.0f },
            { 0, 5, 0.0f, 1.0f }, { 5, 6, 0.0f, 1.0f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3, -1.15f, 0.50f, -0.28f, 0.30f, true },
            MotorConstraint{ 0, 3, 4, 0.20f, 2.25f, 1.12f, 0.36f, true },
            MotorConstraint{ 1, 0, 5, -0.50f, 1.15f, 0.28f, 0.30f, true },
            MotorConstraint{ 0, 5, 6, -2.25f, -0.20f, -1.12f, 0.36f, true }
        };
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 24.0f, 24.0f, 0.060f);
        result.calibrate_motor(1, 32.0f, 32.0f, 0.065f);
        result.calibrate_motor(2, 24.0f, 24.0f, 0.060f);
        result.calibrate_motor(3, 32.0f, 32.0f, 0.065f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::humanoid()
    {
        CreatureBlueprint result = biped();
        result.nodes[0] = { 0.0f, 2.90f };
        result.nodes[1] = { 0.0f, 4.05f };
        result.nodes[2] = { 0.0f, 4.95f };
        result.nodes[3] = { -0.34f, 1.62f };
        result.nodes[4] = { -0.42f, 0.25f };
        result.nodes[5] = { 0.34f, 1.62f };
        result.nodes[6] = { 0.42f, 0.25f };
        result.nodes[7] = { -0.92f, 3.95f };
        result.radii = { 0.26f, 0.31f, 0.29f, 0.19f, 0.17f, 0.19f, 0.17f, 0.15f };
        result.motors[0].strength = 0.28f;
        result.motors[1].strength = 0.34f;
        result.motors[2].strength = 0.28f;
        result.motors[3].strength = 0.34f;
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 24.0f, 24.0f, 0.055f);
        result.calibrate_motor(1, 32.0f, 32.0f, 0.060f);
        result.calibrate_motor(2, 24.0f, 24.0f, 0.055f);
        result.calibrate_motor(3, 32.0f, 32.0f, 0.060f);
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
        result.motors = {
            MotorConstraint{ 1, 0, 3, -1.55f, 0.15f, -0.72f, 0.30f, true },
            MotorConstraint{ 0, 3, 4, 0.05f, 2.20f, 1.05f, 0.35f, true },
            MotorConstraint{ 0, 1, 5, -0.15f, 1.55f, 0.72f, 0.30f, true },
            MotorConstraint{ 1, 5, 6, -2.20f, -0.05f, -1.05f, 0.35f, true }
        };
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 24.0f, 24.0f, 0.060f);
        result.calibrate_motor(1, 34.0f, 34.0f, 0.065f);
        result.calibrate_motor(2, 24.0f, 24.0f, 0.060f);
        result.calibrate_motor(3, 34.0f, 34.0f, 0.065f);
        return result;
    }

    CreatureBlueprint CreatureBlueprint::monoped()
    {
        CreatureBlueprint result{};
        result.nodes = {
            { 0.0f, 2.85f }, { 0.0f, 3.86f }, { 0.0f, 4.62f },
            { 0.0f, 1.62f }, { -0.34f, 0.22f },
            { 0.0f, 0.68f }, { 0.34f, 0.22f }, { -0.68f, 3.52f }
        };
        result.radii = { 0.27f, 0.29f, 0.25f, 0.19f, 0.17f, 0.18f, 0.17f, 0.14f };
        result.bones = {
            { 0, 1, 0.0f, 1.0f }, { 1, 2, 0.0f, 1.0f }, { 1, 7, 0.0f, 0.82f },
            { 0, 3, 0.0f, 1.0f }, { 3, 5, 0.0f, 1.0f },
            { 5, 4, 0.0f, 0.92f }, { 5, 6, 0.0f, 0.92f }
        };
        result.motors = {
            MotorConstraint{ 1, 0, 3, -0.95f, 0.95f, 0.0f, 0.30f, true },
            MotorConstraint{ 0, 3, 5, 0.20f, 2.20f, 1.12f, 0.36f, true },
            MotorConstraint{ 3, 5, 4, -1.30f, 0.20f, -0.45f, 0.24f, true },
            MotorConstraint{ 4, 5, 6, -1.10f, 1.10f, 0.0f, 0.18f, true }
        };
        result.rebuild_rest_lengths();
        result.calibrate_motor(0, 26.0f, 26.0f, 0.060f);
        result.calibrate_motor(1, 34.0f, 34.0f, 0.065f);
        result.calibrate_motor(2, 24.0f, 24.0f, 0.055f);
        result.calibrate_motor(3, 28.0f, 28.0f, 0.055f);
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
        const bool valid = motor.a < nodes.size() && motor.pivot < nodes.size() && motor.c < nodes.size()
            && motor.a != motor.pivot && motor.pivot != motor.c && motor.a != motor.c;
        motor.enabled = valid;
        if (!valid)
            return;
        motor.neutral_angle = rest_joint_angle(motor_index);
        const float negative = clamp(std::abs(negative_degrees), 1.0f, 170.0f) * pi / 180.0f;
        const float positive = clamp(std::abs(positive_degrees), 1.0f, 170.0f) * pi / 180.0f;
        // Limits are deliberately allowed outside [-pi, pi]. That keeps a normal
        // range continuous when the rest pose is near the signed-angle wrap seam.
        motor.minimum_angle = motor.neutral_angle - negative;
        motor.maximum_angle = motor.neutral_angle + positive;
        motor.strength = clamp(power, 0.0f, 1.0f);
    }

    void CreatureBlueprint::calibrate_all_motors(float degrees, float power) noexcept
    {
        for (std::size_t index = 0; index < motors.size(); ++index)
            calibrate_motor(index, degrees, degrees, power);
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
        auto add_float = [&](float value)
        {
            add_u64(std::bit_cast<std::uint32_t>(value));
        };

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
        {
            const float radius = index < radii.size() ? radii[index] : 0.15f;
            output << "N " << nodes[index].x << ' ' << nodes[index].y << ' ' << radius << '\n';
        }
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
            return chicken();
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
            return chicken();
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
                return chicken();
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
            if (!input || tag != 'N' || !std::isfinite(node.x) || !std::isfinite(node.y) || radius <= 0.0f || radius > 4.0f)
            {
                error = "Invalid node data in rig file.";
                return chicken();
            }
            result.nodes.push_back(node);
            result.radii.push_back(radius);
        }

        auto valid_semantic = [node_count](std::uint16_t index) { return index < node_count; };
        if (!valid_semantic(result.root_node) || !valid_semantic(result.torso_node)
            || !valid_semantic(result.head_node) || !valid_semantic(result.left_contact_node)
            || !valid_semantic(result.right_contact_node))
        {
            error = "Rig semantic node index is out of range.";
            return chicken();
        }

        for (std::size_t index = 0; index < bone_count; ++index)
        {
            char tag{};
            DistanceConstraint bone{};
            input >> tag >> bone.a >> bone.b >> bone.rest_length >> bone.stiffness;
            if (!input || tag != 'B' || bone.a >= node_count || bone.b >= node_count || bone.a == bone.b || bone.rest_length <= 0.0f)
            {
                error = "Invalid bone data in rig file.";
                return chicken();
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
            if (!input || tag != 'M' || motor.a >= node_count || motor.pivot >= node_count || motor.c >= node_count)
            {
                error = "Invalid motor data in rig file.";
                return chicken();
            }
            motor.strength = clamp(motor.strength, 0.0f, 1.0f);
            if (motor.minimum_angle > motor.maximum_angle)
                std::swap(motor.minimum_angle, motor.maximum_angle);
            motor.neutral_angle = clamp(motor.neutral_angle, motor.minimum_angle, motor.maximum_angle);
            result.motors[index] = motor;
        }

        error.clear();
        return result;
    }

    Environment::Environment()
        : Environment(CreatureBlueprint::chicken(), 1)
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

    float Environment::random_unit() noexcept
    {
        random_state_ ^= random_state_ >> 12;
        random_state_ ^= random_state_ << 25;
        random_state_ ^= random_state_ >> 27;
        const std::uint64_t value = random_state_ * 2685821657736338717ULL;
        return static_cast<float>(value >> 40) * (1.0f / 16777216.0f);
    }

    void Environment::reset(std::uint64_t seed)
    {
        if (seed != 0)
            random_state_ = seed;
        if (random_state_ == 0)
            random_state_ = 1;

        particles_.clear();
        particles_.reserve(blueprint_.nodes.size());
        const float phase = (random_unit() - 0.5f) * 0.08f;
        for (std::size_t index = 0; index < blueprint_.nodes.size(); ++index)
        {
            Vec2 position = blueprint_.nodes[index];
            position.x += (random_unit() - 0.5f) * 0.015f + phase;
            position.y += (random_unit() - 0.5f) * 0.01f;
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

        Particle& first = particles_[motor.a];
        Particle& pivot_particle = particles_[motor.pivot];
        Particle& third = particles_[motor.c];
        Vec2 first_arm = first.position - pivot_particle.position;
        Vec2 third_arm = third.position - pivot_particle.position;
        const float first_length = length(first_arm);
        const float third_length = length(third_arm);
        if (first_length <= 1.0e-5f || third_length <= 1.0e-5f)
            return;

        const float target = motor_target_angle(motor, action);
        const float current = signed_angle(first_arm, third_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.28f, 0.28f) * motor.strength;

        // A is the reference/parent side. C is the driven/child side. The old
        // solver rotated both arms and shifted the pivot, so a hip command also
        // moved the torso and made the editor preview disagree with physics.
        const Vec2 corrected_third = rotate(third_arm, -correction);
        third.position = pivot_particle.position + corrected_third;
    }

    void Environment::solve_ground(float dt) noexcept
    {
        for (Particle& particle : particles_)
        {
            particle.grounded = false;
            const float minimum_y = ground_height_ + particle.radius;
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

    StepResult Environment::step(std::span<const float, action_count> actions, float dt)
    {
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.head_node) || !valid_node(blueprint_.left_contact_node)
            || !valid_node(blueprint_.right_contact_node))
            return { 0.0f, 0.0f, true };

        dt = clamp(dt, 1.0f / 240.0f, 1.0f / 30.0f);
        constexpr Vec2 gravity{ 0.0f, -22.0f };
        constexpr float damping = 0.996f;

        for (Particle& particle : particles_)
        {
            const Vec2 velocity = (particle.position - particle.previous) * damping;
            particle.previous = particle.position;
            particle.position += velocity + gravity * (dt * dt);
        }

        for (int iteration = 0; iteration < 12; ++iteration)
        {
            for (const DistanceConstraint& bone : blueprint_.bones)
                solve_distance(bone);
            for (std::size_t index = 0; index < action_count; ++index)
                solve_motor(blueprint_.motors[index], actions[index]);
            solve_ground(dt);
        }

        elapsed_seconds_ += dt;
        const Vec2 pelvis_position = particles_[blueprint_.root_node].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[blueprint_.root_node].x;

        std::array<float, action_count> effective_actions{};
        float action_energy = 0.0f;
        for (std::size_t index = 0; index < action_count; ++index)
        {
            effective_actions[index] = blueprint_.motors[index].enabled ? actions[index] : 0.0f;
            action_energy += effective_actions[index] * effective_actions[index];
        }

        for (std::size_t index = 0; index < action_count; ++index)
        {
            const float angle = joint_angle(blueprint_.motors[index]);
            angular_velocities_[index] = wrap_angle(angle - previous_angles_[index]) / dt;
            previous_angles_[index] = angle;
        }

        const float upright = torso_uprightness();
        const float alternating = std::abs(effective_actions[0] + effective_actions[2])
            + std::abs(effective_actions[1] + effective_actions[3]);
        const float foot_contact = (particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f)
            + (particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f);

        const float torso_floor = std::max(0.35f, blueprint_.nodes[blueprint_.torso_node].y * 0.30f);
        const float head_floor = std::max(0.30f, blueprint_.nodes[blueprint_.head_node].y * 0.18f);
        fallen_ = particles_[blueprint_.torso_node].position.y < torso_floor
            || particles_[blueprint_.head_node].position.y < head_floor
            || std::abs(pelvis_position.x) > 500.0f;

        last_reward_ = forward_speed_ * 0.040f
            + std::max(0.0f, upright) * 0.020f
            + foot_contact * 0.0015f
            - action_energy * 0.0012f
            - alternating * 0.00055f;
        if (fallen_)
            last_reward_ -= 1.25f;

        const bool timed_out = elapsed_seconds_ >= 20.0f;
        return { last_reward_, forward_speed_, fallen_ || timed_out };
    }

    std::array<float, observation_count> Environment::observation() const noexcept
    {
        std::array<float, observation_count> result{};
        if (!valid_node(blueprint_.root_node) || !valid_node(blueprint_.torso_node)
            || !valid_node(blueprint_.left_contact_node) || !valid_node(blueprint_.right_contact_node))
            return result;

        const Vec2 torso = normalized(
            particles_[blueprint_.torso_node].position - particles_[blueprint_.root_node].position,
            { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[blueprint_.root_node].position - particles_[blueprint_.root_node].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * 60.0f / 8.0f, -4.0f, 4.0f);
        result[3] = clamp(pelvis_velocity.y * 60.0f / 8.0f, -4.0f, 4.0f);

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
            result[8 + index] = clamp(angular_velocities_[index] / 20.0f, -3.0f, 3.0f);
        }

        result[12] = particles_[blueprint_.left_contact_node].grounded ? 1.0f : 0.0f;
        result[13] = particles_[blueprint_.right_contact_node].grounded ? 1.0f : 0.0f;
        result[14] = clamp((particles_[blueprint_.left_contact_node].position.x
            - particles_[blueprint_.root_node].position.x) / 2.0f, -2.0f, 2.0f);
        result[15] = clamp((particles_[blueprint_.right_contact_node].position.x
            - particles_[blueprint_.root_node].position.x) / 2.0f, -2.0f, 2.0f);
        result[16] = clamp(particles_[blueprint_.root_node].position.y / 5.0f, 0.0f, 2.0f);
        result[17] = 1.0f;
        return result;
    }

}
