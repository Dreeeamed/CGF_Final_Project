#include "garden_decor.h"
#include <algorithm>
#include <cmath>

namespace royal_garden {

// The bench is built straight in its local coordinates (no hierarchy needed:
// nothing on it moves). Parts:
//   two carved legs with armrests and small pyramid finials,
//   the seat slab with a gold edge,
//   a back panel with three dark gothic "windows" (lancets),
//   and a pointed gable on top - the gothic arch shape of the castle.
void bench(Vertices& v, const glm::mat4& instance) {
    Vertices b;   // the whole bench, in local coordinates

    // Legs, armrests and finials, mirrored left and right (side = -1 or +1).
    for (float side : {-1.0f, 1.0f}) {
        const float x0 = side * 0.80f, x1 = side * 0.96f;   // inner and outer face of a leg
        box(b, {std::min(x0, x1), 0.0f, -0.22f}, {std::max(x0, x1), 0.42f, 0.22f}, Marble);
        const float a0 = side * 0.82f, a1 = side * 1.05f;
        box(b, {std::min(a0, a1), 0.52f, -0.25f}, {std::max(a0, a1), 0.70f, 0.20f}, Marble);
        Vertices finial;
        cylinder(finial, 0.25f, 0.10f, 0.01f, 4, Marble);   // a little square pyramid
        place(b, finial, T(side * 0.935f, 0.70f, 0.08f) * RotY(45.0f));
    }

    // Seat, and a thin gold band along its front edge.
    box(b, {-1.0f, 0.42f, -0.25f}, {1.0f, 0.52f, 0.25f}, Marble);
    box(b, {-1.0f, 0.44f, 0.25f}, {1.0f, 0.50f, 0.27f}, Gold);

    // Back panel between the armrests, from the seat up to 1.0 m.
    const float back = -0.25f, front = -0.19f;   // its two faces (Z)
    box(b, {-0.9f, 0.52f, back}, {0.9f, 1.0f, front}, Marble);

    // Three lancet windows: a dark slot with a pointed top, set 5 mm in front of
    // the panel so the two surfaces never share the same depth (no z-fighting).
    const float inlay = front + 0.005f;
    for (float x : {-0.5f, 0.0f, 0.5f}) {
        box(b, {x - 0.09f, 0.60f, front}, {x + 0.09f, 0.86f, inlay}, Soil);
        triangle(b, {x - 0.09f, 0.86f, inlay}, {x + 0.09f, 0.86f, inlay}, {x, 0.95f, inlay}, Soil);
    }

    // Gable: a triangle on top of the panel, given thickness. Front and back
    // triangles, two sloping roofs. Corner order makes every face point outwards.
    const glm::vec3 lf(-0.9f, 1.0f, front), rf(0.9f, 1.0f, front), tf(0.0f, 1.35f, front);
    const glm::vec3 lb(-0.9f, 1.0f, back), rb(0.9f, 1.0f, back), tb(0.0f, 1.35f, back);
    triangle(b, lf, rf, tf, Marble);       // front (+Z)
    triangle(b, rb, lb, tb, Marble);       // back  (-Z)
    quad(b, lb, lf, tf, tb, Marble);       // left slope
    quad(b, tb, tf, rf, rb, Marble);       // right slope
    // A gold finial on the very top, like the castle's gold accents.
    Vertices tip;
    sphere(tip, 1.0f, 4, 6, Gold);
    place(b, tip, T(0.0f, 1.38f, 0.5f * (front + back)) * S(0.05f));

    place(v, b, instance);
}

// The lantern is a small hierarchy: post frame -> head frame -> cage, flame, roof.
//
//   base (octagonal stone foot) and iron post with a gold collar
//   head = T(0, postHeight, 0)
//     |- bottom plate, 4 corner bars (the cage), glowing core
//     |- roof = head * T(0, cageHeight, 0): pyramid + gold ball
void lantern(Vertices& v, const glm::mat4& instance) {
    Vertices l;   // the whole lantern, in local coordinates
    const float postHeight = 2.0f, cageHeight = 0.45f, half = 0.16f;

    prism(l, {0, 0, 0}, 0.22f, 0.25f, 8, Marble);      // stone foot
    Vertices part;
    cylinder(part, postHeight, 0.07f, 0.05f, 10, Iron);
    place(l, part, T(0, 0.25f, 0));
    part.clear();
    cylinder(part, 0.05f, 0.09f, 0.09f, 10, Gold);
    place(l, part, T(0, 1.2f, 0));

    const glm::mat4 head = T(0, 0.25f + postHeight, 0);
    part.clear();
    box(part, {-half - 0.03f, 0, -half - 0.03f}, {half + 0.03f, 0.04f, half + 0.03f}, Iron);
    for (float sx : {-1.0f, 1.0f})                     // the 4 corner bars of the cage
        for (float sz : {-1.0f, 1.0f})
            box(part, {sx * half - 0.02f, 0.04f, sz * half - 0.02f},
                      {sx * half + 0.02f, cageHeight, sz * half + 0.02f}, Iron);
    // The flame: a glowing core inside the cage. It gives off warm light (emission),
    // and the team's bloom turns that into a soft halo.
    box(part, {-half + 0.03f, 0.08f, -half + 0.03f}, {half - 0.03f, cageHeight - 0.04f, half - 0.03f},
        LanternGlow);
    place(l, part, head);

    const glm::mat4 roof = head * T(0, cageHeight, 0);
    part.clear();
    cylinder(part, 0.3f, 0.27f, 0.01f, 4, Iron);       // a square pyramid roof ...
    place(l, part, roof * RotY(45.0f));                 // ... turned to sit square on the cage
    part.clear();
    sphere(part, 1.0f, 4, 6, Gold);
    place(l, part, roof * T(0, 0.32f, 0) * S(0.05f));

    place(v, l, instance);
}

// Pillar parts, bottom to top. Only the shaft length depends on `height`;
// every part above it is placed relative to the top of the shaft.
void gothicPillar(Vertices& v, const glm::mat4& instance, float height) {
    Vertices p;
    box(p, {-0.6f, 0.0f, -0.6f}, {0.6f, 0.5f, 0.6f}, Marble);       // square plinth
    prism(p, {0, 0.5f, 0}, 0.45f, height, 8, Marble);                // octagonal shaft
    for (float y : {1.4f, height * 0.75f}) {                         // two gold bands
        Vertices band;
        ringPrism(band, {0, 0, 0}, 0.5f, 0.38f, 0.08f, 8, Gold);
        place(p, band, T(0, 0.5f + y, 0));
    }
    const float top = 0.5f + height;                                  // top of the shaft
    box(p, {-0.55f, top, -0.55f}, {0.55f, top + 0.35f, 0.55f}, Marble);   // capital
    Vertices spire;
    cylinder(spire, 1.6f, 0.42f, 0.01f, 4, Marble);                  // tall square pinnacle
    place(p, spire, T(0, top + 0.35f, 0) * RotY(45.0f));
    Vertices ball;
    sphere(ball, 1.0f, 4, 6, Gold);
    place(p, ball, T(0, top + 2.0f, 0) * S(0.1f));
    place(v, p, instance);
}

// The wall runs along Z (from the back of the garden to its front edge) and is
// 3 m thick, so from the side camera we see its garden face at an angle and its
// front end face-on: a tall dark block closing the garden like a castle tower.
void castleWallEnd(Vertices& v, float faceX) {
    const float back = faceX - 3.0f, height = 14.0f;
    const float z0 = layout::backZ, z1 = layout::frontZ;
    box(v, {back, 0, z0}, {faceX, height, z1}, CastleStone);
    // A gold band, like the rings on the castle's columns.
    box(v, {faceX, 9.8f, z0}, {faceX + 0.06f, 10.0f, z1}, Gold);
    box(v, {back, 9.8f, z1}, {faceX + 0.06f, 10.0f, z1 + 0.06f}, Gold);
    // Battlements: merlons along the top, 1 m wide, 1 m apart.
    for (float z = z0 + 0.5f; z + 1.0f <= z1; z += 2.0f)
        box(v, {back, height, z}, {faceX, height + 1.2f, z + 1.0f}, CastleStone);

    // The doorway in the garden face, centred on the walking line: two stone jambs
    // standing out of the wall, a pointed arch outline on top, and the dark opening.
    const float half = layout::pathHalfWidth, rise = 4.5f, peak = 6.2f;
    const float out = faceX + 0.3f;     // how far the frame stands out of the wall
    for (float side : {-1.0f, 1.0f}) {
        const float zIn = side * half, zOut = side * (half + 0.5f);
        box(v, {faceX, 0, std::min(zIn, zOut)}, {out, rise, std::max(zIn, zOut)}, Marble);
    }
    // The opening: a rectangle and a pointed top, as thin dark panels 2 cm in front of
    // the wall. Seen from +X, a counter-clockwise order runs from +Z towards -Z.
    const float x = faceX + 0.02f;
    quad(v, {x, 0, half}, {x, 0, -half}, {x, rise, -half}, {x, rise, half}, Soil);
    triangle(v, {x, rise, half}, {x, rise, -half}, {x, peak, 0}, Soil);
    // The arch outline: two slanted stone bars from the jambs to the peak.
    for (float side : {-1.0f, 1.0f}) {
        Vertices bar;
        box(bar, {-0.25f, 0, -0.2f}, {0.25f, 1.0f, 0.2f}, Marble);   // 1 m long, along Y
        const glm::vec3 from(faceX + 0.15f, rise, side * (half + 0.25f));
        const glm::vec3 to(faceX + 0.15f, peak + 0.35f, 0);
        const glm::vec3 d = to - from;
        // Tilt the bar from vertical towards the peak: an angle about the X axis.
        // Rotating about X by a sends +Y to (0, cos a, sin a), so a = atan2(dz, dy).
        const float tilt = std::atan2(d.z, d.y);
        place(v, bar, T(from.x, from.y, from.z) * glm::rotate(glm::mat4(1), tilt, glm::vec3(1, 0, 0))
                          * S(1.0f, glm::length(d), 1.0f));
    }
    Vertices keystone;
    sphere(keystone, 1.0f, 4, 6, Gold);
    place(v, keystone, T(faceX + 0.35f, peak + 0.45f, 0) * S(0.18f));
}
}
