#pragma once

#include "math.hpp"
#include "simulation.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace runner::art
{
    enum class Module : std::uint8_t
    {
        body,
        head,
        tail,
        upper_limb,
        lower_limb,
        terminal,
        hand,
        equipment,
        count
    };

    inline constexpr std::size_t module_count =
        static_cast<std::size_t>(Module::count);

    [[nodiscard]] constexpr std::string_view module_name(Module module) noexcept
    {
        switch (module)
        {
        case Module::body: return "BODY";
        case Module::head: return "HEAD";
        case Module::tail: return "TAIL";
        case Module::upper_limb: return "UPPER";
        case Module::lower_limb: return "LOWER";
        case Module::terminal: return "FOOT";
        case Module::hand: return "HAND";
        case Module::equipment: return "EQUIPMENT";
        case Module::count: break;
        }
        return "UNKNOWN";
    }

    struct ModuleAdjustment
    {
        float anchor_along{};
        float anchor_normal{};
        float pivot{ 0.5f };
        float length_scale{ 1.0f };
        float thickness_scale{ 1.0f };
        float rotation{};
        bool flip_vertical{};
        bool flip_horizontal{};
        std::int8_t layer{};

        [[nodiscard]] bool valid() const noexcept
        {
            return std::isfinite(anchor_along)
                && std::isfinite(anchor_normal)
                && std::isfinite(pivot)
                && std::isfinite(length_scale)
                && std::isfinite(thickness_scale)
                && std::isfinite(rotation)
                && std::abs(anchor_along) <= 0.50f
                && std::abs(anchor_normal) <= 0.50f
                && pivot >= 0.0f && pivot <= 1.0f
                && length_scale >= 0.50f && length_scale <= 1.50f
                && thickness_scale >= 0.50f && thickness_scale <= 1.50f
                && std::abs(rotation) <= pi
                && layer >= -2 && layer <= 2;
        }
    };

    struct Layout
    {
        static constexpr std::uint32_t schema = 1u;
        std::uint32_t file_schema{ schema };
        sim::CreatureSpecies species{ sim::CreatureSpecies::human };
        std::array<ModuleAdjustment, module_count> modules{};

        [[nodiscard]] bool valid() const noexcept
        {
            if (file_schema != schema
                || species == sim::CreatureSpecies::custom)
                return false;
            for (const ModuleAdjustment& module : modules)
                if (!module.valid())
                    return false;
            return true;
        }

        [[nodiscard]] ModuleAdjustment& at(Module module) noexcept
        {
            return modules[static_cast<std::size_t>(module)];
        }

        [[nodiscard]] const ModuleAdjustment& at(Module module) const noexcept
        {
            return modules[static_cast<std::size_t>(module)];
        }
    };

    [[nodiscard]] constexpr Layout default_layout(
        sim::CreatureSpecies species) noexcept
    {
        Layout result{};
        result.species = species == sim::CreatureSpecies::custom
            ? sim::CreatureSpecies::human : species;
        return result;
    }

    struct AdjustedTransform
    {
        Vec2 beginning{};
        Vec2 ending{};
        float thickness{};
        bool flip_vertical{};
        bool flip_horizontal{};
        std::int8_t layer{};
    };

    [[nodiscard]] inline AdjustedTransform adjust_transform(
        Vec2 beginning, Vec2 ending, float thickness,
        const ModuleAdjustment& adjustment,
        bool flip_vertical = false, bool flip_horizontal = false) noexcept
    {
        if (!adjustment.valid())
            return { beginning, ending, thickness,
                flip_vertical, flip_horizontal, 0 };
        const Vec2 delta = ending - beginning;
        const float span = length(delta);
        if (!std::isfinite(span) || span <= 1.0e-5f)
            return { beginning, ending, thickness,
                flip_vertical, flip_horizontal, adjustment.layer };
        const Vec2 axis = delta / span;
        const Vec2 normal{ -axis.y, axis.x };
        const Vec2 offset = axis * (adjustment.anchor_along * span)
            + normal * (adjustment.anchor_normal * span);
        const Vec2 pivot = beginning + axis * (span * adjustment.pivot) + offset;
        Vec2 first = beginning + offset;
        Vec2 second = ending + offset;
        first = pivot + (first - pivot) * adjustment.length_scale;
        second = pivot + (second - pivot) * adjustment.length_scale;
        first = pivot + rotate(first - pivot, adjustment.rotation);
        second = pivot + rotate(second - pivot, adjustment.rotation);
        return { first, second,
            std::max(0.0f, thickness * adjustment.thickness_scale),
            flip_vertical != adjustment.flip_vertical,
            flip_horizontal != adjustment.flip_horizontal,
            adjustment.layer };
    }

    class LayoutHistory
    {
    public:
        static constexpr std::size_t capacity = 32u;

        void reset(const Layout& layout) noexcept
        {
            entries_[0] = layout;
            size_ = 1u;
            cursor_ = 0u;
        }

        void commit(const Layout& layout) noexcept
        {
            if (size_ == 0u)
            {
                reset(layout);
                return;
            }
            size_ = cursor_ + 1u;
            if (size_ < capacity)
            {
                entries_[size_++] = layout;
                cursor_ = size_ - 1u;
                return;
            }
            for (std::size_t index = 1u; index < capacity; ++index)
                entries_[index - 1u] = entries_[index];
            entries_[capacity - 1u] = layout;
            cursor_ = capacity - 1u;
        }

        [[nodiscard]] bool can_undo() const noexcept
        {
            return size_ > 0u && cursor_ > 0u;
        }

        [[nodiscard]] bool can_redo() const noexcept
        {
            return size_ > 0u && cursor_ + 1u < size_;
        }

        [[nodiscard]] bool undo(Layout& layout) noexcept
        {
            if (!can_undo())
                return false;
            layout = entries_[--cursor_];
            return true;
        }

        [[nodiscard]] bool redo(Layout& layout) noexcept
        {
            if (!can_redo())
                return false;
            layout = entries_[++cursor_];
            return true;
        }

    private:
        std::array<Layout, capacity> entries_{};
        std::size_t size_{};
        std::size_t cursor_{};
    };

    [[nodiscard]] inline bool save_layout(const std::filesystem::path& path,
        const Layout& layout, std::string& error)
    {
        if (!layout.valid())
        {
            error = "art layout is outside the supported transform envelope";
            return false;
        }
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = "could not create temporary art layout";
            return false;
        }
        output << "EPOCH2DWALK_ART_LAYOUT " << Layout::schema << '\n';
        output << "SPECIES " << static_cast<unsigned>(layout.species) << '\n';
        for (std::size_t index = 0u; index < layout.modules.size(); ++index)
        {
            const ModuleAdjustment& module = layout.modules[index];
            output << "MODULE " << index << ' ' << module.anchor_along << ' '
                << module.anchor_normal << ' ' << module.pivot << ' '
                << module.length_scale << ' ' << module.thickness_scale << ' '
                << module.rotation << ' ' << module.flip_vertical << ' '
                << module.flip_horizontal << ' '
                << static_cast<int>(module.layer) << '\n';
        }
        output << "END\n";
        output.close();
        if (!output)
        {
            error = "could not finish temporary art layout";
            return false;
        }
        std::error_code ec{};
        std::filesystem::rename(temporary, path, ec);
        if (ec)
        {
            std::filesystem::remove(path, ec);
            ec.clear();
            std::filesystem::rename(temporary, path, ec);
        }
        if (ec)
        {
            std::filesystem::remove(temporary, ec);
            error = "could not replace art layout";
            return false;
        }
        error.clear();
        return true;
    }

    [[nodiscard]] inline bool load_layout(const std::filesystem::path& path,
        sim::CreatureSpecies expected_species, Layout& layout,
        std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        std::string token{};
        std::uint32_t schema{};
        unsigned species{};
        if (!(input >> token >> schema) || token != "EPOCH2DWALK_ART_LAYOUT"
            || schema != Layout::schema
            || !(input >> token >> species) || token != "SPECIES")
        {
            error = "invalid art layout header";
            return false;
        }
        Layout candidate = default_layout(
            static_cast<sim::CreatureSpecies>(species));
        for (std::size_t expected = 0u; expected < module_count; ++expected)
        {
            std::size_t index{};
            int flip_vertical{};
            int flip_horizontal{};
            int layer{};
            ModuleAdjustment module{};
            if (!(input >> token >> index >> module.anchor_along
                    >> module.anchor_normal >> module.pivot
                    >> module.length_scale >> module.thickness_scale
                    >> module.rotation >> flip_vertical >> flip_horizontal
                    >> layer)
                || token != "MODULE" || index != expected
                || (flip_vertical != 0 && flip_vertical != 1)
                || (flip_horizontal != 0 && flip_horizontal != 1))
            {
                error = "invalid or incomplete art module record";
                return false;
            }
            module.flip_vertical = flip_vertical != 0;
            module.flip_horizontal = flip_horizontal != 0;
            module.layer = static_cast<std::int8_t>(layer);
            candidate.modules[index] = module;
        }
        if (!(input >> token) || token != "END" || !candidate.valid()
            || candidate.species != expected_species)
        {
            error = "art layout species or transform validation failed";
            return false;
        }
        layout = candidate;
        error.clear();
        return true;
    }
}
