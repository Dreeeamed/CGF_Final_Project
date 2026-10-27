#include "material.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
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

GLuint createShaderProgram(const char* vertexPath, const char* fragmentPath) {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexPath);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentPath);
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

// Load an image as RGBA and generate mipmaps for distant surfaces.
GLuint loadTexture2D(const char* path) {
    int width, height, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "Could not load texture: " << path << '\n';
        std::cerr << stbi_failure_reason() << '\n';
        return 0;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}
// TODO for final: use the height map for parallax occlusion mapping.
