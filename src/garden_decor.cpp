#include "garden_decor.h"
#include <algorithm>

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
}
