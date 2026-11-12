#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
// On-screen text. Letters are built from small rectangles (stb_easy_font)
// and drawn over the finished 3D picture in pixels: (0,0) is the top-left corner.
class Hud {
public:
    bool init();
    void begin(int width, int height);   // switch to flat 2D drawing
    void rect(float x, float y, float w, float h, const glm::vec4& colour);
    void text(float x, float y, const std::string& s, const glm::vec4& colour);
    float width(const std::string& s);
    float height(const std::string& s);
    void end();                          // switch back to 3D drawing
    void destroy();
private:
    std::vector<float> letters(float x, float y, const std::string& s);
    void draw(const std::vector<float>& xy, const glm::vec4& colour);
    GLuint program = 0, vao = 0, vbo = 0;
    const float scale = 2.0f;//the font is tiny: 1 font pixel = 2 screen pixels
};