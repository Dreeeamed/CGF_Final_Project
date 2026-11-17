// The intro: the first scene of the program. A castle far away across a plain, a trail that
// leads to its gate, the knight standing on the trail, and a START button.
// The whole picture, knight included, is drawn by shaders/intro.frag as 2.5D cut-outs;
// this file moves the knight and the camera and runs the button.
#include "intro.h"
#include "material.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace {

const float pi = 3.14159265359f;

float smoothEase(float x) { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
float smoothRange(float a, float b, float x) { return smoothEase((x - a) / (b - a)); }

// ---- Timing of the walk (seconds). Change these to make the intro slower or faster. ----
const float turnSeconds = 1.3f;          // after START the knight turns round to face the castle
const float walkSeconds = 16.0f;         // from the first step to the gate
const float rampSeconds = 2.2f;          // he speeds up at the start and slows down at the gate
const float cameraBlendSeconds = 2.8f;   // the camera glides from the idle view to behind him
const float fadeSeconds = 2.0f;          // the picture fades to black as he reaches the gate
const float buttonFadeSeconds = 0.6f;    // the title and the button disappear
const float skipFadeSeconds = 0.9f;      // Enter / click during the walk

// ---- Where things are (metres along the trail, from the origin towards the castle) ----
const float knightStartS = 11.5f;        // he idles here: behind the grass, before the near trees
const float gateS = 137.0f;              // he stops just in front of the gate (the castle is at 140)

// ---- The follow camera: it only slides, it never turns ----
const float followBack = 6.0f;           // metres behind the knight
const float followHoldBack = 8.0f;       // near the gate it stops and lets him walk on alone
const float followHeight = 1.7f;         // eye height, like the idle camera (the knight is 1.5 m)

// How far the mouse can move the eye sideways (metres) and up / down.
// The castle is 140 m away and the near trees are 13 m away, so 6 m of camera motion moves
// a near tree across a third of the screen and the castle by only a few pixels.
const float mouseSwingX = 6.0f;
const float mouseSwingY = 0.6f;

// The trail. The same curve as pathX() in shaders/intro.frag: keep the two in step.
float pathX(float s) {
    const float amp = 10.0f * smoothRange(0.0f, 40.0f, s) * (1.0f - smoothRange(60.0f, 140.0f, s));
    return amp * std::sin(0.06f * s);
}
glm::vec3 trailPoint(float s) { return {pathX(s), 0.0f, -s}; }

// Distance and speed along the walk at time t (0..total): speed up for `ramp` seconds,
// walk evenly, slow down for the last `ramp` seconds. The speed never jumps.
void walkProfile(float t, float total, float ramp, float length, float& distance, float& speed) {
    t = std::clamp(t, 0.0f, total);
    const float vMax = length / (total - ramp);   // so that the whole walk covers `length`
    if (t < ramp) {
        speed = vMax * t / ramp;
        distance = 0.5f * vMax * t * t / ramp;
    } else if (t < total - ramp) {
        speed = vMax;
        distance = 0.5f * vMax * ramp + vMax * (t - ramp);
    } else {
        const float left = total - t;
        speed = vMax * left / ramp;
        distance = length - 0.5f * vMax * left * left / ramp;
    }
}

} // namespace

bool Intro::init() {
    program = createShaderProgram("shaders/intro.vert", "shaders/intro.frag");
    if (!program) return false;
    locResolution = glGetUniformLocation(program, "resolution");
    locTime = glGetUniformLocation(program, "time");
    locCamPos = glGetUniformLocation(program, "camPos");
    locCamTarget = glGetUniformLocation(program, "camTarget");
    locFade = glGetUniformLocation(program, "fade");
    locKnightPos = glGetUniformLocation(program, "knightPos");
    locKnightPose = glGetUniformLocation(program, "knightPose");
    locKnightStride = glGetUniformLocation(program, "knightStride");
    // The big triangle has no vertex data, but a core OpenGL context still wants a vertex array.
    glGenVertexArrays(1, &vao);
    reset();
    return true;
}

void Intro::reset() {
    starting = false;
    skipping = false;
    sinceStart = 0.0f;
    skipFade = 0.0f;
    fade = 0.0f;
    pressedOnButton = false;
    knightS = cameraS = knightStartS;
    knightSpeed = 0.0f;
    walkAmount = 0.0f;
    stepPhase = 0.0f;
}

// The START button: centred, in the lower part of the window. It grows with the window.
Intro::Rect Intro::buttonRect(int width, int height) const {
    const float k = std::clamp(float(height) / 900.0f, 0.7f, 2.0f);
    const float w = 300.0f * k, h = 84.0f * k;
    return {(float(width) - w) * 0.5f, float(height) * 0.79f, w, h};   // below the knight
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

    // The mouse position across the window, -1..1, followed smoothly: every frame the eye
    // closes part of the gap, so the motion is soft, not jerky.
    const float targetX = std::clamp(float(mouseX) / fbW * 2.0f - 1.0f, -1.0f, 1.0f);
    const float targetY = std::clamp(float(mouseY) / fbH * 2.0f - 1.0f, -1.0f, 1.0f);
    const float follow = 1.0f - std::exp(-3.0f * dt);
    lookX += (targetX - lookX) * follow;
    lookY += (targetY - lookY) * follow;

    // ---- The button: a click must START and END on it. Enter does the same. ----
    // During the walk, Enter or a click skips to the end.
    const Rect b = buttonRect(fbW, fbH);
    hovered = !starting && mouseX >= b.x && mouseX <= b.x + b.w && mouseY >= b.y && mouseY <= b.y + b.h;
    const bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mouseDown && !mouseWasDown) pressedOnButton = hovered;
    if (!mouseDown && mouseWasDown) {
        if (starting) skipping = true;
        else if (pressedOnButton && hovered) starting = true;
        pressedOnButton = false;
    }
    mouseWasDown = mouseDown;
    const bool enterDown = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                           glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    if (enterDown && !enterWasDown) {
        if (starting) skipping = true;
        else starting = true;
    }
    enterWasDown = enterDown;

    if (starting) sinceStart += dt;
    updateKnight(dt);
    updateCamera(dt);

    // ---- Fade to black: as the knight reaches the gate, or at once when skipping ----
    const float fadeFrom = turnSeconds + walkSeconds - fadeSeconds;
    float walkFade = starting ? std::clamp((sinceStart - fadeFrom) / fadeSeconds, 0.0f, 1.0f) : 0.0f;
    if (skipping) skipFade = std::min(skipFade + dt / skipFadeSeconds, 1.0f);
    fade = std::max(walkFade, skipFade);
}

void Intro::updateKnight(float dt) {
    // Where he is on the trail, and how fast he goes.
    float walked = 0.0f, speed = 0.0f;
    if (starting)
        walkProfile(sinceStart - turnSeconds, walkSeconds, rampSeconds, gateS - knightStartS, walked, speed);
    knightS = knightStartS + walked;
    knightSpeed = speed;

    // Which side of the cut-out we see. A flat figure turns like a card: it gets thinner,
    // is edge-on for a moment, then shows its back. facing = cos(turn angle).
    float facing;
    if (!starting) {
        // Idle: now and then a slow glance to the side (the figure narrows a little).
        const float glance = 0.45f * std::sin(clock * 0.45f) * std::sin(clock * 0.17f + 1.0f);
        facing = std::cos(glance);
    } else {
        facing = std::cos(pi * smoothEase(sinceStart / turnSeconds));
    }

    // The walk cycle: one foot lifts while the other carries him, the body dips and rocks a little.
    // One step = pi radians of stepPhase.
    const bool turning = starting && sinceStart < turnSeconds;
    const float stepsPerSecond = std::max(turning ? 1.8f : 0.0f, std::min(speed / 2.4f, 3.0f));
    stepPhase += dt * pi * stepsPerSecond;
    const float walkTarget = std::clamp(std::max(speed / 2.0f, turning ? 0.7f : 0.0f), 0.0f, 1.0f);
    walkAmount += (walkTarget - walkAmount) * (1.0f - std::exp(-6.0f * dt));

    const float idle = 1.0f - walkAmount;
    const float bob = 0.03f * std::abs(std::sin(stepPhase)) * walkAmount;                  // rises over each step
    const float roll = 0.03f * std::sin(stepPhase) * walkAmount                             // rocks onto the standing leg
                     + 0.02f * std::sin(clock * 0.8f) * idle;                               // idle sway
    const float breathe = 1.0f + 0.012f * std::sin(clock * 1.9f) * idle;                    // idle breathing
    knightPose = glm::vec4(facing, bob, roll, breathe);
    knightStride = std::sin(stepPhase) * walkAmount;
}

void Intro::updateCamera(float dt) {
    // The idle view: the mouse moves the eye and the eye sways on its own, looking at the castle.
    const glm::vec3 idlePos(lookX * mouseSwingX + std::sin(clock * 0.35f) * 2.2f,
                            1.7f - lookY * mouseSwingY + std::sin(clock * 0.27f) * 0.2f, 0.0f);
    const glm::vec3 idleTarget(0.0f, 21.0f, -140.0f);

    // The follow view: behind the knight on the trail. It chases a point that lags a little
    // behind him, so it lets him pull away when he speeds up and catches up when he slows.
    // Near the gate it holds back, so he walks the last stretch into the gate alone.
    if (!starting) cameraS = knightS;
    cameraS += (knightS - cameraS) * (1.0f - std::exp(-4.0f * dt));
    const float behind = cameraS - followBack - followHoldBack * smoothRange(105.0f, 137.0f, knightS);
    const glm::vec3 followPos(pathX(behind), followHeight + 0.04f * std::sin(clock * 0.9f), -behind);

    // 2.5D: the camera never turns. It always looks the same way as the idle camera looks
    // from the origin: straight down the plain to the castle. Only its position changes.
    const glm::vec3 idleDir = glm::normalize(idleTarget - idlePos);
    const glm::vec3 followDir = glm::normalize(idleTarget - glm::vec3(0.0f, 1.7f, 0.0f));

    const float blend = starting ? smoothEase(sinceStart / cameraBlendSeconds) : 0.0f;
    camPos = idlePos + (followPos - idlePos) * blend;
    camTarget = camPos + glm::normalize(idleDir + (followDir - idleDir) * blend) * 100.0f;
}

void Intro::drawScene(int width, int height) {
    const GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasOn = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);   // one flat picture: nothing to sort
    glDisable(GL_CULL_FACE);

    glUseProgram(program);
    glUniform2f(locResolution, float(width), float(height));
    glUniform1f(locTime, clock);
    glUniform3fv(locCamPos, 1, glm::value_ptr(camPos));
    glUniform3fv(locCamTarget, 1, glm::value_ptr(camTarget));
    glUniform1f(locFade, smoothEase(fade));
    const glm::vec3 feet = trailPoint(knightS);
    glUniform3fv(locKnightPos, 1, glm::value_ptr(feet));
    glUniform4fv(locKnightPose, 1, glm::value_ptr(knightPose));
    glUniform1f(locKnightStride, knightStride);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    if (depthWasOn) glEnable(GL_DEPTH_TEST);
    if (cullWasOn) glEnable(GL_CULL_FACE);
}

void Intro::drawButton(Hud& hud, int width, int height) {
    const float k = std::clamp(float(height) / 900.0f, 0.7f, 2.0f);
    const float oldScale = hud.getScale();
    const float visible = starting ? 1.0f - smoothEase(sinceStart / buttonFadeSeconds) : 1.0f;
    hud.begin(width, height);

    if (visible > 0.0f) {
        const glm::vec4 parchment(0.93f, 0.86f, 0.70f, visible);
        const glm::vec4 gold(1.0f, 0.70f, 0.28f, visible);

        // Title.
        const std::string title = "THE CASTLE";
        hud.setScale(7.0f * k);
        hud.text((float(width) - hud.width(title)) * 0.5f, float(height) * 0.10f, title, gold);
        const std::string subtitle = "Topic 15: 2.5D castle with parallax occlusion on walls";
        hud.setScale(2.0f * k);
        hud.text((float(width) - hud.width(subtitle)) * 0.5f, float(height) * 0.10f + 90.0f * k, subtitle, parchment);
        // The story in one line, and who made it.
        const std::string story = "The castle has slept for a hundred years. One knight comes back to wake its heart.";
        hud.text((float(width) - hud.width(story)) * 0.5f, float(height) * 0.10f + 122.0f * k, story,
                 glm::vec4(0.80f, 0.66f, 1.0f, visible));
        const std::string team = "Akezhan Muzamelov      Bolatbek Bolat      Adil Dyussenov";
        hud.text((float(width) - hud.width(team)) * 0.5f, float(height) - 56.0f * k, team, gold);
        const std::string school = "Astana IT University - Computer Graphics Fundamentals, 2026";
        hud.text((float(width) - hud.width(school)) * 0.5f, float(height) - 30.0f * k, school, parchment);

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
    }

    // During the walk: a quiet reminder that it can be skipped.
    if (starting && !skipping) {
        const float a = 0.55f * smoothRange(1.0f, 2.0f, sinceStart) * (1.0f - smoothEase(fade * 3.0f));
        if (a > 0.0f) {
            const std::string skip = "Enter or click - skip";
            hud.setScale(2.0f * k);
            hud.text(float(width) - hud.width(skip) - 32.0f * k, float(height) - 48.0f * k, skip,
                     glm::vec4(0.93f, 0.86f, 0.70f, a));
        }
    }

    hud.end();
    hud.setScale(oldScale);   // the rest of the program expects the normal text size
}

void Intro::destroy() {
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program);
    vao = program = 0;
}
