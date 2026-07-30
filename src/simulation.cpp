#include "simulation.hpp"

#include <algorithm>
#include <array>
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
            MotorConstraint{ chest, pelvis, left_knee, -1.65f, 0.75f, -0.40f, 0.54f },
            MotorConstraint{ pelvis, left_knee, left_foot, 0.05f, 2.70f, 1.25f, 0.62f },
            MotorConstraint{ chest, pelvis, right_knee, -0.75f, 1.65f, 0.40f, 0.54f },
            MotorConstraint{ pelvis, right_knee, right_foot, -2.70f, -0.05f, -1.25f, 0.62f }
        };
        result.rebuild_rest_lengths();
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

    bool CreatureBlueprint::save(const std::filesystem::path& path, std::string& error) const
    {
        std::ofstream output(path, std::ios::trunc);
        if (!output)
        {
            error = "Could not open rig file for writing: " + path.string();
            return false;
        }

        output << "EPOCHRIG 1\n";
        output << nodes.size() << ' ' << bones.size() << ' ' << motors.size() << '\n';
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
            output << "M " << motor.a << ' ' << motor.pivot << ' ' << motor.c << ' '
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
        if (!input || magic != "EPOCHRIG" || version != 1 || node_count < 3 || node_count > 128 || bone_count > 256 || motor_count != action_count)
        {
            error = "Invalid or unsupported EpochRunner rig file.";
            return chicken();
        }

        CreatureBlueprint result{};
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
            input >> tag >> motor.a >> motor.pivot >> motor.c
                >> motor.minimum_angle >> motor.maximum_angle >> motor.neutral_angle >> motor.strength;
            if (!input || tag != 'M' || motor.a >= node_count || motor.pivot >= node_count || motor.c >= node_count)
            {
                error = "Invalid motor data in rig file.";
                return chicken();
            }
            motor.strength = clamp(motor.strength, 0.0f, 1.0f);
            if (motor.minimum_angle > motor.maximum_angle)
                std::swap(motor.minimum_angle, motor.maximum_angle);
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
            particles_.push_back({ position, position, index == head ? 0.65f : 1.0f, radius, false });
        }

        previous_pelvis_ = particles_.empty() ? Vec2{} : particles_[pelvis].position;
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
        if (motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
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

        const float target = clamp(motor.neutral_angle + clamp(action, -1.0f, 1.0f) * 1.15f,
            motor.minimum_angle, motor.maximum_angle);
        const float current = signed_angle(first_arm, third_arm);
        const float error = wrap_angle(current - target);
        const float correction = clamp(error, -0.35f, 0.35f) * motor.strength;

        const float endpoint_weight = first.inverse_mass + third.inverse_mass;
        if (endpoint_weight <= 1.0e-6f)
            return;

        const float first_share = first.inverse_mass / endpoint_weight;
        const float third_share = third.inverse_mass / endpoint_weight;
        const Vec2 corrected_first = rotate(first_arm, correction * first_share);
        const Vec2 corrected_third = rotate(third_arm, -correction * third_share);
        first.position = pivot_particle.position + corrected_first;
        third.position = pivot_particle.position + corrected_third;

        const Vec2 center_shift = ((corrected_first - first_arm) * first.inverse_mass
            + (corrected_third - third_arm) * third.inverse_mass) * -0.18f;
        pivot_particle.position += center_shift;
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
        if (motor.a >= particles_.size() || motor.pivot >= particles_.size() || motor.c >= particles_.size())
            return 0.0f;
        return signed_angle(
            particles_[motor.a].position - particles_[motor.pivot].position,
            particles_[motor.c].position - particles_[motor.pivot].position);
    }

    float Environment::torso_uprightness() const noexcept
    {
        if (particles_.size() <= chest)
            return 0.0f;
        const Vec2 torso = normalized(particles_[chest].position - particles_[pelvis].position, { 0.0f, 1.0f });
        return clamp(torso.y, -1.0f, 1.0f);
    }

    StepResult Environment::step(std::span<const float, action_count> actions, float dt)
    {
        if (particles_.size() < 8)
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
        const Vec2 pelvis_position = particles_[pelvis].position;
        const float raw_speed = (pelvis_position.x - previous_pelvis_.x) / dt;
        forward_speed_ = lerp(forward_speed_, raw_speed, 0.18f);
        previous_pelvis_ = pelvis_position;
        distance_travelled_ = pelvis_position.x - blueprint_.nodes[pelvis].x;

        float action_energy = 0.0f;
        for (const float action : actions)
            action_energy += action * action;

        for (std::size_t index = 0; index < action_count; ++index)
        {
            const float angle = joint_angle(blueprint_.motors[index]);
            angular_velocities_[index] = wrap_angle(angle - previous_angles_[index]) / dt;
            previous_angles_[index] = angle;
        }

        const float upright = torso_uprightness();
        const float alternating = std::abs(actions[0] + actions[2]) + std::abs(actions[1] + actions[3]);
        const float foot_contact = (particles_[left_foot].grounded ? 1.0f : 0.0f)
            + (particles_[right_foot].grounded ? 1.0f : 0.0f);

        fallen_ = particles_[chest].position.y < 1.25f
            || particles_[head].position.y < 0.85f
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
        if (particles_.size() < 8)
            return result;

        const Vec2 torso = normalized(particles_[chest].position - particles_[pelvis].position, { 0.0f, 1.0f });
        const Vec2 pelvis_velocity = particles_[pelvis].position - particles_[pelvis].previous;
        result[0] = torso.x;
        result[1] = torso.y;
        result[2] = clamp(pelvis_velocity.x * 60.0f / 8.0f, -4.0f, 4.0f);
        result[3] = clamp(pelvis_velocity.y * 60.0f / 8.0f, -4.0f, 4.0f);

        for (std::size_t index = 0; index < action_count; ++index)
        {
            const MotorConstraint& motor = blueprint_.motors[index];
            const float range = std::max(0.001f, motor.maximum_angle - motor.minimum_angle);
            result[4 + index] = clamp((joint_angle(motor) - motor.neutral_angle) / range * 2.0f, -2.0f, 2.0f);
            result[8 + index] = clamp(angular_velocities_[index] / 20.0f, -3.0f, 3.0f);
        }

        result[12] = particles_[left_foot].grounded ? 1.0f : 0.0f;
        result[13] = particles_[right_foot].grounded ? 1.0f : 0.0f;
        result[14] = clamp((particles_[left_foot].position.x - particles_[pelvis].position.x) / 2.0f, -2.0f, 2.0f);
        result[15] = clamp((particles_[right_foot].position.x - particles_[pelvis].position.x) / 2.0f, -2.0f, 2.0f);
        result[16] = clamp(particles_[pelvis].position.y / 5.0f, 0.0f, 2.0f);
        result[17] = 1.0f;
        return result;
    }
}
