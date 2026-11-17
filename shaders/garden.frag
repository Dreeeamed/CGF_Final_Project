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
const int SKY = 19;

uniform float time;   // seconds; set by main.cpp (0 in the smoke test)
uniform bool parallaxOn;   // P: the paving's parallax occlusion mapping on / off (main.cpp sends it)
uniform float ending;      // 0..1: the story's end, the crystal wakes up (0 until main.cpp sends it)
// The castle's own stone maps, shared with the hall (main.cpp binds them for every scene).
uniform sampler2D stoneTexture;   // texture unit 0: colour of the stone (albedo.png)
uniform sampler2D normalMap;      // texture unit 2: tilt of the stone surface (normal.png)
// One copy of the castle texture covers 4 x 4 metres, exactly like hall.frag's walls.
const float castleTile = 4.0;
// The fountain's centre in the world: (layout::fountainX, layout::plazaZ).
const vec2 fountainCentre = vec2(0.0, -12.0);

// Point lights, sent by Garden::setLights (src/garden_scene.cpp) from the real
// positions of the models, so a lamp's light always comes from its flame.
uniform int lanternCount;
uniform vec3 lanternLights[16];   // flame centres (16 = maxLanternLights in garden_scene.h)
uniform vec3 crystalLight;        // centre of the fountain's crystal
const vec3 moonDir = normalize(vec3(-0.4, 0.8, 0.45));   // = moonDirection in garden_scene.h

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
const vec3 candleLight = vec3(1.000, 0.620, 0.300);  // light of a flame: the hall chandeliers' colour
const vec3 magicLight  = vec3(0.620, 0.260, 1.000);  // violet magic light: the hall braziers' colour
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

// ---------------------------------------------------------------------------
// Small tools
// ---------------------------------------------------------------------------
float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

// Smooth random values: a random number at every whole-number corner, blended in between.
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
               mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

// ---------------------------------------------------------------------------
// Parallax occlusion mapping on the paving (the project's topic, now on a FLOOR)
// ---------------------------------------------------------------------------
// The hall reads its relief from a picture (height.png). Here the relief is a formula:
// stone slabs of 1.5 m x 1.0 m with joints between them, every slab at its own height.
// A floor makes the tangent space easy: T = +X, B = +Z, N = +Y.

const float pavingDepth = 0.09;   // metres from the top of the highest slab to the bottom of a joint

// 1 = top of a slab, 0 = bottom of a joint. `slab` returns the slab's own random number.
float pavingHeight(vec2 xz, out float slab) {
    vec2 q = xz / vec2(1.5, 1.0);
    q.x += 0.5 * floor(q.y);                    // every second row is shifted ("running bond")
    slab = hash21(floor(q));
    vec2 f = fract(q);
    float edge = min(min(f.x, 1.0 - f.x) * 1.5, min(f.y, 1.0 - f.y));   // metres to the nearest joint
    return smoothstep(0.0, 0.07, edge) * (0.7 + 0.3 * slab);
}
float pavingHeight(vec2 xz) {
    float slab;
    return pavingHeight(xz, slab);
}

// March the view ray down into the relief, layer by layer, and return where it hits
// (the same steps as parallaxUV in hall.frag). V points from the surface to the eye.
vec2 pavingParallax(vec2 xz, vec3 V, out float hitDepth) {
    float layers = mix(40.0, 12.0, clamp(V.y, 0.0, 1.0));   // more layers at grazing angles
    float layerDepth = 1.0 / layers;
    vec2 delta = -V.xz / max(V.y, 0.12) * pavingDepth * layerDepth;   // one layer deeper = this far along
    float rayDepth = 0.0;
    float surfaceDepth = 1.0 - pavingHeight(xz);
    for (int i = 0; i < 48 && rayDepth < surfaceDepth; i++) {
        xz += delta;
        rayDepth += layerDepth;
        surfaceDepth = 1.0 - pavingHeight(xz);
    }
    // The hit lies between the last two layers: blend by how far each one missed.
    vec2 previous = xz - delta;
    float after = surfaceDepth - rayDepth;
    float before = (1.0 - pavingHeight(previous)) - (rayDepth - layerDepth);
    float weight = after / (after - before);
    hitDepth = mix(rayDepth, rayDepth - layerDepth, weight);
    return mix(xz, previous, weight);
}

// Self-shadowing: walk from the hit point towards the moon; if a slab edge is above the
// ray, the point is in shadow (like parallaxShadow in hall.frag).
float pavingShadow(vec2 xz, float hitDepth, vec3 L) {
    const int steps = 12;
    float layerDepth = hitDepth / float(steps);
    vec2 delta = L.xz / max(L.y, 0.2) * pavingDepth * layerDepth;
    float blocked = 0.0;
    for (int i = 1; i <= steps; i++) {
        xz += delta;
        float rayDepth = hitDepth - layerDepth * float(i);
        float stoneAbove = rayDepth - (1.0 - pavingHeight(xz));
        blocked = max(blocked, stoneAbove * (1.0 - float(i) / float(steps)));
    }
    return 1.0 - clamp(blocked * 10.0, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// The painted sky behind the back hedge
// ---------------------------------------------------------------------------
// One big flat sheet, but its picture has three layers that slide by different amounts
// when the camera moves: stars and moon (almost not at all), far towers, near towers.
// Far things move less than near things - parallax again, here with one quad.
vec3 skyBackdrop(vec3 p) {
    vec2 farPos = vec2(p.x - cameraPos.x * 0.92, p.y);   // stars and moon: nearly at infinity
    vec3 colour = mix(vec3(0.060, 0.090, 0.190), vec3(0.010, 0.014, 0.040), clamp(p.y / 45.0, 0.0, 1.0));
    // Stars: one per cell of a grid, most cells empty; they twinkle slowly.
    vec2 cell = floor(farPos * 1.1), inCell = fract(farPos * 1.1) - 0.5;
    float r = hash21(cell);
    float star = step(0.955, r) * (1.0 - smoothstep(0.02, 0.16, length(inCell)));
    colour += vec3(0.75, 0.82, 1.0) * star * (0.6 + 0.4 * sin(time * (1.0 + r * 3.0) + r * 40.0));
    // The moon and its halo.
    float md = length(farPos - vec2(17.0, 15.5));
    colour += vec3(0.90, 0.93, 1.0) * (1.0 - smoothstep(2.3, 2.5, md)) * 1.25;
    colour += vec3(0.30, 0.38, 0.60) * exp(-md * 0.30) * 0.45;

    // The castle round the garden: two rows of towers with lit windows.
    for (int layer = 0; layer < 2; layer++) {
        float slide = layer == 0 ? 0.62 : 0.35;                 // the far row slides more with the camera
        float spacing = layer == 0 ? 31.0 : 23.0;
        float x = p.x - cameraPos.x * slide + float(layer) * 9.0;
        float id = floor(x / spacing + 0.5);
        float tx = x - id * spacing;                            // metres from the tower's centre line
        float halfW = 2.6 + 1.4 * hash21(vec2(id, float(layer)));
        float top = (layer == 0 ? 9.5 : 7.5) + 4.5 * hash21(vec2(id, 7.0 + float(layer)));
        float wall = (layer == 0 ? 8.5 : 6.0) + 0.9 * step(fract(x / 2.2), 0.5);   // battlements
        float roof = top + max(halfW + 0.6 - abs(tx), 0.0) * 2.3;                   // pointed roof
        float skyline = abs(tx) < halfW + 0.6 ? max(roof, wall) : wall;
        if (p.y < skyline) {
            vec3 stone = layer == 0 ? vec3(0.030, 0.040, 0.078) : vec3(0.016, 0.022, 0.045);
            // Windows: a grid on the tower; a random part of them is lit.
            vec2 w = vec2(tx / 1.3, p.y / 2.4);
            vec2 wc = floor(w), wf = fract(w) - 0.5;
            float lit = step(0.62, hash21(wc + vec2(id * 13.0, float(layer) * 5.0)));
            float inside = step(abs(tx), halfW - 0.5) * step(4.0, p.y) * step(p.y, top - 1.0);
            float window = lit * inside * step(abs(wf.x), 0.16) * step(abs(wf.y), 0.27);
            colour = mix(stone, flameColour * (layer == 0 ? 0.8 : 1.25), window);
        }
    }
    return colour;
}

void main() {
    if (Material == SKY) {   // the sky sheet is a painting: no light, no fog
        FragColor = vec4(pow(skyBackdrop(FragPos), vec3(2.2)), 1.0);
        return;
    }
    vec3 n = normalize(Normal);
    vec3 viewDir = normalize(cameraPos - FragPos);   // moved up here: the paving's POM needs it
    float moonShadow = 1.0;   // 0 = in the shadow of the paving's own relief
    float wrap = 0.0;         // leaves and petals: light also reaches round to the dark side

    // The castle stone's texture coordinates and their screen-space derivatives are
    // computed for EVERY pixel, before any "if": mipmapping (L-8-1 p29-33) needs to
    // know how fast uv changes between neighbouring pixels to pick the right level.
    vec2 castleUV;
    vec3 T, B;
    boxFrame(FragPos, n, castleUV, T, B);
    vec2 dx = dFdx(castleUV), dy = dFdy(castleUV);
    float detail = stoneDetail(FragPos, n);

    vec3 base = mossColour;
    if (Material == MOSS) {
        // Patches of darker and lighter moss, and fine grain: two sizes of noise.
        float patches = noise(FragPos.xz * 0.22), grain = noise(FragPos.xz * 3.1);
        base = mossColour * (0.55 + 0.55 * patches) * (0.85 + 0.3 * grain);
        base = mix(base, vec3(0.20, 0.27, 0.22), 0.35 * noise(FragPos.xz * 0.6 + 40.0));
    }
    if (Material == CASTLE_STONE) {
        // The castle wall: the real stone photo as its colour, and its normal map to
        // tilt the normal per pixel (bump / normal mapping, L-8-1 p51-56), so every
        // brick catches the moonlight on its own. Same files as the hall's walls.
        base = textureGrad(stoneTexture, castleUV, dx, dy).rgb;
        vec3 bump = textureGrad(normalMap, castleUV, dx, dy).rgb * 2.0 - 1.0;   // 0..1 -> -1..1
        n = normalize(T * bump.x + B * bump.y + n * bump.z);
    }
    if (Material == PATH_STONE) {
        // Stone slabs measured in world metres (1.5 m x 1.0 m), "running bond".
        vec2 p = FragPos.xz;
        float slab, height;
        if (n.y > 0.7) {
            // The top face: parallax occlusion mapping. The view ray is marched into the
            // relief, so the joints are really deep and near slabs hide far joints.
            float hitDepth = 0.0;
            if (parallaxOn) p = pavingParallax(p, viewDir, hitDepth);
            height = pavingHeight(p, slab);
            if (parallaxOn) moonShadow = pavingShadow(p, 1.0 - height, moonDir);
            // The relief also tilts the normal: slopes of the height field along X and Z.
            const float e = 0.012;
            float hx = pavingHeight(p + vec2(e, 0.0)) - pavingHeight(p - vec2(e, 0.0));
            float hz = pavingHeight(p + vec2(0.0, e)) - pavingHeight(p - vec2(0.0, e));
            n = normalize(vec3(-hx, 2.0 * e / pavingDepth, -hz));
        } else {
            height = pavingHeight(p, slab);   // the sides of a path: no POM, the same pattern
        }
        // Dark in the joints, and every slab a little different in tone.
        base = stoneColour * mix(0.30, 1.0, smoothstep(0.0, 0.6, height)) * (0.85 + 0.4 * slab)
             * stoneDetail(vec3(p.x, FragPos.y, p.y), vec3(0.0, 1.0, 0.0));
    }
    if (Material == GOLD) base = goldColour;
    if (Material == CURB) base = slateColour * detail;
    if (Material == BARK) {
        // Thin vertical grooves: brightness changes round the trunk (UV.x = angle).
        base = barkColour * (0.8 + 0.2 * sin(UV.x * 6.2832 * 7.0));
    }
    if (Material == LEAF) { base = leafColour; wrap = 0.5; }
    if (Material == LEAF_VIOLET) { base = violetColour * 0.6; wrap = 0.5; }
    if (Material == SOIL) base = soilColour;
    if (Material == STEM) { base = leafColour * 0.6; wrap = 0.5; }
    if (Material == PETAL_LAVENDER) { base = lavenderColour; wrap = 0.6; }
    if (Material == PETAL_WHITE) { base = paleColour; wrap = 0.6; }
    if (Material == FLOWER_HEART) base = goldColour;
    if (Material == HEDGE) { base = hedgeColour * (0.8 + 0.4 * noise(FragPos.xy * 2.5 + FragPos.z)); wrap = 0.4; }
    if (Material == MARBLE) base = marbleColour * detail;
    if (Material == WATER) base = waterColour * 0.55;
    if (Material == CRYSTAL) base = violetColour;
    if (Material == IRON) base = ironColour;
    if (Material == LANTERN_GLOW) base = flameColour;

    // How shiny each material is (Blinn-Phong: strength and exponent, L-5-2 Shading).
    // Polished gold and wet water give small sharp highlights; moss and leaves none.
    float specularStrength = 0.0, shininess = 32.0;
    if (Material == GOLD) { specularStrength = 0.7; shininess = 48.0; }
    if (Material == MARBLE || Material == CASTLE_STONE) { specularStrength = 0.12; shininess = 20.0; }
    if (Material == IRON) { specularStrength = 0.3; shininess = 30.0; }
    if (Material == WATER) { specularStrength = 0.9; shininess = 80.0; }
    if (Material == PATH_STONE) { specularStrength = 0.35; shininess = 60.0; }   // worn smooth

    if (Material == WATER) {
        // Ripples tilt the water's normal: rings round the fountain that move outwards.
        // A tilted normal moves the highlights, so the water glitters.
        vec2 away = FragPos.xz - fountainCentre;
        float d = length(away);
        float slope = cos(d * 7.0 - time * 2.5) * 0.22;
        n = normalize(vec3(-slope * away.x / max(d, 0.01), 1.0, -slope * away.y / max(d, 0.01)));
    }

    // ---- Light --------------------------------------------------------------
    // 1. Moonlight: a cool ambient term + one direction light (Lambert diffuse).
    //    Darker than before, so the lanterns' pools of light can be seen.
    // wrap = 0: ordinary Lambert. wrap > 0: (0.5 + 0.5 cos) lets light "wrap" round thin
    // leaves and petals, so their dark side is not black.
    float diffuse = mix(max(dot(n, moonDir), 0.0), 0.5 + 0.5 * dot(n, moonDir), wrap) * moonShadow;
    vec3 colour = base * (vec3(0.22, 0.26, 0.36) + vec3(0.60, 0.66, 0.82) * diffuse);
    if (Material != WATER)   // the moon's highlight (the water has its own sparkle below)
        colour += vec3(0.75, 0.85, 1.0) * specularStrength * moonShadow * 0.5
                * pow(max(dot(n, normalize(moonDir + viewDir)), 0.0), shininess);

    // 2. The lanterns: warm POINT lights (L-5-2 p24, distance terms). Unlike the moon,
    //    a point light has a position: the direction L changes from pixel to pixel, and
    //    the light gets weaker with distance d as 1 / (1 + k d^2). The same model as the
    //    hall's chandeliers, with a bigger k: a small lantern lights only a few metres.
    for (int i = 0; i < lanternCount; i++) {
        vec3 toLamp = lanternLights[i] - FragPos;
        float fade = 2.4 / (1.0 + dot(toLamp, toLamp) * 0.22);
        vec3 L = normalize(toLamp);
        float lambert = mix(max(dot(n, L), 0.0), 0.5 + 0.5 * dot(n, L), wrap);
        float shine = pow(max(dot(n, normalize(L + viewDir)), 0.0), shininess);   // Blinn-Phong
        colour += candleLight * fade * (base * lambert + shine * specularStrength);
    }
    // 3. The crystal: one violet point light, stronger and reaching farther (small k),
    //    so the whole plaza and the water round the fountain turn a little magical.
    vec3 toCrystal = crystalLight - FragPos;
    float glow = (2.0 + 6.0 * ending) / (1.0 + dot(toCrystal, toCrystal) * 0.02);   // brighter at the end
    colour += magicLight * glow * base * max(dot(n, normalize(toCrystal)), 0.0);

    if (Material == WATER) {
        // Ripples: rings round the fountain's centre that move outwards with time.
        // sin(distance * k - time * speed) is a wave travelling away from the centre.
        float d = length(FragPos.xz - fountainCentre);
        float ripple = 0.5 + 0.5 * sin(d * 7.0 - time * 2.5);
        colour += waterColour * 0.12 * ripple;
        // A mirror-like sparkle of the moon (Blinn-Phong specular, L-5-2 Shading):
        // strongest where the halfway vector between moon and eye meets the normal.
        vec3 halfway = normalize(moonDir + viewDir);
        colour += vec3(0.75, 0.85, 1.0) * pow(max(dot(n, halfway), 0.0), shininess) * 0.8;
    }
    // The crystal glows: strong emission, so the team's bloom makes a soft halo.
    if (Material == CRYSTAL) colour += violetColour * (1.6 + 4.0 * ending);
    // Lantern flames: strong warm emission with a gentle flicker. The phase depends on
    // the position, so neighbouring lanterns do not flicker in step.
    if (Material == LANTERN_GLOW)
        colour += flameColour * (1.8 + 0.15 * sin(time * 9.0 + FragPos.x * 3.0)
                                     + 0.1 * sin(time * 23.0 + FragPos.z));
    // Magical flowers give a little light of their own (emission): it is added on top
    // of the reflected light, so they stay visible even on their dark side.
    if (Material == PETAL_LAVENDER) colour += lavenderColour * (0.35 + 0.35 * ending);

    // Depth fog: the farther a point lies behind the walking line (-Z), the more navy.
    float fog = clamp(-FragPos.z / 40.0, 0.0, 0.6);
    // A thin mist lies on the ground: below 2.5 m, thicker farther back.
    fog += 0.18 * smoothstep(2.5, 0.0, FragPos.y) * clamp(-FragPos.z / 20.0, 0.0, 1.0);
    colour = mix(colour, fogColour, min(fog, 0.75));
    // Our palette is picked by eye on the monitor ("gamma-encoded"). The team's HDR
    // pipeline adds light as energy, so decode to linear here, like hall.frag does;
    // post.frag encodes back for the monitor at the very end.
    FragColor = vec4(pow(max(colour, 0.0), vec3(2.2)), 1.0);   // max: pow of a negative number is undefined
}
