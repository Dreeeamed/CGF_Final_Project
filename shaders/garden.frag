#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 UV;
flat in int Material;
out vec4 FragColor;
uniform vec3 cameraPos;

// Material IDs: keep in sync with royal_garden::Material in src/garden_scene.h.
const int MOSS = 0;
const int PATH_STONE = 1;
const int GOLD = 2;
const int CURB = 3;

// Garden palette (hex design targets / 255).
const vec3 mossColour  = vec3(0.188, 0.302, 0.325);  // #304D53 desaturated teal
const vec3 stoneColour = vec3(0.145, 0.180, 0.259);  // #252E42 dark blue-grey
const vec3 slateColour = vec3(0.255, 0.275, 0.369);  // #41465E muted slate
const vec3 goldColour  = vec3(0.773, 0.631, 0.420);  // #C5A16B antique gold
const vec3 fogColour   = vec3(0.043, 0.063, 0.125);  // #0B1020 midnight navy

void main() {
    vec3 base = mossColour;
    if (Material == PATH_STONE) {
        // Stone slabs measured in world metres (1.5 m x 1.0 m). Every second row
        // is shifted by half a slab, like real paving ("running bond").
        vec2 q = FragPos.xz / vec2(1.5, 1.0);
        q.x += 0.5 * floor(q.y);
        vec2 f = fract(q);                          // position inside one slab, 0..1
        float edge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
        float joint = smoothstep(0.02, 0.05, edge); // 0 on the joint line, 1 inside
        base = mix(stoneColour * 0.45, stoneColour, joint);
    }
    if (Material == GOLD) base = goldColour;
    if (Material == CURB) base = slateColour;

    // Temporary lighting until Phase 7: low cool ambient + one moonlight direction
    // (Lambert diffuse, L-5-2 Shading). Enough to tell the faces of a box apart.
    vec3 n = normalize(Normal);
    vec3 moonDir = normalize(vec3(-0.4, 0.8, 0.45));
    float diffuse = max(dot(n, moonDir), 0.0);
    vec3 colour = base * (vec3(0.30, 0.34, 0.45) + vec3(0.70, 0.75, 0.90) * diffuse);

    // Depth fog: the farther a point lies behind the walking line (-Z), the more navy.
    float fog = clamp(-FragPos.z / 40.0, 0.0, 0.6);
    colour = mix(colour, fogColour, fog);
    FragColor = vec4(colour, 1.0);
}
