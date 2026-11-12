#pragma once

#include "acceptance.hpp"
#include "ai_director.hpp"
#include "ai_director_io.hpp"
#include "atomic_file.hpp"
#include "art_authoring.hpp"
#include "autonomy.hpp"
#include "course_geometry.hpp"
#include "course_completion_diagnostic.hpp"
#include "deformable_terrain.hpp"
#include "hybrid_brain_diagnostic.hpp"
#include "integration.hpp"
#include "locomotion_strategy.hpp"
#include "math.hpp"
#include "pixel_art.hpp"
#include "ppo.hpp"
#include "preview_sync.hpp"
#include "raw_inference_diagnostic.hpp"
#include "raw_learning_provenance.hpp"
#include "rig_training_diagnostic.hpp"
#include "simulation.hpp"
#include "species_art_layout.hpp"
#include "training_explainer.hpp"

// The runner namespace remains source-compatible for existing integrations.
// New consumers use the product namespace without duplicating implementation.
namespace epoch2dwalk = runner;
