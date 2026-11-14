#include "garden_scene.h"
#include "garden_fountain.h"
#include "garden_plants.h"
#include "garden_primitives.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstddef>

namespace royal_garden {
namespace {

// ---------------------------------------------------------------------------
// Layout pieces
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

// A raised flowerbed from x0 to x1 (X) and z0 to z1 (Z, z0 < z1), framed by curbs
// and filled with a grid of flowers. `pathX` is the flower path's centre line: the
// colour of a column depends on its distance from that line, so two beds on both
// sides of the path come out as mirror images (a symmetric royal pattern).
void flowerbed(Vertices& soil, Vertices& plants, const PlantKit& kit,
               float x0, float x1, float z0, float z1, float pathX) {
    box(soil, {x0, 0, z0}, {x1, layout::bedTop, z1}, Soil);
    curbAlongX(soil, x0, x1, z0);
    curbAlongX(soil, x0, x1, z1);
    curbAlongZ(soil, x0, z0, z1);
    curbAlongZ(soil, x1, z0, z1);

    const float s = layout::flowerSpacing;
    const int columns = int((x1 - x0) / s), rows = int((z1 - z0) / s);
    for (int i = 0; i < columns; ++i) {
        for (int j = 0; j < rows; ++j) {
            const float x = x0 + s * (i + 0.5f), z = z0 + s * (j + 0.5f);
            // The seed comes from the flower's position, so it never changes between runs.
            const unsigned seed = unsigned(std::abs(x) * 100.0f) * 7919u + unsigned(std::abs(z) * 100.0f);
            // Jitter: move each flower up to 0.2 m off the exact grid, so the bed looks grown.
            const float jx = (random01(seed) - 0.5f) * 0.4f, jz = (random01(seed + 9) - 0.5f) * 0.4f;
            const int stripe = int(std::abs(x - pathX) / s);   // 0 = column nearest the path
            const FlowerColour colour = stripe % 2 == 0 ? FlowerColour::Lavender : FlowerColour::White;
            kit.flower(plants, glm::translate(glm::mat4(1), glm::vec3(x + jx, layout::bedTop, z + jz)),
                       colour, seed);
        }
    }
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

bool Garden::build() {
    Vertices v, green;   // v: stone and ground; green: everything that grows
    const PlantKit kit;  // builds every plant prototype once

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
    // The fountain itself: one instance transformation puts it on the plaza centre.
    fountain(v, T(layout::fountainX, layout::plazaTop, layout::plazaZ));

    // Zone C: the flower path, its round rondel, two mirrored flowerbeds,
    // and a ring of flowers round the rondel (the front, where the path arrives, stays open).
    const float fx = layout::flowerBranchX;
    branchPath(v, fx, layout::flowerBranchEnd);
    prism(v, {fx, 0, layout::rondelZ}, layout::rondelRadius, layout::areaTop, 16, PathStone);
    const float in = layout::bedGap, out = layout::bedGap + layout::bedWidth;
    flowerbed(v, green, kit, fx - out, fx - in, layout::bedBackZ, layout::bedFrontZ, fx);
    flowerbed(v, green, kit, fx + in, fx + out, layout::bedBackZ, layout::bedFrontZ, fx);
    for (int k = 0; k < 24; ++k) {
        const float angle = 2 * pi * k / 24;   // angle 0 = +X; pi/2 = +Z (towards the camera)
        if (std::abs(angle - pi / 2) < 0.6f) continue;
        const glm::vec3 p(fx + 4.0f * std::cos(angle), 0, layout::rondelZ + 4.0f * std::sin(angle));
        kit.flower(green, glm::translate(glm::mat4(1), p),
                   k % 2 ? FlowerColour::White : FlowerColour::Lavender, 500u + unsigned(k));
    }

    // Zone E: the bench path, a square of paving, and a low hedge round three sides.
    const float bxE = layout::benchBranchX, bh = layout::benchAreaHalfWidth;
    branchPath(v, bxE, layout::benchBranchEnd);
    box(v, {bxE - bh, 0, layout::benchAreaBackZ},
           {bxE + bh, layout::areaTop, layout::benchBranchEnd + 0.5f}, PathStone);
    const float hx0 = bxE - bh - 0.5f, hx1 = bxE + bh + 0.5f, hz = layout::benchAreaBackZ - 0.6f;
    kit.hedge(green, {hx0, -9.0f}, {hx0, hz}, 1.4f, 0.8f);   // left side
    kit.hedge(green, {hx0, hz}, {hx1, hz}, 1.4f, 0.8f);      // back
    kit.hedge(green, {hx1, hz}, {hx1, -9.0f}, 1.4f, 0.8f);   // right side

    // The back wall of the garden: a tall hedge across the whole strip.
    kit.hedge(green, {layout::startX, layout::backHedgeZ}, {layout::endX, layout::backHedgeZ}, 2.6f, 1.4f);

    // Zone D: ancient trees. ONE tree model, placed six times with different
    // instance transformations (position, turn, size).
    struct Spot { float x, z, turnDegrees, size; };
    const Spot trees[] = {
        {-46.0f, -20.0f,   0.0f, 1.10f},   // far left, near the entrance
        {-36.0f, -16.0f,  40.0f, 0.90f},   // behind the flower garden
        {-13.0f, -19.0f,  80.0f, 1.00f},   // the fountain's two guardians
        { 13.0f, -19.0f,  20.0f, 1.00f},
        { 35.0f, -17.0f,  60.0f, 0.95f},   // shade for the bench corner
        { 47.0f, -22.0f,  10.0f, 1.20f},   // far right
    };
    for (const Spot& s : trees)
        kit.tree(green, glm::translate(glm::mat4(1), glm::vec3(s.x, 0, s.z))
                      * glm::rotate(glm::mat4(1), glm::radians(s.turnDegrees), glm::vec3(0, 1, 0))
                      * glm::scale(glm::mat4(1), glm::vec3(s.size)));

    // Foreground bushes in front of the path (Z > 0): the nearest depth layer.
    // When the camera slides along X they pass faster than the trees: parallax.
    const float bushX[] = {-52.0f, -39.0f, -17.0f, 8.0f, 29.0f, 50.0f};
    for (int k = 0; k < 6; ++k) {
        const float size = 0.8f + 0.5f * random01(900u + unsigned(k));
        kit.bush(green, glm::translate(glm::mat4(1), glm::vec3(bushX[k], 0, 5.0f))
                      * glm::scale(glm::mat4(1), glm::vec3(size)), 900u + unsigned(k));
    }

    // Scale post: exactly as tall as the knight (1.5 m), standing on the walking line.
    box(v, {-0.2f, 0, layout::walkZ - 0.2f}, {0.2f, 1.5f, layout::walkZ + 0.2f}, Gold);

    ground.upload(v);
    plants.upload(green);
    return spray.init();
}
void Garden::update(float dt) {
    spray.update(dt);
}
// main.cpp has already chosen the garden shader and set its matrices for the solid
// meshes. The spray then switches to its own particle shader.
void Garden::draw(const glm::mat4& view, const glm::mat4& projection, int viewportHeight) const {
    ground.draw();
    plants.draw();
    spray.draw(view, projection, viewportHeight);   // last: see-through things after solid ones
}
void Garden::destroy() {
    ground.destroy();
    plants.destroy();
    spray.destroy();
}
}
