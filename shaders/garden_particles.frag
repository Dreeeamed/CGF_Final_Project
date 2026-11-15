#version 330 core
// A soft round drop. gl_PointCoord runs from (0,0) to (1,1) across the point's square.
in float Brightness;
out vec4 FragColor;
// Pale cyan water light, already in linear light (the team's HDR pipeline).
const vec3 dropColour = vec3(0.30, 0.62, 0.78);
void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;   // -1..1, centre of the point = (0,0)
    float r2 = dot(c, c);                 // squared distance from the centre
    if (r2 > 1.0) discard;                // cut the square into a circle
    float soft = 1.0 - r2;                // brightest in the middle, fading to the edge
    FragColor = vec4(dropColour * 1.4 * Brightness, soft * 0.8);
}
