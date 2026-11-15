#version 330 core
// Lesson 20: one dust mote = one point. It rises, sways and twinkles; all from `time`.
layout(location = 0) in vec4 seed;   // xyz: where it starts, w: its own random number 0..1

uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float pointScale;            // pixels of a mote 1 m away

out float brightness;

void main() {
    // life goes 0 -> 1 in 8 / 0.35 = 23 s, then starts again (mod wraps around).
    // Every mote has its own start (seed.w), so they do not all jump at once.
    float life = mod(time * 0.35 + seed.w * 40.0, 8.0) / 8.0;
    vec3 p = seed.xyz;
    p.y += life * 8.0 - 4.0;                              // rise 8 m during its life
    p.x += sin(time * 0.4 + seed.w * 6.2832) * 1.2;      // sway left and right
    vec4 viewPos = view * vec4(p, 1.0);
    gl_Position = projection * viewPos;
    // Far motes are smaller: size / distance, but never under 1.5 or over 7 pixels.
    gl_PointSize = clamp(pointScale / max(-viewPos.z, 1.0), 1.5, 7.0);
    // Twinkle; and fade in at the bottom and out at the top, so the wrap-around is unseen.
    brightness = (0.55 + 0.45 * sin(time * 1.7 + seed.w * 37.0)) * sin(life * 3.1416);
}
