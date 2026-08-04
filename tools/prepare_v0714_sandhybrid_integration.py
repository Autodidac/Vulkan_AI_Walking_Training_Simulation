from pathlib import Path

# Normalize nested fine-cell state before the combined SandHybrid materializer runs.
ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "src/deformable_terrain.hpp"
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    text = text.replace(old, new, 1)


replace_once(
    '''                const FineCell& top = top_cell(column);
                const float random_firmness = 0.26f
                    + unit_hash(seed_ ^ (static_cast<std::uint64_t>(column)
                        * 0xbf58476d1ce4e5b9ULL)) * 0.30f;
                cells_[column].firmness = top.structural()
                    ? 0.82f : std::clamp(random_firmness, 0.18f, 0.72f);''',
    '''                const FineCell* top = top_cell(column);
                const float random_firmness = 0.26f
                    + unit_hash(seed_ ^ (static_cast<std::uint64_t>(column)
                        * 0xbf58476d1ce4e5b9ULL)) * 0.30f;
                cells_[column].firmness = top != nullptr && top->structural()
                    ? 0.82f : std::clamp(random_firmness, 0.18f, 0.72f);''',
    "reset top-cell pointer",
)
replace_once(
    '''                if (top_cell(high).structural())
                    continue;''',
    '''                const FineCell* high_top = top_cell(high);
                if (high_top == nullptr || high_top->structural())
                    continue;''',
    "settling top-cell pointer",
)
replace_once(
    '''            cell = {};
            changed_cell(column, row);''',
    '''            clear_cell(cell);
            changed_cell(column, row);''',
    "erase explicit fine-cell clear",
)
replace_once(
    '''        static void set_cell(FineCell& cell, sandhybrid::Material material,
            bool structural, float fill) noexcept
        {
            cell.material_id = static_cast<std::uint8_t>(material);
            cell.flags = structural ? FineCell::structural_flag : 0u;
            cell.fill = std::clamp(fill, 0.0f, 1.0f);
            if (cell.fill <= 1.0e-6f)
                cell = {};
        }

        [[nodiscard]] FineCell& top_cell(std::size_t column) noexcept
        {
            const int row = surface_rows_[column];
            if (row < 0)
                return empty_cell_;
            return fine_cells_[fine_index(column, static_cast<std::size_t>(row))];
        }

        [[nodiscard]] const FineCell& top_cell(std::size_t column) const noexcept
        {
            const int row = surface_rows_[column];
            if (row < 0)
                return empty_cell_;
            return fine_cells_[fine_index(column, static_cast<std::size_t>(row))];
        }''',
    '''        static void clear_cell(FineCell& cell) noexcept
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
        }''',
    "explicit cell helpers and nullable top-cell access",
)
replace_once(
    '''                if (cell.fill <= 1.0e-6f)
                    cell = {};
                changed_cell(column, changed_row);''',
    '''                if (cell.fill <= 1.0e-6f)
                    clear_cell(cell);
                changed_cell(column, changed_row);''',
    "removed-volume explicit clear",
)
replace_once(
    '''        std::size_t macro_promotions_{};
        std::size_t macro_demotions_{};
        inline static FineCell empty_cell_{};''',
    '''        std::size_t macro_promotions_{};
        std::size_t macro_demotions_{};''',
    "remove nested static fine-cell sentinel",
)

path.write_text(text, encoding="utf-8")

# The old pressure sample landed inside the new deterministic stone ledge.
# Keep the compaction contract on an actual granular section; hard structural
# walls must not deform merely because a foot presses on them.
test_path = ROOT / "tests/deformable_terrain_tests.cpp"
test_text = test_path.read_text(encoding="utf-8")
old_sample = "    constexpr float x=12.5f;"
new_sample = "    constexpr float x=20.5f;"
if test_text.count(old_sample) != 1:
    raise RuntimeError(
        f"granular pressure sample: expected one match, found {test_text.count(old_sample)}")
test_path.write_text(test_text.replace(old_sample, new_sample, 1), encoding="utf-8")

Path(__file__).unlink()
