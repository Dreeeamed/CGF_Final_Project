#version 330 core
// Lesson 14: "develop" the HDR picture of the castle, like film.
in vec2 uv;
out vec4 FragColor;

uniform sampler2D scene;   // the castle; colours may be brighter than 1.0
uniform vec2 resolution;   // window size in pixels
uniform float time;
uniform bool enabled;      // G: compare with and without

const float exposure = 1.2;   // > 1 brighter, < 1 darker, like a camera

// ACES filmic curve (Narkowicz 2015): any brightness 0..infinity -> 0..1.
// Dark tones go deeper, bright light rolls off softly instead of clipping to flat white.
vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// A pseudo-random number 0..1 for every pixel: film grain.
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec3 c = texture(scene, uv).rgb;
    if (!enabled) {   // the old look: just cut everything above 1.0
        FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
        return;
    }

    // 1. Our hall colours were picked by eye on the monitor, so they are "gamma-encoded".
    //    ACES works with real light energy: decode first (2.2 = the usual monitor gamma).
    c = pow(c, vec3(2.2));

    // 2. Exposure and tone mapping.
    c = aces(c * exposure);

    // 3. Split toning: shadows a little colder (blue), lights a little warmer (gold).
    float brightness = dot(c, vec3(0.2126, 0.7152, 0.0722));   // as the eye sees it: green counts most
    c = mix(c, c * vec3(0.80, 0.95, 1.20), (1.0 - smoothstep(0.0, 0.35, brightness)) * 0.6);
    c = mix(c, c * vec3(1.12, 1.00, 0.82), smoothstep(0.30, 0.90, brightness) * 0.5);

    // 4. Vignette: darker corners lead the eye to the centre.
    vec2 q = uv - 0.5;
    q.x *= resolution.x / resolution.y;   // a circle, not an oval
    c *= mix(0.25, 1.0, 1.0 - smoothstep(0.25, 0.95, length(q)));

    // 5. Encode back for the monitor, then grain: a little noise that changes every frame.
    c = pow(c, vec3(1.0 / 2.2));
    c += (hash(uv * resolution + fract(time)) - 0.5) * 0.02;
    FragColor = vec4(c, 1.0);
}
