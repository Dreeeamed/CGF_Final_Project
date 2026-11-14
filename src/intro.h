#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "hud.h"

// The first scene: the castle seen from far away across a plain, with a START button.
//
// Until START is pressed the picture is alive: the mouse moves the camera a little and the
// camera also sways by itself. Because the mountains, the castle, the trees and the ground
// are at different distances, they move by different amounts: that is the parallax.
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
    void update(GLFWwindow* window, float dt);    // mouse -> camera, button, fade after START
    void drawScene(int width, int height);        // the landscape (draw it into the HDR picture)
    void drawButton(Hud& hud, int width, int height);   // title and START, drawn over the picture
    bool finished() const { return fade >= 1.0f; }      // START was pressed and the fade is over
    void destroy();

private:
    struct Rect { float x, y, w, h; };
    Rect buttonRect(int width, int height) const;

    GLuint program = 0, vao = 0;
    GLint locResolution = -1, locTime = -1, locCamPos = -1, locFade = -1;

    float clock = 0.0f;               // the clock of the intro
    float lookX = 0.0f, lookY = 0.0f; // smoothed mouse position, -1..1
    float camX = 0.0f, camY = 1.7f, camZ = 0.0f;   // the eye, in metres

    int mouseX = 0, mouseY = 0;       // in framebuffer pixels, (0,0) is top-left like the Hud
    bool hovered = false;
    bool pressedOnButton = false;     // the mouse went down on the button and is still down
    bool mouseWasDown = false;
    bool enterWasDown = false;
    bool starting = false;            // START was pressed: fade out and walk towards the gate
    float fade = 0.0f;                // 0 = intro, 1 = black
};
