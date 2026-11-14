#include "garden_scene.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstddef>

namespace royal_garden {
namespace {
constexpr float pi = 3.14159265359f;
using Vertices = std::vector<Vertex>;

// ---------------------------------------------------------------------------
// Primitives: the small solids everything in the garden is built from
// ---------------------------------------------------------------------------

// One flat triangle. Corners go counter-clockwise when seen from the front,
// so the cross product gives the outward normal (right-hand rule).
void triangle(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, Material m,
              glm::vec2 ua = {0, 0}, glm::vec2 ub = {1, 0}, glm::vec2 uc = {0.5f, 1}) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const float id = float(m);
    v.insert(v.end(), {{a, n, ua, id}, {b, n, ub, id}, {c, n, uc, id}});
}

// Two triangles, a-b-c and a-c-d.
void quad(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          Material m) {
    triangle(v, a, b, c, m, {0, 0}, {1, 0}, {1, 1});
    triangle(v, a, c, d, m, {0, 0}, {1, 1}, {0, 1});
}

// An axis-aligned box from its minimum corner `lo` to its maximum corner `hi`.
void box(Vertices& v, glm::vec3 lo, glm::vec3 hi, Material m) {
    quad(v, {lo.x,lo.y,hi.z}, {hi.x,lo.y,hi.z}, {hi.x,hi.y,hi.z}, {lo.x,hi.y,hi.z}, m); // front +Z
    quad(v, {hi.x,lo.y,lo.z}, {lo.x,lo.y,lo.z}, {lo.x,hi.y,lo.z}, {hi.x,hi.y,lo.z}, m); // back  -Z
    quad(v, {lo.x,lo.y,lo.z}, {lo.x,lo.y,hi.z}, {lo.x,hi.y,hi.z}, {lo.x,hi.y,lo.z}, m); // left  -X
    quad(v, {hi.x,lo.y,hi.z}, {hi.x,lo.y,lo.z}, {hi.x,hi.y,lo.z}, {hi.x,hi.y,hi.z}, m); // right +X
    quad(v, {lo.x,hi.y,hi.z}, {hi.x,hi.y,hi.z}, {hi.x,hi.y,lo.z}, {lo.x,hi.y,lo.z}, m); // top   +Y
    quad(v, {lo.x,lo.y,lo.z}, {hi.x,lo.y,lo.z}, {hi.x,lo.y,hi.z}, {lo.x,lo.y,hi.z}, m); // bottom -Y
}

// A regular polygon pushed up into a solid: N sides, centre `base`, corner radius `radius`.
// Corner i sits at angle 2*pi*i/N (polar coordinates). The first corner is turned by
// half a step, so a flat side, not a sharp corner, faces the camera. No bottom face:
// prisms in the garden always stand on the ground.
void prism(Vertices& v, glm::vec3 base, float radius, float height, int sides, Material m) {
    const glm::vec3 top = base + glm::vec3(0, height, 0);
    for (int i = 0; i < sides; ++i) {
        const float a0 = 2 * pi * (i + 0.5f) / sides;
        const float a1 = 2 * pi * (i + 1.5f) / sides;
        const glm::vec3 r0(radius * std::cos(a0), 0, radius * std::sin(a0));
        const glm::vec3 r1(radius * std::cos(a1), 0, radius * std::sin(a1));
        quad(v, base + r1, base + r0, top + r0, top + r1, m); // side wall
        triangle(v, top, top + r1, top + r0, m);              // one "pie slice" of the top
    }
}

// A tapered cylinder standing on the origin and growing up local +Y:
// radius r0 at the bottom, r1 at the top. The side normals point straight out
// from the axis, so the light rolls smoothly round it (smooth shading, L-5-2).
void cylinder(Vertices& v, float height, float r0, float r1, int segments, Material m) {
    const float id = float(m);
    const glm::vec3 top(0, height, 0);
    for (int i = 0; i < segments; ++i) {
        const float a = 2 * pi * i / segments, b = 2 * pi * (i + 1) / segments;
        const glm::vec3 ra(std::cos(a), 0, std::sin(a)), rb(std::cos(b), 0, std::sin(b));
        const glm::vec3 p0 = r0 * ra, p1 = r0 * rb;           // bottom ring
        const glm::vec3 p3 = top + r1 * ra, p2 = top + r1 * rb; // top ring
        // Tilt the normal a little upwards when the cylinder narrows (a cone side).
        const glm::vec3 na = glm::normalize(ra + glm::vec3(0, (r0 - r1) / height, 0));
        const glm::vec3 nb = glm::normalize(rb + glm::vec3(0, (r0 - r1) / height, 0));
        const float u0 = float(i) / segments, u1 = float(i + 1) / segments;
        v.insert(v.end(), {{p0, na, {u0, 0}, id}, {p3, na, {u0, 1}, id}, {p2, nb, {u1, 1}, id},
                           {p0, na, {u0, 0}, id}, {p2, nb, {u1, 1}, id}, {p1, nb, {u1, 0}, id}});
        triangle(v, top, p2, p3, m);  // top cap
    }
}

// A sphere of radius r round the origin, made of `rings` x `segments` patches.
// A point is found from two angles: theta from the north pole down, phi round
// the vertical axis. On a sphere the normal is simply the direction from the centre.
void sphere(Vertices& v, float r, int rings, int segments, Material m) {
    const float id = float(m);
    auto dir = [&](int i, int j) {
        const float theta = pi * i / rings, phi = 2 * pi * j / segments;
        return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                         std::sin(theta) * std::sin(phi));
    };
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            const glm::vec3 a = dir(i, j), b = dir(i + 1, j), c = dir(i + 1, j + 1), d = dir(i, j + 1);
            // Order a-d-c and a-c-b is counter-clockwise seen from OUTSIDE the sphere.
            v.insert(v.end(), {{r * a, a, {0, 0}, id}, {r * d, d, {1, 0}, id}, {r * c, c, {1, 1}, id},
                               {r * a, a, {0, 0}, id}, {r * c, c, {1, 1}, id}, {r * b, b, {0, 1}, id}});
        }
    }
}

// Copy a part built in its OWN local coordinates into `out`, moved by the matrix m.
// This is the instance transformation of L-9 Hierarchical Modeling p3: the same
// symbol (part) placed many times. Points use m; normals use the inverse transpose
// of m, which keeps them perpendicular to the surface even after uneven scaling.
void place(Vertices& out, const Vertices& part, const glm::mat4& m) {
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(m)));
    for (Vertex p : part) {
        p.position = glm::vec3(m * glm::vec4(p.position, 1.0f));
        p.normal = glm::normalize(normalMatrix * p.normal);
        out.push_back(p);
    }
}

// ---------------------------------------------------------------------------
// Garden layout pieces
// ---------------------------------------------------------------------------

// Low stone borders. Each is a thin box centred on a line.
void curbAlongX(Vertices& v, float x0, float x1, float z) {
    const float h = layout::curbWidth * 0.5f;
    box(v, {x0, 0, z - h}, {x1, layout::curbHeight, z + h}, Curb);
}
void curbAlongZ(Vertices& v, float x, float z0, float z1) {
    const float h = layout::curbWidth * 0.5f;
    box(v, {x - h, 0, z0}, {x + h, layout::curbHeight, z1}, Curb);
}

// A side path from the back edge of the main path (Z = -pathHalfWidth)
// into the depth, ending at zEnd, with a curb on each side.
void branchPath(Vertices& v, float x, float zEnd) {
    const float bw = layout::branchHalfWidth;
    const float pw = layout::pathHalfWidth;
    box(v, {x - bw, 0, zEnd}, {x + bw, layout::pathTop, -pw}, PathStone);
    curbAlongZ(v, x - bw, zEnd, -pw);
    curbAlongZ(v, x + bw, zEnd, -pw);
}

// ---------------------------------------------------------------------------
// Zone D: an ancient tree, built as a hierarchy (L-9 Hierarchical Modeling)
// ---------------------------------------------------------------------------

// The shape of one kind of tree. Every number is relative to its parent part.
struct TreeShape {
    float trunkLength = 5.0f;   // metres, before the instance is scaled
    float trunkRadius = 0.35f;
    int levels = 3;             // trunk -> branches -> twigs -> leaf clusters
    int children = 3;           // branches growing from each tip
    float shrink = 0.68f;       // a child is this much shorter and thinner
    float spread = 38.0f;       // degrees between a child and its parent
};

// The prototype parts ("symbols"), built once in their own local coordinates
// and then placed many times with `place`.
struct TreeParts {
    Vertices limb;         // 1 m long, radius 1 at the bottom: scaled for each branch
    Vertices leaves;       // a unit sphere: scaled into a flat, drooping cluster
    Vertices violetLeaves; // the same, with the violet accent colour
};
TreeParts makeTreeParts(float shrink) {
    TreeParts parts;
    cylinder(parts.limb, 1.0f, 1.0f, shrink, 10, Bark);
    sphere(parts.leaves, 1.0f, 6, 10, Leaf);
    sphere(parts.violetLeaves, 1.0f, 6, 10, LeafViolet);
    return parts;
}

// One branch and everything that grows from it.
// `m` is the parent's matrix: it moves this branch's local coordinates into the
// world. Passing it BY VALUE to each child works like PushMatrix (the child gets
// its own copy); returning from the call is PopMatrix (L-9 p32-33, p41).
void growBranch(Vertices& v, const TreeParts& parts, const TreeShape& s,
                const glm::mat4& m, float length, float radius, int level, int index) {
    // 1. Draw this branch: the unit limb scaled to the right length and thickness.
    place(v, parts.limb, m * glm::scale(glm::mat4(1), glm::vec3(radius, length, radius)));

    // 2. The tip of this branch, in this branch's own coordinates: (0, length, 0).
    const glm::mat4 tip = m * glm::translate(glm::mat4(1), glm::vec3(0, length, 0));

    // 3. Leaf node of the tree data structure: a cluster of leaves, wide and flat,
    //    hanging a little below the tip. One cluster in three is violet.
    if (level == 0) {
        const glm::mat4 cluster = tip * glm::translate(glm::mat4(1), glm::vec3(0, -0.25f, 0))
                                      * glm::scale(glm::mat4(1), glm::vec3(1.5f, 0.9f, 1.5f));
        place(v, index % 3 == 2 ? parts.violetLeaves : parts.leaves, cluster);
        return;
    }

    // 4. Children: turn round the branch (Y), lean out (Z), and grow from the tip.
    for (int k = 0; k < s.children; ++k) {
        const float turn = 360.0f * k / s.children + 25.0f * level;   // vary each level
        const glm::mat4 child = tip
            * glm::rotate(glm::mat4(1), glm::radians(turn), glm::vec3(0, 1, 0))
            * glm::rotate(glm::mat4(1), glm::radians(s.spread), glm::vec3(0, 0, 1));
        growBranch(v, parts, s, child, length * s.shrink, radius * s.shrink, level - 1, k);
    }
}

// A whole tree. `instance` places it in the garden: translate, turn and scale.
void tree(Vertices& v, const TreeParts& parts, const TreeShape& s, const glm::mat4& instance) {
    growBranch(v, parts, s, instance, s.trunkLength, s.trunkRadius, s.levels, 0);
}
}

// ---------------------------------------------------------------------------
// Mesh: vertices on the GPU
// ---------------------------------------------------------------------------

void Mesh::upload(const std::vector<Vertex>& vertices) {
    count = static_cast<GLsizei>(vertices.size());
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    // Attribute i matches `layout(location = i)` in shaders/garden.vert.
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
// The garden
// ---------------------------------------------------------------------------

void Garden::build() {
    Vertices v;
    // Ground: one metre thick, its top surface at Y = 0 (same floor height as the hall).
    box(v, {layout::startX, -1, layout::backZ}, {layout::endX, 0, layout::frontZ}, Moss);

    // Main path: the whole garden length, centred on the walking line.
    const float pw = layout::pathHalfWidth;
    box(v, {layout::startX, 0, -pw}, {layout::endX, layout::pathTop, pw}, PathStone);
    // Front curb: one piece. Back curb: broken where each side path leaves.
    curbAlongX(v, layout::startX, layout::endX, pw);
    float x = layout::startX;
    for (float bx : layout::branchX) {
        curbAlongX(v, x, bx - layout::branchHalfWidth, -pw);
        x = bx + layout::branchHalfWidth;
    }
    curbAlongX(v, x, layout::endX, -pw);

    // Zone B: the fountain side path ends half a metre under the octagonal plaza,
    // so no gap can show between them. The fountain will stand at the plaza centre.
    const float plazaFront = layout::plazaZ + layout::plazaRadius * std::cos(pi / 8);
    branchPath(v, layout::fountainX, plazaFront - 0.5f);
    prism(v, {layout::fountainX, 0, layout::plazaZ}, layout::plazaRadius,
          layout::plazaTop, 8, PathStone);

    // Zone C: the flower path leads to a small round paved rondel.
    branchPath(v, layout::flowerBranchX, layout::flowerBranchEnd);
    prism(v, {layout::flowerBranchX, 0, layout::rondelZ}, layout::rondelRadius,
          layout::areaTop, 16, PathStone);

    // Zone E: the bench path leads to a square of paving for the resting area.
    branchPath(v, layout::benchBranchX, layout::benchBranchEnd);
    const float bh = layout::benchAreaHalfWidth;
    box(v, {layout::benchBranchX - bh, 0, layout::benchAreaBackZ},
           {layout::benchBranchX + bh, layout::areaTop, layout::benchBranchEnd + 0.5f}, PathStone);

    // Scale post: exactly as tall as the knight (1.5 m), standing on the walking line.
    box(v, {-0.2f, 0, layout::walkZ - 0.2f}, {0.2f, 1.5f, layout::walkZ + 0.2f}, Gold);
    ground.upload(v);

    // Zone D: ancient trees. ONE tree model, placed six times with different
    // instance transformations (position, turn, size).
    const TreeShape shape;
    const TreeParts parts = makeTreeParts(shape.shrink);
    struct Spot { float x, z, turnDegrees, size; };
    const Spot spots[] = {
        {-46.0f, -20.0f,   0.0f, 1.10f},   // far left, near the entrance
        {-36.0f, -16.0f,  40.0f, 0.90f},   // behind the flower garden
        {-13.0f, -19.0f,  80.0f, 1.00f},   // the fountain's two guardians
        { 13.0f, -19.0f,  20.0f, 1.00f},
        { 35.0f, -17.0f,  60.0f, 0.95f},   // shade for the bench corner
        { 47.0f, -22.0f,  10.0f, 1.20f},   // far right
    };
    Vertices treeVertices;
    for (const Spot& s : spots) {
        const glm::mat4 instance = glm::translate(glm::mat4(1), glm::vec3(s.x, 0, s.z))
            * glm::rotate(glm::mat4(1), glm::radians(s.turnDegrees), glm::vec3(0, 1, 0))
            * glm::scale(glm::mat4(1), glm::vec3(s.size));
        tree(treeVertices, parts, shape, instance);
    }
    trees.upload(treeVertices);
}
void Garden::draw() const {
    ground.draw();
    trees.draw();
}
void Garden::destroy() {
    ground.destroy();
    trees.destroy();
}
}
