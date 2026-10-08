#include "material.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string readFile(const char* path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not read " << path << '\n';
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
}

GLuint compileShader(GLenum type, const char* filename) {
    std::string source = readFile(filename);
    if (source.empty()) return 0;

    GLuint shader = glCreateShader(type);
    const char* code = source.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, info);
        std::cerr << "Shader error in " << filename << ":\n" << info << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

GLuint createShaderProgram() {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, "shaders/stone.vert");
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, "shaders/stone.frag");
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[1024];
        glGetProgramInfoLog(program, 1024, nullptr, info);
        std::cerr << "Shader linking error:\n" << info << '\n';
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// Creates one 128 x 128 RGB brick-color texture in memory.
// No normal map, height map, external image loader, or POM is used.
GLuint createStoneTexture() {
    const int size = 128;
    std::vector<unsigned char> pixels(size * size * 3);

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int brickRow = y / 32;
            int shiftedX = (x + (brickRow % 2) * 32) % 64;
            bool mortar = (y % 32 < 3) || (shiftedX < 3);

            int shade = mortar ? 65 : 145 + ((x / 16 + y / 16) % 3) * 10;
            int index = (y * size + x) * 3;
            pixels[index]     = static_cast<unsigned char>(shade + 7);
            pixels[index + 1] = static_cast<unsigned char>(shade + 5);
            pixels[index + 2] = static_cast<unsigned char>(shade);
        }
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

// TODO for final: load real stone images and add advanced material effects.
