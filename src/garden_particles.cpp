#include "garden_particles.h"
#include "garden_fountain.h"
#include "material.h"
#include <cmath>
#include <cstddef>

namespace royal_garden {
namespace {
constexpr std::size_t maxParticles = 900;
constexpr float emitRate = 450.0f;   // new drops per second
constexpr float dropSize = 0.07f;    // diameter of a drop on screen, metres
const glm::vec3 gravity(0.0f, -9.81f, 0.0f);

// World heights, from the garden layout and the fountain's own sizes.
// The fountain stands on the plaza; its frames add up exactly as in fountain().
namespace f = fountain_size;
const glm::vec3 centre(layout::fountainX, 0.0f, layout::plazaZ);
const float rimY = layout::plazaTop + f::basinBaseY + f::pedestal + f::bowlRimTop;
const float basinWaterY = layout::plazaTop + f::basinBaseY + f::waterLevel;
}

bool FountainSpray::init() {
    program = createShaderProgram("shaders/garden_particles.vert", "shaders/garden_particles.frag");
    if (!program) return false;
    particles.resize(maxParticles);

    // One vec4 per living drop: xyz = position, w = brightness.
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(glm::vec4), nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec4),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);

    // Warm-up: 2 seconds of simulation, so the water already flows when the knight
    // arrives, and the smoke test always sees the same frozen picture.
    for (int i = 0; i < 120; ++i) simulate(1.0f / 60.0f);
    upload();
    return true;
}

// Particle generation (L-10 p17): each drop is born on the rim of the upper bowl.
// Its initial attributes (L-10 p18-20) come from random01, seeded with the drop's
// number, so the "random" spray is the same on every run.
void FountainSpray::emit(int count) {
    for (int n = 0; n < count; ++n) {
        // Find a dead particle to reuse; start where the last search stopped.
        std::size_t tries = 0;
        while (particles[nextSlot].alive && tries < particles.size()) {
            nextSlot = (nextSlot + 1) % particles.size();
            ++tries;
        }
        if (tries == particles.size()) return;   // every particle is in use
        Particle& p = particles[nextSlot];

        const unsigned seed = emitted++ * 4u;
        const float angle = 2.0f * pi * random01(seed);
        const glm::vec3 outward(std::cos(angle), 0.0f, std::sin(angle));
        p.position = centre + outward * f::bowlOuter + glm::vec3(0.0f, rimY, 0.0f);
        // Pushed outwards over the rim, sometimes a little upwards.
        p.velocity = outward * (0.45f + 0.35f * random01(seed + 1))
                   + glm::vec3(0.0f, 0.15f * random01(seed + 2), 0.0f);
        p.brightness = 0.6f + 0.4f * random01(seed + 3);
        p.alive = true;
    }
}

// One time step for every drop: Euler's method (L-10 p43) with gravity as the
// only force (L-10 p33, "Simple Forces"):
//     position(t + dt) = position(t) + dt * velocity(t)
//     velocity(t + dt) = velocity(t) + dt * acceleration
// Particle extinction (L-10 p22): a drop dies when it reaches the basin water.
void FountainSpray::simulate(float dt) {
    emitDebt += emitRate * dt;          // e.g. 450 drops/s * 0.016 s = 7.2 drops
    const int born = int(emitDebt);     // 7 now ...
    emitDebt -= float(born);            // ... and 0.2 saved for the next frame
    emit(born);

    for (Particle& p : particles) {
        if (!p.alive) continue;
        p.position += dt * p.velocity;
        p.velocity += dt * gravity;
        if (p.position.y < basinWaterY) p.alive = false;
    }
}

void FountainSpray::update(float dt) {
    simulate(dt);
    upload();
}

// Copy the living drops into the GPU buffer. GL_STREAM_DRAW tells OpenGL that
// the data changes every frame.
void FountainSpray::upload() {
    std::vector<glm::vec4> data;
    data.reserve(particles.size());
    for (const Particle& p : particles)
        if (p.alive) data.emplace_back(p.position, p.brightness);
    aliveCount = static_cast<GLsizei>(data.size());
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec4), data.data(), GL_STREAM_DRAW);
}

// Particle rendering (L-10 p23): every drop is one point, drawn as a small round,
// soft sprite. The drops are see-through and ADD their light (blending, L-7 Buffers):
// where many overlap, the stream looks brighter, like real falling water.
void FountainSpray::draw(const glm::mat4& view, const glm::mat4& projection,
                         int viewportHeight) const {
    if (!program || aliveCount == 0) return;
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, &projection[0][0]);
    // How many pixels a drop covers at 1 m from the camera. projection[1][1] is
    // 1 / tan(fovy / 2); the vertex shader divides this by the drop's distance.
    const float pointScale = dropSize * 0.5f * float(viewportHeight) * projection[1][1];
    glUniform1f(glGetUniformLocation(program, "pointScale"), pointScale);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // additive: colour is added to what is behind
    glDepthMask(GL_FALSE);               // drops are hidden by solid things, but hide nothing
    glEnable(GL_PROGRAM_POINT_SIZE);     // let the vertex shader set gl_PointSize
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, aliveCount);
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void FountainSpray::destroy() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    vbo = vao = program = 0;
}
}
