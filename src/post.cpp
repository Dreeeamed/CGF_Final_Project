#include "post.h"
#include "material.h"
#include <iostream>

bool PostProcess::init() {
    program = createShaderProgram("shaders/post.vert", "shaders/post.frag");
    if (!program) return false;
    // The full-screen triangle needs no vertex buffer: post.vert makes its 3 corners
    // from gl_VertexID. OpenGL 3.3 core still wants some vertex array to be bound.
    glGenVertexArrays(1, &emptyVao);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "scene"), 0);   // the picture is on texture unit 0
    return true;
}

bool PostProcess::resize(int w, int h) {
    deletePictures();
    width = w;
    height = h;

    // 1) The multisampled picture: the castle is drawn here.
    //    A renderbuffer is memory a framebuffer can draw into, but a shader cannot read.
    glGenRenderbuffers(1, &msColour);
    glBindRenderbuffer(GL_RENDERBUFFER, msColour);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA16F, w, h);   // floats: may go above 1.0
    glGenRenderbuffers(1, &msDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24, w, h);
    glGenFramebuffers(1, &msFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColour);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);
    const bool msOk = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    // 2) A plain texture: the 4 samples of every pixel are averaged into it ("resolve").
    glGenTextures(1, &colour);
    glBindTexture(GL_TEXTURE_2D, colour);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);   // 0 = the window again
    if (!msOk || !ok) std::cerr << "Post-processing framebuffer is incomplete.\n";
    return msOk && ok;
}

void PostProcess::beginScene(int w, int h) {
    if (w != width || h != height) resize(w, h);   // the first frame, or the window changed size
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
    glViewport(0, 0, w, h);
}

void PostProcess::finish(float time, bool enabled) {
    // Resolve: copy the multisampled picture into the texture, averaging the 4 samples.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // Back to the window: one triangle over the whole screen, the shader does the rest.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);   // nothing to hide: every pixel is drawn once
    glUseProgram(program);
    glUniform1f(glGetUniformLocation(program, "time"), time);
    glUniform1i(glGetUniformLocation(program, "enabled"), enabled);
    glUniform2f(glGetUniformLocation(program, "resolution"), float(width), float(height));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colour);
    glBindVertexArray(emptyVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void PostProcess::deletePictures() {
    // Deleting 0 is allowed and does nothing, so this is safe on the first call.
    glDeleteFramebuffers(1, &msFbo);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &msColour);
    glDeleteRenderbuffers(1, &msDepth);
    glDeleteTextures(1, &colour);
    msFbo = fbo = msColour = msDepth = colour = 0;
}

void PostProcess::destroy() {
    deletePictures();
    glDeleteVertexArrays(1, &emptyVao);
    glDeleteProgram(program);
}
