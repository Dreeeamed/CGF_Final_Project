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
          FLAME = 6, WINDOW = 7, DOOR = 8, BEAM = 9, KNIGHT = 10, WALL = 11, GARDEN = 12;

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

// Large, soft brightness variation of plain stone (triplanar: no UVs needed). Not POM.
float stoneVariation(vec3 p, vec3 n) {
    vec3 weights = pow(abs(n), vec3(4.0));
    weights /= max(weights.x + weights.y + weights.z, 0.0001);
    vec3 colour = texture(stoneTexture, p.yz * 0.14).rgb * weights.x
                + texture(stoneTexture, p.xz * 0.14).rgb * weights.y
                + texture(stoneTexture, p.xy * 0.14).rgb * weights.z;
    return mix(0.86, dot(colour, vec3(0.2126, 0.7152, 0.0722)), 0.32);
}

void main() {
    // Debug view (key B): the back side of a triangle is painted red.
    if (showBackFaces && !gl_FrontFacing) { FragColor = vec4(1.0, 0.0, 0.0, 1.0); return; }

    // ---- Materials that are not lit -------------------------------------
    if (Material == BEAM) {
        // Soft stylized shafts: transparent geometry, not volumetric ray marching.
        float edge = smoothstep(0.0, 0.28, UV.x) * smoothstep(0.0, 0.28, 1.0 - UV.x);
        float ends = smoothstep(0.0, 0.12, UV.y) * smoothstep(0.0, 0.10, 1.0 - UV.y);
        float dust = 0.88 + 0.12 * sin(FragPos.y * 0.7 + time * 0.35);
        FragColor = vec4(0.38, 0.56, 0.72, 0.09 * edge * ends * dust);
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
    if (Material == WALL) {
        base = wallColour;
        specularStrength = 0.15;
    }
    if (Material == FLOOR) {
        // Large quartz-grey slabs, with subtle joints in world metres.
        vec2 q = FragPos.xz / 3.0;
        vec2 f = fract(q);
        vec2 edge = min(f, 1.0 - f);
        vec2 aa = max(fwidth(q), vec2(0.0001));
        float joint = min(smoothstep(vec2(0.008), vec2(0.008) + aa, edge).x,
                          smoothstep(vec2(0.008), vec2(0.008) + aa, edge).y);
        float checker = mod(floor(q.x) + floor(q.y), 2.0);
        base = mix(vec3(0.12, 0.15, 0.20), vec3(0.27, 0.33, 0.41) * (0.96 + checker * 0.04), joint);
        base *= 0.93 + 0.07 * variation;
        specularStrength = 0.18;
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
        FragColor = vec4(vec3(1.0, 0.72, 0.31) * (0.94 + 0.06 * sin(time * 4.0 + FragPos.x)), 1.0);
        return;
    }
    if (Material == WINDOW) {
        FragColor = vec4(0.48, 0.63, 0.76, 1.0);   // night sky behind the glass
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

    // ---- Light --------------------------------------------------------------
    vec3 keyDir = normalize(lightDir);
    vec3 viewDir = normalize(cameraPos - FragPos);
    float diffuse = max(dot(n, keyDir), 0.0) * wallShadow;       // Lambert, darkened by POM shadow
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 3.0) * 0.10;   // thin light on the edges
    vec3 colour = base * (vec3(0.14, 0.17, 0.24) + vec3(1.10, 1.12, 1.18) * diffuse + rim);
    float spec = pow(max(dot(n, normalize(keyDir + viewDir)), 0.0), 40.0) * wallShadow;   // Blinn-Phong
    colour += vec3(0.40, 0.52, 0.67) * spec * specularStrength;

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
