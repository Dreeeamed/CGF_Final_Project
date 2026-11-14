#pragma once
// Zone B: the gothic fountain, the focal point of the garden.
#include "garden_primitives.h"

namespace royal_garden {

// Sizes of the fountain in its own local coordinates (metres): the origin is the
// centre of its foundation, on the plaza surface.
namespace fountain_size {
constexpr float basinOuter = 4.2f;     // basin wall, outside
constexpr float basinInner = 3.7f;     // basin wall, inside = the water's edge
constexpr float basinBaseY = 0.43f;    // top of the two foundation steps
constexpr float basinWall = 0.85f;     // height of the basin wall
constexpr float waterLevel = 0.6f;     // water surface, above the basin base
constexpr float pedestal = 2.9f;       // central column, from the basin base to the upper bowl
constexpr float bowlRimTop = 0.59f;    // top of the upper bowl's rim, above the bowl frame
constexpr float bowlOuter = 1.84f;     // outer radius of the upper bowl's gold rim
}

// Adds the whole fountain to `v`. `instance` places it in the garden.
void fountain(Vertices& v, const glm::mat4& instance);
}
