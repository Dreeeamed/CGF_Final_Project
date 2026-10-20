#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "castle_geometry.h"
#include "material.h"

int main(int argc, char* argv[]) {
    const bool smokeTest = argc == 2 && std::string(argv[1]) == "--smoke-test";
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << '\n';
    });
    if (!glfwInit()) {
        std::cout << "Could not start GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(900, 650, "Simple Castle - Midterm",
                                          nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST); // The closest surface appears in front.
    glfwSwapInterval(1);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    GLuint program = createShaderProgram();
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    GLuint texture = createStoneTexture();
    CastleGeometry castle;
    castle.build();

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "stoneTexture"), 0);

    // Small orbit camera: A/D rotate, W/S move closer or farther away.
    float angle = 0.0f;
    float distance = 11.0f;
    float previousTime = static_cast<float>(glfwGetTime());

    std::cout << "A/D: rotate camera | W/S: zoom | Escape: quit\n";
    int renderedFrames = 0;
    int exitCode = 0;

    while (!glfwWindowShouldClose(window)) {
        float time = static_cast<float>(glfwGetTime());
        float dt = std::min(time - previousTime, 0.05f);
        previousTime = time;
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) angle -= dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) angle += dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) distance -= dt * 4.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) distance += dt * 4.0f;
        distance = std::clamp(distance, 6.0f, 18.0f);

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (height == 0) continue;
        glViewport(0, 0, width, height);
        glClearColor(0.48f, 0.66f, 0.82f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 cameraPos(distance * std::sin(angle), 3.0f,
                            distance * std::cos(angle));
        glm::mat4 model(1.0f);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 1.7f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(glm::radians(60.0f),
                               static_cast<float>(width) / height, 0.1f, 100.0f);

        glUseProgram(program);
        glUniformMatrix4fv(glGetUniformLocation(program, "model"), 1,
                           GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1,
                           GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1,
                           GL_FALSE, glm::value_ptr(projection));
        glUniform3f(glGetUniformLocation(program, "lightPos"), -3.0f, 7.0f, 6.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        castle.draw();
        glfwSwapBuffers(window);
        if (smokeTest) {
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL smoke test failed: " << error << '\n';
                exitCode = 1;
                break;
            }
            if (++renderedFrames == 3) {
                std::cout << "Smoke test passed: 3 frames rendered.\n";
                break;
            }
        }
    }

    castle.destroy();
    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}

// TODO for final: add free-look mouse controls and collision detection.
