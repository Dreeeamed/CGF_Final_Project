#include "castle_geometry.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

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
void addArchFace(std::vector<Vertex>& vertices,
    glm::vec3 a, glm::vec3 b,
    glm::vec3 c, glm::vec3 d,
    bool mirrored, bool depthFace = false) {
    const std::size_t first = vertices.size();

    if (mirrored) {
        addQuad(vertices, d, c, b, a, 1.0f, 1.0f);
    }
    else {
        addQuad(vertices, a, b, c, d, 1.0f, 1.0f);
    }

    for (std::size_t i = first; i < vertices.size(); ++i) {
        const glm::vec3& p = vertices[i].position;

        if (depthFace) {
            vertices[i].uv = glm::vec2(p.z, p.y) * 0.5f;
        }
        else {
            vertices[i].uv = glm::vec2(p.x, p.y) * 0.5f;
        }
    }
}
// A box is defined by two opposite corners: minimum and maximum X, Y, Z.
// One texture tile spans two world units on every face.
void addBox(std::vector<Vertex>& vertices, glm::vec3 minimum, glm::vec3 maximum) {
    const float repeatX = (maximum.x - minimum.x) / 2.0f;
    const float repeatY = (maximum.y - minimum.y) / 2.0f;
    const float repeatZ = (maximum.z - minimum.z) / 2.0f;

    // Front (+Z).
    addQuad(vertices,
        {minimum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, maximum.z},
        {maximum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, maximum.z},
        repeatX, repeatY);

    // Back (-Z).
    addQuad(vertices,
        {maximum.x, minimum.y, minimum.z}, {minimum.x, minimum.y, minimum.z},
        {minimum.x, maximum.y, minimum.z}, {maximum.x, maximum.y, minimum.z},
        repeatX, repeatY);

    // Left (-X).
    addQuad(vertices,
        {minimum.x, minimum.y, minimum.z}, {minimum.x, minimum.y, maximum.z},
        {minimum.x, maximum.y, maximum.z}, {minimum.x, maximum.y, minimum.z},
        repeatZ, repeatY);

    // Right (+X).
    addQuad(vertices,
        {maximum.x, minimum.y, maximum.z}, {maximum.x, minimum.y, minimum.z},
        {maximum.x, maximum.y, minimum.z}, {maximum.x, maximum.y, maximum.z},
        repeatZ, repeatY);

    // Top (+Y).
    addQuad(vertices,
        {minimum.x, maximum.y, maximum.z}, {maximum.x, maximum.y, maximum.z},
        {maximum.x, maximum.y, minimum.z}, {minimum.x, maximum.y, minimum.z},
        repeatX, repeatZ);

    // Bottom (-Y).
    addQuad(vertices,
        {minimum.x, minimum.y, minimum.z}, {maximum.x, minimum.y, minimum.z},
        {maximum.x, minimum.y, maximum.z}, {minimum.x, minimum.y, maximum.z},
        repeatX, repeatZ);

}
} // namespace

void CastleGeometry::build() {
    std::vector<Vertex> vertices;
    const float gateHalfWidth = 2.0f;
    const float gateHeight = 6.0f;
    const float wallHeight = 12.0f;
    const float towerHeight = 18.0f;
    const float archRise = 3.0f;
    const float archThickness = 0.6f;
    const int archSegments = 32;


    // 1. Walkway: its top is at Y = 0, with a visible front edge.
    addBox(vertices, { -12.0f, -0.8f, -4.0f }, { 12.0f, 0.0f, 2.0f });

    // 2. Facade: two walls and a lintel leave a real opening in the middle.
    addBox(vertices, { -8.5f, 0.0f, -1.0f }, { -gateHalfWidth, wallHeight, 0.0f });
    addBox(vertices, { gateHalfWidth, 0.0f, -1.0f }, { 8.5f, wallHeight, 0.0f });


    // 3. Towers stand slightly closer to the camera than the facade.
    addBox(vertices, { -10.5f, 0.0f, -1.4f }, { -8.5f, towerHeight, 0.8f });
    addBox(vertices, { 8.5f, 0.0f, -1.4f }, { 10.5f, towerHeight, 0.8f });

    // 4. Projecting caps give the towers a clear silhouette.
    addBox(vertices, { -10.7f, towerHeight - 0.35f, -1.6f },
        { -8.3f, towerHeight + 0.2f, 1.0f });
    addBox(vertices, { 8.3f, towerHeight - 0.35f, -1.6f },
        { 10.7f, towerHeight + 0.2f, 1.0f });

    addBox(vertices,
        { -gateHalfWidth - archThickness, 0.0f, 0.0f },
        { -gateHalfWidth, gateHeight, 0.5f });

    addBox(vertices,
        { gateHalfWidth, 0.0f, 0.0f },
        { gateHalfWidth + archThickness, gateHeight, 0.5f });

    auto atZ = [](glm::vec3 p, float z) {
        p.z = z;
        return p;
        };

    for (int side : {-1, 1}) {
        const bool mirrored = side == 1;

        for (int i = 0; i < archSegments; ++i) {
            const float t0 = static_cast<float>(i) / archSegments;
            const float t1 = static_cast<float>(i + 1) / archSegments;

            glm::vec3 inner0(
                side * gateHalfWidth * (1.0f - t0 * t0),
                gateHeight + archRise * t0,
                0.5f);

            glm::vec3 inner1(
                side * gateHalfWidth * (1.0f - t1 * t1),
                gateHeight + archRise * t1,
                0.5f);

            glm::vec3 outer0(
                side * (gateHalfWidth + archThickness) * (1.0f - t0 * t0),
                gateHeight + (archRise + archThickness) * t0,
                0.5f);

            glm::vec3 outer1(
                side * (gateHalfWidth + archThickness) * (1.0f - t1 * t1),
                gateHeight + (archRise + archThickness) * t1,
                0.5f);

            glm::vec3 top0(inner0.x, wallHeight, 0.0f);
            glm::vec3 top1(inner1.x, wallHeight, 0.0f);

            // Передняя поверхность каменного обрамления.
            addArchFace(vertices,
                outer0, inner0, inner1, outer1,
                mirrored);

            // Стена над проходом: заполняет прежние просветы.
            addArchFace(vertices,
                atZ(inner0, 0.0f), atZ(inner1, 0.0f), top1, top0,
                mirrored);

            // Задняя поверхность стены.
            addArchFace(vertices,
                atZ(top0, -1.0f), atZ(top1, -1.0f),
                atZ(inner1, -1.0f), atZ(inner0, -1.0f),
                mirrored);

            // Внутренняя поверхность проёма: его глубина.
            addArchFace(vertices,
                inner0, atZ(inner0, -1.0f),
                atZ(inner1, -1.0f), inner1,
                mirrored, true);

            // Наружный край выступающего обрамления.
            addArchFace(vertices,
                outer0, outer1,
                atZ(outer1, 0.0f), atZ(outer0, 0.0f),
                mirrored, true);
        }
    }

    // Верхняя поверхность центральной части стены.
    addQuad(vertices,
        { -gateHalfWidth, wallHeight, 0.0f },
        { gateHalfWidth, wallHeight, 0.0f },
        { gateHalfWidth, wallHeight, -1.0f },
        { -gateHalfWidth, wallHeight, -1.0f },
        gateHalfWidth, 0.5f);
    //HallFloor
    addBox(vertices,
        { -5.5f, -0.8f, -36.0f },
        { 5.5f,  0.0f,  -4.0f });
    //HallWalls
    // Left wall.
    addBox(vertices,
        { -5.5f, 0.0f, -36.0f },
        { -5.0f, 14.0f, -4.0f });

    // Right wall.
    addBox(vertices,
        { 5.0f, 0.0f, -36.0f },
        { 5.5f, 14.0f, -4.0f });
    //distantWall
    addBox(vertices,
        { -5.5f,0.0f,-36.5f },
        { 5.5f,  14.0f,  -36.0f });
    //four pair of tall stone pillars
    for (int row = 0; row < 4; ++row) {
        float z = -6.0f - row * 7.0f;

        for (int side : {-1, 1}) {
            float x = side * 3.6f;

            // Wide base.
            addBox(vertices,
                { x - 0.6f, 0.0f, z - 0.6f },
                { x + 0.6f, 0.4f, z + 0.6f });

            // Slender shaft.
            addBox(vertices,
                { x - 0.3f, 0.4f, z - 0.3f },
                { x + 0.3f, 10.6f, z + 0.3f });

            // Projecting capital.
            addBox(vertices,
                { x - 0.5f, 10.6f, z - 0.5f },
                { x + 0.5f, 11.0f, z + 0.5f });
        }
    }
    vertexCount = static_cast<GLsizei>(vertices.size());

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
        vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
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

// Next: a pointed arch, an interior behind the opening and final stone materials.
