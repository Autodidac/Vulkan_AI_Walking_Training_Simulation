#include "simulation.hpp"
#include "ppo.hpp"
#include "species_art_layout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#ifndef RUNNER_SOURCE_DIR
#error RUNNER_SOURCE_DIR must be defined
#endif

namespace
{
    using runner::sim::CreatureBlueprint;
    using runner::sim::CreatureSpecies;

    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    [[nodiscard]] float length(const CreatureBlueprint& rig,
        std::size_t a, std::size_t b)
    {
        const runner::Vec2 delta = rig.nodes[b] - rig.nodes[a];
        return std::sqrt(delta.x * delta.x + delta.y * delta.y);
    }

    [[nodiscard]] bool near(float a, float b, float tolerance = 0.0001f)
    {
        return std::abs(a - b) <= tolerance;
    }

    struct PpmEvidence
    {
        int width{};
        int height{};
        std::size_t content_pixels{};
        int min_x{};
        int min_y{};
        int max_x{};
        int max_y{};
    };

    [[nodiscard]] PpmEvidence inspect_ppm(const std::filesystem::path& path)
    {
        std::ifstream input{ path };
        std::string magic{};
        int width{};
        int height{};
        int maximum{};
        input >> magic >> width >> height >> maximum;
        require(input.good() && magic == "P3" && maximum == 255,
            "runtime art must be a readable P3 image");
        PpmEvidence evidence{ width, height, 0u, width, height, -1, -1 };
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int red{};
                int green{};
                int blue{};
                input >> red >> green >> blue;
                require(input.good(), "runtime art pixel payload is truncated");
                if (red == 255 && green == 0 && blue == 255)
                    continue;
                ++evidence.content_pixels;
                evidence.min_x = std::min(evidence.min_x, x);
                evidence.min_y = std::min(evidence.min_y, y);
                evidence.max_x = std::max(evidence.max_x, x);
                evidence.max_y = std::max(evidence.max_y, y);
            }
        }
        return evidence;
    }
}

int main()
{
    const std::filesystem::path root{ RUNNER_SOURCE_DIR };
    struct Asset
    {
        std::string_view name{};
        int width{};
        int height{};
    };
    constexpr std::array assets{
        Asset{ "head", 76, 60 },
        Asset{ "body", 96, 56 },
        Asset{ "tail", 76, 34 },
        Asset{ "upper_leg", 64, 36 },
        Asset{ "lower_leg", 60, 34 },
        Asset{ "foot", 56, 34 }
    };
    for (const Asset& asset : assets)
    {
        const PpmEvidence evidence = inspect_ppm(root / "assets" / "optional"
            / "species_runtime" / ("dog_" + std::string{ asset.name } + "_side.ppm"));
        require(evidence.width == asset.width && evidence.height == asset.height,
            "Dog runtime module dimensions changed");
        require(evidence.content_pixels
                >= static_cast<std::size_t>(asset.width * asset.height / 12),
            "Dog runtime module is visually empty");
        require(evidence.min_x > 0 && evidence.min_y > 0
                && evidence.max_x < asset.width - 1
                && evidence.max_y < asset.height - 1,
            "Dog runtime module lost its transparent-key gutter");
    }

    const CreatureBlueprint human = CreatureBlueprint::humanoid();
    require(human.valid() && human.human_casual_gait_plan(),
        "Human factory rig must retain the authored casual gait plan");
    require(human.nodes[human.motors[5].c].x
                < human.nodes[human.motors[7].c].x
            && human.nodes[human.motors[4].pivot].x
                > human.nodes[human.motors[6].pivot].x,
        "Human regression fixture must preserve crossed hand and shoulder ordering");
    std::array<float, runner::sim::action_count> turn_actions{};
    turn_actions.fill(1.0f);
    runner::rl::settle_manipulator_actions_to_rest(human, turn_actions, 1.0f);
    for (std::size_t index = 0; index < 4u; ++index)
        require(near(turn_actions[index], 1.0f),
            "turn arm settling must not change Human support motors");
    for (std::size_t index = 4u; index < 8u; ++index)
    {
        require(runner::rl::motor_drives_manipulator_chain(human, index),
            "Human arm motors must be discovered from authored topology");
        require(!near(turn_actions[index], 1.0f),
            "turn arm settling must return Human manipulators toward side-rest");
    }
    require(!runner::rl::motor_drives_manipulator_chain(human, 0u)
            && !runner::rl::motor_drives_manipulator_chain(human, 3u),
        "Human support motors must never be classified as manipulators");
    const CreatureBlueprint dog = CreatureBlueprint::crawler4();
    require(dog.valid(), "Dog factory rig must be valid");
    require(dog.presentation_species() == CreatureSpecies::dog,
        "Dog factory rig lost its owned identity");
    require(dog.horizontal_multi_support_plan() && dog.support_seed_count() == 4u,
        "Dog must remain a four-support horizontal animal");
    require(dog.nodes[2].x > dog.nodes[1].x && dog.nodes[2].y > dog.nodes[1].y,
        "Dog head must attach forward and above the shoulder");
    require(dog.nodes[0].x < dog.nodes[1].x
            && std::abs(dog.nodes[0].y - dog.nodes[1].y) < 0.10f,
        "Dog torso must remain a level canine body");
    require(near(length(dog, 3u, 4u), length(dog, 5u, 6u))
            && near(length(dog, 7u, 8u), length(dog, 9u, 10u)),
        "near/far Dog leg pairs must remain equal");
    require(std::abs(dog.nodes[4].y - dog.nodes[6].y) < 0.001f
            && std::abs(dog.nodes[8].y - dog.nodes[10].y) < 0.001f,
        "Dog paws must share a coherent ground plane");

    const CreatureBlueprint chicken = CreatureBlueprint::chicken();
    const CreatureBlueprint hexapod = CreatureBlueprint::hexapod();
    constexpr runner::art::SpeciesArtProfile chicken_art =
        runner::art::species_art_profile(CreatureSpecies::chicken);
    constexpr runner::art::SpeciesArtProfile dog_art =
        runner::art::species_art_profile(CreatureSpecies::dog);
    constexpr runner::art::SpeciesArtProfile hexapod_art =
        runner::art::species_art_profile(CreatureSpecies::hexapod);
    static_assert(chicken_art.head_scale >= 5.1f
        && chicken_art.head_minimum >= 0.40f
        && chicken_art.tail_length >= 0.54f
        && chicken_art.tail_ratio >= 0.88f);
    static_assert(dog_art.head_scale >= 5.0f
        && dog_art.head_anchor_forward > chicken_art.head_anchor_forward
        && dog_art.tail_length >= 0.66f
        && !dog_art.tail_flip_vertical);
    static_assert(!chicken_art.tail_flip_vertical
        && !hexapod_art.tail_flip_vertical
        && hexapod_art.tail_length == 0.0f);
    static_assert(!runner::art::tail_transverse_mirror(dog_art, false)
        && runner::art::tail_transverse_mirror(dog_art, true)
        && !runner::art::tail_transverse_mirror(chicken_art, false));
    require(chicken_art.head_anchor_up > 0.0f
            && chicken_art.tail_anchor_up > 0.0f
            && dog_art.head_anchor_up > 0.0f
            && dog_art.tail_anchor_up > 0.0f,
        "species head and tail modules lost their authored attachment offsets");
    require(chicken.valid() && chicken.avian_gait(),
        "Chicken factory rig must remain valid avian anatomy");
    require(hexapod.valid() && hexapod.horizontal_multi_support_plan()
            && hexapod.support_seed_count() == 6u,
        "Hexapod factory rig must retain six articulated supports");

    const std::array species{
        CreatureSpecies::human, CreatureSpecies::chicken,
        CreatureSpecies::dog, CreatureSpecies::hexapod
    };
    for (int seed = 0; seed < 32; ++seed)
    {
        for (const CreatureSpecies value : species)
        {
            const CreatureBlueprint first = CreatureBlueprint::for_species(value);
            const CreatureBlueprint repeat = CreatureBlueprint::for_species(value);
            require(first.valid() && repeat.valid()
                    && first.signature() == repeat.signature(),
                "factory rigs must be deterministic across repeated seeds");
        }
    }

    CreatureBlueprint invalid = dog;
    invalid.left_contact_node = static_cast<std::uint16_t>(invalid.nodes.size());
    require(!invalid.valid(),
        "out-of-range Dog support contact must fail validation");

    CreatureBlueprint adversarial = dog;
    adversarial.species_identity = CreatureSpecies::chicken;
    require(!adversarial.topology_compatible_with_species(CreatureSpecies::chicken),
        "cross-species Dog topology must fail validation");

    std::cout << "Runner v0.7.46 species art and four-species rig checks passed\n";
    return 0;
}
