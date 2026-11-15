#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "hud.h"

// The first scene: the castle seen from far away across a plain, with a START button.
// Everything in it is 2.5D: flat cut-outs standing at different distances (mountains, hills,
// the castle, rows of trees, grass) and the KNIGHT, who is a flat cut-out too. The camera
// only slides; it never turns. Near cut-outs move a lot, far ones barely: that is the parallax.
//
// The knight is an original dark knight: blackened armour, a closed helm, a ragged cloak, a long sword.
// Before START: he stands on the trail and idles: he breathes, sways a little and
// glances to the side. The mouse moves the camera a little and the camera also sways by itself.
//
// After START: the button fades out, the knight turns round like a paper figure (he gets thin,
// then shows his back) and walks the trail up to the gate. The camera slides along behind him.
// At the gate the picture fades to black and the castle interior begins.
// Enter (or a click) during the walk skips to the end.
//
// Every frame:
//     intro.update(window, dt);
//     post.beginScene(w, h);  ...clear...;  intro.drawScene(w, h);  post.finish(time, true);
//     intro.drawButton(hud, w, h);
//     if (intro.finished()) { ...switch to the castle... }
class Intro {
public:
    bool init();                                  // load the shader, create the empty vertex array
    void reset();                                 // show the intro again from the beginning
    void update(GLFWwindow* window, float dt);    // mouse -> camera, button, the knight's walk
    void drawScene(int width, int height);        // the landscape and the knight (into the HDR picture)
    void drawButton(Hud& hud, int width, int height);   // title and START, drawn over the picture
    bool finished() const { return fade >= 1.0f; }      // the walk (and the fade) is over
    void destroy();

private:
    struct Rect { float x, y, w, h; };
    Rect buttonRect(int width, int height) const;
    void updateKnight(float dt);                  // where the knight is and how he moves
    void updateCamera(float dt);                  // idle camera -> camera sliding behind the knight

    GLuint program = 0, vao = 0;
    GLint locResolution = -1, locTime = -1, locCamPos = -1, locCamTarget = -1, locFade = -1,
          locKnightPos = -1, locKnightPose = -1, locKnightStride = -1;

    float clock = 0.0f;               // the clock of the intro
    float lookX = 0.0f, lookY = 0.0f; // smoothed mouse position, -1..1
    glm::vec3 camPos{0.0f, 1.7f, 0.0f};          // the eye, in metres
    glm::vec3 camTarget{0.0f, 21.0f, -140.0f};   // the point the eye looks at

    int mouseX = 0, mouseY = 0;       // in framebuffer pixels, (0,0) is top-left like the Hud
    bool hovered = false;
    bool pressedOnButton = false;     // the mouse went down on the button and is still down
    bool mouseWasDown = false;
    bool enterWasDown = false;
    bool starting = false;            // START was pressed: the walk is on
    bool skipping = false;            // Enter / click during the walk: fade out now
    float sinceStart = 0.0f;          // seconds since START
    float skipFade = 0.0f;
    float fade = 0.0f;                // 0 = intro, 1 = black

    // The knight cut-out. s = how far along the trail (metres from the origin towards the castle).
    float knightS = 0.0f;
    float knightSpeed = 0.0f;         // metres per second, for the walk cycle
    float stepPhase = 0.0f;           // the walk cycle, radians
    float walkAmount = 0.0f;          // 0 = standing, 1 = walking: blends idle and walk
    float cameraS = 0.0f;             // the camera's idea of where the knight is: it lags a little
    // His pose, sent to intro.frag: facing (+1 front, -1 back, near 0 = edge-on while turning),
    // hop height (m), waddle tilt (radians), breathing (height scale).
    glm::vec4 knightPose{1.0f, 0.0f, 0.0f, 1.0f};
    float knightStride = 0.0f;        // -1..1: which foot is lifted (0 = both on the ground)
};
