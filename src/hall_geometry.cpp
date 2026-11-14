// Geometry of the grand hall, built from a few simple shapes in metres.
//
// Rule for every face: the corners go COUNTER-CLOCKWISE when seen from the side
// that should be visible. triangle() computes the normal from that order, and
// back-face culling hides the other side.
#include "hall_geometry.h"
#include <cmath>
#include <cstddef>
#include <algorithm>

namespace grand_hall {
namespace {
constexpr float pi = 3.14159265359f;
using Vertices = std::vector<Vertex>;

float columnX(int index) {
    return layout::firstColumnX + index * layout::columnSpacing;
}

// Centre of the bay between column `index` and column `index + 1`.
float bayCenterX(int index) {
    return columnX(index) + layout::columnSpacing * 0.5f;
}

// ---------------------------------------------------------------------------
// Basic shapes
// ---------------------------------------------------------------------------

// One flat triangle. Its normal comes from the corner order (right-hand rule).
void triangle(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c,
              Material material, glm::vec2 ua = {0, 0}, glm::vec2 ub = {1, 0},
              glm::vec2 uc = {0.5f, 1}) {
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    v.push_back({a, normal, ua, float(material)});
    v.push_back({b, normal, ub, float(material)});
    v.push_back({c, normal, uc, float(material)});
}

// A flat quad a-b-c-d as two triangles.
// flip = true walks the corners the other way round (a, d, c, b):
// the quad then shows its front to the other side.
void quad(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          Material material, bool flip = false) {
    if (flip) std::swap(b, d);
    triangle(v, a, b, c, material, {0, 0}, {1, 0}, {1, 1});
    triangle(v, a, c, d, material, {0, 0}, {1, 1}, {0, 1});
}

// A closed box between two opposite corners; every face looks outwards.
void box(Vertices& v, glm::vec3 a, glm::vec3 b, Material m = Stone) {
    quad(v, {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}, m);   // +Z
    quad(v, {b.x, a.y, a.z}, {a.x, a.y, a.z}, {a.x, b.y, a.z}, {b.x, b.y, a.z}, m);   // -Z
    quad(v, {a.x, a.y, a.z}, {a.x, a.y, b.z}, {a.x, b.y, b.z}, {a.x, b.y, a.z}, m);   // -X
    quad(v, {b.x, a.y, b.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, m);   // +X
    quad(v, {a.x, b.y, b.z}, {b.x, b.y, b.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z}, m);   // top
    quad(v, {a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z}, m);   // bottom
}

// A capped frustum (a cylinder whose top radius may differ).
// Radial normals make a smooth shaft, rather than 96 flat faces.
void cylinder(Vertices& v, glm::vec3 bottom, float height, float radius0, float radius1,
              Material material = Stone, int segments = 96) {
    const glm::vec3 top = bottom + glm::vec3(0, height, 0);
    const float m = float(material);
    for (int i = 0; i < segments; ++i) {
        const float a = 2.0f * pi * i / segments;
        const float b = 2.0f * pi * (i + 1) / segments;
        const glm::vec3 ra(std::cos(a), 0, std::sin(a));
        const glm::vec3 rb(std::cos(b), 0, std::sin(b));
        const glm::vec3 p0 = bottom + radius0 * ra, p1 = bottom + radius0 * rb;
        const glm::vec3 p2 = top + radius1 * rb, p3 = top + radius1 * ra;
        const glm::vec3 na = glm::normalize(ra + glm::vec3(0, (radius0 - radius1) / height, 0));
        const glm::vec3 nb = glm::normalize(rb + glm::vec3(0, (radius0 - radius1) / height, 0));
        const float u0 = float(i) / segments, u1 = float(i + 1) / segments;
        // The side strip, with the smooth normals.
        v.insert(v.end(), {
            {p0, na, {u0, 0}, m}, {p3, na, {u0, 1}, m}, {p2, nb, {u1, 1}, m},
            {p0, na, {u0, 0}, m}, {p2, nb, {u1, 1}, m}, {p1, nb, {u1, 0}, m}});
        // The two caps.
        triangle(v, bottom, p0, p1, material);
        triangle(v, top, p2, p3, material);
    }
}

// A thin 12-sided bar from a to b (railings, chains, the gate grille).
void rod(Vertices& v, glm::vec3 a, glm::vec3 b, float radius, Material m = Brass) {
    const glm::vec3 axis = glm::normalize(b - a);
    const glm::vec3 helper = std::abs(axis.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    const glm::vec3 u = glm::normalize(glm::cross(axis, helper));
    const glm::vec3 w = glm::cross(axis, u);
    for (int i = 0; i < 12; ++i) {
        const float t0 = 2 * pi * i / 12, t1 = 2 * pi * (i + 1) / 12;
        const glm::vec3 r0 = radius * (std::cos(t0) * u + std::sin(t0) * w);
        const glm::vec3 r1 = radius * (std::cos(t1) * u + std::sin(t1) * w);
        quad(v, a + r0, a + r1, b + r1, b + r0, m);
        triangle(v, a, a + r1, a + r0, m);
        triangle(v, b, b + r0, b + r1, m);
    }
}

// ---------------------------------------------------------------------------
// Parts of the hall
// ---------------------------------------------------------------------------

// Lesson 16: a black marble shaft with gold trims.
void column(Vertices& v, float x, float z) {
    // All horizontal radii shrink together; the supporting height stays at 44 m.
    const float radius = layout::columnRadius;
    const float baseRadius = layout::columnBaseRadius;
    const float collarRadius = layout::columnCollarRadius;
    cylinder(v, {x, 0, z},     0.7f,  baseRadius, baseRadius, Gold);   // plinth
    cylinder(v, {x, 0.7f, z},  0.7f,  baseRadius, radius, Gold);       // base moulding
    cylinder(v, {x, 1.4f, z},  40.4f, radius, radius, Marble);         // shaft
    cylinder(v, {x, 41.8f, z}, 1.4f,  radius, baseRadius, Gold);       // capital
    cylinder(v, {x, 43.2f, z}, 0.8f,  baseRadius, baseRadius, Gold);   // abacus
    // Narrow gold collars divide the huge vertical silhouette.
    cylinder(v, {x, 10, z}, 0.35f, collarRadius, collarRadius, Gold);
    cylinder(v, {x, 31, z}, 0.35f, collarRadius, collarRadius, Gold);
}

// A pointed arch with a real six-metre recess behind it. Only its far end is dark.
// The wall is filled ABOVE the arch, never with a rectangle across the opening.
//   cx, base       - centre and floor of the opening
//   halfWidth      - half of the opening width
//   spring, rise   - height of the straight sides, then of the pointed top
//   wallZ, side    - the wall plane, and which wall: -1 far, +1 near
//   wallTop        - fill the wall above the arch up to this height
void portal(Vertices& v, float cx, float base, float halfWidth, float spring,
            float rise, float wallZ, int side, float wallTop, Material fill = Darkness) {
    const float backZ = wallZ + side * 6.0f;   // the far end of the recess
    const float rimZ = wallZ - side * 0.5f;    // the stone frame sticks out of the wall
    const float thickness = 0.7f;
    for (int half : {-1, 1}) {
        // Each -1 is a mirror, and a mirror reverses the corner order.
        // Two mirrors (half and side) cancel out; one mirror must be undone.
        const bool flip = half * side < 0;
        const float x0 = cx + half * halfWidth;
        const float x1 = cx + half * (halfWidth + thickness);

        // The straight side of the frame, and the side wall of the recess.
        box(v, {std::min(x0, x1), base, std::min(wallZ, rimZ)},
               {std::max(x0, x1), base + spring, std::max(wallZ, rimZ)});
        quad(v, {x0, base, rimZ}, {x0, base, backZ},
                {x0, base + spring, backZ}, {x0, base + spring, rimZ}, Stone, flip);

        // The pointed top, in 32 slices. point() walks up the curve as t goes 0 -> 1.
        for (int j = 0; j < 32; ++j) {
            const float t0 = j / 32.0f, t1 = (j + 1) / 32.0f;
            auto point = [&](float t, float w, float r, float z) {
                return glm::vec3(cx + half * w * (1 - t * t), base + spring + r * t, z);
            };
            const auto a = point(t0, halfWidth, rise, rimZ);                          // inner edge
            const auto b = point(t1, halfWidth, rise, rimZ);
            const auto c = point(t0, halfWidth + thickness, rise + thickness, rimZ);  // outer edge
            const auto d = point(t1, halfWidth + thickness, rise + thickness, rimZ);
            quad(v, a, b, d, c, Stone, flip);                                         // frame front
            quad(v, c, d, {d.x, d.y, wallZ}, {c.x, c.y, wallZ}, Stone, flip);         // frame outside
            quad(v, a, {a.x, a.y, backZ}, {b.x, b.y, backZ}, b, Stone, flip);         // vault of the recess
            if (wallTop > base + spring + rise + thickness) {
                quad(v, {c.x, c.y, wallZ}, {d.x, d.y, wallZ},                         // wall above
                        {d.x, wallTop, wallZ}, {c.x, wallTop, wallZ}, Wall, flip);
            }
            // The far end of the recess under the pointed top.
            const glm::vec3 top(cx, base + spring, backZ), A(a.x, a.y, backZ), B(b.x, b.y, backZ);
            if (flip) triangle(v, top, A, B, fill);
            else      triangle(v, top, B, A, fill);
        }
    }
    // Far end and floor of the recess. Written for the far wall, so the near wall flips them.
    quad(v, {cx - halfWidth, base, backZ}, {cx + halfWidth, base, backZ},
            {cx + halfWidth, base + spring, backZ}, {cx - halfWidth, base + spring, backZ}, fill, side > 0);
    quad(v, {cx - halfWidth, base, wallZ}, {cx + halfWidth, base, wallZ},
            {cx + halfWidth, base, backZ}, {cx - halfWidth, base, backZ}, Stone, side > 0);
}

// Lesson 17: a long banner hanging from a gold rod; its bottom edge ends in a point.
//   topLeft - the top-left corner, across - from the left edge to the right edge,
//   height  - of the straight part, tip - how much lower the point hangs.
// UV: x goes 0..1 across, y goes from 0 at the tip to 1 at the top (the shader draws the sigil).
void hangingBanner(Vertices& v, glm::vec3 topLeft, glm::vec3 across, float height, float tip, Material m) {
    constexpr int panels = 16;
    const float total = height + tip;
    const glm::vec3 out = glm::normalize(glm::cross(across, glm::vec3(0, 1, 0)));   // sideways: for the waves
    for (int i = 0; i < panels; ++i) {
        const float u0 = float(i) / panels, u1 = float(i + 1) / panels;
        // Top corners, waved a little; the bottom hangs lower towards the middle (the point).
        const glm::vec3 a0 = topLeft + across * u0 + out * (0.12f * std::sin(u0 * pi * 6));
        const glm::vec3 a1 = topLeft + across * u1 + out * (0.12f * std::sin(u1 * pi * 6));
        const float drop0 = height + tip * (1.0f - std::abs(2 * u0 - 1));
        const float drop1 = height + tip * (1.0f - std::abs(2 * u1 - 1));
        const glm::vec3 b0 = a0 - glm::vec3(0, drop0, 0), b1 = a1 - glm::vec3(0, drop1, 0);
        const glm::vec2 ta0(u0, 1), ta1(u1, 1), tb0(u0, 1 - drop0 / total), tb1(u1, 1 - drop1 / total);
        // Cloth has two sides: the front pair, then the same corners in reverse order.
        triangle(v, b0, b1, a1, m, tb0, tb1, ta1);
        triangle(v, b0, a1, a0, m, tb0, ta1, ta0);
        triangle(v, b0, a1, b1, m, tb0, ta1, tb1);
        triangle(v, b0, a0, a1, m, tb0, ta0, ta1);
    }
    rod(v, topLeft - across * 0.06f + glm::vec3(0, 0.3f, 0),
           topLeft + across * 1.06f + glm::vec3(0, 0.3f, 0), 0.15f, Gold);
}

// One long side of the hall: wall piers, columns, two rows of arches, high windows,
// banners and the gallery. side = -1 is the far wall, +1 the near (cutaway) wall.
void sideWall(Vertices& v, int side) {
    const float z = side * 20.0f;
    const float pier = layout::wallPierHalfWidth;
    const float opening = layout::archHalfWidth;
    // A 1 m thick slab of wall, always written from the smaller z to the larger one.
    const float wallNear = std::min(z, z + side * 1.0f);
    const float wallFar = std::max(z, z + side * 1.0f);

    for (int i = 0; i < layout::columnsPerSide; ++i) {
        const float x = columnX(i);
        box(v, {x - pier, 0, wallNear}, {x + pier, 60, wallFar}, Wall);
        column(v, x, side * layout::rowZ);
    }
    for (int i = 0; i < layout::columnsPerSide - 1; ++i) {
        const float x = bayCenterX(i);
        portal(v, x, 0, opening, 13, 8, z, side, 23.0f);        // ground floor arch
        portal(v, x, 24.8f, opening, 10, 8, z, side, 44.0f);    // gallery arch
        // A narrow high window above each gallery bay.
        portal(v, x, 46, 2.2f, 6, 3, z, side, 60.0f, Window);
        box(v, {x - opening, 44, wallNear}, {x + opening, 46, wallFar}, Wall);
        for (int half : {-1, 1}) {
            const float a = x + half * 2.9f, b = x + half * opening;
            box(v, {std::min(a, b), 46, wallNear}, {std::max(a, b), 60, wallFar}, Wall);
        }
        // The window cross.
        rod(v, {x, 46, z - side * 0.6f}, {x, 54.7f, z - side * 0.6f}, 0.10f, Stone);
        rod(v, {x - 2.1f, 50, z - side * 0.6f}, {x + 2.1f, 50, z - side * 0.6f}, 0.10f, Stone);
    }

    // Galleries extend towards the aisle, but stay outside its clear centre.
    const float inner = side * 8.5f;
    box(v, {-98, 23, std::min(inner, z)}, {98, 24, std::max(inner, z)});   // gallery floor
    box(v, {-98, 25.8f, inner - 0.15f}, {98, 26.1f, inner + 0.15f});      // hand rail
    box(v, {-98, 24.3f, inner - 0.12f}, {98, 24.5f, inner + 0.12f});      // lower rail
    for (int i = 0; i <= 196; ++i)                                          // balusters
        cylinder(v, {-98.0f + i, 24, inner}, 1.9f, 0.075f, 0.075f, Stone, 12);
    box(v, {-108, 44, std::min(inner, z)}, {108, 45, std::max(inner, z)}); // upper cornice

    // Wall ends beyond the repeated bays.
    for (int end : {-1, 1}) {
        const float a = end < 0 ? layout::firstColumnX - pier : layout::lastColumnX + pier;
        const float b = end * layout::length * 0.5f;
        box(v, {std::min(a, b), 0, wallNear}, {std::max(a, b), 60, wallFar}, Wall);
    }
}

// Lesson 16: height of the pointed (gothic) vault above the point z across the hall.
// Two circle arcs meet at the top in a point. The arc that starts at the far wall
// (z = -20) has its centre on the other side of the middle, at z = +c, and the other
// way round: that is why they meet at an angle instead of making one round curve.
float vaultY(float z) {
    const float a = layout::width * 0.5f;                      // 20 m: half of the width
    const float h = layout::vaultApex - layout::vaultSpring;   // 24 m: how high the vault rises
    const float c = (h * h - a * a) / (2.0f * a);              // 4.4 m: the centre past the middle
    const float r = a + c;                                     // 24.4 m: radius of each arc
    const float d = std::abs(z) + c;                           // across distance to that centre
    return layout::vaultSpring + std::sqrt(std::max(r * r - d * d, 0.0f));   // circle: y^2 = r^2 - d^2
}

// Lesson 16: the vault in 1 m strips across the hall, the two end walls below it,
// and gold ribs: one arc over every column and two diagonals crossing in every bay.
// The far half (z < 0) is always drawn; the near half belongs to the cutaway foreground.
void vault(Vertices& v, Vertices& foreground) {
    constexpr int strips = 40;
    auto rib = [](float x, float z) { return glm::vec3(x, vaultY(z) - 0.35f, z); };   // just under the stone
    for (int i = 0; i < strips; ++i) {
        const float z0 = -20.0f + i, z1 = z0 + 1.0f;
        const float y0 = vaultY(z0), y1 = vaultY(z1);
        Vertices& target = z1 <= 0.0f ? v : foreground;
        // z grows across the WHOLE hall here, so one corner order faces down on both halves.
        quad(target, {-108, y0, z0}, {108, y0, z0}, {108, y1, z1}, {-108, y1, z1}, Stone);
        // The end walls under this strip (POM stone). The wall at x = -108 looks the other way.
        for (float x : {-108.0f, 108.0f})
            quad(target, {x, 0, z0}, {x, 0, z1}, {x, y1, z1}, {x, y0, z0}, Wall, x < 0);
        // This strip's piece of every rib.
        const float t0 = float(i) / strips, t1 = float(i + 1) / strips;
        for (int col = 0; col < layout::columnsPerSide; ++col)
            rod(target, rib(columnX(col), z0), rib(columnX(col), z1), 0.38f, Gold);
        for (int bay = 0; bay + 1 < layout::columnsPerSide; ++bay) {
            const float xa = columnX(bay), xb = columnX(bay + 1);
            rod(target, rib(xa + (xb - xa) * t0, z0), rib(xa + (xb - xa) * t1, z1), 0.22f, Gold);
            rod(target, rib(xb + (xa - xb) * t0, z0), rib(xb + (xa - xb) * t1, z1), 0.22f, Gold);
        }
    }
    // The ridge rib along the top, and a gold boss where the diagonals cross.
    rod(v, {-108, layout::vaultApex - 0.45f, 0}, {108, layout::vaultApex - 0.45f, 0}, 0.42f, Gold);
    for (int bay = 0; bay + 1 < layout::columnsPerSide; ++bay)
        cylinder(v, {bayCenterX(bay), layout::vaultApex - 2.2f, 0}, 1.6f, 0.4f, 1.1f, Gold, 16);
}

// Lesson 17: pointed arches from column top to column top along both rows of columns,
// with a POM stone wall above them up to the cornice at 62 m.
// The nave face looks at the middle of the hall, the aisle face at the side wall.
void arcade(Vertices& v, Vertices& foreground) {
    const float spring = 44.0f, rise = 12.0f;   // the arches start on the columns and rise 12 m
    const float band = 2.2f, depth = 2.6f, top = 62.0f;
    for (int side : {-1, 1}) {
        Vertices& target = side < 0 ? v : foreground;
        const float naveZ = side * layout::rowZ - side * depth * 0.5f;
        const float aisleZ = side * layout::rowZ + side * depth * 0.5f;
        for (int bay = 0; bay + 1 < layout::columnsPerSide; ++bay) {
            const float xa = columnX(bay), xb = columnX(bay + 1);
            const float cx = (xa + xb) * 0.5f, a = (xb - xa) * 0.5f;
            // The same two-arc pointed curve as vaultY(), now standing between two columns.
            const float c = (rise * rise - a * a) / (2.0f * a), r = a + c;
            auto arch = [&](float t) {   // t = 0 at column xa, t = 1 at column xb
                const float x = xa + (xb - xa) * t, d = std::abs(x - cx) + c;
                return glm::vec2(x, spring + std::sqrt(std::max(r * r - d * d, 0.0f)));
            };
            constexpr int n = 24;
            for (int i = 0; i < n; ++i) {
                const glm::vec2 p0 = arch(float(i) / n), p1 = arch(float(i + 1) / n);       // inner edge
                const glm::vec2 q0 = p0 + glm::vec2(0, band), q1 = p1 + glm::vec2(0, band);  // outer edge
                // Both faces: the marble band of the arch and the POM wall above it.
                // Written facing +z; flip the faces that must look towards -z.
                for (bool nave : {true, false}) {
                    const float z = nave ? naveZ : aisleZ;
                    const bool flip = nave == (side > 0);
                    quad(target, {p0.x, p0.y, z}, {p1.x, p1.y, z}, {q1.x, q1.y, z}, {q0.x, q0.y, z}, Marble, flip);
                    quad(target, {q0.x, q0.y, z}, {q1.x, q1.y, z}, {q1.x, top, z}, {q0.x, top, z}, Wall, flip);
                }
                // The underside of the arch (seen from below), and a gold line along its nave edge.
                quad(target, {p0.x, p0.y, naveZ}, {p1.x, p1.y, naveZ},
                             {p1.x, p1.y, aisleZ}, {p0.x, p0.y, aisleZ}, Marble, side < 0);
                rod(target, {p0.x, p0.y - 0.05f, naveZ - side * 0.1f},
                            {p1.x, p1.y - 0.05f, naveZ - side * 0.1f}, 0.12f, Gold);
            }
        }
        // The cornice on top, with a gold line along it.
        box(target, {layout::firstColumnX, top - 0.4f, std::min(naveZ, aisleZ) - 0.4f},
                    {layout::lastColumnX, top + 0.6f, std::max(naveZ, aisleZ) + 0.4f}, Marble);
        rod(target, {layout::firstColumnX, top - 0.5f, naveZ - side * 0.45f},
                    {layout::lastColumnX, top - 0.5f, naveZ - side * 0.45f}, 0.16f, Gold);
    }
}

void chandelier(Vertices& v) {
    rod(v, {0, layout::vaultApex, 0}, {0, 40, 0}, 0.12f);   // lesson 16: hangs from the new vault
    for (int tier = 0; tier < 2; ++tier) {
        const float y = 33.0f - tier * 5.0f, r = 7.5f - tier * 3.0f;
        // The ring, made of 96 short rods.
        for (int i = 0; i < 96; ++i) {
            const float a = 2 * pi * i / 96, b = 2 * pi * (i + 1) / 96;
            rod(v, {r * std::cos(a), y, r * std::sin(a)},
                   {r * std::cos(b), y, r * std::sin(b)}, 0.17f);
        }
        // 16 candles: holder, candle, flame; every fourth one has a chain to the centre.
        for (int i = 0; i < 16; ++i) {
            const float angle = 2 * pi * i / 16;
            const float x = r * std::cos(angle), z = r * std::sin(angle);
            cylinder(v, {x, y, z}, 0.45f, 0.35f, 0.24f, Brass, 24);
            cylinder(v, {x, y + 0.45f, z}, 0.9f, 0.10f, 0.09f, Floor, 16);
            cylinder(v, {x, y + 1.35f, z}, 0.5f, 0.15f, 0.02f, Flame, 16);
            if (i % 4 == 0) rod(v, {x, y, z}, {0, 40, 0}, 0.07f);
        }
    }
    cylinder(v, {0, 25.5f, 0}, 2.5f, 0.1f, 0.7f, Brass, 32);   // the bottom finial
    rod(v, {0, 28, 0}, {0, 40, 0}, 0.2f);
}

void throne(Vertices& v) {
    // Five steps, each one narrower and shallower than the one below.
    for (int i = 0; i < 5; ++i) {
        const float halfDepth = 4.8f - i * 0.45f;
        box(v, {88 + i * 0.7f, i * 0.32f, -halfDepth},
               {105 - i * 0.6f, (i + 1) * 0.32f, halfDepth}, Floor);
    }
    box(v, {95, 1.6f, -2.7f}, {100, 4.2f, 2.7f}, Stone);             // seat block
    box(v, {94.8f, 4.2f, -2.7f}, {99.7f, 4.6f, 2.7f}, Banner);      // cushion
    box(v, {99, 4.2f, -2.9f}, {100.2f, 12, 2.9f}, Brass);           // back
    box(v, {98.8f, 4.6f, -2.4f}, {99, 11.4f, 2.4f}, Banner);        // back cloth
    for (int side : {-1, 1}) {                                      // arm rests
        const float z = side * 2.9f;
        box(v, {94.8f, 1.6f, z - 0.35f}, {95.5f, 6.0f, z + 0.35f}, Brass);
        box(v, {94.7f, 5.6f, z - 0.4f}, {100.2f, 6.2f, z + 0.4f}, Brass);
    }
    // Five spikes on top of the back, the middle one the tallest.
    for (int i = -2; i <= 2; ++i)
        cylinder(v, {99.5f, 12, i * 1.2f}, 2.8f - std::abs(i) * 0.7f, 0.45f, 0.015f, Brass, 24);
}

void gate(Vertices& v, Vertices& grille) {
    // The entrance gate. Behind it lies the inner garden, painted by the shader (Garden).
    portal(v, -100, 0, 4.5f, 6, 3, -8, -1, 12, Garden);
    // The gate stands away from the wall: give its deep sides an outer face too.
    for (int half : {-1, 1})
        box(v, {-100 + half * 4.5f - 0.35f, 0, -14}, {-100 + half * 4.5f + 0.35f, 6, -7.5f});
    // The grille: nine vertical bars and two cross bars. A separate mesh, so it can rise.
    for (int i = -4; i <= 4; ++i)
        rod(grille, {-100.0f + i, 0, -13.8f}, {-100.0f + i, 6.1f, -13.8f}, 0.06f, Brass);
    for (float y : {1.0f, 4.8f})
        rod(grille, {-104.4f, y, -13.7f}, {-95.6f, y, -13.7f}, 0.09f, Brass);
}

// The golden key, 2 m long, glowing (Flame material), centred on the origin.
void keyShape(Vertices& v) {
    for (int i = 0; i < 16; ++i) {                                     // the bow: a ring
        const float a = 2 * pi * i / 16, b = 2 * pi * (i + 1) / 16;
        rod(v, {-0.7f + 0.3f * std::cos(a), 0.3f * std::sin(a), 0},
               {-0.7f + 0.3f * std::cos(b), 0.3f * std::sin(b), 0}, 0.06f, Flame);
    }
    rod(v, {-0.4f, 0, 0}, {1.0f, 0, 0}, 0.07f, Flame);                 // the shaft
    rod(v, {0.75f, 0, 0}, {0.75f, -0.35f, 0}, 0.07f, Flame);           // two teeth
    rod(v, {0.95f, 0, 0}, {0.95f, -0.25f, 0}, 0.07f, Flame);
}
} // namespace

// ---------------------------------------------------------------------------
// Mesh: one vertex buffer on the graphics card
// ---------------------------------------------------------------------------

void Mesh::upload(const std::vector<Vertex>& vertices) {
    count = static_cast<GLsizei>(vertices.size());
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    // Shader inputs 0..3: position, normal, uv, material.
    const int sizes[] = {3, 3, 2, 1};
    const std::size_t offsets[] = {offsetof(Vertex, position), offsetof(Vertex, normal),
                                   offsetof(Vertex, uv), offsetof(Vertex, material)};
    for (GLuint i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<void*>(offsets[i]));
    }
    glBindVertexArray(0);
}

void Mesh::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, count);
    glBindVertexArray(0);
}

void Mesh::destroy() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    vbo = vao = 0;
    count = 0;
}

// ---------------------------------------------------------------------------
// The whole hall
// ---------------------------------------------------------------------------

void Hall::build() {
    Vertices v, front, light, marker, bars, goldenKey;
    box(v, {-108, -1, -20}, {108, 0, 20}, Floor);
    box(v, {-106, 0.025f, -3}, {88, 0.055f, 3}, Carpet);
    sideWall(v, -1);       // far wall: always drawn
    sideWall(front, 1);    // near wall: only with C (cutaway off)
    vault(v, front);   // lesson 16
    arcade(v, front);  // lesson 17
    // Lesson 17: a long banner in front of every arch, red and purple in turn.
    for (int i = 0; i + 1 < layout::columnsPerSide; ++i) {
        const Material cloth = i % 2 ? BannerPurple : BannerRed;
        hangingBanner(v,     {bayCenterX(i) - 2.0f, 55.0f, -9.4f}, {4, 0, 0}, 26.0f, 2.5f, cloth);
        hangingBanner(front, {bayCenterX(i) - 2.0f, 55.0f,  9.4f}, {4, 0, 0}, 26.0f, 2.5f, cloth);
    }
    chandelier(v);
    throne(v);
    gate(v, bars);
    keyShape(goldenKey);
    for (int i = 0; i < layout::columnsPerSide - 1; ++i) {
        const float x = bayCenterX(i);
        // Keep each shaft in its bay so its transparent plane does not slice a column.
        const float halfWidth = layout::lightShaftHalfWidth;
        quad(light, {x - halfWidth, 0.1f, 1}, {x + halfWidth, 0.1f, 1},
                    {x + 2.1f, 54.5f, -19.2f}, {x - 2.1f, 54.5f, -19.2f}, Beam);
    }
    // An original, deliberately simple knight silhouette for scale only.
    triangle(marker, {-0.32f, 0, 0.15f}, {0.32f, 0, 0.15f}, {0, 1.1f, 0.15f}, Banner);
    cylinder(marker, {0, 0.85f, 0}, 0.48f, 0.20f, 0.17f, Knight, 32);
    cylinder(marker, {0, 1.33f, 0}, 0.17f, 0.17f, 0.015f, Brass, 32);
    box(marker, {-0.16f, 1.11f, 0.173f}, {0.16f, 1.16f, 0.19f}, Darkness);
    rod(marker, {0.35f, 0.2f, 0}, {0.35f, 0.95f, 0}, 0.025f, Knight);
    architecture.upload(v);
    foreground.upload(front);
    lightShafts.upload(light);
    knight.upload(marker);
    grille.upload(bars);
    key.upload(goldenKey);
}

void Hall::destroy() {
    architecture.destroy();
    foreground.destroy();
    lightShafts.destroy();
    knight.destroy();
    grille.destroy();
    key.destroy();
}
}
