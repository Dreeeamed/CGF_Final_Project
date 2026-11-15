#pragma once
// The fountain's water as a particle system (L-10 Procedural Methods, Particle Systems).
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace royal_garden {

// Water spilling over the rim of the upper bowl and falling into the basin.
// Every frame (L-10 "For each frame..."): new drops are born, all drops move,
// drops that reach the basin water die, and the living ones are drawn.
class FountainSpray {
public:
    bool init();                       // shader, GPU buffer, and 2 s of warm-up
    void update(float dt);             // move the drops and send them to the GPU
    void draw(const glm::mat4& view, const glm::mat4& projection, int viewportHeight) const;
    void destroy();

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float brightness = 1.0f;
        bool alive = false;
    };
    void emit(int count);
    void simulate(float dt);
    void upload();

    std::vector<Particle> particles;
    std::size_t nextSlot = 0;   // where to look for a free particle (round robin)
    float emitDebt = 0.0f;      // fractions of a drop left over from earlier frames
    unsigned emitted = 0;       // counts every drop ever born: the seed of its "random" values
    GLsizei aliveCount = 0;
    GLuint program = 0, vao = 0, vbo = 0;
};
}
