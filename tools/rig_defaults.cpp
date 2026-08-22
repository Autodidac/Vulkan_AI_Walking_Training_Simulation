#include "simulation.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: RunnerRigDefaults <output-directory>\n";
        return 2;
    }

    const std::filesystem::path output_directory{ argv[1] };
    std::error_code filesystem_error{};
    std::filesystem::create_directories(output_directory, filesystem_error);
    if (filesystem_error)
    {
        std::cerr << "could not create rig output directory: "
                  << filesystem_error.message() << '\n';
        return 1;
    }

    struct DefaultRig
    {
        runner::sim::CreatureSpecies species{};
        runner::sim::CreatureBlueprint blueprint{};
    };
    const std::array rigs{
        DefaultRig{ runner::sim::CreatureSpecies::human,
            runner::sim::CreatureBlueprint::humanoid() },
        DefaultRig{ runner::sim::CreatureSpecies::chicken,
            runner::sim::CreatureBlueprint::chicken() },
        DefaultRig{ runner::sim::CreatureSpecies::dog,
            runner::sim::CreatureBlueprint::crawler4() },
        DefaultRig{ runner::sim::CreatureSpecies::hexapod,
            runner::sim::CreatureBlueprint::hexapod() }
    };

    for (const DefaultRig& rig : rigs)
    {
        const std::filesystem::path path = output_directory
            / runner::sim::creature_species_rig_filename(rig.species);
        std::string error{};
        if (!rig.blueprint.save(path, error))
        {
            std::cerr << path.string() << ": " << error << '\n';
            return 1;
        }
        if (!runner::sim::CreatureBlueprint::load_for_species(
            path, rig.species, error))
        {
            std::cerr << path.string() << ": generated rig failed validation: "
                      << error << '\n';
            return 1;
        }
    }
    return 0;
}
