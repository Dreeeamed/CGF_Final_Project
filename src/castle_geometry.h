#pragma once

#include <glad/glad.h>

class CastleGeometry {
public:
    void build();
    void draw() const;
    void destroy();

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
};
