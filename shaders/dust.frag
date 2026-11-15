#version 330 core
// Lesson 20: a point is drawn as a little square; gl_PointCoord goes 0..1 across it.
in float brightness;
out vec4 FragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;                  // -1..1, the centre is 0
    float dot2 = dot(c, c);                              // squared distance from the centre
    if (dot2 > 1.0) discard;                             // the corners: not part of the dot
    float a = (1.0 - dot2) * (1.0 - dot2) * brightness;  // soft round dot, brightest in the middle
    // Warm candle-lit dust, added as light (GL_ONE, GL_ONE): it never darkens anything.
    FragColor = vec4(vec3(1.0, 0.62, 0.30) * 1.4 * a, 1.0);
}
