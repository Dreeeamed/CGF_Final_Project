#version 330 core
// Intro: one big triangle that covers the whole window. No vertex buffer is needed:
// the three corners come from gl_VertexID (0, 1, 2) -> (0,0), (2,0), (0,2) -> -1..3 on the screen.
out vec2 ndc;   // -1..1 across the window, y up

void main() {
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    ndc = corner * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
