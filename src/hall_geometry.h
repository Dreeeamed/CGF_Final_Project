#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace grand_hall {

// Metres. X: length / walking; Y: height; Z: decorative depth.
namespace layout {
constexpr float length = 216.0f;
constexpr float width = 40.0f;
constexpr float height = 60.0f;
constexpr int columnsPerSide = 12;
constexpr float columnRadius = 4.9f;
constexpr float columnBaseRadius = columnRadius * (7.6f / 7.0f);
constexpr float columnCollarRadius = columnRadius * (7.12f / 7.0f);
constexpr float firstColumnX = -84.0f;
constexpr float lastColumnX = 84.0f;
// Keep the first and last columns in place as the number of columns changes.
constexpr float columnSpacing = (lastColumnX - firstColumnX) / (columnsPerSide - 1);
constexpr float wallPierHalfWidth = columnRadius * (6.4f / 7.0f);
constexpr float archHalfWidth = columnSpacing * 0.5f - wallPierHalfWidth;
constexpr float lightShaftHalfWidth = columnSpacing * 0.5f - columnBaseRadius - 0.15f;
constexpr float rowZ = 12.0f;
constexpr float galleryY = 24.0f;
constexpr float gateHeight = 9.0f;
constexpr float aisleWidth = 2.0f * (rowZ - columnRadius);
static_assert(columnsPerSide > 1, "At least two columns define the repeated bays");
static_assert(2.0f * (rowZ - columnBaseRadius) > 6.0f, "Keep the carpet clear of column bases");
static_assert(rowZ + columnBaseRadius < width * 0.5f, "Columns must fit inside the hall");
static_assert(columnSpacing > 2.0f * columnBaseRadius, "Column bases must not overlap");
static_assert(archHalfWidth > 2.9f, "The high windows and their rims must fit in each bay");
}

enum Material { Stone, Floor, Darkness, Banner, Carpet, Brass, Flame, Window, Door, Beam, Knight };

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    float material;
};

struct Mesh {
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
    void upload(const std::vector<Vertex>& vertices);
    void draw() const;
    void destroy();
};

struct Hall {
    Mesh architecture;
    Mesh foreground; // The complete second column row and its wall / gallery.
    Mesh lightShafts;
    Mesh knight;     // A 1.5 m scale marker, not the final character.
    void build();
    void destroy();
};
}
