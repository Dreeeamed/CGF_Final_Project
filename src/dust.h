#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// Lesson 20: dust in the air of the hall.
// Every mote is ONE point (GL_POINTS): the vertex shader moves it (it rises slowly,
// sways, twinkles) and sets its size in pixels; the fragment shader makes the square
// point a soft round dot. The points never change on the CPU: only `time` does.
class Dust {
public:
    // Where the motes start, how many there are and their colour. The defaults are the
    // hall's dust; the garden makes its own cloud (royal_garden::Garden).
    struct Cloud {
        glm::vec3 lo{-106.0f, 0.5f, -18.0f}, hi{106.0f, 52.0f, 18.0f};
        int count = 1200;
        glm::vec3 colour{1.0f, 0.62f, 0.30f};   // warm candle-lit dust
    };
    bool init(const Cloud& cloud = Cloud());
    void draw(const glm::mat4& view, const glm::mat4& projection, float time, int screenHeight) const;
    void destroy();

private:
    GLuint program = 0, vao = 0, vbo = 0;
    int count = 0;
    glm::vec3 colour{1.0f};
};
