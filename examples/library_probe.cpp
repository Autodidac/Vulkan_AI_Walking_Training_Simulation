#include <Epoch2DWalkEngine/Engine.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

int main()
{
    const epoch2dwalk::sim::CreatureBlueprint rig =
        epoch2dwalk::sim::CreatureBlueprint::humanoid();
    if (!rig.valid() || rig.presentation_species()
            != epoch2dwalk::sim::CreatureSpecies::human)
        return 1;

    epoch2dwalk::sim::Environment environment{ rig, 0xE20Du };
    environment.set_course(epoch2dwalk::sim::CourseStage::uneven, 0.25f);
    const auto observation = environment.observation();
    if (observation.size() != epoch2dwalk::sim::observation_count)
        return 2;
    for (const float value : observation)
    {
        if (!std::isfinite(value))
            return 3;
    }

    std::array<float, epoch2dwalk::sim::action_count> action{};
    const epoch2dwalk::sim::StepResult result = environment.step(action);
    if (!std::isfinite(result.reward)
        || !std::isfinite(environment.uprightness()))
        return 4;

    std::cout << "Epoch2DWalkEngine 0.7.49 library API passed\n";
    return 0;
}
