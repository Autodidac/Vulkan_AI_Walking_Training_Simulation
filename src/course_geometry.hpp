#pragma once

#include <cstddef>

namespace runner::course_geometry
{
    inline constexpr std::size_t authored_cell_count = 640u;
    inline constexpr float fine_cell_spacing_meters = 0.140625f;
    inline constexpr float authored_span_meters =
        static_cast<float>(authored_cell_count) * fine_cell_spacing_meters;
    inline constexpr float authored_center_meters =
        authored_span_meters * 0.5f;

    // The open-water lesson keeps a full-depth interior while its two banks
    // use twelve canonical cells each.  This is wide enough for every
    // production foot plate to acquire/release contacts one node at a time;
    // it is still authored cell geometry, never a collision-only spline.
    inline constexpr std::size_t shore_transition_cell_count = 12u;
    inline constexpr float shore_transition_meters =
        static_cast<float>(shore_transition_cell_count)
        * fine_cell_spacing_meters;

    // Ordinary walk/run surfaces may change by at most this much between
    // adjacent canonical columns. Dedicated climb lessons own true ledges.
    inline constexpr float traversable_neighbor_rise_meters = 0.105f;

    // Presentation texture remains canonical SceneCell state.  Seeded lenses
    // are broad enough to read as sediment and ore veins in the full-world
    // view; the protected cap keeps every visual treatment away from the
    // walking cells that own contact and traction.
    inline constexpr std::size_t geology_lens_count = 28u;
    inline constexpr std::size_t geology_surface_clearance_cells = 14u;

    // Underground spaces are sized from the tallest production rig rather
    // than the viewport.  Corridors must clear a rig but no individual room
    // may read as an arena-sized debug void.
    inline constexpr std::size_t minimum_facility_clear_height_cells = 22u;
    inline constexpr std::size_t maximum_facility_clear_height_cells = 44u;
    inline constexpr std::size_t maximum_cavern_open_span_cells = 120u;
    inline constexpr std::size_t maximum_cavern_open_height_cells = 44u;
}
