#include "hud.h"
#include "material.h"
#include "stb_easy_font.h"
#include <glm/gtc/type_ptr.hpp>

bool Hud::init() {
    program = createShaderProgram("shaders/text.vert", "shaders/text.frag");
    if (!program) return false;
    // The buffer starts empty: the letters are rebuilt every frame (the FPS changes).
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);   // x, y, x, y, ...
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return true;
}

void Hud::begin(int width, int height) {
    glDisable(GL_DEPTH_TEST);                   // text is always on top of the castle
    cullingWasOn = glIsEnabled(GL_CULL_FACE);   // remember it, end() puts it back
    glDisable(GL_CULL_FACE);                    // flipping y reverses the corner order of every letter
    glEnable(GL_BLEND);                         // so the dark panel can be half transparent
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "screenSize"), float(width), float(height));
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
}

// Upload the points and draw them as triangles in one colour.
void Hud::draw(const std::vector<float>& xy, const glm::vec4& colour) {
    glUniform4fv(glGetUniformLocation(program, "colour"), 1, glm::value_ptr(colour));
    glBufferData(GL_ARRAY_BUFFER, xy.size() * sizeof(float), xy.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(xy.size() / 2));
}

// A rectangle is two triangles.
void Hud::rect(float x, float y, float w, float h, const glm::vec4& colour) {
    draw({x, y,  x + w, y,  x + w, y + h,     x, y,  x + w, y + h,  x, y + h}, colour);
}

// Turn a string into triangles: stb_easy_font gives rectangles, we cut each into two.
std::vector<float> Hud::letters(float x, float y, const std::string& s) {
    struct Vertex { float x, y, z; unsigned char rgba[4]; };   // the format stb_easy_font writes
    std::vector<Vertex> quads(s.size() * 40 + 16);             // ~17 points per letter, with room to spare
    std::string copy = s;                                      // stb wants char*, not const char*
    const int count = stb_easy_font_print(0, 0, copy.data(), nullptr, quads.data(),
                                          int(quads.size() * sizeof(Vertex)));
    std::vector<float> xy;
    const int corners[6] = {0, 1, 2, 0, 2, 3};                 // one rectangle = two triangles
    for (int q = 0; q < count; q++) {
        for (int c : corners) {
            xy.push_back(x + quads[q * 4 + c].x * scale);
            xy.push_back(y + quads[q * 4 + c].y * scale);
        }
    }
    return xy;
}

// Text with a black shadow 2 pixels down-right, readable on any background.
void Hud::text(float x, float y, const std::string& s, const glm::vec4& colour) {
    draw(letters(x + scale, y + scale, s), glm::vec4(0, 0, 0, colour.a));   // shadow
    draw(letters(x, y, s), colour);
}

float Hud::width(const std::string& s) {
    std::string copy = s;
    return stb_easy_font_width(copy.data()) * scale;
}

float Hud::height(const std::string& s) {
    std::string copy = s;
    return stb_easy_font_height(copy.data()) * scale;
}

// Put back what begin() changed, so the next 3D frame is drawn correctly.
void Hud::end() {
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    if (cullingWasOn) glEnable(GL_CULL_FACE);
}

void Hud::destroy() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
}
