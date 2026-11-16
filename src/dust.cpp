#include "dust.h"
#include "material.h"
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <vector>

bool Dust::init(const Cloud& cloud) {
    program = createShaderProgram("shaders/dust.vert", "shaders/dust.frag");
    if (!program) return false;

    // One vec4 per mote: x, y, z = where it starts in the hall, w = its own random number.
    // A fixed seed (7) gives the same dust every run: the test pictures stay comparable.
    std::mt19937 random(7);
    std::uniform_real_distribution<float> x(cloud.lo.x, cloud.hi.x), y(cloud.lo.y, cloud.hi.y),
                                          z(cloud.lo.z, cloud.hi.z), w(0.0f, 1.0f);
    std::vector<glm::vec4> motes;
    for (int i = 0; i < cloud.count; ++i)
        motes.push_back({x(random), y(random), z(random), w(random)});
    count = int(motes.size());
    colour = cloud.colour;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, motes.size() * sizeof(glm::vec4), motes.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);   // dust.vert: layout(location = 0) in vec4 seed
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
    glBindVertexArray(0);
    return true;
}

void Dust::draw(const glm::mat4& view, const glm::mat4& projection, float time, int screenHeight) const {
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(program, "time"), time);
    glUniform3fv(glGetUniformLocation(program, "colour"), 1, glm::value_ptr(colour));
    // A mote 1 m away is this many pixels; the shader divides by the distance.
    glUniform1f(glGetUniformLocation(program, "pointScale"), float(screenHeight) * 0.33f);
    glEnable(GL_PROGRAM_POINT_SIZE);   // let the vertex shader set gl_PointSize
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, count);
    glBindVertexArray(0);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void Dust::destroy() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
}
