#include "garden_scene.h"
#include <cmath>
#include <cstddef>

namespace royal_garden {
namespace {
constexpr float pi = 3.14159265359f;
using Vertices = std::vector<Vertex>;

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

// Low stone borders. Each is a thin box centred on a line.
void curbAlongX(Vertices& v, float x0, float x1, float z) {
    const float h = layout::curbWidth * 0.5f;
    box(v, {x0, 0, z - h}, {x1, layout::curbHeight, z + h}, Curb);
}
void curbAlongZ(Vertices& v, float x, float z0, float z1) {
    const float h = layout::curbWidth * 0.5f;
    box(v, {x - h, 0, z0}, {x + h, layout::curbHeight, z1}, Curb);
}
void branchPath(Vertices& v, float x, float zEnd) {
    const float bw = layout::branchHalfWidth;
    const float pw = layout::pathHalfWidth;
    box(v, { x - bw, 0, zEnd }, { x + bw, layout::pathTop, -pw }, PathStone);
    curbAlongZ(v, x - bw, zEnd, -pw);
    curbAlongZ(v, x + bw, zEnd, -pw);
}
}
// A side path from the back edge of the main path (Z = -pathHalfWidth)
// into the depth, ending at zEnd, with a curb on each side.


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

    // Fountain side path: from the back of the main path into the plaza.
    const float bw = layout::branchHalfWidth;
    const float plazaFront = layout::plazaZ + layout::plazaRadius * std::cos(pi / 8);

    box(v, {layout::fountainX - bw, 0, plazaFront - 0.5f},
           {layout::fountainX + bw, layout::pathTop, -pw}, PathStone);
    curbAlongZ(v, layout::fountainX - bw, plazaFront, -pw);
    curbAlongZ(v, layout::fountainX + bw, plazaFront, -pw);

    // Fountain plaza: an octagonal stone platform. The fountain will stand at its centre.
    prism(v, {layout::fountainX, 0, layout::plazaZ}, layout::plazaRadius,
          layout::plazaTop, 8, PathStone);

    // Scale post: exactly as tall as the knight (1.5 m), standing on the walking line.
    box(v, {-0.2f, 0, layout::walkZ - 0.2f}, {0.2f, 1.5f, layout::walkZ + 0.2f}, Gold);
    ground.upload(v);

}
void Garden::draw() const {
    ground.draw();
}
void Garden::destroy() {
    ground.destroy();
}
}
