#pragma once

#include <glad/glad.h>

GLuint createShaderProgram(const char* vertexPath = "shaders/stone.vert",
                           const char* fragmentPath = "shaders/stone.frag");
GLuint loadTexture2D(const char* path);
