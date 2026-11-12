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

void column(Vertices& v, float x, float z) {
    // All horizontal radii shrink together; the supporting height stays at 44 m.
    const float radius = layout::columnRadius;
    const float baseRadius = layout::columnBaseRadius;
    const float collarRadius = layout::columnCollarRadius;
    cylinder(v, {x, 0, z},     0.7f,  baseRadius, baseRadius);   // plinth
    cylinder(v, {x, 0.7f, z},  0.7f,  baseRadius, radius);       // base moulding
    cylinder(v, {x, 1.4f, z},  40.4f, radius, radius);           // shaft
    cylinder(v, {x, 41.8f, z}, 1.4f,  radius, baseRadius);       // capital
    cylinder(v, {x, 43.2f, z}, 0.8f,  baseRadius, baseRadius);   // abacus
    // Narrow collars divide the huge vertical silhouette.
    cylinder(v, {x, 10, z}, 0.35f, collarRadius, collarRadius);
    cylinder(v, {x, 31, z}, 0.35f, collarRadius, collarRadius);
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

// A hanging banner on a rod: a cloth strip with a gentle wave and a pointed bottom.
void banner(Vertices& v, float x, float z) {
    rod(v, {x - 2.3f, 34, z}, {x + 2.3f, 34, z}, 0.12f);
    constexpr int panels = 24;
    for (int i = 0; i < panels; ++i) {
        const float u0 = float(i) / panels, u1 = float(i + 1) / panels;
        const float x0 = x - 1.8f + 3.6f * u0, x1 = x - 1.8f + 3.6f * u1;
        const float z0 = z + 0.18f * std::sin(u0 * pi * 6), z1 = z + 0.18f * std::sin(u1 * pi * 6);
        const float y0 = 19 + 1.5f * std::abs(2 * u0 - 1), y1 = 19 + 1.5f * std::abs(2 * u1 - 1);
        triangle(v, {x0, y0, z0}, {x1, y1, z1}, {x1, 33.7f, z1}, Banner, {u0, 0}, {u1, 0}, {u1, 1});
        triangle(v, {x0, y0, z0}, {x1, 33.7f, z1}, {x0, 33.7f, z0}, Banner, {u0, 0}, {u1, 1}, {u0, 1});
        // Cloth has two sides: the same two triangles again, corners in reverse order.
        triangle(v, {x0, y0, z0}, {x1, 33.7f, z1}, {x1, y1, z1}, Banner, {u0, 0}, {u1, 1}, {u1, 0});
        triangle(v, {x0, y0, z0}, {x0, 33.7f, z0}, {x1, 33.7f, z1}, Banner, {u0, 0}, {u0, 1}, {u1, 1});
    }
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
        banner(v, x, side * 8.7f);
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

void roof(Vertices& v, Vertices& foreground) {
    // Keep the spring above the clerestory windows (their tops are at 55 m).
    // This shallow barrel vault reaches exactly 60 m at the centre.
    for (int side : {-1, 1}) {
        // Like the side walls, the near half (side = +1) belongs to the cutaway foreground.
        Vertices& target = side < 0 ? v : foreground;
        for (int i = 0; i < 48; ++i) {
            const float t0 = i / 48.0f, t1 = (i + 1) / 48.0f;
            const float z0 = side * 20 * (1 - t0 * t0), z1 = side * 20 * (1 - t1 * t1);
            const float y0 = 56 + 4 * t0, y1 = 56 + 4 * t1;
            // One strip of the vault. Written for the far half, so the near half flips it.
            quad(target, {-108, y0, z0}, {108, y0, z0}, {108, y1, z1}, {-108, y1, z1}, Stone, side > 0);
            // End walls close both ends of the hall. Their top follows the vault curve,
            // so they never stick out above the roof. Two mirrors here: x and side.
            for (float x : {-108.0f, 108.0f})
                quad(target, {x, 0, z0}, {x, 0, z1}, {x, y1, z1}, {x, y0, z0}, Wall, x * side > 0);
            // Brass ribs under the vault, above every column.
            for (int col = 0; col < layout::columnsPerSide; ++col) {
                const float x = columnX(col);
                quad(target, {x - 0.5f, y0 - 0.35f, z0}, {x + 0.5f, y0 - 0.35f, z0},
                             {x + 0.5f, y1 - 0.35f, z1}, {x - 0.5f, y1 - 0.35f, z1}, Brass, side > 0);
            }
        }
    }
}

void chandelier(Vertices& v) {
    rod(v, {0, 59.8f, 0}, {0, 40, 0}, 0.12f);
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

void gate(Vertices& v) {
    // Side-composition entrance: the exterior transition will arrive here later.
    portal(v, -100, 0, 4.5f, 6, 3, -8, -1, 12, Door);
    // The gate stands away from the wall: give its deep sides an outer face too.
    for (int half : {-1, 1})
        box(v, {-100 + half * 4.5f - 0.35f, 0, -14}, {-100 + half * 4.5f + 0.35f, 6, -7.5f});
    // The grille: nine vertical bars, two cross bars and a middle bar.
    for (int i = -4; i <= 4; ++i)
        rod(v, {-100.0f + i, 0, -13.8f}, {-100.0f + i, 6.1f, -13.8f}, 0.06f, Brass);
    for (float y : {1.0f, 4.8f})
        rod(v, {-104.4f, y, -13.7f}, {-95.6f, y, -13.7f}, 0.09f, Brass);
    rod(v, {-100, 0, -13.7f}, {-100, layout::gateHeight, -13.7f}, 0.07f, Brass);
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
    Vertices v, front, light, marker;
    box(v, {-108, -1, -20}, {108, 0, 20}, Floor);
    box(v, {-106, 0.025f, -3}, {88, 0.055f, 3}, Carpet);
    sideWall(v, -1);       // far wall: always drawn
    sideWall(front, 1);    // near wall: only with C (cutaway off)
    roof(v, front);
    chandelier(v);
    throne(v);
    gate(v);
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
}

void Hall::destroy() {
    architecture.destroy();
    foreground.destroy();
    lightShafts.destroy();
    knight.destroy();
}
}
