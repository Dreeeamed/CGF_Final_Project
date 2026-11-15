#version 330 core
// Fountain drops: one point per drop, sized by its distance from the camera.
layout(location=0) in vec3 aPosition;    // already in world coordinates
layout(location=1) in float aBrightness;
uniform mat4 view;
uniform mat4 projection;
uniform float pointScale;                // pixels covered by a drop at 1 m distance
out float Brightness;
void main() {
    vec4 eye = view * vec4(aPosition, 1.0);   // camera coordinates: the camera looks along -z
    gl_Position = projection * eye;
    // Perspective: twice as far away = half as big (L-5-1 Viewing, perspective division).
    gl_PointSize = clamp(pointScale / -eye.z, 1.0, 24.0);
    Brightness = aBrightness;
}
