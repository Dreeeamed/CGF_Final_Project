#include "garden_scene.h"
#include <cstddef>

namespace royal_garden {
namespace {
using Vertices = std::vector<Vertex>;

// Two triangles, a-b-c and a-c-d. Corners go counter-clockwise when seen
// from the front, so the cross product gives the outward normal.
void quad(Vertices& v, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
          Material m) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const float id = float(m);
    v.insert(v.end(), {
        {a, n, {0, 0}, id}, {b, n, {1, 0}, id}, {c, n, {1, 1}, id},
        {a, n, {0, 0}, id}, {c, n, {1, 1}, id}, {d, n, {0, 1}, id}});
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
}

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
