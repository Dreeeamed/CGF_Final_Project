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
const int BARK = 4;
const int LEAF = 5;
const int LEAF_VIOLET = 6;
const int SOIL = 7;
const int STEM = 8;
const int PETAL_LAVENDER = 9;
const int PETAL_WHITE = 10;
const int FLOWER_HEART = 11;
const int HEDGE = 12;
const int MARBLE = 13;
const int WATER = 14;
const int CRYSTAL = 15;
const int IRON = 16;
const int LANTERN_GLOW = 17;
const int CASTLE_STONE = 18;

uniform float time;   // seconds; set by main.cpp (0 in the smoke test)
// The castle's own stone maps, shared with the hall (main.cpp binds them for every scene).
uniform sampler2D stoneTexture;   // texture unit 0: colour of the stone (albedo.png)
uniform sampler2D normalMap;      // texture unit 2: tilt of the stone surface (normal.png)
// One copy of the castle texture covers 4 x 4 metres, exactly like hall.frag's walls.
const float castleTile = 4.0;
// The fountain's centre in the world: (layout::fountainX, layout::plazaZ).
const vec2 fountainCentre = vec2(0.0, -12.0);

// Garden palette (hex design targets / 255).
const vec3 mossColour  = vec3(0.188, 0.302, 0.325);  // #304D53 desaturated teal
const vec3 stoneColour = vec3(0.145, 0.180, 0.259);  // #252E42 dark blue-grey
const vec3 slateColour = vec3(0.255, 0.275, 0.369);  // #41465E muted slate
const vec3 goldColour  = vec3(0.773, 0.631, 0.420);  // #C5A16B antique gold
const vec3 barkColour  = vec3(0.105, 0.110, 0.150);  // near-black blue bark
const vec3 leafColour  = vec3(0.220, 0.360, 0.380);  // #304D53 teal, a little lighter
const vec3 violetColour = vec3(0.545, 0.439, 0.780); // #8B70C7 soft violet accent
const vec3 lavenderColour = vec3(0.722, 0.663, 0.910); // #B8A9E8 pale lavender (glowing flowers)
const vec3 paleColour  = vec3(0.780, 0.820, 0.900);  // bluish white petals
const vec3 soilColour  = vec3(0.070, 0.075, 0.090);  // almost black earth
const vec3 hedgeColour = vec3(0.120, 0.215, 0.230);  // darker, clipped teal
const vec3 marbleColour = vec3(0.330, 0.370, 0.470); // pale blue-grey: the fountain stands out
const vec3 waterColour = vec3(0.392, 0.608, 0.682);  // #649BAE muted cyan
const vec3 ironColour  = vec3(0.060, 0.065, 0.085);  // blackened iron
const vec3 flameColour = vec3(1.000, 0.700, 0.330);  // warm lantern light, near #C5A16B
const vec3 fogColour   = vec3(0.043, 0.063, 0.125);  // #0B1020 midnight navy

// ---------------------------------------------------------------------------
// Texture coordinates
// ---------------------------------------------------------------------------

// Box mapping (L-8 Texture Mapping p19): our boxes and prisms have no useful (u, v)
// of their own, so we make them from the WORLD position. Pick the axis the normal
// points along; T and B are the two directions lying in that face, and the texture
// coordinate is "how many metres along T and B", divided by the size of one tile.
// The axes are the same as hall.frag's wallFrame, so the stone lines up the same way.
void boxFrame(vec3 p, vec3 n, out vec2 uv, out vec3 T, out vec3 B) {
    vec3 a = abs(n);
    if (a.y > max(a.x, a.z)) {        // top or bottom face
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 0.0, 1.0);
    } else if (a.z > a.x) {           // face looking towards / away from the camera
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 1.0, 0.0);
    } else {                          // side face
        T = vec3(0.0, 0.0, 1.0);
        B = vec3(0.0, 1.0, 0.0);
    }
    uv = vec2(dot(p, T), dot(p, B)) / castleTile;
}

// Fine detail for plain stone: only the BRIGHTNESS of the castle texture, read from
// all three planes and blended by how much the face looks along each axis
// (triplanar mapping). It is the same recipe as hall.frag's stoneVariation, so the
// garden stone has the same grain as the castle's columns.
float stoneDetail(vec3 p, vec3 n) {
    vec3 w = pow(abs(n), vec3(4.0));
    w /= max(w.x + w.y + w.z, 0.0001);
    vec3 c = texture(stoneTexture, p.yz * 0.14).rgb * w.x
           + texture(stoneTexture, p.xz * 0.14).rgb * w.y
           + texture(stoneTexture, p.xy * 0.14).rgb * w.z;
    return mix(0.86, dot(c, vec3(0.2126, 0.7152, 0.0722)), 0.32);   // luminance
}

void main() {
    vec3 n = normalize(Normal);

    // The castle stone's texture coordinates and their screen-space derivatives are
    // computed for EVERY pixel, before any "if": mipmapping (L-8-1 p29-33) needs to
    // know how fast uv changes between neighbouring pixels to pick the right level.
    vec2 castleUV;
    vec3 T, B;
    boxFrame(FragPos, n, castleUV, T, B);
    vec2 dx = dFdx(castleUV), dy = dFdy(castleUV);
    float detail = stoneDetail(FragPos, n);

    vec3 base = mossColour;
    if (Material == CASTLE_STONE) {
        // The castle wall: the real stone photo as its colour, and its normal map to
        // tilt the normal per pixel (bump / normal mapping, L-8-1 p51-56), so every
        // brick catches the moonlight on its own. Same files as the hall's walls.
        base = textureGrad(stoneTexture, castleUV, dx, dy).rgb;
        vec3 bump = textureGrad(normalMap, castleUV, dx, dy).rgb * 2.0 - 1.0;   // 0..1 -> -1..1
        n = normalize(T * bump.x + B * bump.y + n * bump.z);
    }
    if (Material == PATH_STONE) {
        // Stone slabs measured in world metres (1.5 m x 1.0 m). Every second row
        // is shifted by half a slab, like real paving ("running bond").
        vec2 q = FragPos.xz / vec2(1.5, 1.0);
        q.x += 0.5 * floor(q.y);
        vec2 f = fract(q);                          // position inside one slab, 0..1
        float edge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
        float joint = smoothstep(0.02, 0.05, edge); // 0 on the joint line, 1 inside
        base = mix(stoneColour * 0.45, stoneColour, joint) * detail;
    }
    if (Material == GOLD) base = goldColour;
    if (Material == CURB) base = slateColour * detail;
    if (Material == BARK) {
        // Thin vertical grooves: brightness changes round the trunk (UV.x = angle).
        base = barkColour * (0.8 + 0.2 * sin(UV.x * 6.2832 * 7.0));
    }
    if (Material == LEAF) base = leafColour;
    if (Material == LEAF_VIOLET) base = violetColour * 0.6;
    if (Material == SOIL) base = soilColour;
    if (Material == STEM) base = leafColour * 0.6;
    if (Material == PETAL_LAVENDER) base = lavenderColour;
    if (Material == PETAL_WHITE) base = paleColour;
    if (Material == FLOWER_HEART) base = goldColour;
    if (Material == HEDGE) base = hedgeColour;
    if (Material == MARBLE) base = marbleColour * detail;
    if (Material == WATER) base = waterColour * 0.55;
    if (Material == CRYSTAL) base = violetColour;
    if (Material == IRON) base = ironColour;
    if (Material == LANTERN_GLOW) base = flameColour;

    // Temporary lighting until Phase 7: low cool ambient + one moonlight direction
    // (Lambert diffuse, L-5-2 Shading). Enough to tell the faces of a box apart.
    vec3 moonDir = normalize(vec3(-0.4, 0.8, 0.45));
    float diffuse = max(dot(n, moonDir), 0.0);
    vec3 colour = base * (vec3(0.30, 0.34, 0.45) + vec3(0.70, 0.75, 0.90) * diffuse);
    if (Material == WATER) {
        // Ripples: rings round the fountain's centre that move outwards with time.
        // sin(distance * k - time * speed) is a wave travelling away from the centre.
        float d = length(FragPos.xz - fountainCentre);
        float ripple = 0.5 + 0.5 * sin(d * 7.0 - time * 2.5);
        colour += waterColour * 0.12 * ripple;
        // A mirror-like sparkle of the moon (Blinn-Phong specular, L-5-2 Shading):
        // strongest where the halfway vector between moon and eye meets the normal.
        vec3 viewDir = normalize(cameraPos - FragPos);
        vec3 halfway = normalize(moonDir + viewDir);
        colour += vec3(0.75, 0.85, 1.0) * pow(max(dot(n, halfway), 0.0), 80.0) * 0.8;
    }
    // The crystal glows: strong emission, so the team's bloom makes a soft halo.
    if (Material == CRYSTAL) colour += violetColour * 1.6;
    // Lantern flames: strong warm emission with a gentle flicker. The phase depends on
    // the position, so neighbouring lanterns do not flicker in step.
    if (Material == LANTERN_GLOW)
        colour += flameColour * (1.8 + 0.15 * sin(time * 9.0 + FragPos.x * 3.0)
                                     + 0.1 * sin(time * 23.0 + FragPos.z));
    // Magical flowers give a little light of their own (emission): it is added on top
    // of the reflected light, so they stay visible even on their dark side.
    if (Material == PETAL_LAVENDER) colour += lavenderColour * 0.35;

    // Depth fog: the farther a point lies behind the walking line (-Z), the more navy.
    float fog = clamp(-FragPos.z / 40.0, 0.0, 0.6);
    colour = mix(colour, fogColour, fog);
    // Our palette is picked by eye on the monitor ("gamma-encoded"). The team's HDR
    // pipeline adds light as energy, so decode to linear here, like hall.frag does;
    // post.frag encodes back for the monitor at the very end.
    FragColor = vec4(pow(colour, vec3(2.2)), 1.0);
}
