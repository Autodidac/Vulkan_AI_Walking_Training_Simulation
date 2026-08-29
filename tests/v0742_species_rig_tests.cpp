#include "simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

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

    [[nodiscard]] std::string read_text(const std::filesystem::path& path)
    {
        std::ifstream input{ path, std::ios::binary };
        std::string text{ std::istreambuf_iterator<char>{ input },
            std::istreambuf_iterator<char>{} };
        text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
        return text;
    }

    [[nodiscard]] float segment_length(const CreatureBlueprint& rig,
        std::size_t a, std::size_t b)
    {
        const runner::Vec2 delta = rig.nodes[b] - rig.nodes[a];
        return std::sqrt(delta.x * delta.x + delta.y * delta.y);
    }

    [[nodiscard]] bool near(float a, float b, float tolerance = 0.0001f)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int main()
{
    struct SpeciesFixture
    {
        CreatureSpecies species{};
        CreatureBlueprint rig{};
        std::string_view slug{};
    };
    const std::array fixtures{
        SpeciesFixture{ CreatureSpecies::human, CreatureBlueprint::humanoid(), "human" },
        SpeciesFixture{ CreatureSpecies::chicken, CreatureBlueprint::chicken(), "chicken" },
        SpeciesFixture{ CreatureSpecies::dog, CreatureBlueprint::crawler4(), "dog" },
        SpeciesFixture{ CreatureSpecies::hexapod, CreatureBlueprint::hexapod(), "hexapod" }
    };

    const std::filesystem::path test_directory =
        std::filesystem::temp_directory_path() / "runner-v0742-species-rig-tests";
    std::error_code filesystem_error{};
    std::filesystem::remove_all(test_directory, filesystem_error);
    filesystem_error.clear();
    std::filesystem::create_directories(test_directory, filesystem_error);
    require(!filesystem_error, "could not create test directory");

    for (const SpeciesFixture& fixture : fixtures)
    {
        require(fixture.rig.valid(), "factory species rig must be valid");
        const CreatureBlueprint selected_factory =
            CreatureBlueprint::for_species(fixture.species);
        require(selected_factory.signature() == fixture.rig.signature(),
            "species factory selector returned the wrong anatomy");
        require(fixture.rig.presentation_species() == fixture.species,
            "factory topology must identify its species");
        require(runner::sim::creature_species_slug(fixture.species) == fixture.slug,
            "species slug must be stable");
        require(runner::sim::creature_species_from_slug(fixture.slug) == fixture.species,
            "species slug must round trip");

        const std::filesystem::path filename =
            runner::sim::creature_species_rig_filename(fixture.species);
        require(filename == std::filesystem::path{ std::string{ fixture.slug } + ".rig" },
            "species filename must be explicit and predictable");
        const runner::sim::CreatureSpeciesPaths paths =
            runner::sim::creature_species_paths(fixture.species);
        const std::string state_prefix = "runner-v0747-"
            + std::string{ fixture.slug };
        require(paths.rig == filename,
            "species path set must own the authored rig filename");
        require(paths.autosave_checkpoint == state_prefix + "-autosave.eppo"
                && paths.evolved_rig == state_prefix + "-evolved.rig"
                && paths.autonomy_state == state_prefix + "-autonomy.state",
            "all automatic training state must share the species identity");
        const std::filesystem::path path = test_directory / filename;
        std::string error{};
        require(fixture.rig.save(path, error), error);
        const std::string serialized = read_text(path);
        require(serialized.starts_with("RUNRIG 5\n"),
            "species rigs must use version 5");
        require(serialized.find("P " + std::string{ fixture.slug } + "\n")
                != std::string::npos,
            "species rigs must declare their identity");

        const std::optional<CreatureBlueprint> loaded =
            CreatureBlueprint::load_for_species(path, fixture.species, error);
        require(loaded.has_value(), error);
        require(loaded->presentation_species() == fixture.species,
            "validated load changed species");
        require(loaded->signature() == fixture.rig.signature(),
            "species rig signature changed across its own save/load round trip");

        const CreatureSpecies wrong_species = fixture.species == CreatureSpecies::human
            ? CreatureSpecies::dog : CreatureSpecies::human;
        error.clear();
        require(!CreatureBlueprint::load_for_species(path, wrong_species, error),
            "cross-species load must be rejected");
        require(error.find("Rig topology is ") != std::string::npos &&
                    error.find(std::string{fixture.slug}) != std::string::npos &&
                    error.find(std::string{creature_species_slug(wrong_species)}) != std::string::npos,
            "cross-species rejection must name the actual and requested species");

        const std::filesystem::path repeat_path = test_directory
            / (std::string{ fixture.slug } + "-repeat.rig");
        error.clear();
        require(fixture.rig.save(repeat_path, error), error);
        require(read_text(repeat_path) == serialized,
            "same factory rig must serialize byte-identically across repeated seeds");
    }

    require(!runner::sim::creature_species_from_slug("quadruped"),
        "retired aliases must not become production species identities");

    std::string error{};
    require(!CreatureBlueprint::load_for_species(test_directory / "missing.rig",
            CreatureSpecies::human, error),
        "missing species rig must not produce an implicit cross-species fallback");
    require(!error.empty(), "missing species rig must explain the fallback decision");

    const std::filesystem::path human_path = test_directory / "human.rig";
    const std::filesystem::path dog_path = test_directory / "dog.rig";
    const std::string human_before_dog_edit = read_text(human_path);
    const std::string dog_before_edit = read_text(dog_path);
    error.clear();
    std::optional<CreatureBlueprint> edited_dog = CreatureBlueprint::load_for_species(
        dog_path, CreatureSpecies::dog, error);
    require(edited_dog.has_value(), error);
    const std::uint64_t dog_before_pair_enforcement = edited_dog->signature();
    require(edited_dog->enforce_human_paired_segment_lengths(2u),
        "Human pairing helper must be a safe no-op for Dog");
    require(edited_dog->signature() == dog_before_pair_enforcement,
        "Human pairing helper changed non-Human anatomy");
    edited_dog->nodes[2].x += 0.07f;
    edited_dog->rebuild_rest_lengths();
    error.clear();
    require(edited_dog->save(dog_path, error), error);
    require(read_text(dog_path) != dog_before_edit,
        "Dog edit did not publish to dog.rig");
    require(read_text(human_path) == human_before_dog_edit,
        "Dog edit overwrote Human geometry");

    CreatureBlueprint human = CreatureBlueprint::humanoid();
    human.nodes[3] += { -0.24f, 0.11f };
    human.radii[3] = 0.23f;
    const float edited_left_shin = segment_length(human, 3u, 4u);
    const runner::Vec2 edited_left_shin_direction =
        (human.nodes[4] - human.nodes[3]) / edited_left_shin;
    require(human.enforce_human_paired_segment_lengths(3u),
        "valid Human edit must preserve paired anatomy");
    require(near(segment_length(human, 0u, 3u), segment_length(human, 0u, 5u)),
        "Human thighs must remain equal");
    require(near(segment_length(human, 3u, 4u), segment_length(human, 5u, 6u)),
        "Human shins must remain equal");
    require(near(segment_length(human, 7u, 8u), segment_length(human, 10u, 11u)),
        "Human upper arms must remain equal");
    require(near(segment_length(human, 8u, 9u), segment_length(human, 11u, 12u)),
        "Human forearms must remain equal");
    require(near(human.radii[3], human.radii[5]),
        "paired Human node radii must remain equal");
    const runner::Vec2 rebuilt_left_shin_direction =
        (human.nodes[4] - human.nodes[3]) / segment_length(human, 3u, 4u);
    require(near(rebuilt_left_shin_direction.x, edited_left_shin_direction.x)
            && near(rebuilt_left_shin_direction.y, edited_left_shin_direction.y),
        "moving a Human knee must not invert its distal bend direction");

    CreatureBlueprint right_origin_human = CreatureBlueprint::humanoid();
    right_origin_human.nodes[5] += { 0.21f, 0.09f };
    right_origin_human.radii[5] = 0.22f;
    const float edited_right_shin = segment_length(right_origin_human, 5u, 6u);
    const runner::Vec2 edited_right_shin_direction =
        (right_origin_human.nodes[6] - right_origin_human.nodes[5])
        / edited_right_shin;
    require(right_origin_human.enforce_human_paired_segment_lengths(5u),
        "right-origin Human edit must preserve paired anatomy");
    require(near(segment_length(right_origin_human, 0u, 3u),
                 segment_length(right_origin_human, 0u, 5u))
            && near(segment_length(right_origin_human, 3u, 4u),
                    segment_length(right_origin_human, 5u, 6u)),
        "right-origin Human edit did not lock both leg chains");
    require(near(right_origin_human.radii[3], 0.22f)
            && near(right_origin_human.radii[5], 0.22f),
        "right-origin Human edit did not lock paired radii");
    const runner::Vec2 rebuilt_right_shin_direction =
        (right_origin_human.nodes[6] - right_origin_human.nodes[5])
        / segment_length(right_origin_human, 5u, 6u);
    require(near(rebuilt_right_shin_direction.x, edited_right_shin_direction.x)
            && near(rebuilt_right_shin_direction.y, edited_right_shin_direction.y),
        "right-origin Human edit inverted its distal bend direction");

    CreatureBlueprint invalid_human = CreatureBlueprint::humanoid();
    invalid_human.nodes[3] = invalid_human.nodes[0];
    const CreatureBlueprint invalid_snapshot = invalid_human;
    require(!invalid_human.enforce_human_paired_segment_lengths(3u),
        "zero-length Human segment must be rejected");
    require(invalid_human.nodes.size() == invalid_snapshot.nodes.size(),
        "rejected pair enforcement changed the node count");
    for (std::size_t node = 0u; node < invalid_human.nodes.size(); ++node)
    {
        require(near(invalid_human.nodes[node].x, invalid_snapshot.nodes[node].x)
                && near(invalid_human.nodes[node].y, invalid_snapshot.nodes[node].y),
            "rejected pair enforcement must not partially mutate the rig");
    }

    const std::filesystem::path preserved_path = test_directory / "preserved.rig";
    {
        std::ofstream output{ preserved_path, std::ios::binary | std::ios::trunc };
        output << "preserve-existing-rig";
    }
    error.clear();
    require(!invalid_human.save(preserved_path, error),
        "invalid Human rig must not publish");
    require(read_text(preserved_path) == "preserve-existing-rig",
        "failed rig save damaged the previously published file");

    CreatureBlueprint asymmetric_human = CreatureBlueprint::humanoid();
    asymmetric_human.nodes[3] += { -0.17f, 0.08f };
    asymmetric_human.nodes[11] += { 0.13f, -0.04f };
    const std::filesystem::path asymmetric_a = test_directory / "human-asymmetric-a.rig";
    const std::filesystem::path asymmetric_b = test_directory / "human-asymmetric-b.rig";
    error.clear();
    require(asymmetric_human.save(asymmetric_a, error), error);
    error.clear();
    require(asymmetric_human.save(asymmetric_b, error), error);
    require(read_text(asymmetric_a) == read_text(asymmetric_b),
        "repeated asymmetric Human saves must normalize byte-identically");
    error.clear();
    const std::optional<CreatureBlueprint> normalized_human =
        CreatureBlueprint::load_for_species(asymmetric_a,
            CreatureSpecies::human, error);
    require(normalized_human.has_value(), error);
    require(near(segment_length(*normalized_human, 0u, 3u),
                 segment_length(*normalized_human, 0u, 5u))
            && near(segment_length(*normalized_human, 7u, 8u),
                    segment_length(*normalized_human, 10u, 11u)),
        "serialized Human rig did not retain paired limb dimensions");

    std::string human_text = read_text(human_path);
    const std::filesystem::path legacy_path = test_directory / "creature.rig";
    human_text.replace(0u, std::string_view{ "RUNRIG 5" }.size(), "RUNRIG 4");
    const std::size_t identity_begin = human_text.find("P human\n");
    require(identity_begin != std::string::npos, "Human fixture identity missing");
    human_text.erase(identity_begin, std::string_view{ "P human\n" }.size());
    {
        std::ofstream output{ legacy_path, std::ios::binary | std::ios::trunc };
        output << human_text;
    }
    error.clear();
    require(CreatureBlueprint::load_for_species(
            legacy_path, CreatureSpecies::human, error).has_value(),
        "compatible version-4 creature.rig must migrate by inferred topology");
    error.clear();
    require(!CreatureBlueprint::load_for_species(
            legacy_path, CreatureSpecies::dog, error),
        "legacy migration must not cross species");

    const std::filesystem::path migration_directory = test_directory / "migration";
    filesystem_error.clear();
    std::filesystem::create_directories(migration_directory, filesystem_error);
    require(!filesystem_error, "could not create migration test directory");
    const std::filesystem::path migration_legacy = migration_directory / "creature.rig";
    {
        std::ofstream output{ migration_legacy, std::ios::binary | std::ios::trunc };
        output << human_text;
    }
    const std::filesystem::path migrated_human = migration_directory / "human.rig";
    std::string source_note{};
    CreatureBlueprint centrally_migrated = CreatureBlueprint::load_owned_or_default(
        CreatureSpecies::human, migrated_human, migration_legacy, source_note);
    require(centrally_migrated.presentation_species() == CreatureSpecies::human,
        "central loader migrated the wrong species");
    require(std::filesystem::exists(migrated_human)
            && std::filesystem::exists(migration_legacy),
        "legacy migration must publish human.rig without deleting creature.rig");
    require(source_note.find("MIGRATED TO human.rig") != std::string::npos,
        "legacy migration source was not reported");
    error.clear();
    require(CreatureBlueprint::load_for_species(migrated_human,
            CreatureSpecies::human, error).has_value(),
        "central loader did not publish a valid species-owned rig");

    const std::filesystem::path rejected_directory = test_directory / "rejected-owned";
    filesystem_error.clear();
    std::filesystem::create_directories(rejected_directory, filesystem_error);
    require(!filesystem_error, "could not create rejected-owned test directory");
    const std::filesystem::path rejected_owned = rejected_directory / "human.rig";
    const std::filesystem::path valid_legacy = rejected_directory / "creature.rig";
    {
        std::ofstream output{ rejected_owned, std::ios::binary | std::ios::trunc };
        output << "RUNRIG 5\nP dog\nN broken\n";
    }
    {
        std::ofstream output{ valid_legacy, std::ios::binary | std::ios::trunc };
        output << human_text;
    }
    const std::string rejected_bytes = read_text(rejected_owned);
    source_note.clear();
    const CreatureBlueprint rejected_fallback =
        CreatureBlueprint::load_owned_or_default(CreatureSpecies::human,
            rejected_owned, valid_legacy, source_note);
    require(rejected_fallback.signature()
            == CreatureBlueprint::humanoid().signature(),
        "invalid owned rig must select the Human factory fallback");
    require(source_note.find("human.rig REJECTED") != std::string::npos,
        "invalid owned rig rejection was hidden");
    require(read_text(rejected_owned) == rejected_bytes,
        "invalid owned rig was silently overwritten by legacy migration");

    const std::filesystem::path wrong_legacy_directory = test_directory / "wrong-legacy";
    filesystem_error.clear();
    std::filesystem::create_directories(wrong_legacy_directory, filesystem_error);
    require(!filesystem_error, "could not create wrong-legacy test directory");
    const std::filesystem::path wrong_legacy = wrong_legacy_directory / "creature.rig";
    const std::filesystem::path absent_dog = wrong_legacy_directory / "dog.rig";
    {
        std::ofstream output{ wrong_legacy, std::ios::binary | std::ios::trunc };
        output << human_text;
    }
    source_note.clear();
    const CreatureBlueprint wrong_legacy_fallback =
        CreatureBlueprint::load_owned_or_default(CreatureSpecies::dog,
            absent_dog, wrong_legacy, source_note);
    require(wrong_legacy_fallback.signature()
            == CreatureBlueprint::crawler4().signature(),
        "cross-species legacy file must select the Dog factory fallback");
    require(!std::filesystem::exists(absent_dog),
        "cross-species legacy file created dog.rig");
    require(source_note.find("REJECTED FOR dog") != std::string::npos,
        "cross-species legacy rejection was not actionable");

    const std::filesystem::path empty_directory = test_directory / "empty";
    filesystem_error.clear();
    std::filesystem::create_directories(empty_directory, filesystem_error);
    require(!filesystem_error, "could not create empty loader test directory");
    source_note.clear();
    const CreatureBlueprint empty_fallback =
        CreatureBlueprint::load_owned_or_default(CreatureSpecies::chicken,
            empty_directory / "chicken.rig", empty_directory / "creature.rig",
            source_note);
    require(empty_fallback.signature() == CreatureBlueprint::chicken().signature()
            && source_note == "FACTORY DEFAULT",
        "missing owned and legacy rigs must select the species factory explicitly");

    std::string mismatched = read_text(human_path);
    const std::size_t human_identity = mismatched.find("P human\n");
    require(human_identity != std::string::npos, "declared identity missing");
    mismatched.replace(human_identity, std::string_view{ "P human" }.size(), "P dog");
    const std::filesystem::path mismatched_path = test_directory / "mismatch.rig";
    {
        std::ofstream output{ mismatched_path, std::ios::binary | std::ios::trunc };
        output << mismatched;
    }
    error.clear();
    (void)CreatureBlueprint::load(mismatched_path, error);
    require(error.find("declared species") != std::string::npos,
        "declared identity/topology mismatch must be rejected");

    const std::filesystem::path malformed_path = test_directory / "malformed.rig";
    {
        std::ofstream output{ malformed_path, std::ios::binary | std::ios::trunc };
        output << "RUNRIG 5\nP human\nN nope\n";
    }
    error.clear();
    require(!CreatureBlueprint::load_for_species(
            malformed_path, CreatureSpecies::human, error),
        "malformed species rig must be rejected");
    require(!error.empty(), "malformed species rig rejection must be actionable");

    filesystem_error.clear();
    std::filesystem::remove_all(test_directory, filesystem_error);
    require(!filesystem_error, "could not clean test directory");
    std::cout << "Runner v0.7.47 species rig persistence tests passed\n";
    return 0;
}
