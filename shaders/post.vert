#version 330 core
// Lesson 14: one triangle bigger than the screen. The part of it inside the screen
// (-1..1) covers every pixel exactly once. No vertex buffer: the corners are here.
const vec2 corners[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

out vec2 uv;   // 0..1 across the screen: where to read the picture of the castle

void main() {
    vec2 p = corners[gl_VertexID];   // gl_VertexID = 0, 1, 2
    uv = p * 0.5 + 0.5;              // -1..1 -> 0..1
    gl_Position = vec4(p, 0.0, 1.0);
}
