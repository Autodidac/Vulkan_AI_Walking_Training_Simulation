#pragma once

#include "ai_director.hpp"
#include "course_geometry.hpp"

#include <sandhybrid/camera_policy.hpp>
#include <sandhybrid/library.hpp>
#include <sandhybrid/material.hpp>
#include <sandhybrid/material_color.hpp>
#include <sandhybrid/section_grid.hpp>
#include <sandhybrid/terrain_generation.hpp>
#include <sandhybrid/world_layout.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace runner::sim
{
    // Runner material-course state is derived from the pinned SandHybrid contracts.
    enum class TerrainRegion : std::uint8_t
    {
        firm,
        dry_sand,
        waterlogged,
        shallow_water,
        hole
    };

    [[nodiscard]] inline std::string_view terrain_region_name(
        TerrainRegion region) noexcept
    {
        switch (region)
        {
        case TerrainRegion::firm: return "FIRM GROUND";
        case TerrainRegion::dry_sand: return "DRY SAND CELLS";
        case TerrainRegion::waterlogged: return "WATERLOGGED SAND";
        case TerrainRegion::shallow_water: return "SHALLOW WATER";
        case TerrainRegion::hole: return "GROUND HOLE";
        }
        return "UNKNOWN TERRAIN";
    }

    struct TerrainChallenge
    {
        std::uint32_t stable_id{};
        std::uint16_t sequence{};
        director::ChallengeKind kind{ director::ChallengeKind::flat };
        TerrainRegion region{ TerrainRegion::firm };
        float begin{};
        float end{};

        friend bool operator==(const TerrainChallenge&,
            const TerrainChallenge&) = default;

        [[nodiscard]] float center() const noexcept
        {
            return 0.5f * (begin + end);
        }

        [[nodiscard]] float length() const noexcept
        {
            return std::max(0.0f, end - begin);
        }

        [[nodiscard]] bool contains(float local_x) const noexcept
        {
            return local_x >= begin && local_x < end;
        }
    };

    class DeformableTerrain
    {
    public:
        struct Cell
        {
            float height{};
            float rest_height{};
            float firmness{ 0.35f };
            float loose_fraction{ 0.65f };
            float water_surface{};
            float water_depth{};
            sandhybrid::Material surface_material{ sandhybrid::Material::sand };
            TerrainRegion region{ TerrainRegion::dry_sand };
        };

        struct FineCell
        {
            sandhybrid::SceneCell state{
                static_cast<std::uint32_t>(sandhybrid::Material::atmosphere),
                0u, 20, 0u };
            float fill{};
            std::uint8_t metadata{};

            [[nodiscard]] sandhybrid::Material material() const noexcept
            {
                return static_cast<sandhybrid::Material>(state.material);
            }

            [[nodiscard]] bool occupied() const noexcept
            {
                return material() != sandhybrid::Material::empty
                    && material() != sandhybrid::Material::atmosphere
                    && fill > 1.0e-6f;
            }

            [[nodiscard]] bool structural() const noexcept
            {
                return (state.aux & structural_aux) != 0u;
            }

            [[nodiscard]] bool authored() const noexcept
            {
                return (metadata & authored_metadata) != 0u;
            }

            [[nodiscard]] const sandhybrid::SceneCell& canonical_cell() const noexcept
            {
                return state;
            }

            [[nodiscard]] bool same_state(const FineCell& other) const noexcept
            {
                return state.material == other.state.material
                    && state.age == other.state.age
                    && state.temperature == other.state.temperature
                    && state.aux == other.state.aux
                    && fill == other.fill && metadata == other.metadata;
            }

            static constexpr std::uint32_t structural_aux = 0x04000000u;
            static constexpr std::uint32_t moved_aux = 0x01000000u;
            static constexpr std::uint8_t authored_metadata = 0x01u;
        };

        struct MacroTile
        {
            std::uint64_t occupied_mask{};
            std::uint64_t structural_mask{};
            sandhybrid::Material uniform_material{ sandhybrid::Material::empty };
            bool macro_ready{};
            bool active{};
        };

        static constexpr std::size_t macro_cell_side = sandhybrid::terrain::tile_size;
        static constexpr float fine_cell_spacing =
            course_geometry::fine_cell_spacing_meters;
        static constexpr float cell_spacing = fine_cell_spacing;
        static constexpr float macro_tile_size = fine_cell_spacing
            * static_cast<float>(macro_cell_side);
        static constexpr std::size_t cell_count =
            sandhybrid::pre_expansion_world_width;
        static constexpr std::size_t vertical_cell_count =
            sandhybrid::pre_expansion_world_height;
        static constexpr sandhybrid::WorldSizePreset training_world_preset =
            sandhybrid::WorldSizePreset::compact;
        static constexpr auto training_world_dimensions =
            sandhybrid::world_dimensions(training_world_preset);
        static constexpr std::uint32_t training_district = 0u;
        static constexpr std::uint32_t training_district_origin_x =
            sandhybrid::persistent_world_district_origin_x(
                training_world_dimensions.width, training_district);
        static constexpr std::uint32_t training_world_surface_y =
            sandhybrid::persistent_world_surface_y(training_world_dimensions.height);
        static constexpr std::size_t fine_cell_count = cell_count * vertical_cell_count;
        static constexpr std::size_t macro_columns = cell_count / macro_cell_side;
        static constexpr std::size_t macro_rows = vertical_cell_count / macro_cell_side;
        static constexpr std::size_t macro_tile_count = macro_columns * macro_rows;
        static constexpr float period = course_geometry::authored_span_meters;
        static constexpr std::size_t terrain_challenge_count = 10u;
        static constexpr std::uint32_t primary_flat_challenge_id = 0xE2D00001u;
        static constexpr std::uint32_t loose_cells_challenge_id = 0xE2D00002u;
        static constexpr std::uint32_t middle_flat_challenge_id = 0xE2D00003u;
        static constexpr std::uint32_t waterlogged_cells_challenge_id = 0xE2D00004u;
        static constexpr std::uint32_t shallow_water_challenge_id = 0xE2D00005u;
        static constexpr std::uint32_t gap_challenge_id = 0xE2D00006u;
        static constexpr std::uint32_t tail_loose_cells_challenge_id = 0xE2D00007u;
        static constexpr std::uint32_t tail_flat_challenge_id = 0xE2D00008u;
        static constexpr std::uint32_t return_flat_challenge_id = 0xE2D00009u;
        static constexpr std::uint32_t shore_challenge_id = 0xE2D0000Au;
        static constexpr std::uint32_t step_challenge_id = 0xE2D0000Bu;
        static constexpr std::uint32_t ramp_challenge_id = 0xE2D0000Cu;
        static constexpr std::uint32_t post_lake_recovery_challenge_id =
            0xE2D0000Du;
        static constexpr float step_challenge_begin = 61.875f;
        static constexpr float step_challenge_end = 66.09375f;
        static constexpr float ramp_challenge_begin = 69.75f;
        static constexpr float ramp_challenge_end = 78.1875f;
        static constexpr float launch_pad_half_width = 0.70f;
        static constexpr float launch_transition_width = 0.55f;
        // Keep the authored surface at the middle row of SandHybrid's canonical
        // 640x360 footprint.  A shallow -4.5 m floor populated only ~32 rows
        // and left most of the visible cross-section as a renderer fallback.
        // The deeper half-world is real SceneCell-compatible material state:
        // presentation, save/export, and collision inspection all see the same
        // cells instead of an opaque decorative slab.
        static constexpr float world_bottom =
            -static_cast<float>(vertical_cell_count / 2u) * fine_cell_spacing;
        static constexpr float world_top = world_bottom
            + static_cast<float>(vertical_cell_count) * fine_cell_spacing;

        static_assert(macro_cell_side == 8u);
        static_assert(cell_count == course_geometry::authored_cell_count);
        static_assert(cell_count % macro_cell_side == 0u);
        static_assert(vertical_cell_count % macro_cell_side == 0u);
        static_assert(macro_tile_size == 1.125f);
        static_assert(world_bottom == -25.3125f);
        static_assert(world_top == 25.3125f);
        static_assert(sandhybrid::material_count < 256u);

        void reset(std::uint64_t seed, float difficulty) noexcept
        {
            seed_ = seed == 0u ? 1u : seed;
            difficulty_ = std::clamp(difficulty, 0.0f, 1.0f);
            live_surface_updates_ = false;
            std::fill(cells_.begin(), cells_.end(), Cell{});
            std::fill(fine_cells_.begin(), fine_cells_.end(), FineCell{});
            std::fill(macro_tiles_.begin(), macro_tiles_.end(), MacroTile{});
            std::fill(surface_rows_.begin(), surface_rows_.end(), -1);
            section_grid_ = {};
            tick_ = 1u;
            macro_promotions_ = 0u;
            macro_demotions_ = 0u;

            for (std::size_t column = 0; column < cell_count; ++column)
                initialize_column(column);
            author_natural_geology_texture();
            author_training_map_features();
            for (std::size_t column = 0; column < cell_count; ++column)
            {
                refresh_column(column);
                cells_[column].rest_height = cells_[column].height;
                const FineCell* top = top_cell(column);
                const float variation = unit_hash(seed_ ^ (static_cast<std::uint64_t>(column)
                    * 0xbf58476d1ce4e5b9ULL));
                Cell& cell = cells_[column];
                switch (cell.region)
                {
                case TerrainRegion::firm:
                    cell.firmness = top != nullptr && top->structural()
                        ? 0.96f : 0.88f + variation * 0.08f;
                    break;
                case TerrainRegion::dry_sand:
                    cell.firmness = 0.52f - difficulty_ * 0.24f
                        + variation * 0.18f;
                    break;
                case TerrainRegion::waterlogged:
                    cell.firmness = 0.12f + variation * 0.12f;
                    break;
                case TerrainRegion::shallow_water:
                    cell.firmness = 0.28f + variation * 0.14f;
                    break;
                case TerrainRegion::hole:
                    cell.firmness = 0.58f + variation * 0.20f;
                    break;
                }
                if (launch_pad_at(static_cast<float>(column) * fine_cell_spacing))
                {
                    cell.firmness = 1.0f;
                    cell.loose_fraction = 0.0f;
                }
                else
                {
                    cell.loose_fraction = std::clamp(1.0f - cell.firmness
                        + (cell.region == TerrainRegion::waterlogged ? 0.10f : 0.0f),
                        0.0f, 1.0f);
                }
            }
            live_surface_updates_ = true;
            refresh_all_macro_tiles();
            macro_promotions_ = 0u;
            macro_demotions_ = 0u;
            section_grid_.mark_dirty({ 0, 0,
                static_cast<std::int32_t>(cell_count),
                static_cast<std::int32_t>(vertical_cell_count) });
            section_grid_.begin_tick(tick_);
        }

        [[nodiscard]] float height_at(float course_x) const noexcept
        {
            return cells_[nearest_index(course_x)].height;
        }

        [[nodiscard]] static bool launch_pad_at(float course_x) noexcept
        {
            float local = std::fmod(course_x, period);
            if (local < 0.0f)
                local += period;
            return std::min(local, period - local) <= launch_pad_half_width;
        }

        [[nodiscard]] float firmness_at(float course_x) const noexcept
        {
            return std::clamp(cells_[nearest_index(course_x)].firmness, 0.0f, 1.0f);
        }

        [[nodiscard]] float looseness_at(float course_x) const noexcept
        {
            return std::clamp(cells_[nearest_index(course_x)].loose_fraction, 0.0f, 1.0f);
        }

        [[nodiscard]] TerrainRegion region_at(float course_x) const noexcept
        {
            return cells_[nearest_index(course_x)].region;
        }

        [[nodiscard]] sandhybrid::Material surface_material_at(
            float course_x) const noexcept
        {
            return cells_[nearest_index(course_x)].surface_material;
        }

        [[nodiscard]] float water_depth_at(float course_x) const noexcept
        {
            return std::max(0.0f, cells_[nearest_index(course_x)].water_depth);
        }

        [[nodiscard]] float water_surface_at(float course_x) const noexcept
        {
            return cells_[nearest_index(course_x)].water_surface;
        }

        [[nodiscard]] std::array<TerrainChallenge, terrain_challenge_count>
        terrain_challenges() const noexcept
        {
            const CourseBoundaries boundaries = course_boundaries();
            const auto kind_for_region = [](TerrainRegion region) noexcept
            {
                switch (region)
                {
                case TerrainRegion::firm:
                    return director::ChallengeKind::flat;
                case TerrainRegion::dry_sand:
                    return director::ChallengeKind::loose_cells;
                case TerrainRegion::waterlogged:
                    return director::ChallengeKind::waterlogged_cells;
                case TerrainRegion::shallow_water:
                    return director::ChallengeKind::shallow_water;
                case TerrainRegion::hole:
                    return director::ChallengeKind::gap;
                }
                return director::ChallengeKind::flat;
            };
            const TerrainRegion tail_first = boundaries.sand_first
                ? TerrainRegion::dry_sand : TerrainRegion::firm;
            const TerrainRegion tail_second = boundaries.sand_first
                ? TerrainRegion::firm : TerrainRegion::dry_sand;
            return {{
                { primary_flat_challenge_id, 0u, director::ChallengeKind::flat,
                    TerrainRegion::firm,
                    0.0f, boundaries.first },
                { loose_cells_challenge_id, 1u,
                    director::ChallengeKind::loose_cells, TerrainRegion::dry_sand,
                    boundaries.first, boundaries.second },
                { middle_flat_challenge_id, 2u, director::ChallengeKind::flat,
                    TerrainRegion::firm,
                    boundaries.second, boundaries.third },
                { waterlogged_cells_challenge_id,
                    3u, director::ChallengeKind::waterlogged_cells,
                    TerrainRegion::waterlogged,
                    boundaries.third, boundaries.fourth },
                { shallow_water_challenge_id,
                    4u, director::ChallengeKind::shallow_water,
                    TerrainRegion::shallow_water,
                    boundaries.fourth, boundaries.fifth },
                { post_lake_recovery_challenge_id, 5u,
                    director::ChallengeKind::flat, TerrainRegion::firm,
                    boundaries.fifth, boundaries.sixth },
                { gap_challenge_id, 6u, director::ChallengeKind::gap,
                    TerrainRegion::hole,
                    boundaries.sixth, boundaries.seventh },
                { tail_first == TerrainRegion::dry_sand
                        ? tail_loose_cells_challenge_id : tail_flat_challenge_id,
                    7u, kind_for_region(tail_first), tail_first,
                    boundaries.seventh, boundaries.tail_split },
                { tail_second == TerrainRegion::dry_sand
                        ? tail_loose_cells_challenge_id : tail_flat_challenge_id,
                    8u, kind_for_region(tail_second), tail_second,
                    boundaries.tail_split, boundaries.tail_end },
                { return_flat_challenge_id, 9u, director::ChallengeKind::flat,
                    TerrainRegion::firm,
                    boundaries.tail_end, period }
            }};
        }

        [[nodiscard]] std::optional<TerrainChallenge> challenge_for(
            director::ChallengeKind kind) const noexcept
        {
            std::uint32_t stable_id{};
            switch (kind)
            {
            case director::ChallengeKind::flat:
                stable_id = primary_flat_challenge_id;
                break;
            case director::ChallengeKind::loose_cells:
                stable_id = loose_cells_challenge_id;
                break;
            case director::ChallengeKind::waterlogged_cells:
                stable_id = waterlogged_cells_challenge_id;
                break;
            case director::ChallengeKind::shallow_water:
                stable_id = shallow_water_challenge_id;
                break;
            case director::ChallengeKind::gap:
                stable_id = gap_challenge_id;
                break;
            case director::ChallengeKind::step:
            {
                const float center = 0.5f
                    * (step_challenge_begin + step_challenge_end);
                return TerrainChallenge{ step_challenge_id, 10u,
                    director::ChallengeKind::step, region_at(center),
                    step_challenge_begin, step_challenge_end };
            }
            case director::ChallengeKind::ramp:
            {
                const float center = 0.5f
                    * (ramp_challenge_begin + ramp_challenge_end);
                return TerrainChallenge{ ramp_challenge_id, 11u,
                    director::ChallengeKind::ramp, region_at(center),
                    ramp_challenge_begin, ramp_challenge_end };
            }
            case director::ChallengeKind::shore:
            {
                const auto shallow = challenge_for(
                    director::ChallengeKind::shallow_water);
                if (!shallow)
                    return std::nullopt;
                return TerrainChallenge{ shore_challenge_id, shallow->sequence,
                    director::ChallengeKind::shore,
                    TerrainRegion::shallow_water,
                    std::max(shallow->begin,
                        shallow->end - 2.0f * fine_cell_spacing),
                    std::min(period,
                        shallow->end + 2.0f * fine_cell_spacing) };
            }
            case director::ChallengeKind::ledge:
            case director::ChallengeKind::low_clearance:
            case director::ChallengeKind::aim_range:
            case director::ChallengeKind::moving_target:
            case director::ChallengeKind::construction_anchor:
                return std::nullopt;
            }
            for (const TerrainChallenge& challenge : terrain_challenges())
                if (challenge.stable_id == stable_id)
                    return challenge;
            return std::nullopt;
        }

        [[nodiscard]] TerrainChallenge challenge_at(float course_x) const noexcept
        {
            const float local = wrapped_course_x(course_x);
            for (const director::ChallengeKind overlay : {
                director::ChallengeKind::shore,
                director::ChallengeKind::step,
                director::ChallengeKind::ramp })
            {
                const auto challenge = challenge_for(overlay);
                if (challenge && challenge->contains(local))
                    return *challenge;
            }
            for (const TerrainChallenge& challenge : terrain_challenges())
                if (challenge.contains(local))
                    return challenge;
            return terrain_challenges().back();
        }

        [[nodiscard]] float challenge_progress_at(float course_x) const noexcept
        {
            const TerrainChallenge challenge = challenge_at(course_x);
            return std::clamp((wrapped_course_x(course_x) - challenge.begin)
                / std::max(challenge.length(), fine_cell_spacing), 0.0f, 1.0f);
        }

        [[nodiscard]] float slope_at(float course_x) const noexcept
        {
            return (height_at(course_x + fine_cell_spacing)
                - height_at(course_x - fine_cell_spacing))
                / (fine_cell_spacing * 2.0f);
        }

        void apply_pressure(float course_x, float normalized_load, float slip_speed,
            float dt) noexcept
        {
            // Authored fine cells are immutable course geometry. Contact load is
            // an observation only; it may not excavate, compact, or raise a berm.
            static_cast<void>(course_x);
            static_cast<void>(normalized_load);
            static_cast<void>(slip_speed);
            static_cast<void>(dt);
        }

        void deposit(float course_x, float height_volume, float material_firmness,
            sandhybrid::Material material = sandhybrid::Material::sand) noexcept
        {
            const std::size_t center = nearest_index(course_x);
            if (launch_pad_at(static_cast<float>(center) * fine_cell_spacing))
                return;
            const float amount = std::max(0.0f, height_volume);
            constexpr std::array<float, 5> weights{ 0.10f, 0.22f, 0.36f, 0.22f, 0.10f };
            float remaining = amount;
            for (std::size_t offset = 0; offset < weights.size(); ++offset)
            {
                const auto signed_offset = static_cast<std::ptrdiff_t>(offset) - 2;
                const std::size_t column_index = wrap_column(
                    static_cast<std::ptrdiff_t>(center) + signed_offset);
                if (launch_pad_at(static_cast<float>(column_index) * fine_cell_spacing))
                    continue;
                const float requested = offset + 1u == weights.size()
                    ? remaining : amount * weights[offset];
                const float added = add_volume(column_index, requested,
                    material, false);
                remaining -= added;
                Cell& column = cells_[column_index];
                column.firmness = std::lerp(column.firmness,
                    std::clamp(material_firmness, 0.0f, 1.0f),
                    std::clamp(added * 5.0f, 0.0f, 0.30f));
                column.loose_fraction = std::clamp(
                    column.loose_fraction + added * 4.0f, 0.0f, 1.0f);
            }
            if (remaining > 0.0f)
                static_cast<void>(add_volume(center, remaining,
                    material, false));
        }

        [[nodiscard]] float excavate(float course_x, float height_volume,
            float radius) noexcept
        {
            if (!std::isfinite(course_x) || !std::isfinite(height_volume)
                || !std::isfinite(radius) || height_volume <= 0.0f)
                return 0.0f;
            const std::size_t center = nearest_index(course_x);
            const int radius_cells = std::clamp(static_cast<int>(std::ceil(
                std::max(radius, fine_cell_spacing) / fine_cell_spacing)), 1, 18);
            float weight_sum{};
            for (int offset = -radius_cells; offset <= radius_cells; ++offset)
                weight_sum += 1.0f - static_cast<float>(std::abs(offset))
                    / static_cast<float>(radius_cells + 1);
            float removed{};
            for (int offset = -radius_cells; offset <= radius_cells; ++offset)
            {
                const std::size_t column = wrap_column(
                    static_cast<std::ptrdiff_t>(center) + offset);
                if (launch_pad_at(static_cast<float>(column) * fine_cell_spacing))
                    continue;
                const FineCell* surface = top_cell(column);
                if (surface == nullptr || surface->structural() || surface->authored())
                    continue;
                const float weight = 1.0f - static_cast<float>(std::abs(offset))
                    / static_cast<float>(radius_cells + 1);
                const float column_removed = remove_excavatable_volume(column,
                    height_volume * weight / std::max(weight_sum, 1.0e-6f));
                removed += column_removed;
                cells_[column].loose_fraction = std::clamp(
                    cells_[column].loose_fraction + column_removed * 2.0f,
                    0.0f, 1.0f);
            }
            return removed;
        }
        void step(float dt) noexcept
        {
            const float bounded_dt = std::clamp(dt, 0.0f, 0.05f);
            section_grid_.begin_tick(++tick_);
            const bool reverse = (tick_ & 1u) != 0u;
            for (std::size_t offset = 0; offset < cell_count; ++offset)
            {
                const std::size_t index = reverse
                    ? cell_count - 1u - offset : offset;
                const std::size_t right = (index + 1u) % cell_count;
                const float index_x = static_cast<float>(index) * fine_cell_spacing;
                const float right_x = static_cast<float>(right) * fine_cell_spacing;
                if (launch_pad_at(index_x) || launch_pad_at(right_x))
                    continue;
                const float difference = cells_[index].height - cells_[right].height;
                const float average_firmness = 0.5f
                    * (cells_[index].firmness + cells_[right].firmness);
                const float repose = fine_cell_spacing
                    * (0.60f + average_firmness * 1.35f);
                const float excess = std::abs(difference) - repose;
                if (excess <= 0.0f)
                    continue;

                const std::size_t high = difference > 0.0f ? index : right;
                const std::size_t low = difference > 0.0f ? right : index;
                const FineCell* high_top = top_cell(high);
                if (high_top == nullptr || high_top->structural() || high_top->authored()
                    || (high_top->material() != sandhybrid::Material::sand
                        && high_top->material() != sandhybrid::Material::mud))
                    continue;
                const float mobility = std::clamp(1.0f - average_firmness, 0.08f, 1.0f);
                const float movement = std::min(excess * 0.20f,
                    excess * mobility * bounded_dt * 2.8f);
                const float moved = move_loose_volume(high, low, movement);
                if (moved <= 0.0f)
                    continue;
                cells_[high].loose_fraction = std::clamp(
                    cells_[high].loose_fraction + moved * 1.5f, 0.0f, 1.0f);
                cells_[low].loose_fraction = std::clamp(
                    cells_[low].loose_fraction + moved * 2.0f, 0.0f, 1.0f);
            }
        }

        [[nodiscard]] double total_height_volume() const noexcept
        {
            double result = 0.0;
            for (const FineCell& cell : fine_cells_)
            {
                if (cell.occupied())
                    result += static_cast<double>(cell.fill)
                        * static_cast<double>(fine_cell_spacing);
            }
            return result;
        }

        [[nodiscard]] float maximum_neighbor_delta() const noexcept
        {
            float result = 0.0f;
            for (std::size_t index = 0; index < cell_count; ++index)
                result = std::max(result, std::abs(cells_[index].height
                    - cells_[(index + 1u) % cell_count].height));
            return result;
        }

        [[nodiscard]] std::size_t hard_ledge_count() const noexcept
        {
            std::size_t result = 0u;
            for (std::size_t index = 0; index < cell_count; ++index)
            {
                const float delta = std::abs(cells_[index].height
                    - cells_[(index + 1u) % cell_count].height);
                if (delta >= macro_tile_size * 0.75f)
                    ++result;
            }
            return result;
        }

        [[nodiscard]] std::size_t irregular_boundary_count() const noexcept
        {
            std::size_t result = 0u;
            for (std::size_t index = 0; index < cell_count; ++index)
            {
                const float delta = std::abs(cells_[index].height
                    - cells_[(index + 1u) % cell_count].height);
                const auto delta_cells = static_cast<std::size_t>(std::lround(
                    delta / fine_cell_spacing));
                if (delta_cells > 0u && delta_cells % macro_cell_side != 0u)
                    ++result;
            }
            return result;
        }

        [[nodiscard]] static float actor_height_in_macro_tiles(float actor_height) noexcept
        {
            return actor_height / macro_tile_size;
        }

        [[nodiscard]] const std::vector<Cell>& cells() const noexcept
        {
            return cells_;
        }

        [[nodiscard]] const std::vector<FineCell>& fine_cells() const noexcept
        {
            return fine_cells_;
        }

        [[nodiscard]] const std::vector<MacroTile>& macro_tiles() const noexcept
        {
            return macro_tiles_;
        }

        [[nodiscard]] const FineCell& fine_cell(std::size_t column,
            std::size_t row) const noexcept
        {
            return fine_cells_[fine_index(column % cell_count,
                std::min(row, vertical_cell_count - 1u))];
        }

        [[nodiscard]] const MacroTile& macro_tile(std::size_t column,
            std::size_t row) const noexcept
        {
            return macro_tiles_[macro_index(column % macro_columns,
                std::min(row, macro_rows - 1u))];
        }

        [[nodiscard]] std::size_t macro_ready_count() const noexcept
        {
            return static_cast<std::size_t>(std::count_if(macro_tiles_.begin(),
                macro_tiles_.end(), [](const MacroTile& tile)
                {
                    return tile.macro_ready;
                }));
        }

        [[nodiscard]] std::size_t macro_promotions() const noexcept
        {
            return macro_promotions_;
        }

        [[nodiscard]] std::size_t macro_demotions() const noexcept
        {
            return macro_demotions_;
        }

        [[nodiscard]] std::size_t resident_section_count() const noexcept
        {
            return section_grid_.resident_section_count();
        }

        [[nodiscard]] std::size_t active_section_count() const noexcept
        {
            return section_grid_.active_section_count();
        }

        bool erase_cell(std::size_t column, std::size_t row) noexcept
        {
            column %= cell_count;
            if (row >= vertical_cell_count)
                return false;
            FineCell& cell = fine_cells_[fine_index(column, row)];
            if (!cell.occupied())
                return false;
            clear_cell(cell);
            changed_cell(column, row);
            return true;
        }

        bool paint_cell(std::size_t column, std::size_t row,
            sandhybrid::Material material, bool structural) noexcept
        {
            column %= cell_count;
            if (row >= vertical_cell_count || material == sandhybrid::Material::empty)
                return false;
            FineCell& cell = fine_cells_[fine_index(column, row)];
            set_cell(cell, material, structural, 1.0f, true);
            changed_cell(column, row);
            return true;
        }

        [[nodiscard]] static std::size_t wrap_column(std::ptrdiff_t column) noexcept
        {
            const auto count = static_cast<std::ptrdiff_t>(cell_count);
            column %= count;
            if (column < 0)
                column += count;
            return static_cast<std::size_t>(column);
        }

        [[nodiscard]] static float row_world_bottom(std::size_t row) noexcept
        {
            return world_bottom + static_cast<float>(row) * fine_cell_spacing;
        }

        [[nodiscard]] static std::uint32_t global_cell_x(float course_x) noexcept
        {
            if (!std::isfinite(course_x))
                return training_district_origin_x;
            const auto local = static_cast<std::ptrdiff_t>(std::floor(
                wrapped_course_x(course_x) / fine_cell_spacing));
            return training_district_origin_x
                + static_cast<std::uint32_t>(wrap_column(local));
        }

        [[nodiscard]] static std::uint32_t global_cell_y(float world_y) noexcept
        {
            if (!std::isfinite(world_y))
                return training_world_surface_y;
            const auto offset = static_cast<std::int32_t>(
                std::lround(world_y / fine_cell_spacing));
            return static_cast<std::uint32_t>(std::clamp(
                static_cast<std::int32_t>(training_world_surface_y) - offset,
                0, static_cast<std::int32_t>(training_world_dimensions.height - 1u)));
        }

    private:
        struct Sample
        {
            std::size_t first{};
            std::size_t second{};
            float fraction{};
        };

        struct SurfaceProfile
        {
            float height{};
            bool structural_ledge{};
            sandhybrid::Material material{ sandhybrid::Material::sand };
            TerrainRegion region{ TerrainRegion::dry_sand };
            float water_surface{};
        };

        struct CourseBoundaries
        {
            float first{};
            float second{};
            float third{};
            float fourth{};
            float fifth{};
            float sixth{};
            float seventh{};
            float tail_split{};
            float tail_end{};
            bool sand_first{};
        };

        [[nodiscard]] static std::uint64_t mix(std::uint64_t value) noexcept
        {
            value ^= value >> 30u;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27u;
            value *= 0x94d049bb133111ebULL;
            value ^= value >> 31u;
            return value;
        }

        [[nodiscard]] static float unit_hash(std::uint64_t value) noexcept
        {
            constexpr double denominator = static_cast<double>(1ULL << 53u);
            return static_cast<float>(static_cast<double>(mix(value) >> 11u)
                / denominator);
        }

        [[nodiscard]] CourseBoundaries course_boundaries() const noexcept
        {
            CourseBoundaries boundaries{};
            boundaries.first = launch_pad_half_width + launch_transition_width
                + 0.15f + unit_hash(seed_ ^ 0x10a2u) * 0.65f;
            boundaries.second = boundaries.first + 7.0f
                + unit_hash(seed_ ^ 0x20b3u) * 2.5f;
            boundaries.third = boundaries.second + 6.0f
                + unit_hash(seed_ ^ 0x30c4u) * 2.5f;
            boundaries.fourth = boundaries.third + 6.0f
                + unit_hash(seed_ ^ 0x40d5u) * 2.5f;
            boundaries.fifth = boundaries.fourth + 7.0f
                + unit_hash(seed_ ^ 0x50e6u) * 2.5f;
            // A dry, level recovery runway follows the complete lake.  The
            // old lake-to-hole seam asked a rig to establish a far-shore
            // plant and immediately negotiate a depression on the same foot
            // transfer, turning the lake into a universal sticking point.
            boundaries.sixth = boundaries.fifth + 3.25f
                + unit_hash(seed_ ^ 0x60f7u) * 1.25f;
            boundaries.seventh = boundaries.sixth + 5.0f
                + unit_hash(seed_ ^ 0x68f9u) * 1.5f;
            boundaries.tail_end = period - 5.0f;
            boundaries.tail_split = std::lerp(boundaries.seventh,
                boundaries.tail_end, 0.42f + unit_hash(seed_ ^ 0x70a8u) * 0.16f);
            boundaries.sand_first = (mix(seed_ ^ 0x80b9u) & 1u) == 0u;
            return boundaries;
        }

        [[nodiscard]] SurfaceProfile authored_surface(float course_x) const noexcept
        {
            float local = std::fmod(course_x, period);
            if (local < 0.0f)
                local += period;

            const CourseBoundaries boundaries = course_boundaries();
            const float first_boundary = boundaries.first;
            const float second_boundary = boundaries.second;
            const float third_boundary = boundaries.third;
            const float fourth_boundary = boundaries.fourth;
            const float fifth_boundary = boundaries.fifth;
            const float sixth_boundary = boundaries.sixth;
            const float seventh_boundary = boundaries.seventh;
            const float tail_end = boundaries.tail_end;
            const float tail_split = boundaries.tail_split;
            const bool return_pad = local > tail_end;
            const float distance_from_launch = std::min(local, period - local);
            const bool launch_pad = distance_from_launch <= launch_pad_half_width;

            TerrainRegion region = TerrainRegion::firm;
            sandhybrid::Material material = sandhybrid::Material::dirt;
            float region_begin = 0.0f;
            float region_end = first_boundary;
            if (!return_pad && local >= first_boundary && local < second_boundary)
            {
                region = TerrainRegion::dry_sand;
                material = sandhybrid::Material::sand;
                region_begin = first_boundary;
                region_end = second_boundary;
            }
            else if (!return_pad && local >= second_boundary && local < third_boundary)
            {
                region = TerrainRegion::firm;
                region_begin = second_boundary;
                region_end = third_boundary;
            }
            else if (!return_pad && local >= third_boundary && local < fourth_boundary)
            {
                region = TerrainRegion::waterlogged;
                region_begin = third_boundary;
                region_end = fourth_boundary;
            }
            else if (!return_pad && local >= fourth_boundary && local < fifth_boundary)
            {
                region = TerrainRegion::shallow_water;
                region_begin = fourth_boundary;
                region_end = fifth_boundary;
            }
            else if (!return_pad && local >= fifth_boundary && local < sixth_boundary)
            {
                region = TerrainRegion::firm;
                region_begin = fifth_boundary;
                region_end = sixth_boundary;
            }
            else if (!return_pad && local >= sixth_boundary
                && local < seventh_boundary)
            {
                region = TerrainRegion::hole;
                region_begin = sixth_boundary;
                region_end = seventh_boundary;
            }
            else if (!return_pad && local >= seventh_boundary)
            {
                const bool sand_first = boundaries.sand_first;
                const bool first_tail = local < tail_split;
                region = (first_tail == sand_first)
                    ? TerrainRegion::dry_sand : TerrainRegion::firm;
                material = region == TerrainRegion::dry_sand
                    ? sandhybrid::Material::sand : sandhybrid::Material::dirt;
                region_begin = first_tail ? seventh_boundary : tail_split;
                region_end = first_tail ? tail_split : tail_end;
            }
            if (region == TerrainRegion::waterlogged
                || region == TerrainRegion::shallow_water)
                material = sandhybrid::Material::mud;

            const bool step = local >= step_challenge_begin
                && local < step_challenge_end;
            const bool ramp = local >= ramp_challenge_begin
                && local < ramp_challenge_end;
            // Step and ramp lessons are firm natural earth. Combining them
            // with a randomly loose tail previously made their geometry and
            // traction change together, which obscured what was being tested.
            if (step || ramp)
            {
                region = TerrainRegion::firm;
                material = sandhybrid::Material::dirt;
                region_begin = step ? step_challenge_begin : ramp_challenge_begin;
                region_end = step ? step_challenge_end : ramp_challenge_end;
            }

            const auto smoothstep = [](float value) noexcept
            {
                value = std::clamp(value, 0.0f, 1.0f);
                return value * value * (3.0f - 2.0f * value);
            };
            const float edge_distance = std::max(0.0f,
                std::min(local - region_begin, region_end - local));
            const float edge_transition = region == TerrainRegion::shallow_water
                ? course_geometry::shore_transition_meters
                : region == TerrainRegion::waterlogged
                    ? 8.0f * fine_cell_spacing
                    : region == TerrainRegion::hole
                        ? course_geometry::shore_transition_meters : 0.80f;
            const float region_fade = smoothstep(edge_distance
                / edge_transition);
            const float transition = std::clamp((distance_from_launch
                - launch_pad_half_width) / launch_transition_width, 0.0f, 1.0f);
            const float smooth_transition = smoothstep(transition);
            constexpr float tau = 6.28318530717958647692f;
            const float broad_phase = unit_hash(seed_ ^ 0x91c51u) * tau;
            const float detail_phase = unit_hash(seed_ ^ 0xa61d3u) * tau;
            const float natural_grade = smooth_transition * (
                std::sin(local * (tau / 45.0f) + broad_phase) * 0.080f
                + std::sin(local * (tau / 18.0f) + detail_phase) * 0.035f);
            const float roughness = (region == TerrainRegion::firm ? 0.010f
                : region == TerrainRegion::hole ? 0.014f
                : 0.016f + difficulty_ * 0.018f) * smooth_transition
                * region_fade;
            float height = natural_grade;
            height += std::sin(local * 0.43f + detail_phase) * roughness;
            height += std::sin(local * 0.91f + broad_phase) * roughness * 0.38f;

            if (region == TerrainRegion::hole)
            {
                const float center = 0.5f * (region_begin + region_end);
                const float half_width = std::max(1.0f,
                    0.5f * (region_end - region_begin));
                const float normalized = std::clamp(
                    std::abs(local - center) / half_width, 0.0f, 1.0f);
                const float bowl = 1.0f - normalized * normalized
                    * (3.0f - 2.0f * normalized);
                height -= bowl * (0.36f + difficulty_ * 0.54f);
            }
            else if (region == TerrainRegion::waterlogged)
            {
                height -= (0.08f + difficulty_ * 0.08f) * region_fade;
            }
            else if (region == TerrainRegion::shallow_water)
            {
                height -= (0.18f + difficulty_ * 0.20f) * region_fade;
            }

            if (step)
            {
                const float edge = std::min(local - step_challenge_begin,
                    step_challenge_end - local);
                const float step_shape = smoothstep(edge
                    / (5.0f * fine_cell_spacing));
                height += (0.25f + difficulty_ * 0.10f) * step_shape;
            }
            else if (ramp)
            {
                const float fraction = std::clamp(
                    (local - ramp_challenge_begin)
                        / (ramp_challenge_end - ramp_challenge_begin),
                    0.0f, 1.0f);
                const float triangular = 1.0f
                    - std::abs(fraction * 2.0f - 1.0f);
                const float smooth_ramp = smoothstep(triangular);
                height += (0.48f + difficulty_ * 0.20f) * smooth_ramp;
            }

            const float water_surface = region == TerrainRegion::shallow_water
                ? std::lerp(height,
                    0.065f, region_fade)
                : region == TerrainRegion::waterlogged
                    ? height + 0.025f * region_fade : height;
            return { std::clamp(height, -1.25f, 3.50f),
                launch_pad,
                material, region, water_surface };
        }

        void initialize_column(std::size_t column) noexcept
        {
            const float course_x = static_cast<float>(column) * fine_cell_spacing;
            const SurfaceProfile surface = authored_surface(course_x);
            cells_[column].surface_material = surface.material;
            cells_[column].region = surface.region;
            cells_[column].water_surface = surface.water_surface;
            cells_[column].water_depth = std::max(0.0f,
                surface.water_surface - surface.height);
            const float scaled = std::clamp(
                (surface.height - world_bottom) / fine_cell_spacing,
                1.0f, static_cast<float>(vertical_cell_count) - 1.0f);
            const auto complete_rows = static_cast<std::size_t>(std::floor(scaled));
            const float top_fraction = scaled - static_cast<float>(complete_rows);

            for (std::size_t row = 0; row < complete_rows; ++row)
            {
                const std::uint32_t depth = static_cast<std::uint32_t>(
                    complete_rows - 1u - row);
                const std::uint32_t cell_x = static_cast<std::uint32_t>(column);
                const std::uint32_t cell_y = static_cast<std::uint32_t>(row);
                const std::uint32_t seed_salt = static_cast<std::uint32_t>(seed_)
                    ^ static_cast<std::uint32_t>(seed_ >> 32u);
                const std::int32_t top_boundary = 8
                    + sandhybrid::resident_ground_boundary_offset(
                        cell_x, seed_salt ^ 0x51f2a3b7u, 3);
                const std::int32_t middle_boundary = 44
                    + sandhybrid::resident_ground_boundary_offset(
                        cell_x, seed_salt ^ 0x6d31c47bu, 7);
                const std::int32_t deep_boundary = 108
                    + sandhybrid::resident_ground_boundary_offset(
                        cell_x, seed_salt ^ 0x9a27e115u, 11);
                const std::int32_t signed_depth = static_cast<std::int32_t>(depth);
                sandhybrid::Material base{};
                if (signed_depth < middle_boundary)
                {
                    base = sandhybrid::resident_transition_material(
                        surface.material, sandhybrid::Material::dirt,
                        signed_depth - top_boundary, 4, cell_x, cell_y,
                        seed_salt ^ 0x2b61d4c9u);
                }
                else if (signed_depth < deep_boundary)
                {
                    base = sandhybrid::resident_transition_material(
                        sandhybrid::Material::dirt, sandhybrid::Material::silt,
                        signed_depth - middle_boundary, 7, cell_x, cell_y,
                        seed_salt ^ 0x73a918efu);
                }
                else
                {
                    base = sandhybrid::resident_transition_material(
                        sandhybrid::Material::silt, sandhybrid::Material::stone,
                        signed_depth - deep_boundary, 10, cell_x, cell_y,
                        seed_salt ^ 0xc41752a3u);
                }
                if (surface.structural_ledge && depth < 18u)
                    base = sandhybrid::Material::stone;
                const sandhybrid::terrain::Sample sample =
                    sandhybrid::resident_ground_sample(
                        base, cell_x, cell_y, depth);
                if (sample.material == sandhybrid::Material::empty)
                    continue;
                set_cell(fine_cells_[fine_index(column, row)], sample.material,
                    sample.structural || surface.structural_ledge, 1.0f, true);
            }
            if (top_fraction > 1.0e-5f && complete_rows < vertical_cell_count)
            {
                const sandhybrid::Material material = surface.structural_ledge
                    ? sandhybrid::Material::stone : surface.material;
                set_cell(fine_cells_[fine_index(column, complete_rows)], material,
                    surface.structural_ledge, top_fraction, true);
            }

            // EpochSimEngine's authored connected world uses a real material
            // veneer, not a screen-space green line.  Preserve the exact
            // collision height/fill/aux state and change only exposed firm
            // dirt to the canonical Grass material.  Sand, mud, water and
            // structural challenge decks keep their own physical identity.
            if (surface.region == TerrainRegion::firm
                && !surface.structural_ledge && complete_rows > 0u)
            {
                const std::size_t exposed_row = top_fraction > 1.0e-5f
                    ? complete_rows : complete_rows - 1u;
                FineCell& exposed = fine_cells_[fine_index(column, exposed_row)];
                if (exposed.occupied()
                    && exposed.material() == sandhybrid::Material::dirt)
                {
                    exposed.state.material = static_cast<std::uint32_t>(
                        sandhybrid::Material::grass);
                    cells_[column].surface_material =
                        sandhybrid::Material::grass;
                }
            }
        }

        void paint_authored_rectangle(std::size_t left, std::size_t right,
            std::size_t bottom, std::size_t top,
            sandhybrid::Material material, bool structural) noexcept
        {
            right = std::min(right, cell_count);
            top = std::min(top, vertical_cell_count);
            for (std::size_t row = bottom; row < top; ++row)
                for (std::size_t column = left; column < right; ++column)
                    set_cell(fine_cells_[fine_index(column, row)], material,
                        structural, 1.0f, true);
        }

        void clear_authored_rectangle(std::size_t left, std::size_t right,
            std::size_t bottom, std::size_t top) noexcept
        {
            right = std::min(right, cell_count);
            top = std::min(top, vertical_cell_count);
            for (std::size_t row = bottom; row < top; ++row)
                for (std::size_t column = left; column < right; ++column)
                    clear_cell(fine_cells_[fine_index(column, row)]);
        }

        [[nodiscard]] static bool natural_substrate_material(
            sandhybrid::Material material) noexcept
        {
            return material == sandhybrid::Material::sand
                || material == sandhybrid::Material::dirt
                || material == sandhybrid::Material::silt
                || material == sandhybrid::Material::stone
                || material == sandhybrid::Material::mud;
        }

        void author_natural_geology_texture() noexcept
        {
            // EpochSimEngine's authored bitmap reads as material because its
            // strata contain coherent lenses instead of full-width flat bands
            // or a grid of rectangular patches.  Apply the same principle to
            // canonical SceneCells.  The lenses are deliberately below each
            // column's live surface and retain fill/aux/temperature, so this
            // cannot become a hidden collision or traction edit.
            constexpr std::size_t surface_clearance =
                course_geometry::geology_surface_clearance_cells;
            const std::uint32_t seed_salt = static_cast<std::uint32_t>(seed_)
                ^ static_cast<std::uint32_t>(seed_ >> 32u) ^ 0x74ac39d1u;

            std::array<std::size_t, cell_count> surface_rows{};
            for (std::size_t column = 0u; column < cell_count; ++column)
            {
                std::size_t surface_row{};
                bool found_surface{};
                for (std::size_t row = 0u; row < vertical_cell_count; ++row)
                {
                    if (fine_cells_[fine_index(column, row)].occupied())
                    {
                        surface_row = row;
                        found_surface = true;
                    }
                }
                surface_rows[column] = found_surface ? surface_row : 0u;
            }

            for (std::size_t lens = 0u;
                lens < course_geometry::geology_lens_count; ++lens)
            {
                const std::uint32_t lens_hash = sandhybrid::terrain::hash(
                    static_cast<std::uint32_t>(lens), seed_salt,
                    0x5a17c9e3u);
                const std::int32_t center_x = static_cast<std::int32_t>(
                    12u + lens_hash % (cell_count - 24u));
                const std::int32_t center_depth = static_cast<std::int32_t>(
                    24u + (lens_hash >> 9u) % 132u);
                const std::int32_t radius_x = static_cast<std::int32_t>(
                    13u + (lens_hash >> 17u) % 31u);
                const std::int32_t radius_y = static_cast<std::int32_t>(
                    4u + (lens_hash >> 25u) % 8u);
                const std::uint32_t material_choice = (lens_hash >> 6u) % 9u;
                const sandhybrid::Material replacement = center_depth < 52
                    ? (material_choice < 4u ? sandhybrid::Material::sand
                        : sandhybrid::Material::silt)
                    : center_depth < 112
                        ? (material_choice < 5u ? sandhybrid::Material::dirt
                            : sandhybrid::Material::stone)
                        : (material_choice < 6u ? sandhybrid::Material::silt
                            : sandhybrid::Material::iron_ore);

                const std::int32_t left = std::max(0, center_x - radius_x - 2);
                const std::int32_t right = std::min(
                    static_cast<std::int32_t>(cell_count) - 1,
                    center_x + radius_x + 2);
                for (std::int32_t x = left; x <= right; ++x)
                {
                    const std::size_t column = static_cast<std::size_t>(x);
                    if (surface_rows[column] <= surface_clearance)
                        continue;
                    const float normalized_x = static_cast<float>(x - center_x)
                        / static_cast<float>(radius_x);
                    const float wave = std::sin((static_cast<float>(x)
                        + static_cast<float>(lens * 19u)) * 0.083f) * 1.8f;
                    const float center_y = static_cast<float>(surface_rows[column])
                        - static_cast<float>(center_depth) + wave;
                    const std::int32_t bottom = std::max(4,
                        static_cast<std::int32_t>(std::floor(center_y))
                            - radius_y - 2);
                    const std::int32_t top = std::min(
                        static_cast<std::int32_t>(surface_rows[column]
                            - surface_clearance - 1u),
                        static_cast<std::int32_t>(std::ceil(center_y))
                            + radius_y + 2);
                    for (std::int32_t y = bottom; y <= top; ++y)
                    {
                        const float normalized_y =
                            (static_cast<float>(y) - center_y)
                                / static_cast<float>(radius_y);
                        const std::uint32_t edge_hash = sandhybrid::terrain::hash(
                            static_cast<std::uint32_t>(x),
                            static_cast<std::uint32_t>(y),
                            seed_salt ^ static_cast<std::uint32_t>(lens * 977u));
                        const float edge = 0.88f
                            + static_cast<float>(edge_hash & 255u)
                                / 255.0f * 0.20f;
                        if (normalized_x * normalized_x
                                + normalized_y * normalized_y > edge)
                            continue;
                        FineCell& cell = fine_cells_[fine_index(column,
                            static_cast<std::size_t>(y))];
                        if (!cell.occupied() || cell.fill < 0.999f
                            || !natural_substrate_material(cell.material()))
                            continue;
                        cell.state.material = static_cast<std::uint32_t>(
                            replacement);
                    }
                }
            }
        }

        void author_training_map_features() noexcept
        {
            // This is a real 640x360 canonical-cell map, built with the same
            // material IDs and aligned 8x8 construction rule as SandHybrid's
            // reset world.  These underground facilities do not alter the
            // surface course: they make the full-map view expose chambers,
            // reservoirs, structural shells, resources, and access routes
            // instead of broad decorative rectangles.
            constexpr std::size_t tile = macro_cell_side;
            const auto eased_unit = [](float value) noexcept
            {
                value = std::clamp(value, 0.0f, 1.0f);
                return value * value * (3.0f - 2.0f * value);
            };
            const auto clear_rounded_chamber = [&](std::size_t left,
                std::size_t right, std::size_t bottom, std::size_t top,
                std::size_t radius) noexcept
            {
                if (right <= left || top <= bottom)
                    return;
                const float inner_left = static_cast<float>(left + radius);
                const float inner_right = static_cast<float>(right - radius - 1u);
                const float inner_bottom = static_cast<float>(bottom + radius);
                const float inner_top = static_cast<float>(top - radius - 1u);
                const float radius_squared = static_cast<float>(radius * radius);
                for (std::size_t row = bottom;
                    row < std::min(top, vertical_cell_count); ++row)
                {
                    for (std::size_t column = left;
                        column < std::min(right, cell_count); ++column)
                    {
                        const float x = static_cast<float>(column);
                        const float y = static_cast<float>(row);
                        const float dx = x < inner_left ? inner_left - x
                            : x > inner_right ? x - inner_right : 0.0f;
                        const float dy = y < inner_bottom ? inner_bottom - y
                            : y > inner_top ? y - inner_top : 0.0f;
                        if (dx * dx + dy * dy <= radius_squared)
                            clear_cell(fine_cells_[fine_index(column, row)]);
                    }
                }
            };
            const auto carve_gallery = [&](std::size_t left,
                std::size_t right, float start_row, float end_row,
                float half_height, float bow, std::uint32_t salt) noexcept
            {
                if (right <= left)
                    return;
                const float span = static_cast<float>(right - left - 1u);
                for (std::size_t column = left;
                    column < std::min(right, cell_count); ++column)
                {
                    const float t = span > 0.0f
                        ? static_cast<float>(column - left) / span : 0.0f;
                    const std::uint32_t noise_hash = sandhybrid::terrain::hash(
                        static_cast<std::uint32_t>(column / 4u), salt,
                        static_cast<std::uint32_t>(seed_));
                    const float noise = static_cast<float>(noise_hash & 255u)
                        / 255.0f - 0.5f;
                    const float center = std::lerp(start_row, end_row, t)
                        + std::sin(t * 3.14159265358979323846f) * bow
                        + noise * 1.3f;
                    const int bottom = std::max(1,
                        static_cast<int>(std::floor(center - half_height)));
                    const int top = std::min(
                        static_cast<int>(vertical_cell_count) - 1,
                        static_cast<int>(std::ceil(center + half_height)));
                    for (int row = bottom; row <= top; ++row)
                        clear_cell(fine_cells_[fine_index(column,
                            static_cast<std::size_t>(row))]);
                }
            };

            // West operations gallery: two rig-scale rooms use the natural
            // geology as their shell.  Discontinuous glass bays, machinery,
            // and an actual stair make this read as a constructed place rather
            // than one viewport-sized framed rectangle.
            clear_rounded_chamber(32u, 96u, 96u, 138u, 9u);
            clear_rounded_chamber(88u, 156u, 104u, 144u, 9u);
            carve_gallery(80u, 108u, 115.0f, 124.0f, 8.0f, 0.0f,
                0x31f47u);
            paint_authored_rectangle(40u, 80u, 92u, 96u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(96u, 148u, 100u, 104u,
                sandhybrid::Material::conveyor, true);
            paint_authored_rectangle(40u, 56u, 132u, 136u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(64u, 80u, 132u, 136u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(104u, 120u, 138u, 142u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(128u, 144u, 138u, 142u,
                sandhybrid::Material::glass, true);
            for (std::size_t step = 0u; step < 3u; ++step)
                paint_authored_rectangle(80u + step * tile,
                    88u + step * tile, 94u + step * 2u,
                    98u + step * 2u, sandhybrid::Material::stone, true);
            paint_authored_rectangle(40u, 48u, 96u, 128u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(144u, 152u, 104u, 112u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(144u, 152u, 120u, 140u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(104u, 112u, 104u, 112u,
                sandhybrid::Material::smelter, true);
            paint_authored_rectangle(112u, 120u, 104u, 112u,
                sandhybrid::Material::assembler, true);
            paint_authored_rectangle(120u, 128u, 104u, 112u,
                sandhybrid::Material::copper, true);
            paint_authored_rectangle(128u, 136u, 104u, 120u,
                sandhybrid::Material::power_cell, true);
            paint_authored_rectangle(136u, 144u, 104u, 120u,
                sandhybrid::Material::factory_core, true);

            // A sloped service approach ties the lab to a compact waterworks.
            // The basin has a curved canonical-cell bed and a level waterline;
            // it is not the old screen-wide rectangular tank.  Air gallery,
            // pump tower and paired catwalks remain visibly traversable.
            carve_gallery(148u, 176u, 124.0f, 128.0f, 8.0f, -2.0f,
                0x8ac21u);
            clear_rounded_chamber(164u, 272u, 100u, 142u, 10u);
            constexpr int basin_center = 218;
            constexpr int basin_half_width = 34;
            constexpr int basin_waterline = 107;
            for (int x = basin_center - basin_half_width;
                x <= basin_center + basin_half_width; ++x)
            {
                const float t = static_cast<float>(x - basin_center)
                    / static_cast<float>(basin_half_width);
                const std::uint32_t bed_hash = sandhybrid::terrain::hash(
                    static_cast<std::uint32_t>(x / 3), 0x71a4u,
                    static_cast<std::uint32_t>(seed_));
                const int bed = 72 + static_cast<int>(std::lround(
                    t * t * 13.0f
                    + (static_cast<float>(bed_hash & 7u) - 3.0f) * 0.28f));
                for (int row = bed; row < basin_waterline; ++row)
                {
                    FineCell& cell = fine_cells_[fine_index(
                        static_cast<std::size_t>(x),
                        static_cast<std::size_t>(row))];
                    set_cell(cell, row < bed + 2
                        ? sandhybrid::Material::stone
                        : sandhybrid::Material::water,
                        row < bed + 2, 1.0f, true);
                }
            }
            paint_authored_rectangle(176u, 184u, 94u, 128u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(252u, 260u, 94u, 128u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(184u, 208u, 112u, 116u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(228u, 252u, 112u, 116u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(208u, 216u, 108u, 136u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(216u, 224u, 108u, 124u,
                sandhybrid::Material::power_cell, true);
            paint_authored_rectangle(224u, 232u, 108u, 136u,
                sandhybrid::Material::steel, true);

            // Couple the visible surface lake to the waterworks through a
            // shallow, capped aquifer and one glass-lined well.  Keeping two
            // solid canonical cells at the surface preserves the exact
            // ground-height/traction contract while the cutaway now explains
            // where the lake and waterworks meet.
            const CourseBoundaries boundaries = course_boundaries();
            const std::size_t lake_left = std::clamp(
                static_cast<std::size_t>(std::floor(
                    boundaries.fourth / fine_cell_spacing)), std::size_t{ 2u },
                cell_count - std::size_t{ 3u });
            const std::size_t lake_right = std::clamp(
                static_cast<std::size_t>(std::ceil(
                    boundaries.fifth / fine_cell_spacing)), lake_left + 1u,
                cell_count - std::size_t{ 2u });
            const std::size_t lake_center = (lake_left + lake_right) / 2u;
            for (std::size_t column = lake_left;
                column <= lake_right; ++column)
            {
                std::size_t surface_row{};
                for (std::size_t row = 0u; row < vertical_cell_count; ++row)
                    if (fine_cells_[fine_index(column, row)].occupied())
                        surface_row = row;
                if (surface_row < 12u)
                    continue;
                const float edge_distance = static_cast<float>(std::min(
                    column - lake_left, lake_right - column));
                const float bank = eased_unit(edge_distance
                    / static_cast<float>(
                        course_geometry::shore_transition_cell_count));
                const std::size_t depth = 2u + static_cast<std::size_t>(
                    std::lround(bank * 7.0f));
                const std::size_t water_top = surface_row - 2u;
                const std::size_t water_bottom = water_top > depth
                    ? water_top - depth : 1u;
                for (std::size_t row = water_bottom; row < water_top; ++row)
                    set_cell(fine_cells_[fine_index(column, row)],
                        sandhybrid::Material::water, false, 1.0f, true);
            }
            const std::size_t well_left = lake_center >= 4u
                ? lake_center - 4u : 0u;
            const std::size_t well_right = std::min(
                cell_count, lake_center + 4u);
            for (std::size_t column = well_left;
                column < well_right; ++column)
            {
                std::size_t surface_row{};
                for (std::size_t row = 0u; row < vertical_cell_count; ++row)
                    if (fine_cells_[fine_index(column, row)].occupied())
                        surface_row = row;
                // Leave a four-cell service crossover between the pump tower
                // and the vertical well.  In a side-view world a solid shaft
                // would otherwise bisect the entire underground route.
                for (std::size_t row = 140u;
                    row + 2u < surface_row; ++row)
                    set_cell(fine_cells_[fine_index(column, row)],
                        sandhybrid::Material::water, false, 1.0f, true);
            }
            if (well_left >= 2u && well_right + 2u < cell_count)
            {
                paint_authored_rectangle(well_left - 2u, well_left,
                    140u, 158u, sandhybrid::Material::glass, true);
                paint_authored_rectangle(well_right, well_right + 2u,
                    140u, 158u, sandhybrid::Material::glass, true);
            }

            // The waterworks feeds a seed-stable natural gallery.  Two modest
            // rounded chambers sit on a bowed access path, so the cavern is a
            // connected destination instead of a sealed dark lens.
            carve_gallery(260u, 302u, 123.0f, 108.0f, 7.0f, -4.0f,
                0x6d31u);
            clear_rounded_chamber(290u, 356u, 88u, 122u, 10u);
            clear_rounded_chamber(374u, 442u, 100u, 132u, 10u);
            carve_gallery(338u, 390u, 110.0f, 116.0f, 6.0f, 18.0f,
                0x11c493u);
            const auto paint_vein = [&](std::int32_t center_x,
                std::int32_t center_y, std::int32_t radius_x,
                std::int32_t radius_y, sandhybrid::Material material,
                std::uint32_t salt) noexcept
            {
                for (std::int32_t y = center_y - radius_y;
                    y <= center_y + radius_y; ++y)
                {
                    for (std::int32_t x = center_x - radius_x;
                        x <= center_x + radius_x; ++x)
                    {
                        if (x < 0 || y < 0
                            || x >= static_cast<std::int32_t>(cell_count)
                            || y >= static_cast<std::int32_t>(vertical_cell_count))
                            continue;
                        const float nx = static_cast<float>(x - center_x)
                            / static_cast<float>(radius_x);
                        const float ny = static_cast<float>(y - center_y)
                            / static_cast<float>(radius_y);
                        const std::uint32_t noise = sandhybrid::terrain::hash(
                            static_cast<std::uint32_t>(x),
                            static_cast<std::uint32_t>(y),
                            static_cast<std::uint32_t>(seed_) ^ salt);
                        const float edge = 0.82f
                            + static_cast<float>(noise & 255u) / 255.0f * 0.22f;
                        FineCell& cell = fine_cells_[fine_index(
                            static_cast<std::size_t>(x),
                            static_cast<std::size_t>(y))];
                        if (nx * nx + ny * ny <= edge && cell.occupied())
                            set_cell(cell, material, false, 1.0f, true);
                    }
                }
            };
            paint_vein(296, 78, 11, 6,
                sandhybrid::Material::iron_ore, 0x41f331u);
            paint_vein(348, 72, 9, 5,
                sandhybrid::Material::copper, 0x6b28a7u);
            paint_vein(438, 82, 10, 6,
                sandhybrid::Material::crystal, 0x8e4cd9u);

            // A rising gallery opens into the compact east construction bay.
            // Its broken glass clerestory and natural roof remove the last
            // giant boxed silhouette while retaining aligned production cells.
            carve_gallery(438u, 518u, 116.0f, 124.0f, 7.0f, 3.5f,
                0xb512u);
            paint_authored_rectangle(432u, 440u, 100u, 124u,
                sandhybrid::Material::stone, true);
            paint_authored_rectangle(432u, 440u, 132u, 140u,
                sandhybrid::Material::stone, true);
            paint_authored_rectangle(464u, 472u, 100u, 112u,
                sandhybrid::Material::stone, true);
            paint_authored_rectangle(464u, 472u, 120u, 140u,
                sandhybrid::Material::stone, true);
            clear_rounded_chamber(504u, 628u, 100u, 144u, 10u);
            paint_authored_rectangle(512u, 620u, 96u, 100u,
                sandhybrid::Material::stone, true);
            paint_authored_rectangle(512u, 520u, 100u, 136u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(612u, 620u, 100u, 136u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(544u, 552u, 100u, 132u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(544u, 552u, 140u, 144u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(560u, 584u, 140u, 144u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(592u, 612u, 140u, 144u,
                sandhybrid::Material::glass, true);
            paint_authored_rectangle(584u, 592u, 100u, 120u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(584u, 592u, 128u, 140u,
                sandhybrid::Material::steel, true);
            paint_authored_rectangle(560u, 568u, 100u, 108u,
                sandhybrid::Material::smelter, true);
            paint_authored_rectangle(568u, 576u, 100u, 108u,
                sandhybrid::Material::assembler, true);
            paint_authored_rectangle(592u, 600u, 100u, 108u,
                sandhybrid::Material::gold, true);
            paint_authored_rectangle(604u, 612u, 100u, 108u,
                sandhybrid::Material::aluminum, true);
            paint_authored_rectangle(600u, 608u, 108u, 116u,
                sandhybrid::Material::power_cell, true);
        }

        [[nodiscard]] static std::size_t fine_index(std::size_t column,
            std::size_t row) noexcept
        {
            return row * cell_count + column;
        }

        [[nodiscard]] static std::size_t macro_index(std::size_t column,
            std::size_t row) noexcept
        {
            return row * macro_columns + column;
        }

        static void clear_cell(FineCell& cell) noexcept
        {
            cell.state = { static_cast<std::uint32_t>(
                sandhybrid::Material::atmosphere), 0u, 20, 0u };
            cell.fill = 0.0f;
            cell.metadata = 0u;
        }

        static void set_cell(FineCell& cell, sandhybrid::Material material,
            bool structural, float fill, bool authored = false) noexcept
        {
            cell.state = { static_cast<std::uint32_t>(material), 0u, 20,
                structural ? FineCell::structural_aux : 0u };
            cell.metadata = authored ? FineCell::authored_metadata : 0u;
            cell.fill = std::clamp(fill, 0.0f, 1.0f);
            if (cell.fill <= 1.0e-6f)
                clear_cell(cell);
        }

        [[nodiscard]] FineCell* top_cell(std::size_t column) noexcept
        {
            const int row = surface_rows_[column];
            return row < 0 ? nullptr
                : &fine_cells_[fine_index(column, static_cast<std::size_t>(row))];
        }

        [[nodiscard]] const FineCell* top_cell(std::size_t column) const noexcept
        {
            const int row = surface_rows_[column];
            return row < 0 ? nullptr
                : &fine_cells_[fine_index(column, static_cast<std::size_t>(row))];
        }

        void refresh_column(std::size_t column) noexcept
        {
            int top = static_cast<int>(vertical_cell_count) - 1;
            while (top >= 0 && !fine_cells_[fine_index(column,
                static_cast<std::size_t>(top))].occupied())
                --top;
            surface_rows_[column] = top;
            cells_[column].height = top < 0 ? world_bottom
                : world_bottom + (static_cast<float>(top)
                    + std::clamp(fine_cells_[fine_index(column,
                        static_cast<std::size_t>(top))].fill, 0.0f, 1.0f))
                    * fine_cell_spacing;
            if (!live_surface_updates_)
                return;
            Cell& surface = cells_[column];
            const FineCell* top_surface = top_cell(column);
            surface.surface_material = top_surface == nullptr
                ? sandhybrid::Material::atmosphere : top_surface->material();
            if (launch_pad_at(static_cast<float>(column) * fine_cell_spacing))
                surface.region = TerrainRegion::firm;
            else if (surface.water_depth > 0.08f)
                surface.region = TerrainRegion::shallow_water;
            else if (surface.water_depth > 0.001f
                || surface.surface_material == sandhybrid::Material::mud)
                surface.region = TerrainRegion::waterlogged;
            else if (surface.height < surface.rest_height - 0.16f)
                surface.region = TerrainRegion::hole;
            else if (surface.surface_material == sandhybrid::Material::sand)
                surface.region = TerrainRegion::dry_sand;
            else
                surface.region = TerrainRegion::firm;
        }

        void refresh_all_macro_tiles() noexcept
        {
            for (std::size_t row = 0; row < macro_rows; ++row)
                for (std::size_t column = 0; column < macro_columns; ++column)
                    refresh_macro_tile(column, row);
        }

        void refresh_macro_tile(std::size_t macro_column,
            std::size_t macro_row) noexcept
        {
            MacroTile& tile = macro_tiles_[macro_index(macro_column, macro_row)];
            const bool was_ready = tile.macro_ready;
            std::uint64_t occupied{};
            std::uint64_t structural{};
            sandhybrid::Material uniform = sandhybrid::Material::empty;
            bool same_material = true;
            bool all_full = true;
            for (std::size_t local_y = 0; local_y < macro_cell_side; ++local_y)
            {
                for (std::size_t local_x = 0; local_x < macro_cell_side; ++local_x)
                {
                    const std::size_t bit = local_y * macro_cell_side + local_x;
                    const std::size_t column = macro_column * macro_cell_side + local_x;
                    const std::size_t row = macro_row * macro_cell_side + local_y;
                    const FineCell& cell = fine_cells_[fine_index(column, row)];
                    if (!cell.occupied())
                    {
                        all_full = false;
                        same_material = false;
                        continue;
                    }
                    occupied |= std::uint64_t{ 1 } << bit;
                    if (cell.structural())
                        structural |= std::uint64_t{ 1 } << bit;
                    if (uniform == sandhybrid::Material::empty)
                        uniform = cell.material();
                    else if (uniform != cell.material())
                        same_material = false;
                    if (cell.fill < 0.999f)
                        all_full = false;
                }
            }
            tile.occupied_mask = occupied;
            tile.structural_mask = structural;
            tile.uniform_material = same_material ? uniform : sandhybrid::Material::empty;
            tile.macro_ready = all_full && same_material
                && occupied == std::numeric_limits<std::uint64_t>::max();
            tile.active = !tile.macro_ready && occupied != 0u;
            if (!was_ready && tile.macro_ready)
                ++macro_promotions_;
            else if (was_ready && !tile.macro_ready)
                ++macro_demotions_;
        }

        void changed_cell(std::size_t column, std::size_t row) noexcept
        {
            refresh_column(column);
            refresh_macro_tile(column / macro_cell_side, row / macro_cell_side);
            section_grid_.mark_dirty_cell({
                static_cast<std::int32_t>(column),
                static_cast<std::int32_t>(row) });
        }

        [[nodiscard]] float remove_excavatable_volume(std::size_t column,
            float requested) noexcept
        {
            float remaining = std::max(0.0f, requested);
            float removed = 0.0f;
            while (remaining > 1.0e-7f)
            {
                const int top = surface_rows_[column];
                if (top < 0)
                    break;
                FineCell& cell = fine_cells_[fine_index(column,
                    static_cast<std::size_t>(top))];
                if (cell.material() == sandhybrid::Material::stone || cell.authored())
                    break;
                const float available = cell.fill * fine_cell_spacing;
                const float take = std::min(remaining, available);
                cell.fill -= take / fine_cell_spacing;
                remaining -= take;
                removed += take;
                const std::size_t changed_row = static_cast<std::size_t>(top);
                if (cell.fill <= 1.0e-6f)
                    clear_cell(cell);
                changed_cell(column, changed_row);
            }
            return removed;
        }
        [[nodiscard]] float remove_loose_volume(std::size_t column,
            float requested) noexcept
        {
            float remaining = std::max(0.0f, requested);
            float removed = 0.0f;
            while (remaining > 1.0e-7f)
            {
                const int top = surface_rows_[column];
                if (top < 0)
                    break;
                FineCell& cell = fine_cells_[fine_index(column,
                    static_cast<std::size_t>(top))];
                if (cell.structural() || cell.authored())
                    break;
                const float available = cell.fill * fine_cell_spacing;
                const float take = std::min(remaining, available);
                cell.fill -= take / fine_cell_spacing;
                remaining -= take;
                removed += take;
                const std::size_t changed_row = static_cast<std::size_t>(top);
                if (cell.fill <= 1.0e-6f)
                    clear_cell(cell);
                changed_cell(column, changed_row);
            }
            return removed;
        }

        [[nodiscard]] float add_volume(std::size_t column, float requested,
            sandhybrid::Material material, bool structural,
            const sandhybrid::SceneCell* prototype = nullptr) noexcept
        {
            float remaining = std::max(0.0f, requested);
            float added = 0.0f;
            while (remaining > 1.0e-7f)
            {
                int top = surface_rows_[column];
                std::size_t row = top < 0 ? 0u : static_cast<std::size_t>(top);
                FineCell* target = top < 0 ? &fine_cells_[fine_index(column, row)]
                    : &fine_cells_[fine_index(column, row)];
                if (target->occupied()
                    && (target->authored() || target->material() != material
                        || target->structural() != structural
                        || target->fill >= 0.999999f))
                {
                    ++row;
                    if (row >= vertical_cell_count)
                        break;
                    target = &fine_cells_[fine_index(column, row)];
                }
                if (!target->occupied())
                {
                    target->state = prototype == nullptr
                        ? sandhybrid::SceneCell{
                            static_cast<std::uint32_t>(material), 0u, 20, 0u }
                        : *prototype;
                    target->state.material = static_cast<std::uint32_t>(material);
                    target->state.aux &= ~(FineCell::structural_aux
                        | FineCell::moved_aux);
                    if (structural)
                        target->state.aux |= FineCell::structural_aux;
                    target->fill = 0.0f;
                    target->metadata = 0u;
                }
                const float capacity = (1.0f - target->fill) * fine_cell_spacing;
                const float put = std::min(remaining, capacity);
                target->fill += put / fine_cell_spacing;
                remaining -= put;
                added += put;
                changed_cell(column, row);
            }
            return added;
        }

        [[nodiscard]] float move_loose_volume(std::size_t from,
            std::size_t to, float requested) noexcept
        {
            const FineCell* source = top_cell(from);
            if (source == nullptr || source->structural() || source->authored())
                return 0.0f;
            const sandhybrid::Material material = source->material();
            const sandhybrid::SceneCell source_state = source->canonical_cell();
            const float removed = remove_loose_volume(from, requested);
            if (removed <= 0.0f)
                return 0.0f;
            const float added = add_volume(to, removed, material, false,
                &source_state);
            const float remainder = removed - added;
            if (remainder > 0.0f)
                static_cast<void>(add_volume(from, remainder, material, false,
                    &source_state));
            return added;
        }

        [[nodiscard]] static float wrapped_course_x(float course_x) noexcept
        {
            float wrapped = std::fmod(course_x, period);
            if (wrapped < 0.0f)
                wrapped += period;
            return wrapped;
        }

        [[nodiscard]] static Sample sample_coordinates(float course_x) noexcept
        {
            const float scaled = wrapped_course_x(course_x) / fine_cell_spacing;
            const auto first_signed = static_cast<std::ptrdiff_t>(std::floor(scaled));
            const std::size_t first = wrap_column(first_signed);
            return { first, (first + 1u) % cell_count,
                scaled - static_cast<float>(first_signed) };
        }

        [[nodiscard]] static std::size_t nearest_index(float course_x) noexcept
        {
            const float scaled = wrapped_course_x(course_x) / fine_cell_spacing;
            return wrap_column(static_cast<std::ptrdiff_t>(std::floor(scaled + 0.5f)));
        }

        // Keep the canonical live map off Environment's stack frame. The old
        // fixed arrays made each Environment several hundred KiB and overflowed
        // the default 1 MiB Windows thread stack when tests held multiple rigs.
        // std::vector preserves deep-copy value semantics without sharing terrain.
        // Never use list initialization here; for example, the audit string
        // std::vector<FineCell> fine_cells_{ fine_cell_count };
        // creates a one-element vector through aggregate initialization.
        std::vector<Cell> cells_ = std::vector<Cell>(cell_count);
        std::vector<FineCell> fine_cells_ = std::vector<FineCell>(fine_cell_count);
        std::vector<MacroTile> macro_tiles_ = std::vector<MacroTile>(macro_tile_count);
        std::vector<int> surface_rows_ = std::vector<int>(cell_count, -1);
        sandhybrid::SparseSectionGrid section_grid_{};
        std::uint64_t seed_{ 1u };
        std::uint64_t tick_{ 1u };
        float difficulty_{ 0.25f };
        std::size_t macro_promotions_{};
        std::size_t macro_demotions_{};
        bool live_surface_updates_{};
    };

    static_assert(sizeof(DeformableTerrain) < 128u * 1024u,
        "DeformableTerrain must remain safe to embed in Windows stack-resident rigs");
}
