#version 330 core
// On-screen text: the points come in pixels, not in metres.
layout(location = 0) in vec2 aPixel;
uniform vec2 screenSize;

void main() {
    // Pixels (0..width, y down) -> OpenGL screen coordinates (-1..1, y up).
    vec2 ndc = aPixel / screenSize * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
