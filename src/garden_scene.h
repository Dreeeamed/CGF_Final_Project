#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// The Forgotten Royal Garden: a separate scene, independent of the hall and facade.
namespace royal_garden {

// Metres, same axes as the hall: X walking, Y height, Z depth (negative = farther).
namespace layout {
constexpr float startX = -60.0f;  // left end of the garden strip
constexpr float endX = 60.0f;     // right end
constexpr float backZ = -30.0f;   // farthest decorative layer
constexpr float frontZ = 8.0f;    // nearest foreground layer
constexpr float walkZ = 0.0f;     // the knight walks on this line
}

// Material IDs, read by shaders/garden.frag. Keep the numbers in sync with it.
enum Material { Moss, PathStone, Gold };

// Same layout as the hall's vertex, so both shaders read the same inputs.
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

struct Garden {
    Mesh ground;
    void build();
    void draw() const;
    void destroy();
};
}
