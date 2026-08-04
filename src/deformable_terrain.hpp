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

namespace runner::sim
{
    class DeformableTerrain
    {
    public:
        struct Cell
        {
            float height{};
            float rest_height{};
            float firmness{ 0.35f };
            float loose_fraction{ 0.65f };
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
            cells_.fill(Cell{});
            fine_cells_.fill(FineCell{});
            macro_tiles_.fill(MacroTile{});
            surface_rows_.fill(-1);
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
                const float random_firmness = 0.26f
                    + unit_hash(seed_ ^ (static_cast<std::uint64_t>(column)
                        * 0xbf58476d1ce4e5b9ULL)) * 0.30f;
                cells_[column].firmness = top != nullptr && top->structural()
                    ? 0.82f : std::clamp(random_firmness, 0.18f, 0.72f);
                cells_[column].loose_fraction = 1.0f - cells_[column].firmness;
            }
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
            Cell& column = cells_[center];
            const float load = std::clamp(normalized_load, 0.0f, 4.0f);
            const float slip = std::clamp(std::abs(slip_speed), 0.0f, 5.0f);
            const float softness = std::clamp(1.0f - column.firmness, 0.0f, 1.0f);
            const float requested = std::min(0.018f,
                (load * 0.065f + slip * 0.015f) * softness
                    * std::clamp(dt, 0.0f, 0.05f));
            if (requested <= 0.0f)
                return;

            const float removed = remove_loose_volume(center, requested);
            if (removed <= 0.0f)
                return;
            const std::size_t left = wrap_column(static_cast<std::ptrdiff_t>(center) - 1);
            const std::size_t right = wrap_column(static_cast<std::ptrdiff_t>(center) + 1);
            const float left_added = add_volume(left, removed * 0.5f,
                sandhybrid::Material::sand, false);
            const float right_added = add_volume(right, removed - left_added,
                sandhybrid::Material::sand, false);
            const float returned = removed - left_added - right_added;
            if (returned > 0.0f)
                static_cast<void>(add_volume(center, returned,
                    sandhybrid::Material::sand, false));

            column.firmness = std::clamp(column.firmness
                + load * dt * 0.12f, 0.0f, 1.0f);
            column.loose_fraction = std::clamp(column.loose_fraction
                - load * dt * 0.08f, 0.0f, 1.0f);
            cells_[left].loose_fraction = std::clamp(
                cells_[left].loose_fraction + removed * 2.0f, 0.0f, 1.0f);
            cells_[right].loose_fraction = std::clamp(
                cells_[right].loose_fraction + removed * 2.0f, 0.0f, 1.0f);
        }

        void deposit(float course_x, float height_volume, float material_firmness) noexcept
        {
            const std::size_t center = nearest_index(course_x);
            const float amount = std::max(0.0f, height_volume);
            constexpr std::array<float, 5> weights{ 0.10f, 0.22f, 0.36f, 0.22f, 0.10f };
            float remaining = amount;
            for (std::size_t offset = 0; offset < weights.size(); ++offset)
            {
                const auto signed_offset = static_cast<std::ptrdiff_t>(offset) - 2;
                const std::size_t column_index = wrap_column(
                    static_cast<std::ptrdiff_t>(center) + signed_offset);
                const float requested = offset + 1u == weights.size()
                    ? remaining : amount * weights[offset];
                const float added = add_volume(column_index, requested,
                    sandhybrid::Material::sand, false);
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
                    sandhybrid::Material::sand, false));
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
                if (high_top == nullptr || high_top->structural())
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

            for (Cell& column : cells_)
            {
                column.firmness = std::clamp(column.firmness
                    + bounded_dt * (0.006f - column.loose_fraction * 0.004f),
                    0.0f, 1.0f);
            }
        }

        [[nodiscard]] float total_height_volume() const noexcept
        {
            double result = 0.0;
            for (const Cell& column : cells_)
                result += static_cast<double>(column.height);
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

        [[nodiscard]] const std::array<Cell, cell_count>& cells() const noexcept
        {
            return cells_;
        }

        [[nodiscard]] const std::array<FineCell, fine_cell_count>& fine_cells() const noexcept
        {
            return fine_cells_;
        }

        [[nodiscard]] const std::array<MacroTile, macro_tile_count>& macro_tiles() const noexcept
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
            const float amplitude = 0.72f + difficulty_ * 1.10f;
            float height = 0.0f;
            if (local >= 27.0f && local < 33.0f)
            {
                const float t = (local - 27.0f) / 6.0f;
                const float smooth = t * t * (3.0f - 2.0f * t);
                height = amplitude * smooth;
            }
            else if (local >= 33.0f && local < 38.0f)
            {
                height = amplitude;
            }
            else if (local >= 38.0f && local < 44.0f)
            {
                const float t = (local - 38.0f) / 6.0f;
                const float smooth = t * t * (3.0f - 2.0f * t);
                height = amplitude * (1.0f - smooth);
            }

            const float roughness = 0.09f + difficulty_ * 0.12f;
            height += std::sin(course_x * 0.61f) * roughness;
            height += std::sin(course_x * 1.73f + 0.7f) * roughness * 0.44f;
            height += (unit_hash(seed_ ^ static_cast<std::uint64_t>(
                std::floor(local / fine_cell_spacing))) - 0.5f)
                * fine_cell_spacing * 1.30f;

            const std::size_t macro_x = static_cast<std::size_t>(
                std::floor(local / macro_tile_size));
            bool ledge = false;
            if ((macro_x >= 10u && macro_x <= 12u)
                || (macro_x >= 47u && macro_x <= 49u))
            {
                const float ledge_height = macro_x < 20u
                    ? macro_tile_size : macro_tile_size * 2.0f;
                height += ledge_height;
                ledge = true;
            }
            return { std::clamp(height, -1.25f, 3.50f), ledge };
        }

        void initialize_column(std::size_t column) noexcept
        {
            const float course_x = static_cast<float>(column) * fine_cell_spacing;
            const SurfaceProfile surface = authored_surface(course_x);
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
                    ? sandhybrid::Material::sand
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
                    ? sandhybrid::Material::stone : sandhybrid::Material::sand;
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
            const float removed = remove_loose_volume(from, requested);
            if (removed <= 0.0f)
                return 0.0f;
            const float added = add_volume(to, removed,
                sandhybrid::Material::sand, false);
            const float remainder = removed - added;
            if (remainder > 0.0f)
                static_cast<void>(add_volume(from, remainder,
                    sandhybrid::Material::sand, false));
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

        std::array<Cell, cell_count> cells_{};
        std::array<FineCell, fine_cell_count> fine_cells_{};
        std::array<MacroTile, macro_tile_count> macro_tiles_{};
        std::array<int, cell_count> surface_rows_{};
        sandhybrid::SparseSectionGrid section_grid_{};
        std::uint64_t seed_{ 1u };
        std::uint64_t tick_{ 1u };
        float difficulty_{ 0.25f };
        std::size_t macro_promotions_{};
        std::size_t macro_demotions_{};
    };
}
