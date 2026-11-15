#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// Lesson 20: dust in the air of the hall.
// Every mote is ONE point (GL_POINTS): the vertex shader moves it (it rises slowly,
// sways, twinkles) and sets its size in pixels; the fragment shader makes the square
// point a soft round dot. The points never change on the CPU: only `time` does.
class Dust {
public:
    bool init();
    void draw(const glm::mat4& view, const glm::mat4& projection, float time, int screenHeight);
    void destroy();

private:
    GLuint program = 0, vao = 0, vbo = 0;
    int count = 0;
};
