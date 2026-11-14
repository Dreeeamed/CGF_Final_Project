#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "garden_particles.h"

// The Forgotten Royal Garden: a separate scene, independent of the hall and facade.
namespace royal_garden {

// Metres, same axes as the hall: X walking, Y height, Z depth (negative = farther).
namespace layout {
constexpr float startX = -60.0f;  // left end of the garden strip
constexpr float endX = 60.0f;     // right end
constexpr float backZ = -30.0f;   // farthest decorative layer
constexpr float frontZ = 8.0f;    // nearest foreground layer
constexpr float walkZ = 0.0f;     // the knight walks on this line

// Main path along X; the knight walks on its centre line.
constexpr float pathHalfWidth = 2.0f;
constexpr float pathTop = 0.05f;  // a few cm above the ground, so the two never z-fight
constexpr float curbWidth = 0.3f;
constexpr float curbHeight = 0.25f;

// Side paths leave the main path into the depth (-Z), one per garden zone.
constexpr float branchHalfWidth = 1.5f;
constexpr float flowerBranchX = -25.0f;  // Zone C: royal flower garden
constexpr float fountainX = 0.0f;        // Zone B: central fountain (focal point)
constexpr float benchBranchX = 22.0f;    // Zone E: quiet resting area
constexpr float branchX[] = {flowerBranchX, fountainX, benchBranchX};

// Fountain plaza: an octagon behind the main path.
constexpr float plazaZ = -12.0f;
constexpr float plazaRadius = 6.0f;
constexpr float plazaTop = 0.15f;

// Where each side path ends, and the small paved area waiting there.
constexpr float flowerBranchEnd = -10.0f;   // Zone C: a round "rondel" among the flowerbeds
constexpr float rondelZ = -13.0f;
constexpr float rondelRadius = 3.2f;
constexpr float benchBranchEnd = -8.0f;     // Zone E: a square of paving for benches
constexpr float benchAreaHalfWidth = 4.0f;
constexpr float benchAreaBackZ = -14.0f;
constexpr float areaTop = 0.1f;             // between pathTop and plazaTop: no z-fighting

// Zone C: two flowerbeds, mirror images on both sides of the flower path.
constexpr float bedFrontZ = -3.0f;
constexpr float bedBackZ = -9.0f;
constexpr float bedGap = 2.2f;              // from the flower path's centre line to a bed
constexpr float bedWidth = 6.0f;
constexpr float bedTop = 0.12f;             // raised soil
constexpr float flowerSpacing = 0.75f;      // metres between neighbouring flowers

// The garden's back wall: a tall clipped hedge across the whole strip.
constexpr float backHedgeZ = -27.0f;
}

// Material IDs, read by shaders/garden.frag. Keep the numbers in sync with it.
enum Material { Moss, PathStone, Gold, Curb, Bark, Leaf, LeafViolet,
                Soil, Stem, PetalLavender, PetalWhite, FlowerHeart, Hedge,
                Marble, Water, Crystal };

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
    Mesh ground;          // ground, paths, curbs, plaza, pavings and the fountain
    Mesh plants;          // trees, flowers, bushes and hedges
    FountainSpray spray;  // the fountain's falling water (particles)
    bool build();         // false if a garden shader could not be loaded
    void update(float dt);   // animation: called every frame while the garden is shown
    void draw(const glm::mat4& view, const glm::mat4& projection, int viewportHeight) const;
    void destroy();
};
}
