#include "castle_geometry.h"

#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

namespace {

// Only the three vertex attributes needed for basic texturing and lighting.
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

// A quad is two triangles: (a,b,c) and (a,c,d).
void addQuad(std::vector<Vertex>& vertices,
             glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
             float repeatU, float repeatV) {
    glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));

    vertices.push_back({a, normal, {0.0f,    0.0f}});
    vertices.push_back({b, normal, {repeatU, 0.0f}});
    vertices.push_back({c, normal, {repeatU, repeatV}});

    vertices.push_back({a, normal, {0.0f,    0.0f}});
    vertices.push_back({c, normal, {repeatU, repeatV}});
    vertices.push_back({d, normal, {0.0f,    repeatV}});
}

} // namespace

void CastleGeometry::build() {
    std::vector<Vertex> vertices;

    // Front wall. Three rectangles leave a visible doorway in the middle.
    addQuad(vertices, {-4, 0, 2}, {-1, 0, 2}, {-1, 3.5f, 2}, {-4, 3.5f, 2}, 3, 4);
    addQuad(vertices, { 1, 0, 2}, { 4, 0, 2}, { 4, 3.5f, 2}, { 1, 3.5f, 2}, 3, 4);
    addQuad(vertices, {-1, 2.4f, 2}, {1, 2.4f, 2}, {1, 3.5f, 2}, {-1, 3.5f, 2}, 2, 1);

    // Back wall and left/right side walls (all are flat quads in 3D space).
    addQuad(vertices, { 4, 0,-2}, {-4, 0,-2}, {-4, 3.5f,-2}, { 4, 3.5f,-2}, 8, 4);
    addQuad(vertices, {-4, 0,-2}, {-4, 0, 2}, {-4, 3.5f, 2}, {-4, 3.5f,-2}, 4, 4);
    addQuad(vertices, { 4, 0, 2}, { 4, 0,-2}, { 4, 3.5f,-2}, { 4, 3.5f, 2}, 4, 4);

    // Battlements: eight little rectangles on the front wall.
    for (int i = 0; i < 8; ++i) {
        float x = -4.0f + i;
        addQuad(vertices,
                {x, 3.5f, 2.01f}, {x + 0.65f, 3.5f, 2.01f},
                {x + 0.65f, 4.1f, 2.01f}, {x, 4.1f, 2.01f}, 1, 1);
    }

    // Ground plane. Its four corners are ordered so the normal points up.
    addQuad(vertices, {-7,-0.03f, 7}, {7,-0.03f, 7},
                      { 7,-0.03f,-7}, {-7,-0.03f,-7}, 8, 8);

    vertexCount = static_cast<GLsizei>(vertices.size());

    // Transfer vertices to the GPU once; the castle is not animated.
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2); // UV texture coordinates
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glBindVertexArray(0);
}

void CastleGeometry::draw() const {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void CastleGeometry::destroy() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    vbo = vao = 0;
    vertexCount = 0;
}

// TODO for final: add taller towers, a proper arched gate and other details.
