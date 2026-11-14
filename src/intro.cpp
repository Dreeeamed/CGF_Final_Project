// The intro: the first scene of the program. A castle far away across a plain, a trail that
// leads to its gate, and a START button. The whole picture is drawn by shaders/intro.frag;
// this file feeds it the camera and runs the button.
#include "intro.h"
#include "material.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace {

float smoothEase(float x) { return x * x * (3.0f - 2.0f * x); }

// How far the mouse can move the eye sideways (metres) and up / down.
// The castle is 140 m away and the near trees are 13 m away, so 6 m of camera motion moves
// a near tree across a third of the screen and the castle by only a few pixels.
const float mouseSwingX = 6.0f;
const float mouseSwingY = 0.6f;

} // namespace

bool Intro::init() {
    program = createShaderProgram("shaders/intro.vert", "shaders/intro.frag");
    if (!program) return false;
    locResolution = glGetUniformLocation(program, "resolution");
    locTime = glGetUniformLocation(program, "time");
    locCamPos = glGetUniformLocation(program, "camPos");
    locFade = glGetUniformLocation(program, "fade");
    // The big triangle has no vertex data, but a core OpenGL context still wants a vertex array.
    glGenVertexArrays(1, &vao);
    return true;
}

void Intro::reset() {
    starting = false;
    fade = 0.0f;
    camZ = 0.0f;
    pressedOnButton = false;
}

// The START button: centred, in the lower part of the window. It grows with the window.
Intro::Rect Intro::buttonRect(int width, int height) const {
    const float k = std::clamp(float(height) / 900.0f, 0.7f, 2.0f);
    const float w = 300.0f * k, h = 84.0f * k;
    return {(float(width) - w) * 0.5f, float(height) * 0.76f, w, h};
}

void Intro::update(GLFWwindow* window, float dt) {
    clock += dt;

    // ---- Mouse, in framebuffer pixels (on a high-DPI screen the window is smaller) ----
    int winW, winH, fbW, fbH;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (winW <= 0 || winH <= 0 || fbW <= 0 || fbH <= 0) return;
    double cursorX, cursorY;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    mouseX = int(cursorX * fbW / winW);
    mouseY = int(cursorY * fbH / winH);

    // ---- Camera: the mouse moves the eye, and the eye also sways on its own ----
    // The target is the mouse position across the window, -1..1. We follow it smoothly:
    // every frame the eye closes part of the gap, so the motion is soft, not jerky.
    const float targetX = std::clamp(float(mouseX) / fbW * 2.0f - 1.0f, -1.0f, 1.0f);
    const float targetY = std::clamp(float(mouseY) / fbH * 2.0f - 1.0f, -1.0f, 1.0f);
    const float follow = 1.0f - std::exp(-3.0f * dt);
    lookX += (targetX - lookX) * follow;
    lookY += (targetY - lookY) * follow;
    camX = lookX * mouseSwingX + std::sin(clock * 0.35f) * 2.2f;
    camY = 1.7f - lookY * mouseSwingY + std::sin(clock * 0.27f) * 0.2f;

    // ---- The button ----
    const Rect b = buttonRect(fbW, fbH);
    hovered = !starting && mouseX >= b.x && mouseX <= b.x + b.w && mouseY >= b.y && mouseY <= b.y + b.h;
    const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mouseDown && !mouseWasDown) pressedOnButton = hovered;   // the click must START on the button...
    if (!mouseDown && mouseWasDown) {
        if (pressedOnButton && hovered) starting = true;         // ...and END on it: then it counts
        pressedOnButton = false;
    }
    mouseWasDown = mouseDown;
    // Enter does the same, for the keyboard.
    const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    if (enterDown && !enterWasDown && !starting) starting = true;
    enterWasDown = enterDown;

    // ---- After START: the parallax keeps going while we walk towards the castle and fade out ----
    if (starting) {
        fade = std::min(fade + dt / 1.0f, 1.0f);
        camZ = -45.0f * smoothEase(fade);
    }
}

void Intro::drawScene(int width, int height) {
    const GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasOn = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);   // one flat picture: nothing to sort
    glDisable(GL_CULL_FACE);

    glUseProgram(program);
    glUniform2f(locResolution, float(width), float(height));
    glUniform1f(locTime, clock);
    glUniform3f(locCamPos, camX, camY, camZ);
    glUniform1f(locFade, smoothEase(fade));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (depthWasOn) glEnable(GL_DEPTH_TEST);
    if (cullWasOn) glEnable(GL_CULL_FACE);
}

void Intro::drawButton(Hud& hud, int width, int height) {
    const float k = std::clamp(float(height) / 900.0f, 0.7f, 2.0f);
    const float visible = 1.0f - smoothEase(fade);   // everything fades out with the picture
    const glm::vec4 parchment(0.93f, 0.86f, 0.70f, visible);
    const glm::vec4 gold(1.0f, 0.70f, 0.28f, visible);
    const float oldScale = hud.getScale();

    hud.begin(width, height);

    // Title.
    const std::string title = "THE CASTLE";
    hud.setScale(7.0f * k);
    hud.text((float(width) - hud.width(title)) * 0.5f, float(height) * 0.10f, title, parchment);
    const std::string subtitle = "Topic 15: 2.5D castle with parallax occlusion on walls";
    hud.setScale(2.0f * k);
    hud.text((float(width) - hud.width(subtitle)) * 0.5f, float(height) * 0.10f + 90.0f * k, subtitle, gold);

    // Button: a dark plate with a golden frame; it lights up under the mouse.
    const Rect b = buttonRect(width, height);
    const float border = std::max(2.0f, 3.0f * k);
    const bool down = pressedOnButton && hovered;
    const glm::vec4 plate = down    ? glm::vec4(0.55f, 0.30f, 0.08f, 0.92f * visible)
                          : hovered ? glm::vec4(0.40f, 0.22f, 0.06f, 0.88f * visible)
                                    : glm::vec4(0.03f, 0.05f, 0.09f, 0.72f * visible);
    hud.rect(b.x - 4.0f * k, b.y - 4.0f * k, b.w + 8.0f * k, b.h + 8.0f * k, glm::vec4(0.0f, 0.0f, 0.0f, 0.35f * visible));
    hud.rect(b.x + border, b.y + border, b.w - 2 * border, b.h - 2 * border, plate);   // the plate
    hud.rect(b.x, b.y, b.w, border, gold);                                              // the golden frame:
    hud.rect(b.x, b.y + b.h - border, b.w, border, gold);                               // four thin bars
    hud.rect(b.x, b.y + border, border, b.h - 2 * border, gold);
    hud.rect(b.x + b.w - border, b.y + border, border, b.h - 2 * border, gold);
    const std::string label = "START";
    hud.setScale(5.0f * k);
    hud.text(b.x + (b.w - hud.width(label)) * 0.5f, b.y + (b.h - hud.height(label)) * 0.5f, label,
             hovered ? glm::vec4(1.0f, 0.93f, 0.75f, visible) : parchment);

    // Small hint under the button.
    const std::string hint = "move the mouse to look around   |   click START or press Enter";
    hud.setScale(2.0f * k);
    hud.text((float(width) - hud.width(hint)) * 0.5f, b.y + b.h + 28.0f * k, hint, parchment);

    hud.end();
    hud.setScale(oldScale);   // the rest of the program expects the normal text size
}

void Intro::destroy() {
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    vao = program = 0;
}
