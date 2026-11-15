#version 330 core
// Materials and light of the grand hall.
// The headline technique is parallax occlusion mapping (POM) on the walls:
//   parallaxUV()     - find the point of the stone relief the eye really sees,
//   parallaxShadow() - is that point hidden from the light by other stone?

in vec3 FragPos;        // world position of this pixel, metres
in vec3 Normal;
in vec2 UV;
flat in int Material;   // which material: see the list below
out vec4 FragColor;

uniform sampler2D stoneTexture;   // texture unit 0: colour of the stone
uniform sampler2D heightMap;      // texture unit 1: white = stone sticks out, black = groove
uniform sampler2D normalMap;      // texture unit 2: tilt of the stone surface
uniform vec3 cameraPos;
uniform vec3 lightDir;            // towards the light (J/L/I/K move it)
uniform float time;               // animation clock, stands still on pause
uniform bool parallaxOn;          // P
uniform vec2 pomLayers;           // x: layers when looking straight at the wall, y: when looking along it
uniform int shadowSteps;          // steps of the walk towards the light
uniform bool showBackFaces;       // B: culling is off, paint back sides red
uniform float gateLift;           // 0 = gate closed, 1 = fully raised

// Material numbers, the same order as `enum Material` in hall_geometry.h.
const int STONE = 0, FLOOR = 1, DARKNESS = 2, BANNER = 3, CARPET = 4, BRASS = 5,
          FLAME = 6, WINDOW = 7, DOOR = 8, BEAM = 9, KNIGHT = 10, WALL = 11, GARDEN = 12,
          MARBLE = 13, GOLD = 14, BANNER_RED = 15, BANNER_PURPLE = 16,
          DARK_STEEL = 17, CLOAK = 18, CLOAK_LINING = 19, BLADE = 20;

// ---------------------------------------------------------------------------
// Wall axes
// ---------------------------------------------------------------------------

// One copy of the wall texture covers 4 x 4 metres.
const float wallTileSize = 4.0;

// Pick the axis the normal points along. T and B are the two directions
// that lie IN the wall; the UV is the world position measured along them.
void wallFrame(vec3 p, vec3 n, out vec2 uv, out vec3 T, out vec3 B) {
    vec3 a = abs(n);
    if (a.y > max(a.x, a.z)) {        // top or bottom of a wall block
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 0.0, 1.0);
    } else if (a.z > a.x) {           // wall facing the camera
        T = vec3(1.0, 0.0, 0.0);
        B = vec3(0.0, 1.0, 0.0);
    } else {                          // side face of a block
        T = vec3(0.0, 0.0, 1.0);
        B = vec3(0.0, 1.0, 0.0);
    }
    uv = vec2(dot(p, T), dot(p, B)) / wallTileSize;
}

// ---------------------------------------------------------------------------
// Parallax occlusion mapping
// ---------------------------------------------------------------------------

// How deep the relief looks, in texture units.
const float heightScale = 0.05;

// Depth of the groove at uv: 0 = top of the stone, 1 = bottom of the deepest groove.
// textureGrad: the mip level comes from dx/dy measured once, outside the loops.
float depthAt(vec2 uv, vec2 dx, vec2 dy) {
    return 1.0 - textureGrad(heightMap, uv, dx, dy).r;
}

// Walk along the view ray into the stone, layer by layer, until the ray goes
// under the surface. Then interpolate between the last two layers.
vec2 parallaxUV(vec2 uv, vec3 viewTS, vec2 dx, vec2 dy) {
    // Grazing view (viewTS.z near 0) needs more layers than a straight view.
    float layers = mix(pomLayers.y, pomLayers.x, viewTS.z);
    float layerDepth = 1.0 / layers;
    // How far the texture shifts per layer; the 0.2 limit stops a huge shift at grazing angles.
    vec2 deltaUV = viewTS.xy / max(viewTS.z, 0.2) * heightScale / layers;

    float rayDepth = 0.0;
    float surfaceDepth = depthAt(uv, dx, dy);
    vec2 previousUV = uv;
    float gapBefore = surfaceDepth;
    for (int i = 0; i < 128 && rayDepth < surfaceDepth; i++) {
        previousUV = uv;
        gapBefore = surfaceDepth - rayDepth;   // ray still above the stone: gap > 0
        uv -= deltaUV;
        rayDepth += layerDepth;
        surfaceDepth = depthAt(uv, dx, dy);
    }
    if (rayDepth == 0.0) return uv;            // hit the top of the stone at once

    // The ray crossed the surface between previousUV and uv: find where.
    float gapAfter = surfaceDepth - rayDepth;  // ray below the stone: gap < 0
    float weight = gapAfter / (gapAfter - gapBefore);
    return mix(uv, previousUV, weight);
}

// POM step 7: from the point the eye sees, walk towards the light.
// If stone rises above that path, the point is in shadow. Soft: stone close to
// the point casts a darker shadow than stone far away.
const float shadowStrength = 8.0;
float parallaxShadow(vec2 uv, float hitDepth, vec3 lightTS, vec2 dx, vec2 dy) {
    if (lightTS.z <= 0.0) return 0.0;   // the light is behind the wall: fully in shadow
    int steps = shadowSteps;
    float layerDepth = hitDepth / float(steps);
    vec2 deltaUV = lightTS.xy / max(lightTS.z, 0.1) * heightScale * layerDepth;
    float rayDepth = hitDepth;
    float blocked = 0.0;
    for (int i = 1; i <= steps; i++) {
        uv += deltaUV;
        rayDepth -= layerDepth;
        float stoneAbove = rayDepth - depthAt(uv, dx, dy);
        blocked = max(blocked, stoneAbove * (1.0 - float(i) / float(steps)));
    }
    return 1.0 - clamp(blocked * shadowStrength, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Other materials
// ---------------------------------------------------------------------------

// Lesson 16: marble veins from "value noise".
// hash3: a pseudo-random number 0..1 for every whole-metre point of space.
float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453);
}

// noise3: the 8 random numbers at the corners of the 1 m cube around p, blended smoothly.
// The result changes softly through space, like clouds (not like TV static).
float noise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);   // smooth step between the corners
    return mix(mix(mix(hash3(i),                  hash3(i + vec3(1, 0, 0)), f.x),
                   mix(hash3(i + vec3(0, 1, 0)),  hash3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash3(i + vec3(0, 0, 1)),  hash3(i + vec3(1, 0, 1)), f.x),
                   mix(hash3(i + vec3(0, 1, 1)),  hash3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

// Thin pale veins: stripes of sin(), bent by noise of three sizes.
// 1.0 on a vein, 0.0 on the dark stone between them.
float marbleVeins(vec3 p) {
    float bend = noise3(p * 0.35) * 0.6 + noise3(p * 0.9) * 0.3 + noise3(p * 2.3) * 0.1;
    float stripes = abs(sin(p.x * 0.55 + p.y * 0.8 + p.z * 0.45 + bend * 7.0));
    return pow(1.0 - stripes, 14.0) * 0.7 + pow(1.0 - stripes, 60.0) * 0.5;   // wide glow + sharp line
}

// Lesson 17: our own coat of arms on the long banners - a ring, a four-pointed star and
// three crown points - plus gold borders. Returns 1 on gold, 0 on cloth.
// Each shape is a "distance": below 0 inside the shape, above 0 outside. min() of two
// distances is both shapes together. p is measured in banner widths from the sigil centre.
float sigil(float width, float height) {
    vec2 p = vec2((UV.x - 0.5) * width, (UV.y - 0.70) * height) / width;
    float aa = 1.5 * fwidth(p.x);                            // a soft edge, about one pixel wide
    float ring = abs(length(p) - 0.30) - 0.035;              // a circle line, radius 0.30
    vec2 q = abs(p);                                         // the star is the same in all 4 quarters
    float star = (min(q.x / 0.10 + q.y / 0.42, q.x / 0.42 + q.y / 0.10) - 1.0) * 0.08;
    float crown = 1.0;
    for (int i = -1; i <= 1; i++) {                          // three small spikes above the ring
        vec2 c = p - vec2(float(i) * 0.17, 0.43 - 0.05 * abs(float(i)));
        crown = min(crown, max(abs(c.x) * 2.6 + c.y, -c.y - 0.06) - 0.07);
    }
    float shape = min(min(ring, star), crown);
    float edgeX = min(UV.x, 1.0 - UV.x) * width;             // metres from the nearest side edge
    float border = step(edgeX, 0.22) + step(1.0 - UV.y, 0.012);
    float line = step(abs(edgeX - 0.55), 0.05);              // a thin gold line inside the border
    return clamp(1.0 - smoothstep(-aa, aa, shape) + border + line, 0.0, 1.0);
}

// Large, soft brightness variation of plain stone (triplanar: no UVs needed). Not POM.
float stoneVariation(vec3 p, vec3 n) {
    vec3 weights = pow(abs(n), vec3(4.0));
    weights /= max(weights.x + weights.y + weights.z, 0.0001);
    vec3 colour = texture(stoneTexture, p.yz * 0.14).rgb * weights.x
                + texture(stoneTexture, p.xz * 0.14).rgb * weights.y
                + texture(stoneTexture, p.xy * 0.14).rgb * weights.z;
    return mix(0.86, dot(colour, vec3(0.2126, 0.7152, 0.0722)), 0.32);
}

// Lesson 15: the old main() is now shade(); the new main() at the end of the file
// turns its colour into light energy for the post pass.
void shade() {
    // Debug view (key B): the back side of a triangle is painted red.
    if (showBackFaces && !gl_FrontFacing) { FragColor = vec4(1.0, 0.0, 0.0, 1.0); return; }

    // ---- Materials that are not lit -------------------------------------
    if (Material == BEAM) {
        // Soft stylized shafts: transparent geometry, not volumetric ray marching.
        float edge = smoothstep(0.0, 0.28, UV.x) * smoothstep(0.0, 0.28, 1.0 - UV.x);
        float ends = smoothstep(0.0, 0.12, UV.y) * smoothstep(0.0, 0.10, 1.0 - UV.y);
        float dust = 0.88 + 0.12 * sin(FragPos.y * 0.7 + time * 0.35);
        FragColor = vec4(0.38, 0.56, 0.72, 0.06 * edge * ends * dust);   // lesson 15: thinner, blending is in light energy now
        return;
    }
    if (Material == GARDEN) {
        // Lesson 12: the inner garden behind the gate, painted like a theatre backdrop.
        // p is in metres: x = 0 in the middle of the gate, y = 0 on the floor.
        vec2 p = vec2(FragPos.x + 100.0, FragPos.y);
        vec3 colour = mix(vec3(0.10, 0.16, 0.26), vec3(0.02, 0.04, 0.09),       // night sky
                          clamp((p.y - 2.0) / 7.0, 0.0, 1.0));
        vec2 toMoon = p - vec2(2.2, 7.0);
        float moon = smoothstep(0.75, 0.70, length(toMoon));                     // sharp disc
        float halo = 0.25 / (1.0 + 6.0 * dot(toMoon, toMoon));                   // soft glow
        colour += vec3(0.75, 0.85, 1.0) * (moon + halo);
        float treeTops = 2.6 + 0.7 * sin(p.x * 1.9) + 0.35 * sin(p.x * 4.7 + 1.0);
        colour = mix(colour, vec3(0.03, 0.07, 0.07), step(p.y, treeTops));       // far trees
        float hedgeTop = 1.2 + 0.15 * sin(p.x * 6.0);
        colour = mix(colour, vec3(0.05, 0.12, 0.08), step(p.y, hedgeTop));       // near hedge
        float path = step(abs(p.x), 1.2 - 0.6 * p.y) * step(p.y, 1.0);           // stone path, narrower far away
        colour = mix(colour, vec3(0.20, 0.22, 0.24), path * 0.8);
        // Fireflies: one random spot per half-metre cell, only some cells get one.
        vec2 cell = floor(p * 2.0);
        float h = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
        vec2 spot = cell + vec2(fract(h * 13.1), fract(h * 7.7));
        float fly = smoothstep(0.08, 0.0, length(p * 2.0 - spot)) * step(0.85, h) * step(p.y, 4.0);
        colour += vec3(0.9, 0.8, 0.3) * fly * (0.6 + 0.4 * sin(time * 3.0 + h * 20.0));
        // Through the closed bars the garden looks dim; the open gate lets the moonlight in.
        FragColor = vec4(colour * mix(0.55, 1.0, gateLift), 1.0);
        return;
    }
    if (Material == DARKNESS) {
        // Deep recesses stay dark even when the camera moves closer.
        FragColor = vec4(vec3(0.010, 0.017, 0.031), 1.0);
        return;
    }

    // The normal must face the camera. Culling already hides back sides, but the
    // light shafts, the cutaway foreground and the B view draw both sides.
    vec3 n = normalize(Normal);
    if (dot(n, cameraPos - FragPos) < 0.0) n = -n;

    // ---- Walls: parallax occlusion mapping --------------------------------
    vec3 wallColour = vec3(0.0);
    float wallShadow = 1.0;
    if (Material == WALL) {
        vec2 uv;
        vec3 T, B;
        wallFrame(FragPos, n, uv, T, B);
        // The view direction in the wall's own axes (tangent space).
        vec3 V = normalize(cameraPos - FragPos);
        vec3 viewTS = vec3(dot(V, T), dot(V, B), dot(V, n));
        vec2 dx = dFdx(uv), dy = dFdy(uv);
        vec2 pomUV = parallaxOn ? parallaxUV(uv, viewTS, dx, dy) : uv;
        // POM step 7: self-shadowing, measured along the light in the wall's own axes.
        // lightTS uses the flat wall normal, before the normal map below changes n.
        vec3 L = normalize(lightDir);
        vec3 lightTS = vec3(dot(L, T), dot(L, B), dot(L, n));
        if (parallaxOn) wallShadow = parallaxShadow(pomUV, depthAt(pomUV, dx, dy), lightTS, dx, dy);
        // POM step 6: colour and bumps of the stone at the point the eye really sees.
        wallColour = textureGrad(stoneTexture, pomUV, dx, dy).rgb;
        vec3 bump = textureGrad(normalMap, pomUV, dx, dy).rgb * 2.0 - 1.0;   // 0..1 -> -1..1
        n = normalize(T * bump.x + B * bump.y + n * bump.z);
    }

    // ---- Base colour and shine of each material ----------------------------
    float variation = stoneVariation(FragPos, n);
    vec3 base = vec3(0.31, 0.40, 0.53) * variation;   // plain stone: columns, frames, galleries
    float specularStrength = 0.07;
    float shininess = 40.0;                          // lesson 16: bigger = smaller, sharper highlight
    vec3 specularColour = vec3(0.40, 0.52, 0.67);    // lesson 16: cool moonlight; gold reflects warm
    if (Material == WALL) {
        base = wallColour;
        specularStrength = 0.15;
    }
    if (Material == FLOOR) {
        // Lesson 16: polished black marble slabs, 4 x 4 m, with thin gold lines between them.
        vec2 q = FragPos.xz / 4.0;                   // position in slabs
        vec2 f = fract(q);
        vec2 edge = min(f, 1.0 - f);                 // distance to the nearest joint
        vec2 aa = max(fwidth(q), vec2(0.0001));      // one pixel, measured in slabs: soft, no flicker
        float slab = min(smoothstep(vec2(0.008), vec2(0.008) + aa, edge).x,
                         smoothstep(vec2(0.008), vec2(0.008) + aa, edge).y);   // 0 on a joint
        float checker = mod(floor(q.x) + floor(q.y), 2.0);   // every other slab a little lighter
        vec3 marble = mix(vec3(0.035, 0.035, 0.045), vec3(0.20, 0.19, 0.18),
                          checker * 0.25 + marbleVeins(FragPos) * 0.6);
        base = mix(vec3(0.62, 0.45, 0.17), marble, slab);
        specularStrength = 0.5;
        shininess = 120.0;
    }
    if (Material == MARBLE) {
        // Lesson 16: polished black marble with pale veins (columns).
        base = mix(vec3(0.04, 0.04, 0.05), vec3(0.32, 0.31, 0.30), marbleVeins(FragPos));
        specularStrength = 0.7;
        shininess = 90.0;
    }
    if (Material == BANNER_RED || Material == BANNER_PURPLE) {
        // Lesson 17: long banners, deep red and royal purple, with the gold sigil.
        vec3 cloth = Material == BANNER_RED ? vec3(0.34, 0.025, 0.045) : vec3(0.16, 0.035, 0.26);
        float folds = 0.85 + 0.15 * sin(UV.x * 40.0);
        base = mix(cloth * folds, vec3(0.78, 0.56, 0.22), sigil(4.0, 28.5));
        specularStrength = 0.0;
    }
    if (Material == GOLD) {
        // Lesson 16: gold trims and ribs. Metal: the highlight takes the colour of the metal.
        base = vec3(0.62, 0.45, 0.17);
        specularStrength = 1.0;
        shininess = 60.0;
        specularColour = vec3(1.0, 0.80, 0.45);
    }
    if (Material == BANNER) {
        // Dark red cloth with a gold border and a gold crest.
        base = vec3(0.28, 0.018, 0.038) * (0.85 + 0.15 * sin(UV.x * 19.0));
        float border = step(UV.x, 0.055) + step(0.945, UV.x) + step(UV.y, 0.025) + step(0.975, UV.y);
        float crest = abs(UV.x - 0.5) * 2.5 + abs(UV.y - 0.67) * 5.0;
        float emblem = 1.0 - smoothstep(0.04, 0.075, abs(crest - 0.55));
        base = mix(base, vec3(0.64, 0.43, 0.18), clamp(border + emblem, 0.0, 1.0));
        specularStrength = 0.0;
    }
    if (Material == CARPET) {
        // Red carpet with gold edges and a diamond pattern every 4 metres.
        base = vec3(0.36, 0.016, 0.036);
        float edge = abs(FragPos.z);
        float trim = smoothstep(2.58, 2.61, edge) * (1.0 - smoothstep(2.78, 2.81, edge));
        float pattern = abs(fract(FragPos.x / 4.0) - 0.5) * 2.0 + abs(FragPos.z) * 0.7;
        float ornament = (1.0 - smoothstep(0.035, 0.07, abs(pattern - 0.8))) * 0.36;
        base = mix(base, vec3(0.60, 0.38, 0.12), max(trim, ornament));
        specularStrength = 0.0;
    }
    if (Material == BRASS) {
        base = vec3(0.45, 0.29, 0.10);
        specularStrength = 0.8;
    }
    if (Material == FLAME) {
        // Candle flames glow by themselves and flicker a little.
        // Lesson 15: brighter than white (HDR), so the post pass makes them glow.
        FragColor = vec4(vec3(1.0, 0.72, 0.31) * 3.0 * (0.94 + 0.06 * sin(time * 4.0 + FragPos.x)), 1.0);
        return;
    }
    if (Material == WINDOW) {
        FragColor = vec4(vec3(0.48, 0.63, 0.76) * 1.3, 1.0);   // night sky behind the glass; lesson 15: glows a little
        return;
    }
    if (Material == DOOR) {
        float grain = 0.85 + 0.15 * sin(FragPos.x * 25.0 + sin(FragPos.y));   // wood grain
        base = vec3(0.11, 0.085, 0.07) * grain;
    }
    if (Material == KNIGHT) {
        base = vec3(0.72, 0.79, 0.83);   // polished steel
        specularStrength = 0.45;
    }
    // The dark knight: blackened plate, a near-black cloak with a dull red lining, a bright blade.
    // The same colours as the knight cut-out in intro.frag.
    if (Material == DARK_STEEL) {
        base = vec3(0.16, 0.17, 0.20);
        specularStrength = 0.6;
        shininess = 60.0;
    }
    if (Material == CLOAK) {
        base = vec3(0.05, 0.05, 0.065) * (0.8 + 0.2 * sin(FragPos.x * 90.0));   // folds
        specularStrength = 0.0;
    }
    if (Material == CLOAK_LINING) {
        base = vec3(0.22, 0.03, 0.04) * (0.8 + 0.2 * sin(FragPos.x * 90.0));
        specularStrength = 0.0;
    }
    if (Material == BLADE) {
        base = vec3(0.42, 0.46, 0.52);
        specularStrength = 0.9;
        shininess = 80.0;
    }

    // ---- Light --------------------------------------------------------------
    vec3 keyDir = normalize(lightDir);
    vec3 viewDir = normalize(cameraPos - FragPos);
    float diffuse = max(dot(n, keyDir), 0.0) * wallShadow;       // Lambert, darkened by POM shadow
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 3.0) * 0.10;   // thin light on the edges
    vec3 colour = base * (vec3(0.14, 0.17, 0.24) + vec3(1.10, 1.12, 1.18) * diffuse + rim);
    float spec = pow(max(dot(n, normalize(keyDir + viewDir)), 0.0), shininess) * wallShadow;   // Blinn-Phong
    colour += specularColour * spec * specularStrength;

    // A warm local pool below the chandelier, against the cool high windows.
    vec3 toLamp = vec3(0, 31, 0) - FragPos;
    float warm = 1.0 / (1.0 + dot(toLamp, toLamp) * 0.0025);
    float lampDiffuse = max(dot(n, normalize(toLamp)), 0.0);
    colour += base * vec3(0.95, 0.42, 0.11) * warm * (0.12 + lampDiffuse);

    // Fog depends mostly on depth into the hall; zooming out won't erase the scene.
    float depth = clamp((-FragPos.z + 5.0) / 60.0, 0.0, 1.0);
    float lowMist = exp(-max(FragPos.y, 0.0) * 0.23) * 0.13;
    float fog = min(0.28, depth * 0.13 + lowMist);
    colour = mix(colour, vec3(0.075, 0.12, 0.18), fog);
    FragColor = vec4(colour, 1.0);
}

// Lesson 15: we picked the colours by eye on the monitor, so they are "gamma-encoded".
// Light adds up and blurs correctly only as energy: decode here, once for every material.
// post.frag encodes back for the monitor at the very end.
void main() {
    shade();
    FragColor.rgb = pow(max(FragColor.rgb, 0.0), vec3(2.2));
}
