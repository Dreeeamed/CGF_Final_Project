#pragma once
#include <glad/glad.h>

// Lesson 14: post-processing.
// The castle is no longer drawn straight to the screen. It goes into our own picture
// (a framebuffer) that keeps colours brighter than 1.0 (HDR: 16-bit floats).
// Then one big triangle covers the screen, and a shader "develops" that picture
// like film: tone mapping, cold shadows and warm lights, dark corners, grain.
//
// Every frame: post.beginScene(w, h);  ...draw the castle...;  post.finish(time, on);
class PostProcess {
public:
    bool init();
    void beginScene(int width, int height);   // from now on, draw into our HDR picture
    void finish(float time, bool enabled);    // develop the picture onto the screen
    void destroy();

private:
    bool resize(int width, int height);       // (re)create the pictures for this window size
    void deletePictures();

    GLuint program = 0, emptyVao = 0;
    GLuint msFbo = 0, msColour = 0, msDepth = 0;   // 4 samples per pixel: smooth edges (MSAA)
    GLuint fbo = 0, colour = 0;                    // 1 sample per pixel: a texture a shader can read
    int width = 0, height = 0;
};