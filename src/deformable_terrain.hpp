#pragma once

#include <sandhybrid/library.hpp>
#include <sandhybrid/material.hpp>
#include <sandhybrid/material_color.hpp>
#include <sandhybrid/section_grid.hpp>
#include <sandhybrid/terrain_generation.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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
        case TerrainRegion::dry_sand: return "DRY DEFORMABLE SAND";
        case TerrainRegion::waterlogged: return "WATERLOGGED SAND";
        case TerrainRegion::shallow_water: return "SHALLOW WATER";
        case TerrainRegion::hole: return "GROUND HOLE";
        }
        return "UNKNOWN TERRAIN";
    }

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
            std::uint8_t material_id{};
            std::uint8_t flags{};
            float fill{};

            [[nodiscard]] sandhybrid::Material material() const noexcept
            {
                return static_cast<sandhybrid::Material>(material_id);
            }

            [[nodiscard]] bool occupied() const noexcept
            {
                return material() != sandhybrid::Material::empty && fill > 1.0e-6f;
            }

            [[nodiscard]] bool structural() const noexcept
            {
                return (flags & structural_flag) != 0u;
            }

            static constexpr std::uint8_t structural_flag = 0x01u;
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
        static constexpr float fine_cell_spacing = 0.140625f;
        static constexpr float cell_spacing = fine_cell_spacing;
        static constexpr float macro_tile_size = fine_cell_spacing
            * static_cast<float>(macro_cell_side);
        static constexpr std::size_t cell_count = 448u;
        static constexpr std::size_t vertical_cell_count = 96u;
        static constexpr std::size_t fine_cell_count = cell_count * vertical_cell_count;
        static constexpr std::size_t macro_columns = cell_count / macro_cell_side;
        static constexpr std::size_t macro_rows = vertical_cell_count / macro_cell_side;
        static constexpr std::size_t macro_tile_count = macro_columns * macro_rows;
        static constexpr float period = static_cast<float>(cell_count) * fine_cell_spacing;
        static constexpr float launch_pad_half_width = 0.70f;
        static constexpr float launch_transition_width = 0.55f;
        static constexpr float world_bottom = -4.5f;
        static constexpr float world_top = world_bottom
            + static_cast<float>(vertical_cell_count) * fine_cell_spacing;

        static_assert(macro_cell_side == 8u);
        static_assert(cell_count % macro_cell_side == 0u);
        static_assert(vertical_cell_count % macro_cell_side == 0u);
        static_assert(macro_tile_size == 1.125f);
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
            const Sample sample = sample_coordinates(course_x);
            return std::lerp(cells_[sample.first].height,
                cells_[sample.second].height, sample.fraction);
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
            const Sample sample = sample_coordinates(course_x);
            return std::clamp(std::lerp(cells_[sample.first].firmness,
                cells_[sample.second].firmness, sample.fraction), 0.0f, 1.0f);
        }

        [[nodiscard]] float looseness_at(float course_x) const noexcept
        {
            const Sample sample = sample_coordinates(course_x);
            return std::clamp(std::lerp(cells_[sample.first].loose_fraction,
                cells_[sample.second].loose_fraction, sample.fraction), 0.0f, 1.0f);
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
            const Sample sample = sample_coordinates(course_x);
            return std::max(0.0f, std::lerp(cells_[sample.first].water_depth,
                cells_[sample.second].water_depth, sample.fraction));
        }

        [[nodiscard]] float water_surface_at(float course_x) const noexcept
        {
            const Sample sample = sample_coordinates(course_x);
            return std::lerp(cells_[sample.first].water_surface,
                cells_[sample.second].water_surface, sample.fraction);
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
            const std::size_t center = nearest_index(course_x);
            if (launch_pad_at(static_cast<float>(center) * fine_cell_spacing))
                return;
            FineCell* surface = top_cell(center);
            if (surface == nullptr || surface->structural())
                return;
            const sandhybrid::Material displaced_material = surface->material();
            if (displaced_material != sandhybrid::Material::sand
                && displaced_material != sandhybrid::Material::mud)
                return;
            Cell& column = cells_[center];
            const float load = std::clamp(normalized_load, 0.0f, 4.0f);
            const float slip = std::clamp(std::abs(slip_speed), 0.0f, 5.0f);
            const float softness = std::clamp(1.0f - column.firmness, 0.0f, 1.0f);
            const float deformation_scale = 0.06f
                + difficulty_ * difficulty_ * 0.94f;
            const float requested = std::min(0.018f,
                (load * 0.065f + slip * 0.015f) * softness
                    * std::clamp(dt, 0.0f, 0.05f) * deformation_scale);
            if (requested <= 0.0f)
                return;

            const float removed = remove_loose_volume(center, requested);
            if (removed <= 0.0f)
                return;
            // A loaded footprint settles. Displaced grains form a broad berm
            // outside the immediate contact patch instead of a one-cell spike
            // directly under the next footfall.
            constexpr std::array<std::ptrdiff_t, 28> offsets{
                -5, 5, -6, 6, -7, 7, -8, 8, -9, 9, -10, 10,
                -11, 11, -12, 12, -13, 13, -14, 14, -15, 15,
                -16, 16, -17, 17, -18, 18 };
            constexpr std::array<float, 28> weights{
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f,
                1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f, 1.0f / 28.0f };
            float deposited{};
            for (std::size_t index = 0; index < offsets.size(); ++index)
            {
                const std::size_t target = wrap_column(
                    static_cast<std::ptrdiff_t>(center) + offsets[index]);
                if (launch_pad_at(static_cast<float>(target) * fine_cell_spacing))
                    continue;
                const float added = add_volume(target, removed * weights[index],
                    displaced_material, false);
                deposited += added;
                cells_[target].loose_fraction = std::clamp(
                    cells_[target].loose_fraction + added * 2.0f, 0.0f, 1.0f);
            }
            const float returned = removed - deposited;
            if (returned > 0.0f)
                static_cast<void>(add_volume(center, returned,
                    displaced_material, false));

            column.firmness = std::clamp(column.firmness
                + load * dt * 0.12f, 0.0f, 1.0f);
            column.loose_fraction = std::clamp(column.loose_fraction
                - load * dt * 0.08f, 0.0f, 1.0f);
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
                if (surface == nullptr || surface->structural())
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
                if (high_top == nullptr || high_top->structural()
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

            for (std::size_t index = 0; index < cells_.size(); ++index)
            {
                Cell& column = cells_[index];
                if (launch_pad_at(static_cast<float>(index) * fine_cell_spacing))
                {
                    column.firmness = 1.0f;
                    column.loose_fraction = 0.0f;
                    continue;
                }
                column.firmness = std::clamp(column.firmness
                    + bounded_dt * (0.006f - column.loose_fraction * 0.004f),
                    0.0f, 1.0f);
            }
        }

        [[nodiscard]] float total_height_volume() const noexcept
        {
            double result = 0.0;
            for (const FineCell& cell : fine_cells_)
            {
                if (cell.occupied())
                    result += static_cast<double>(cell.fill)
                        * static_cast<double>(fine_cell_spacing);
            }
            return static_cast<float>(result);
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
            set_cell(cell, material, structural, 1.0f);
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

        [[nodiscard]] SurfaceProfile authored_surface(float course_x) const noexcept
        {
            float local = std::fmod(course_x, period);
            if (local < 0.0f)
                local += period;

            const float first_boundary = launch_pad_half_width
                + launch_transition_width + 0.15f
                + unit_hash(seed_ ^ 0x10a2u) * 0.65f;
            const float second_boundary = first_boundary + 7.0f
                + unit_hash(seed_ ^ 0x20b3u) * 2.5f;
            const float third_boundary = second_boundary + 6.0f
                + unit_hash(seed_ ^ 0x30c4u) * 2.5f;
            const float fourth_boundary = third_boundary + 6.0f
                + unit_hash(seed_ ^ 0x40d5u) * 2.5f;
            const float fifth_boundary = fourth_boundary + 7.0f
                + unit_hash(seed_ ^ 0x50e6u) * 2.5f;
            const float sixth_boundary = fifth_boundary + 5.0f
                + unit_hash(seed_ ^ 0x60f7u) * 2.0f;
            const float tail_end = period - 5.0f;
            const float tail_split = std::lerp(sixth_boundary, tail_end,
                0.42f + unit_hash(seed_ ^ 0x70a8u) * 0.16f);
            const bool return_pad = local > tail_end;
            const float distance_from_launch = std::min(local, period - local);
            const bool launch_pad = distance_from_launch <= launch_pad_half_width;

            TerrainRegion region = TerrainRegion::firm;
            sandhybrid::Material material = sandhybrid::Material::dirt;
            float region_begin = 0.0f;
            float region_end = first_boundary;
            std::array<TerrainRegion, 3> middle_regions{
                TerrainRegion::firm, TerrainRegion::waterlogged,
                TerrainRegion::shallow_water };
            if ((mix(seed_ ^ 0x7315a9d3u) & 1u) != 0u)
                std::swap(middle_regions[1], middle_regions[2]);
            if (!return_pad && local >= first_boundary && local < second_boundary)
            {
                region = TerrainRegion::dry_sand;
                material = sandhybrid::Material::sand;
                region_begin = first_boundary;
                region_end = second_boundary;
            }
            else if (!return_pad && local >= second_boundary && local < third_boundary)
            {
                region = middle_regions[0];
                region_begin = second_boundary;
                region_end = third_boundary;
            }
            else if (!return_pad && local >= third_boundary && local < fourth_boundary)
            {
                region = middle_regions[1];
                region_begin = third_boundary;
                region_end = fourth_boundary;
            }
            else if (!return_pad && local >= fourth_boundary && local < fifth_boundary)
            {
                region = middle_regions[2];
                region_begin = fourth_boundary;
                region_end = fifth_boundary;
            }
            else if (!return_pad && local >= fifth_boundary && local < sixth_boundary)
            {
                region = TerrainRegion::hole;
                region_begin = fifth_boundary;
                region_end = sixth_boundary;
            }
            else if (!return_pad && local >= sixth_boundary)
            {
                const bool sand_first = (mix(seed_ ^ 0x80b9u) & 1u) == 0u;
                const bool first_tail = local < tail_split;
                region = (first_tail == sand_first)
                    ? TerrainRegion::dry_sand : TerrainRegion::firm;
                material = region == TerrainRegion::dry_sand
                    ? sandhybrid::Material::sand : sandhybrid::Material::dirt;
                region_begin = first_tail ? sixth_boundary : tail_split;
                region_end = first_tail ? tail_split : tail_end;
            }
            if (region == TerrainRegion::waterlogged
                || region == TerrainRegion::shallow_water)
                material = sandhybrid::Material::mud;

            float height = 0.0f;
            const float edge_distance = std::max(0.0f,
                std::min(local - region_begin, region_end - local));
            const float edge_fraction = std::clamp(
                edge_distance / 0.80f, 0.0f, 1.0f);
            const float region_fade = edge_fraction * edge_fraction
                * (3.0f - 2.0f * edge_fraction);
            const float transition = std::clamp((distance_from_launch
                - launch_pad_half_width) / launch_transition_width, 0.0f, 1.0f);
            const float smooth_transition = transition * transition
                * (3.0f - 2.0f * transition);
            const float roughness = (region == TerrainRegion::firm ? 0.018f
                : region == TerrainRegion::hole ? 0.025f
                : 0.028f + difficulty_ * 0.040f) * smooth_transition
                * region_fade;
            height += std::sin(course_x * 0.61f) * roughness;
            height += std::sin(course_x * 1.73f + 0.7f) * roughness * 0.44f;
            height += (unit_hash(seed_ ^ static_cast<std::uint64_t>(
                std::floor(local / fine_cell_spacing))) - 0.5f)
                * fine_cell_spacing * (region == TerrainRegion::firm ? 0.12f
                    : region == TerrainRegion::hole ? 0.16f
                    : 0.18f + difficulty_ * 0.20f)
                * smooth_transition * region_fade;

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

            const float water_surface = region == TerrainRegion::shallow_water
                ? std::lerp(height,
                    0.06f + std::sin(course_x * 0.19f) * 0.012f, region_fade)
                : region == TerrainRegion::waterlogged
                    ? height + 0.025f * region_fade : height;
            return { std::clamp(height, -1.25f, 3.50f), launch_pad,
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
                sandhybrid::Material base = depth < 8u
                    ? surface.material
                    : depth < 18u ? sandhybrid::Material::dirt
                    : sandhybrid::Material::stone;
                if (surface.structural_ledge && depth < 18u)
                    base = sandhybrid::Material::stone;
                const sandhybrid::terrain::Sample sample =
                    sandhybrid::terrain::sample(base,
                        static_cast<std::uint32_t>(column),
                        static_cast<std::uint32_t>(row), depth);
                if (sample.material == sandhybrid::Material::empty)
                    continue;
                set_cell(fine_cells_[fine_index(column, row)], sample.material,
                    sample.structural || surface.structural_ledge, 1.0f);
            }
            if (top_fraction > 1.0e-5f && complete_rows < vertical_cell_count)
            {
                const sandhybrid::Material material = surface.structural_ledge
                    ? sandhybrid::Material::stone : surface.material;
                set_cell(fine_cells_[fine_index(column, complete_rows)], material,
                    surface.structural_ledge, top_fraction);
            }
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
            cell.material_id = 0u;
            cell.flags = 0u;
            cell.fill = 0.0f;
        }

        static void set_cell(FineCell& cell, sandhybrid::Material material,
            bool structural, float fill) noexcept
        {
            cell.material_id = static_cast<std::uint8_t>(material);
            cell.flags = structural ? FineCell::structural_flag : 0u;
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
                ? sandhybrid::Material::empty : top_surface->material();
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
                if (cell.material() == sandhybrid::Material::stone)
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
                if (cell.structural())
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
            sandhybrid::Material material, bool structural) noexcept
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
                    && (target->material() != material || target->structural() != structural
                        || target->fill >= 0.999999f))
                {
                    ++row;
                    if (row >= vertical_cell_count)
                        break;
                    target = &fine_cells_[fine_index(column, row)];
                }
                if (!target->occupied())
                {
                    target->material_id = static_cast<std::uint8_t>(material);
                    target->flags = structural ? FineCell::structural_flag : 0u;
                    target->fill = 0.0f;
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
            if (source == nullptr || source->structural())
                return 0.0f;
            const sandhybrid::Material material = source->material();
            const float removed = remove_loose_volume(from, requested);
            if (removed <= 0.0f)
                return 0.0f;
            const float added = add_volume(to, removed, material, false);
            const float remainder = removed - added;
            if (remainder > 0.0f)
                static_cast<void>(add_volume(from, remainder, material, false));
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
