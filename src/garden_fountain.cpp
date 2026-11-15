#include "garden_fountain.h"

namespace royal_garden {

// The fountain is a hierarchy of frames (L-9 Hierarchical Modeling):
//
//   instance (the plaza centre)
//    |- foundation: two octagonal steps
//    |- basin frame  = instance * T(0, basinBaseY, 0)
//    |   |- basin wall, gold rim, water surface
//    |   |- 8 x pinnacle frame = basin * T(0, rim top, 0) * RotY(corner angle) * T(radius, 0, 0)
//    |   |       |- pillar, pyramid cap
//    |   |- pedestal: base drum, column, two gold collars
//    |   |- bowl frame = basin * T(0, pedestal, 0)
//    |       |- bowl, rim, gold rim, water
//    |       |- spire frame = bowl * T(0, bowl top, 0)
//    |            |- slender column, gold collar, cone, glowing crystal
//
// Each child frame is its parent's frame times a local step. Raise the pedestal
// and the bowl, the spire and the crystal all rise with it.
void fountain(Vertices& v, const glm::mat4& instance) {
    namespace f = fountain_size;
    Vertices part;   // one part at a time, built round the origin, then placed
    auto add = [&](const glm::mat4& frame) {
        place(v, part, frame);
        part.clear();
    };

    // Foundation: a wide low step, and a narrower one on top of it.
    prism(part, {0, 0, 0}, 4.9f, 0.18f, 8, Curb);
    add(instance);
    prism(part, {0, 0, 0}, 4.5f, 0.25f, 8, Marble);
    add(instance * T(0, 0.18f, 0));

    // Basin: an octagonal wall with a thin gold band on top (the castle's gold rings),
    // and the water surface inside it. The water uses the same octagon as the inner
    // wall, so its edge meets the wall exactly.
    const glm::mat4 basin = instance * T(0, f::basinBaseY, 0);
    ringPrism(part, {0, 0, 0}, f::basinOuter, f::basinInner, f::basinWall, 8, Marble);
    add(basin);
    ringPrism(part, {0, 0, 0}, f::basinOuter + 0.06f, f::basinInner - 0.06f, 0.07f, 8, Gold);
    add(basin * T(0, f::basinWall, 0));
    prism(part, {0, 0, 0}, f::basinInner, 0.02f, 8, Water);
    add(basin * T(0, f::waterLevel, 0));

    // Eight gothic pinnacles, one on each corner of the rim. The octagon's corners
    // sit at angles 45 * (i + 0.5) degrees, measured from +X towards +Z.
    // RotY(+a) turns +X towards -Z, so we turn by -a to reach the corner (cos a, 0, sin a).
    const float rimMiddle = 0.5f * (f::basinOuter + f::basinInner);
    for (int i = 0; i < 8; ++i) {
        const float corner = 45.0f * (i + 0.5f);
        const glm::mat4 pinnacle = basin * T(0, f::basinWall + 0.07f, 0) * RotY(-corner)
                                         * T(rimMiddle, 0, 0);
        box(part, {-0.18f, 0, -0.18f}, {0.18f, 0.5f, 0.18f}, Marble);
        add(pinnacle);
        cylinder(part, 0.6f, 0.27f, 0.01f, 4, Marble);   // 4 sides = a pyramid
        add(pinnacle * T(0, 0.5f, 0) * RotY(45.0f));      // turn it to sit square on the pillar
    }

    // Pedestal: a wide drum rising out of the water, the column, two gold collars.
    cylinder(part, f::waterLevel + 0.15f, 0.95f, 0.6f, 24, Marble);  // its top stays above the water
    add(basin);
    cylinder(part, f::pedestal, 0.55f, 0.5f, 24, Marble);
    add(basin);
    for (float y : {1.3f, 2.5f}) {
        cylinder(part, 0.08f, 0.6f, 0.6f, 24, Gold);
        add(basin * T(0, y, 0));
    }

    // Upper bowl on top of the pedestal: a flared cone with a closed top, a rim,
    // a gold band and its own small pool of water.
    const glm::mat4 bowl = basin * T(0, f::pedestal, 0);
    cylinder(part, 0.45f, 0.5f, 1.75f, 24, Marble);
    add(bowl);
    ringPrism(part, {0, 0, 0}, 1.8f, 1.55f, 0.14f, 16, Marble);
    add(bowl * T(0, 0.45f, 0));
    ringPrism(part, {0, 0, 0}, f::bowlOuter, 1.51f, 0.04f, 16, Gold);
    add(bowl * T(0, f::bowlRimTop, 0));
    prism(part, {0, 0, 0}, 1.55f, 0.02f, 16, Water);
    add(bowl * T(0, 0.5f, 0));

    // Spire: a slender column, a gold collar, a pointed cone, and a floating crystal
    // that gives off violet light - the "magic" heart of the garden.
    const glm::mat4 spire = bowl * T(0, f::bowlRimTop, 0);
    cylinder(part, 1.4f, 0.18f, 0.14f, 12, Marble);
    add(spire);
    cylinder(part, 0.06f, 0.22f, 0.22f, 12, Gold);
    add(spire * T(0, 0.7f, 0));
    cylinder(part, 0.7f, 0.3f, 0.01f, 8, Marble);
    add(spire * T(0, 1.4f, 0));
    sphere(part, 1.0f, 6, 8, Crystal);
    add(spire * T(0, 2.35f, 0) * S(0.2f, 0.42f, 0.2f));
}
}
