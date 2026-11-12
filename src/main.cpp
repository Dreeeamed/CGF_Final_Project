// The castle demo: window, controls, camera and the frame loop.
//
// Every frame does the same five things:
//   1. read the keyboard,
//   2. move the knight, the camera and the light,
//   3. draw the hall (or the facade),
//   4. draw the on-screen text over it,
//   5. show the picture.
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "castle_geometry.h"
#include "hall_geometry.h"
#include "hud.h"
#include "material.h"

namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

void setMatrix(GLuint program, const char* name, const glm::mat4& matrix) {
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

// Save the real OpenGL picture (before the buffers are swapped) as a PPM image.
bool captureFrame(const std::filesystem::path& path, int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << "P6\n" << width << ' ' << height << "\n255\n";
    // OpenGL stores the bottom row first; image files start with the top row.
    for (int y = height - 1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(pixels.data() + y * width * 3), width * 3);
    return bool(file);
}

// A key that acts ONCE per press: holding it down does not repeat the action.
// It remembers whether the key was down in the previous frame.
struct KeyPress {
    int key;
    bool wasDown = false;

    bool justPressed(GLFWwindow* window) {
        const bool down = glfwGetKey(window, key) == GLFW_PRESS;
        const bool pressedNow = down && !wasDown;
        wasDown = down;
        return pressedNow;
    }
};

// Q cycles the POM quality: how many layers the ray march and the shadow walk may use.
struct Quality {
    const char* name;
    float minLayers;   // looking straight at the wall
    float maxLayers;   // looking along the wall
    int shadowSteps;
};
const Quality qualities[] = {
    {"Low",     8.0f, 24.0f,  8},
    {"Medium", 12.0f, 48.0f, 16},
    {"High",   24.0f, 96.0f, 32},
};

// ---------------------------------------------------------------------------
// Texts
// ---------------------------------------------------------------------------

const char* consoleHelp =
    "F1 exterior | F2 hall | A/D walk | Shift faster | W/S zoom\n"
    "Up/Down look | C full hall / cutaway | O overview | P parallax | Home reset | Esc quit\n"
    "J/L turn the light | I/K raise / lower the light\n"
    "Q quality (Low / Medium / High) | V VSync on / off\n"
    "H help on screen | Space pause | B back faces (culling off, back sides red)\n"
    "Tab free camera: W/S/A/D fly, R/F up / down, arrows or right mouse look, Shift faster\n";

// The on-screen help is two columns, because the font is not monospaced:
// line N of `helpKeys` belongs to line N of `helpActions`.
const char* helpKeys =
    "A / D\nShift\nW / S\nUp / Down\nO\nC\nHome\nF1 / F2\n"
    "P\nQ\nJ / L\nI / K\nSpace\nB\nTab\nV\nH\nEsc";
const char* helpActions =
    "walk left / right\nwalk faster\nzoom in / out\nlook higher / lower\n"
    "overview of the hall\nfull hall / cutaway\nreset the camera\nexterior / hall\n"
    "parallax occlusion on / off\nPOM quality: Low / Medium / High\nturn the light\n"
    "raise / lower the light\npause animation\nback-face culling on / off\n"
    "free camera: WASD fly, R / F up / down, arrows or right mouse look\n"
    "VSync on / off\nhide this help\nquit";

// The window title: frame rate and switches, updated twice a second.
std::string titleText(float fps, bool parallaxOn, const Quality& quality, bool vsync) {
    char title[160];
    std::snprintf(title, sizeof(title), "Castle - %.0f FPS (%.1f ms) | POM %s | Quality %s | VSync %s",
                  fps, 1000.0f / fps, parallaxOn ? "ON" : "OFF", quality.name, vsync ? "on" : "off");
    return title;
}

// Top-left panel: topic, team, switches and frame rate.
std::string infoText(bool parallaxOn, const Quality& quality, bool vsync, bool paused, bool freeCamera, bool hasKey, float fps) {
    char status[160];
    std::snprintf(status, sizeof(status),
                  "POM: %s   Quality: %s   VSync: %s%s%s\n%.0f FPS (%.1f ms) Key: %s   H - help",
                  parallaxOn ? "ON" : "OFF", quality.name, vsync ? "on" : "off",
                  paused ? "   PAUSED" : "",freeCamera ? "   FREE CAMERA" : "", fps, fps > 0.0f ? 1000.0f / fps : 0.0f,
                  hasKey ? "found" : "not found");
    return std::string("Topic 15: 2.5D castle with parallax occlusion on walls\n")
         + "Akezhan Muzamelov, Bolatbek Bolat, Adil Dyussenov\n" + status;
}

// Lesson 9: text over the picture - who we are, what is switched on, how to play.
void drawHud(Hud& hud, int width, int height, const std::string& info, bool showHelp, bool culling) {
    const glm::vec4 panel(0.02f, 0.03f, 0.05f, 0.65f);
    const glm::vec4 parchment(0.93f, 0.86f, 0.70f, 1.0f);
    const glm::vec4 gold(1.0f, 0.70f, 0.28f, 1.0f);
    const glm::vec4 warning(1.0f, 0.35f, 0.3f, 1.0f);
    const std::string& hint;
    hud.begin(width, height);
    hud.rect(16, 16, hud.width(info) + 32, hud.height(info) + 24, panel);
    hud.text(32, 28, info, parchment);

    if (showHelp) {
        const float top = hud.height(info) + 56;
        const float column = hud.width(helpKeys) + 32;
        hud.rect(16, top, column + hud.width(helpActions) + 32, hud.height(helpKeys) + 24, panel);
        hud.text(32, top + 12, helpKeys, gold);
        hud.text(32 + column, top + 12, helpActions, parchment);
    }
    if (!culling)
        hud.text(32, float(height) - 48, "CULLING OFF: red = back side of a triangle", warning);
    if (!hint.empty()) {
        // Centred near the bottom: (screen width - text width) / 2.
        const float x = (float(width) - hud.width(hint)) * 0.5f;
        const float y = float(height) - 120.0f;
        hud.rect(x - 16, y - 12, hud.width(hint) + 32, hud.height(hint) + 16, panel);
        hud.text(x, y, hint, gold);
    }
    hud.end();
}

} // namespace
// Lesson 11: the free-fly camera looks along this direction.
// yaw turns left / right, pitch looks up / down (radians).
// yaw = 0, pitch = 0 looks along -Z: from the camera side into the hall.
// Lesson 12: what the knight can do right here, shown at the bottom of the screen.
std::string questHint(bool nearKey, bool nearGate, bool hasKey, float gateLift, bool inGarden) {
    if (inGarden) return "You are in the inner garden.   E - back to the hall";
    if (nearKey) return "A golden key at the foot of the throne.   E - take it";
    if (nearGate) {
        if (!hasKey) return "The gate is locked. The key lies at the foot of the throne.";
        if (gateLift == 0.0f) return "E - open the gate with the key";
        if (gateLift < 1.0f) return "The gate is rising...";
        return "E - enter the inner garden";
    }
    return "";
}
glm::vec3 directionFromAngles(float yaw, float pitch) {
    return glm::vec3(std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        -std::cos(pitch) * std::cos(yaw));
}

int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // Command line: --smoke-test draws a few frames in a hidden window and quits;
    // --capture-dir <dir> also saves six fixed views as images.
    // -----------------------------------------------------------------------
    const bool capture = argc == 3 && std::string(argv[1]) == "--capture-dir";
    const bool smokeTest = capture || (argc == 2 && std::string(argv[1]) == "--smoke-test");
    const std::filesystem::path captureDir = capture ? argv[2] : "";
    if (capture) {
        std::error_code error;
        std::filesystem::create_directories(captureDir, error);
        if (error) { std::cerr << "Could not create capture directory.\n"; return 1; }
    }

    // -----------------------------------------------------------------------
    // Window and OpenGL 3.3 core
    // -----------------------------------------------------------------------
    glfwSetErrorCallback([](int code, const char* description) {
        std::cerr << "GLFW error " << code << ": " << description << '\n';
    });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (smokeTest) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1440, 900, "Castle - The Grand Hall (2.5D)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);   // skip the back side of every triangle (front = counter-clockwise)
    glfwSwapInterval(smokeTest ? 0 : 1);
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << '\n';

    // -----------------------------------------------------------------------
    // Shaders, textures and geometry
    // -----------------------------------------------------------------------
    GLuint stoneProgram = createShaderProgram();
    GLuint hallProgram = createShaderProgram("shaders/hall.vert", "shaders/hall.frag");
    GLuint texture = loadTexture2D("assets/textures/stone_wall/albedo.png");
    GLuint heightMap = loadTexture2D("assets/textures/stone_wall/height.png");
    GLuint normalMap = loadTexture2D("assets/textures/stone_wall/normal.png");
    if (!stoneProgram || !hallProgram || !texture || !heightMap || !normalMap) {
        glDeleteProgram(stoneProgram); glDeleteProgram(hallProgram);
        glDeleteTextures(1, &texture); glDeleteTextures(1, &heightMap); glDeleteTextures(1, &normalMap);
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    CastleGeometry castle;
    castle.build();
    grand_hall::Hall hall;
    hall.build();
    // Texture unit numbers: 0 colour, 1 height map, 2 normal map.
    for (GLuint program : {stoneProgram, hallProgram}) {
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "stoneTexture"), 0);
        glUniform1i(glGetUniformLocation(program, "heightMap"), 1);
        glUniform1i(glGetUniformLocation(program, "normalMap"), 2);
    }
    Hud hud;
    if (!hud.init()) {
        std::cerr << "Could not create the on-screen text.\n";
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }

    // -----------------------------------------------------------------------
    // State that changes while the program runs
    // -----------------------------------------------------------------------
    // Scene and camera. Start at the entrance gate: the end wall is seen at a
    // grazing angle, so the POM shows at once.
    bool insideHall = true;       // F1 / F2: facade or hall
    bool showForeground = false;  // C: draw the near half of the hall too
    bool overview = false;        // O: the whole hall from far away
    float knightX = -98.0f;       // the knight walks along X
    float cameraX = -78.0f;       // the camera follows him smoothly
    float distance = 40.0f;       // how far the camera is from the centre line
    float lookHeight = 10.0f;     // the height the camera looks at
    // Lesson 12, the quest: take the key at the throne, open the gate, enter the garden.
    const float keyX = 80.0f;     // the key floats at the foot of the throne
    const float gateX = -100.0f;  // the entrance gate, where the knight starts
    bool hasKey = false;
    bool gateOpening = false;     // set once, when the key opens the gate
    float gateLift = 0.0f;        // 0 = closed, 1 = fully raised
    bool inGarden = false;        // where the knight wants to be
    float gardenWalk = 0.0f;      // 0 = on the carpet, 1 = inside the gate

    // Free-fly camera (Tab): its own position and look angles.
    bool freeCamera = false;
    glm::vec3 freePos(0.0f);
    float freeYaw = 0.0f, freePitch = 0.0f;
    double lastMouseX = 0.0, lastMouseY = 0.0;   // to measure how far the mouse moved

    // Switches.
    bool parallaxOn = true;       // P: the headline technique on / off, for comparing
    int quality = 1;              // Q: index into `qualities`, starts at Medium
    bool vsync = !smokeTest;      // V
    bool showHelp = false;        // H
    bool paused = false;          // Space
    bool culling = true;          // B

    // The light: yaw turns it around the hall, pitch raises it (radians).
    float lightYaw = glm::radians(60.0f);
    float lightPitch = glm::radians(25.0f);

    // Clocks and counters.
    float previousTime = static_cast<float>(glfwGetTime());
    float animTime = 0.0f;   // clock of the animations; stands still while paused
    float shownFps = 0.0f;   // last measured FPS, for the on-screen text
    int fpsFrames = 0;
    float fpsTime = 0.0f;
    int renderedFrames = 0;
    int exitCode = 0;

    KeyPress keyC{GLFW_KEY_C}, keyO{GLFW_KEY_O}, keyP{GLFW_KEY_P}, keyQ{GLFW_KEY_Q},
             keyV{GLFW_KEY_V}, keyH{GLFW_KEY_H}, keySpace{GLFW_KEY_SPACE}, keyB{GLFW_KEY_B}, keyTab{ GLFW_KEY_TAB };
    const char* captures[] = {"hall-centre.ppm", "hall-overview.ppm", "hall-entrance.ppm",
                              "hall-throne.ppm", "hall-full-row.ppm", "exterior.ppm","hall-closeup.ppm" };
    std::cout << consoleHelp;

    while (!glfwWindowShouldClose(window)) {
        // ---- 1. Time ------------------------------------------------------
        const float time = static_cast<float>(glfwGetTime());
        const float frameTime = time - previousTime;    // real time of this frame
        const float dt = std::min(frameTime, 0.05f);    // for movement: no big jumps after a pause
        previousTime = time;
        glfwPollEvents();
        auto pressed = [&](int key) { return glfwGetKey(window, key) == GLFW_PRESS; };

        // ---- 2. Keys that switch something --------------------------------
        if (pressed(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);
        if (pressed(GLFW_KEY_F1) && insideHall) {
            insideHall = false; overview = false;freeCamera = false;
            cameraX = 0; distance = 14; lookHeight = 3.5f;
        }
        if (pressed(GLFW_KEY_F2) && !insideHall) {
            insideHall = true;
            cameraX = knightX; distance = 84; lookHeight = 24;
        }
        if (keyC.justPressed(window)) showForeground = !showForeground;
        if (keyO.justPressed(window) && insideHall) {
            overview = !overview;
            distance = overview ? 154.0f : 84.0f;
            lookHeight = overview ? 26.0f : 24.0f;
        }
        if (keyP.justPressed(window)) {
            parallaxOn = !parallaxOn;
            std::cout << "Parallax " << (parallaxOn ? "ON" : "OFF") << '\n';
        }
        if (keyQ.justPressed(window)) {
            quality = (quality + 1) % 3;
            const Quality& q = qualities[quality];
            std::cout << "Quality " << q.name << ": " << q.minLayers << "-" << q.maxLayers
                << " layers, " << q.shadowSteps << " shadow steps\n";
        }
        if (keyV.justPressed(window)) {
            vsync = !vsync;
            glfwSwapInterval(vsync ? 1 : 0);
            std::cout << "VSync " << (vsync ? "ON" : "OFF") << '\n';
        }
        if (keyH.justPressed(window)) showHelp = !showHelp;
        if (keySpace.justPressed(window)) paused = !paused;
        if (keyB.justPressed(window)) {
            culling = !culling;
            if (culling) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        }
        if (keyTab.justPressed(window) && insideHall) {
            freeCamera = !freeCamera;
            if (freeCamera) {
                // Start inside the hall, above the 2.5D camera's spot, looking at what it looked at.
                freePos = glm::vec3(cameraX, lookHeight + 12.0f, 16.0f);
                const glm::vec3 look = glm::normalize(glm::vec3(cameraX, lookHeight, 0) - freePos);
                freeYaw = std::atan2(look.x, -look.z);
                freePitch = std::asin(look.y);
                glfwGetCursorPos(window, &lastMouseX, &lastMouseY);
            }
        }
        if (pressed(GLFW_KEY_HOME) && insideHall) {
            overview = false; showForeground = false;
            distance = 84; lookHeight = 24;
        }
        if (!paused) animTime += dt;

        // ---- 3. Light (J/L turn, I/K raise) -------------------------------
        // Keys held down: +1, -1 or 0, times the speed, times the frame time.
        lightYaw += (float(pressed(GLFW_KEY_L)) - float(pressed(GLFW_KEY_J))) * dt * 1.2f;
        lightPitch += (float(pressed(GLFW_KEY_I)) - float(pressed(GLFW_KEY_K))) * dt * 0.8f;
        lightPitch = std::clamp(lightPitch, 0.05f, 1.5f);
        // Angles -> direction (spherical coordinates): towards the light.
        const glm::vec3 lightDir(std::cos(lightPitch) * std::sin(lightYaw),
            std::sin(lightPitch),
            std::cos(lightPitch) * std::cos(lightYaw));

        // ---- 4. Frame rate in the window title, twice a second -------------
        fpsFrames++;
        fpsTime += frameTime;
        if (fpsTime >= 0.5f) {
            shownFps = fpsFrames / fpsTime;
            glfwSetWindowTitle(window, titleText(shownFps, parallaxOn, qualities[quality], vsync).c_str());
            fpsFrames = 0;
            fpsTime = 0.0f;
        }

        // ---- 5. Knight and camera -----------------------------------------
        const float direction = float(pressed(GLFW_KEY_D)) - float(pressed(GLFW_KEY_A));
        if (freeCamera) {
            // Look around: arrow keys, or drag with the right mouse button.
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                freeYaw += float(mouseX - lastMouseX) * 0.004f;
                freePitch -= float(mouseY - lastMouseY) * 0.004f;   // screen y goes down
            }
            lastMouseX = mouseX;
            lastMouseY = mouseY;
            freeYaw += (float(pressed(GLFW_KEY_RIGHT)) - float(pressed(GLFW_KEY_LEFT))) * dt * 1.5f;
            freePitch += (float(pressed(GLFW_KEY_UP)) - float(pressed(GLFW_KEY_DOWN))) * dt * 1.0f;
            freePitch = std::clamp(freePitch, -1.5f, 1.5f);   // never straight up: lookAt needs a clear "up"

            // Fly: W/S along the view, A/D sideways, R/F straight up and down.
            const glm::vec3 forward = directionFromAngles(freeYaw, freePitch);
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
            const float ahead = float(pressed(GLFW_KEY_W)) - float(pressed(GLFW_KEY_S));
            const float rise = float(pressed(GLFW_KEY_R)) - float(pressed(GLFW_KEY_F));
            float speed = pressed(GLFW_KEY_LEFT_SHIFT) ? 18.0f : 7.0f;
            if (gardenWalk > 0.0f) speed = 0.0f;   // walking through the gate: A/D do nothing
            freePos += (forward * ahead + right * direction + glm::vec3(0, rise, 0)) * speed * dt;
            // Stay inside the hall: the walls have no outer side to look at.
            freePos = glm::clamp(freePos, glm::vec3(-106, 1, -18), glm::vec3(106, 58, 18));
        }else if (insideHall) {
            const float speed = pressed(GLFW_KEY_LEFT_SHIFT) ? 18.0f : 7.0f;
            knightX = std::clamp(knightX + direction * speed * dt, -105.0f, 87.0f);
            // The camera does not jump to the knight; every frame it closes part of the gap.
            const float targetX = overview ? 0.0f : std::clamp(knightX, -78.0f, 78.0f);
            cameraX += (targetX - cameraX) * (1.0f - std::exp(-5.0f * dt));
        } else {
            cameraX = std::clamp(cameraX + direction * 3.0f * dt, -11.0f, 11.0f);
        }
        // In the free camera W/S and the arrows fly and look, so they do not zoom here.
        const float zoom = freeCamera ? 0.0f : float(pressed(GLFW_KEY_S)) - float(pressed(GLFW_KEY_W));
        const float tilt = freeCamera ? 0.0f : float(pressed(GLFW_KEY_UP)) - float(pressed(GLFW_KEY_DOWN));
        distance += zoom * dt * (insideHall ? 28.0f : 4.0f);
        lookHeight += tilt * dt * (insideHall ? 12.0f : 6.0f);
        distance = insideHall ? std::clamp(distance, 24.0f, 160.0f) : std::clamp(distance, 6.0f, 18.0f);
        lookHeight = insideHall ? std::clamp(lookHeight, 0.0f, 52.0f) : std::clamp(lookHeight, -5.0f, 25.0f);

        // Smoke test: fixed views, so the saved images can be compared between versions.
        if (smokeTest) {
            insideHall = true; showForeground = false; freeCamera = false;
            hasKey = false; gateLift = 0.0f; inGarden = false; gardenWalk = 0.0f;
            cameraX = 0; knightX = -6; distance = 84; lookHeight = 24;
            if (capture) {
                if (renderedFrames == 1) { distance = 154; lookHeight = 26; }
                if (renderedFrames == 2) { cameraX = -89; knightX = -98; distance = 40; lookHeight = 10; }
                if (renderedFrames == 3) { cameraX = 88; knightX = 85; distance = 42; lookHeight = 12; }
                if (renderedFrames == 4) showForeground = true;
                if (renderedFrames == 5) { insideHall = false; distance = 14; lookHeight = 3.5f; }
                if (renderedFrames == 6) {   // free camera: POM close-up of the left end wall
                    freeCamera = true;
                    freePos = glm::vec3(-101.0f, 5.0f, 6.0f);
                    freeYaw = -0.9f;
                    freePitch = -0.12f;
				}
                if (renderedFrames == 7) {   // the end of the quest: open gate, knight walking in
                    cameraX = -92; knightX = -100; distance = 30; lookHeight = 6;
                    hasKey = true; gateLift = 1.0f; inGarden = true; gardenWalk = 0.6f;
                }
            } else {
                if (renderedFrames == 1) showForeground = true;
                if (renderedFrames == 2) { insideHall = false; distance = 14; lookHeight = 3.5f; }
            }
        }
        // ---- 5b. The quest ---------------------------------------------------
        const bool nearKey = !hasKey && std::abs(knightX - keyX) < 3.0f;
        const bool nearGate = std::abs(knightX - gateX) < 5.0f;
        if (keyE.justPressed(window) && insideHall && !freeCamera) {
            if (nearKey) hasKey = true;
            else if (nearGate && hasKey && !gateOpening) gateOpening = true;
            else if (nearGate && gateLift >= 1.0f) inGarden = !inGarden;
        }
        if (!paused && !smokeTest) {   // the smoke test shows fixed states
            // The gate rises in 2.5 s; the walk through it takes 2 s each way.
            if (gateOpening) gateLift = std::min(gateLift + dt / 2.5f, 1.0f);
            const float walkTarget = inGarden ? 1.0f : 0.0f;
            gardenWalk += std::clamp(walkTarget - gardenWalk, -dt / 2.0f, dt / 2.0f);
        }
        // ---- 6. Draw the scene ---------------------------------------------
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        // Minimised window: sleep until the next event instead of spinning the CPU.
        if (width == 0 || height == 0) { glfwWaitEvents(); continue; }
        glViewport(0, 0, width, height);
        glClearColor(0.025f, 0.042f, 0.072f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::vec3 cameraPos(cameraX, insideHall ? lookHeight + 12.0f : 4.0f, distance);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(cameraX, lookHeight, 0), glm::vec3(0, 1, 0));
        if (freeCamera) {
            cameraPos = freePos;
            view = glm::lookAt(freePos, freePos + directionFromAngles(freeYaw, freePitch), glm::vec3(0, 1, 0));
        }
        const glm::mat4 projection = glm::perspective(glm::radians(50.0f), float(width) / height, 0.1f, 500.0f);

        const GLuint program = insideHall ? hallProgram : stoneProgram;
        glUseProgram(program);
        setMatrix(program, "model", glm::mat4(1));
        setMatrix(program, "view", view);
        setMatrix(program, "projection", projection);
        glUniform3fv(glGetUniformLocation(program, "lightDir"), 1, glm::value_ptr(lightDir));
        glUniform3fv(glGetUniformLocation(program, "cameraPos"), 1, glm::value_ptr(cameraPos));
        glUniform3f(glGetUniformLocation(program, "lightPos"), -8, 12, 6);
        const float clock = smokeTest ? 0.0f : animTime;   // fixed in the smoke test
        glUniform1f(glGetUniformLocation(program, "time"), clock);
        glUniform1f(glGetUniformLocation(program, "gateLift"), gateLift);
        glUniform1i(glGetUniformLocation(program, "parallaxOn"), parallaxOn);
        glUniform1i(glGetUniformLocation(program, "showBackFaces"), !culling);
        glUniform2f(glGetUniformLocation(program, "pomLayers"),
                    qualities[quality].minLayers, qualities[quality].maxLayers);
        glUniform1i(glGetUniformLocation(program, "shadowSteps"), qualities[quality].shadowSteps);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, heightMap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, normalMap);
        glActiveTexture(GL_TEXTURE0);

        if (insideHall) {
            hall.architecture.draw();
            if (showForeground || freeCamera) {
                // The 2.5D camera looks at the near wall from OUTSIDE, at the back of its
                // inner skin: show both sides. The free camera is inside: normal culling.
                if (!freeCamera) glDisable(GL_CULL_FACE);
                hall.foreground.draw();
                if (culling) glEnable(GL_CULL_FACE);
            }
            // The knight: on the carpet, or part of the way into the gate (gardenWalk).
            const float walkX = knightX + (gateX - knightX) * gardenWalk;
            const float walkZ = -11.0f * gardenWalk;
            setMatrix(program, "model", glm::translate(glm::mat4(1), glm::vec3(walkX, 0.06f, walkZ)));
            hall.knight.draw();
            // The portcullis, lifted by up to 4.6 m.
            setMatrix(program, "model", glm::translate(glm::mat4(1), glm::vec3(0, gateLift * 4.6f, 0)));
            hall.grille.draw();
            // The key: spins and bobs over the carpet until it is taken.
            if (!hasKey) {
                glm::mat4 model = glm::translate(glm::mat4(1), glm::vec3(keyX, 1.8f + 0.2f * std::sin(clock * 2.0f), 0));
                model = glm::rotate(model, clock * 1.5f, glm::vec3(0, 1, 0));
                setMatrix(program, "model", model);
                hall.key.draw();
            }
            setMatrix(program, "model", glm::mat4(1));

            // Light shafts are see-through: blend them over the hall, do not hide what is behind.
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);   // a light shaft is a flat sheet: visible from both sides
            hall.lightShafts.draw();
            if (culling) glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        } else {
            castle.draw();
        }

        // ---- 7. Text over the picture --------------------------------------
        const std::string info = infoText(parallaxOn, qualities[quality], vsync, paused, freeCamera,
            hasKey, shownFps);
        const std::string hint = (insideHall && !freeCamera)
            ? questHint(nearKey, nearGate, hasKey, gateLift, inGarden) : "";
        drawHud(hud, width, height, info, showHelp, culling, hint);

        // ---- 8. Show the picture (and check it in the smoke test) ----------
        if (capture && !captureFrame(captureDir / captures[renderedFrames], width, height)) {
            std::cerr << "Could not save capture.\n"; exitCode = 1; break;
        }
        glfwSwapBuffers(window);
        if (smokeTest) {
            const GLenum error = glGetError();
            if (error != GL_NO_ERROR) {
                std::cerr << "OpenGL smoke test failed: " << error << '\n'; exitCode = 1; break;
            }
            if (++renderedFrames == (capture ? 8 : 3)) {
                std::cout << "Smoke test passed: " << renderedFrames << " frames rendered.\n";
                break;
            }
        }
    }

    hud.destroy();
    hall.destroy();
    castle.destroy();
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &heightMap);
    glDeleteTextures(1, &normalMap);
    glDeleteProgram(hallProgram);
    glDeleteProgram(stoneProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
