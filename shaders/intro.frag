#version 330 core
// Intro scene: the castle seen from far away across a plain, at night.
//
// How the parallax works. Nothing here is a 2D picture that is simply slid sideways.
// The world is a set of flat "cut-outs" standing at different distances in front of the camera:
//
//      far  mountains (330 m) -> hills (200 m) -> CASTLE (140 m) -> far trees (80 m)
//      near middle trees (34 m) -> near trees (13 m) -> grass (5.5 m)   close
//
// plus a real ground plane with a winding trail that leads to the castle gate.
// Every pixel shoots a ray from the camera, hits the ground and the cut-outs, and the layers
// are painted from far to near. When the camera moves sideways a little, a near cut-out sweeps
// across the picture while a far one barely moves, and the ground slides in perspective.
// That difference IS the parallax.
//
// The knight is a cut-out too. His plane moves with him along the trail, so he is painted
// in between the other layers at his own distance: a near tree or the grass covers him,
// and he covers the castle. The camera only slides, it never turns: pure 2.5D.
in vec2 ndc;
out vec4 FragColor;

uniform vec2 resolution;   // window size in pixels
uniform float time;        // seconds
uniform vec3 camPos;       // the eye, in metres. y = height, z gets smaller when we walk forward
uniform vec3 camTarget;    // the point the eye looks at
uniform float fade;        // 0..1: at the end of the walk the picture fades to black
uniform vec3 knightPos;    // his feet on the ground; -knightPos.z = his distance along the trail
uniform vec4 knightPose;   // x: facing (+1 front, -1 back, 0 edge-on), y: dip (m), z: rock (rad), w: breathing
uniform float knightStride;   // -1..1: >0 lifts his left foot, <0 his right one

// Colours are written like normal screen colours and turned into "light energy" with lin().
// The post-processing pass (post.frag) expects energy, like hall.frag gives it.
vec3 lin(vec3 c) { return pow(c, vec3(2.2)); }

const float FOV_TAN   = 0.5543;   // tan(29 deg): the vertical field of view is 58 degrees
const float FOG       = 0.004;    // how fast distant things melt into the haze

// Distances of the cut-outs from the origin, along -z.
const float D_MOUNTAIN = 330.0;
const float D_HILLS    = 200.0;
const float D_CASTLE   = 140.0;
const float D_FOREST   = 80.0;
const float D_MID      = 34.0;
const float D_NEAR     = 13.0;
const float D_GRASS    = 5.5;

// ---------------------------------------------------------------------------
// Noise
// ---------------------------------------------------------------------------
float hash11(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x),
               mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < 4; i++) {
        sum += amp * noise(p);
        p = p * 2.03 + vec2(17.1, 9.2);
        amp *= 0.5;
    }
    return sum;
}

// 1 inside a shape (d < 0), 0 outside, a soft one-pixel edge in between. px = pixel size in metres.
float cover(float d, float px) { return 1.0 - smoothstep(-px, px, d); }

// ---------------------------------------------------------------------------
// The trail. s = how many metres ahead of the origin.
// It starts under the camera, bends right, then left, and runs straight into the castle gate.
// ---------------------------------------------------------------------------
float pathX(float s) {
    float amp = 10.0 * smoothstep(0.0, 40.0, s) * (1.0 - smoothstep(60.0, D_CASTLE, s));
    return amp * sin(0.06 * s);
}
float pathHalfWidth(float s) { return 2.4 + 2.2 * smoothstep(105.0, D_CASTLE, s); }

// ---------------------------------------------------------------------------
// Sky
// ---------------------------------------------------------------------------
vec3 horizonColour() { return lin(vec3(0.17, 0.22, 0.36)); }

vec3 skyBase(vec3 d) {
    float h = clamp(d.y, 0.0, 1.0);
    vec3 c = mix(horizonColour(), lin(vec3(0.07, 0.10, 0.22)), smoothstep(0.0, 0.25, h));
    return mix(c, lin(vec3(0.015, 0.025, 0.07)), smoothstep(0.15, 0.9, h));
}

vec3 skyFull(vec3 d) {
    vec3 c = skyBase(d);
    vec3 moonDir = normalize(vec3(-0.38, 0.36, -0.85));
    float md = dot(d, moonDir);

    // Stars: a grid on the sky, one random star in some cells, twinkling.
    vec2 g = vec2(atan(d.x, -d.z), asin(clamp(d.y, -1.0, 1.0))) * 70.0;
    vec2 cell = floor(g);
    vec2 local = fract(g) - 0.5;
    vec2 offset = (vec2(hash21(cell + 3.1), hash21(cell + 7.7)) - 0.5) * 0.5;
    float r = hash21(cell);
    float star = step(0.975, r) * (1.0 - smoothstep(0.0, 0.18, length(local - offset)));
    star *= 0.6 + 0.4 * sin(time * 2.0 + r * 50.0);
    star *= smoothstep(0.02, 0.35, d.y);
    c += vec3(0.85, 0.92, 1.0) * star * 1.3;

    // Moon: a bright disc (more than 1.0, so the glow pass makes it shine) and a halo.
    float disc = smoothstep(0.99935, 0.99955, md) * (0.8 + 0.4 * noise(d.xy * 240.0));
    float halo = pow(max(md, 0.0), 400.0) * 0.35 + pow(max(md, 0.0), 60.0) * 0.07;
    c += vec3(1.0, 0.96, 0.85) * (disc * 3.5 + halo);

    // Thin clouds, lit from the moon's side. The sky is infinitely far: almost no parallax.
    vec2 cp = d.xz / max(d.y, 0.04) + vec2(camPos.x * 0.01, 0.0);
    float cloud = smoothstep(0.50, 0.78, fbm(cp * 0.55 + vec2(time * 0.012, 0.0)));
    cloud *= smoothstep(0.03, 0.22, d.y) * 0.55;
    vec3 cloudColour = lin(vec3(0.20, 0.25, 0.38)) + vec3(0.25, 0.25, 0.22) * pow(max(md, 0.0), 6.0);
    return mix(c, cloudColour, cloud);
}

// ---------------------------------------------------------------------------
// Cut-out layers. Every one gets the position on its own plane p = (x, y) in metres
// (y = 0 is the ground) and the size of one pixel on that plane, and returns colour + opacity.
// ---------------------------------------------------------------------------
vec4 mountainLayer(vec2 p, float px) {
    float ridge = 38.0 + 70.0 * fbm(vec2(p.x * 0.006 + 3.0, 1.0)) + 14.0 * fbm(vec2(p.x * 0.03, 7.0));
    float d = p.y - ridge;
    float rim = smoothstep(-14.0, 0.0, d);   // a little moonlight along the ridge
    vec3 c = mix(lin(vec3(0.05, 0.07, 0.14)), lin(vec3(0.17, 0.21, 0.33)), rim * 0.7);
    return vec4(c, cover(d, px));
}

vec4 hillLayer(vec2 p, float px) {
    float ridge = 8.0 + 24.0 * fbm(vec2(p.x * 0.012 + 11.0, 4.0)) + 3.0 * noise(vec2(p.x * 0.09, 2.0));
    float d = p.y - ridge;
    float rim = smoothstep(-6.0, 0.0, d);
    vec3 c = mix(lin(vec3(0.03, 0.05, 0.08)), lin(vec3(0.10, 0.14, 0.20)), rim * 0.6);
    return vec4(c, cover(d, px));
}

// --- the castle -------------------------------------------------------------
float sdBox(vec2 p, vec2 centre, vec2 half_) {
    vec2 q = abs(p - centre) - half_;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}

// A pointed roof: base at baseY, apex h above it.
float sdRoof(vec2 p, float cx, float baseY, float halfW, float h) {
    float slope = (abs(p.x - cx) * h + (p.y - baseY) * halfW - halfW * h) / length(vec2(h, halfW));
    return max(slope, baseY - p.y);
}

// Height of a wall top with battlements: a merlon every 3 m.
float battlement(float x, float top) { return top + 1.4 * step(fract(x / 3.0), 0.55); }

float gateSdf(vec2 p) {   // the arched gate in the middle of the keep
    return min(sdBox(p, vec2(0.0, 4.5), vec2(4.2, 4.5)), length(p - vec2(0.0, 9.0)) - 4.2);
}

float castleSdf(vec2 p) {
    float d = max(abs(p.x) - 56.0, p.y - battlement(p.x, 17.0));            // the long outer wall
    d = min(d, max(abs(p.x) - 15.0, p.y - battlement(p.x + 1.0, 33.0)));     // the keep
    d = min(d, sdBox(p, vec2(0.0, 30.0), vec2(6.5, 22.0)));                  // the tall central tower
    d = min(d, sdRoof(p, 0.0, 52.0, 8.5, 20.0));
    for (int side = -1; side <= 1; side += 2) {
        float x = float(side);
        d = min(d, sdBox(p, vec2(32.0 * x, 19.0), vec2(6.0, 19.0)));         // inner towers
        d = min(d, sdRoof(p, 32.0 * x, 38.0, 7.5, 15.0));
        d = min(d, sdBox(p, vec2(56.0 * x, 15.0), vec2(5.0, 15.0)));         // corner towers
        d = min(d, sdRoof(p, 56.0 * x, 30.0, 6.5, 12.0));
    }
    return d;
}

vec4 castleLayer(vec2 p, float px) {
    float d = castleSdf(p);
    float a = cover(d, px);
    if (a <= 0.001) return vec4(0.0);

    // Cold stone, a little lighter higher up, with a faint moonlit rim on every edge.
    float up = clamp(p.y / 55.0, 0.0, 1.0);
    vec3 c = mix(lin(vec3(0.06, 0.075, 0.11)), lin(vec3(0.17, 0.20, 0.28)), up * 0.6);
    c += lin(vec3(0.18, 0.22, 0.32)) * (1.0 - smoothstep(-2.2, 0.0, d)) * 0.6;
    c *= 0.88 + 0.12 * step(0.5, fract(p.y / 2.2));   // rows of stones

    // Windows: a grid, about half of them lit, flickering like candles.
    vec2 grid = p / vec2(4.5, 6.5);
    vec2 wc = floor(grid), wl = fract(grid) - 0.5;
    float lit = step(0.5, hash21(wc)) * step(6.0, p.y);
    float window = (1.0 - smoothstep(0.10, 0.14, abs(wl.x))) * (1.0 - smoothstep(0.22, 0.26, abs(wl.y)));
    window *= lit * (1.0 - smoothstep(-0.2, 0.0, d + 1.0));   // keep windows off the very edge of a wall
    float flicker = 0.8 + 0.2 * sin(time * (3.0 + 4.0 * hash21(wc + 9.0)) + hash21(wc) * 20.0);
    c += lin(vec3(1.0, 0.68, 0.30)) * window * 2.2 * flicker;

    // The heart of the castle: one tall window high in the central tower glows violet,
    // the same magic colour as the braziers in the throne hall and the crystal in the garden.
    vec2 hq = p - vec2(0.0, 45.0);
    float heart = (1.0 - smoothstep(0.9, 1.2, abs(hq.x))) * (1.0 - smoothstep(2.6, 3.0, abs(hq.y)));
    float beat = 0.8 + 0.2 * sin(time * 1.6);
    c = mix(c, lin(vec3(0.62, 0.26, 1.0)) * 3.2 * beat, heart);
    c += lin(vec3(0.62, 0.26, 1.0)) * exp(-dot(hq, hq) * 0.02) * 0.55 * beat;   // light on the stone round it

    // The gate: warm light from the courtyard behind iron bars, and a torch on each side.
    float gate = gateSdf(p);
    if (gate < 0.0) {
        float bars = smoothstep(0.07, 0.13, abs(fract(p.x * 0.9) - 0.5));
        vec3 glow = lin(vec3(1.0, 0.62, 0.28)) * (1.5 - 0.8 * clamp(p.y / 13.0, 0.0, 1.0));
        c = glow * mix(0.35, 1.0, bars);
    }
    for (int side = -1; side <= 1; side += 2) {
        vec2 q = p - vec2(6.8 * float(side), 7.5);
        float flame = exp(-dot(q, q) * 1.6) * (0.85 + 0.15 * sin(time * 9.0 + float(side) * 2.0));
        c += lin(vec3(1.0, 0.55, 0.2)) * flame * 2.5;
        c += lin(vec3(1.0, 0.5, 0.2)) * exp(-dot(q, q) * 0.08) * 0.35;   // light spilling on the stone
    }
    return vec4(c, a);
}

// --- pine trees: a row of them, with a gap where nothing may stand ---------------------
vec4 pineLayer(vec2 p, float px, float spacing, float hMin, float hMax, float seed,
               float gapCentre, float gapHalf, float density, vec3 colour) {
    float cell = floor(p.x / spacing);
    float dMin = 1e5;
    for (int k = -1; k <= 1; k++) {   // a tree can lean over into the next cell: look at the neighbours too
        float ci = cell + float(k);
        if (hash11(ci * 7.13 + seed) > density) continue;
        float cx = (ci + 0.2 + 0.6 * hash11(ci * 1.9 + seed + 11.0)) * spacing;
        if (abs(cx - gapCentre) < gapHalf) continue;
        float h = mix(hMin, hMax, hash11(ci * 3.7 + seed + 5.0));
        float w = spacing * 0.9 * (0.8 + 0.4 * hash11(ci + seed * 2.0));
        float tier = abs(fract(p.y / (h * 0.22)) - 0.5) * 2.0;           // the layers of branches
        float halfW = w * 0.5 * (1.0 - p.y / h) * (0.72 + 0.28 * tier);
        float d = max(abs(p.x - cx) - halfW, p.y - h);
        d = min(d, max(abs(p.x - cx) - 0.06 * w, p.y - h * 0.2));        // the trunk
        dMin = min(dMin, d);
    }
    return vec4(colour, cover(dMin, px));
}

vec4 grassLayer(vec2 p, float px, float gapCentre, float gapHalf) {
    float cellSize = 0.26;
    float cell = floor(p.x / cellSize);
    float sway = sin(time * 1.6 + cell) * p.y * 0.12;
    float fx = (fract(p.x / cellSize) - 0.5) * cellSize + sway;
    float h = 0.3 + 0.8 * hash11(cell * 1.7 + 4.0);
    float d = max(abs(fx) - 0.06 * (1.0 - p.y / h), p.y - h);
    if (hash11(cell * 5.3) > 0.8 || abs(p.x - gapCentre) < gapHalf) d = 1.0;
    return vec4(lin(vec3(0.012, 0.022, 0.03)), cover(d, px));
}

// --- the knight: an original dark knight, as a flat cut-out ------------------------------
// About 1.75 m tall: blackened plate armour, a closed round-topped helm with one eye slit,
// big round shoulder plates, a ragged dark cloak and a long sword held point-down.
// Simple flat shapes, shaded like the other cut-outs: dark, with a cold moonlit rim.
float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}
float sdRoundBox(vec2 p, vec2 centre, vec2 half_, float r) {
    vec2 q = abs(p - centre) - half_ + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
// A piece narrower at the bottom than at the top (or the other way round).
float sdTrapezoid(vec2 p, float y0, float y1, float half0, float half1) {
    float k = clamp((p.y - y0) / (y1 - y0), 0.0, 1.0);
    return max(abs(p.x) - mix(half0, half1, k), abs(p.y - 0.5 * (y0 + y1)) - 0.5 * (y1 - y0));
}

// Paint one part: colour c with coverage a over what is already there.
void part(inout vec4 acc, vec3 c, float a) { acc = mix(acc, vec4(c, 1.0), a); }

vec4 knightLayer(vec2 p, float px) {
    float facing = knightPose.x;
    float side = facing < 0.0 ? -1.0 : 1.0;   // -1: we see his back (everything is mirrored)
    float thin = max(abs(facing), 0.04);       // the turn: the card gets thin, then flips

    // To his own axes: feet at the origin, minus the dip, the rock and the breathing.
    vec2 q = p - vec2(knightPos.x, knightPose.y);
    float c = cos(knightPose.z), s = sin(knightPose.z);
    q = vec2(c * q.x + s * q.y, -s * q.x + c * q.y);
    q.y /= knightPose.w;
    q.x /= thin * side;
    float pxq = px / thin;                     // squeezing him makes his pixels wider
    bool front = side > 0.0;

    // ---- Colours: blackened steel, a near-black cloak with a dull red lining ----
    vec3 steel = lin(vec3(0.16, 0.17, 0.20));
    vec3 moonRim = lin(vec3(0.55, 0.64, 0.85));
    vec3 cloth = lin(vec3(0.05, 0.05, 0.065));
    vec3 lining = lin(vec3(0.22, 0.03, 0.04));
    // Round armour: x across a part says where its surface faces. The moon is ahead of him
    // (so behind him when he faces us) and to the left: his edges catch the cold light.
    float moonSide = front ? 0.35 : 1.0;

    // ---- The ragged cloak: hangs from the shoulders, sways with his steps and the wind ----
    float sway = 0.04 * knightStride + 0.015 * sin(time * 1.3 + q.y * 3.0);
    vec2 cq = q - vec2(sway * (1.4 - q.y), 0.0);
    float hem = 0.22 + 0.05 * abs(fract(cq.x * 7.0 + 0.3) - 0.5) * 2.0 + 0.03 * sin(cq.x * 23.0);
    float dCloak = max(sdTrapezoid(cq, 0.0, 1.38, 0.36, 0.22), hem - cq.y);
    float aCloak = cover(dCloak, pxq);
    float fold = 0.75 + 0.25 * sin(cq.x * 28.0 + cq.y * 2.0);
    vec3 cloakColour = cloth * fold * (0.6 + 0.6 * moonSide);
    // Seen from the front, only the edges of the cloak show, with the red lining inside.
    if (front) cloakColour = mix(cloakColour, lining * fold, 0.7);

    // ---- Legs: armoured, one lifts while the other stands ----
    float liftL = max(knightStride, 0.0) * 0.13;
    float liftR = max(-knightStride, 0.0) * 0.13;
    float dLegL = sdSegment(q, vec2(-0.09, 0.85), vec2(-0.10 - 0.02 * knightStride, 0.07 + liftL)) - 0.065;
    float dLegR = sdSegment(q, vec2( 0.09, 0.85), vec2( 0.10 - 0.02 * knightStride, 0.07 + liftR)) - 0.065;
    float dFeet = min(sdRoundBox(q, vec2(-0.10, 0.04 + liftL), vec2(0.085, 0.045), 0.03),
                      sdRoundBox(q, vec2( 0.10, 0.04 + liftR), vec2(0.085, 0.045), 0.03));
    float dLegs = min(min(dLegL, dLegR), dFeet);

    // ---- Body: plate skirt, breastplate (wide at the shoulders), shoulder plates, helm ----
    float dSkirt = sdTrapezoid(q, 0.70, 0.96, 0.21, 0.17);
    float dChest = sdTrapezoid(q, 0.94, 1.40, 0.15, 0.23);
    vec2 shL = vec2(-0.25, 1.33), shR = vec2(0.25, 1.33);
    float dShoulders = max(min(length(q - shL), length(q - shR)) - 0.115, 1.22 - q.y);
    float dHelm = sdRoundBox(q, vec2(0.0, 1.57), vec2(0.105, 0.17), 0.075);
    float dSlit = max(abs(q.x) - 0.075, abs(q.y - 1.58) - 0.011);
    float dRidge = max(abs(q.x) - 0.012, abs(q.y - 1.62) - 0.12);   // a raised ridge over the helm

    // ---- The sword: held point-down at his side ----
    float dBlade = max(abs(q.x - 0.34) - 0.022 * (0.4 + 0.6 * clamp((q.y - 0.12) / 0.2, 0.0, 1.0)),
                       abs(q.y - 0.52) - 0.42);
    float dGuard = sdRoundBox(q, vec2(0.34, 0.97), vec2(0.10, 0.018), 0.01);
    float dGrip = min(max(abs(q.x - 0.34) - 0.018, abs(q.y - 1.06) - 0.08), length(q - vec2(0.34, 1.16)) - 0.03);

    // Shading of one armour part from where x falls across it (-1..1) and how wide it is.
    #define ARMOUR(halfW, cx) (steel * (0.35 + 0.55 * sqrt(max(1.0 - pow(clamp((q.x - (cx)) / (halfW), -1.0, 1.0), 2.0), 0.0)) * moonSide) \
        + moonRim * 0.30 * pow(clamp(abs((q.x - (cx)) / (halfW)), 0.0, 1.0), 6.0))
    vec3 bladeColour = lin(vec3(0.42, 0.46, 0.52)) * (0.6 + 0.6 * moonSide);

    vec4 acc = vec4(0.0);
    if (front) {
        part(acc, cloakColour, aCloak);                         // the cloak behind him
        part(acc, ARMOUR(0.11, sign(q.x) * 0.1), cover(dLegs, pxq));
        part(acc, ARMOUR(0.20, 0.0) * 0.9, cover(dSkirt, pxq));
        part(acc, ARMOUR(0.21, 0.0), cover(dChest, pxq));
        part(acc, ARMOUR(0.12, sign(q.x) * 0.25), cover(dShoulders, pxq));
        part(acc, ARMOUR(0.105, 0.0), cover(dHelm, pxq));
        part(acc, ARMOUR(0.03, 0.0) * 1.3, cover(dRidge, pxq) * step(1.6, q.y));
        part(acc, lin(vec3(0.004)), cover(dSlit, pxq));         // the dark eye slit
        part(acc, bladeColour, cover(dBlade, pxq));
        part(acc, ARMOUR(0.1, 0.34), cover(min(dGuard, dGrip), pxq));
    } else {
        part(acc, ARMOUR(0.11, sign(q.x) * 0.1), cover(dLegs, pxq));
        part(acc, ARMOUR(0.21, 0.0), cover(min(dChest, dSkirt), pxq));
        part(acc, bladeColour, cover(dBlade, pxq));
        part(acc, ARMOUR(0.1, 0.34), cover(min(dGuard, dGrip), pxq));
        part(acc, cloakColour, aCloak);                         // the cloak covers his back
        part(acc, ARMOUR(0.12, sign(q.x) * 0.25), cover(dShoulders, pxq));
        part(acc, ARMOUR(0.105, 0.0), cover(dHelm, pxq));
        part(acc, ARMOUR(0.03, 0.0) * 1.3, cover(dRidge, pxq) * step(1.6, q.y));
    }
    #undef ARMOUR

    // The gate's warm light reaches him when he gets close.
    float warm = exp(-max(D_CASTLE - (-knightPos.z), 0.0) / 14.0);
    acc.rgb += acc.rgb * lin(vec3(1.0, 0.62, 0.28)) * 3.0 * warm;
    return acc;
}

// ---------------------------------------------------------------------------
// The ground: dark grass, and a dirt trail with two wheel ruts that leads to the gate.
// ---------------------------------------------------------------------------
vec3 groundColour(vec3 gp, float t) {
    float s = -gp.z;
    float dx = abs(gp.x - pathX(s));
    float hw = pathHalfWidth(s);
    float detail = 1.0 - smoothstep(25.0, 90.0, t);   // small details melt away with distance (no shimmering)

    vec3 grass = mix(lin(vec3(0.035, 0.06, 0.07)), lin(vec3(0.075, 0.115, 0.105)), fbm(gp.xz * 0.35));
    grass *= 0.8 + 0.4 * mix(0.5, noise(gp.xz * 2.5), detail);

    // The trail: a ragged edge, two ruts, a strip of weeds in the middle, a few pebbles.
    float edge = hw + (noise(vec2(s * 0.7, gp.x * 0.2)) - 0.5) * 0.9;
    float onPath = 1.0 - smoothstep(edge - 0.25, edge + 0.25, dx);
    vec3 dirt = lin(vec3(0.20, 0.17, 0.15)) * (0.8 + 0.4 * noise(gp.xz * vec2(1.5, 0.5)));
    float rut = exp(-pow((dx - hw * 0.42) / (hw * 0.14), 2.0));
    dirt *= 1.0 - 0.45 * rut * detail;
    dirt = mix(dirt, grass * 1.2, (1.0 - smoothstep(hw * 0.12, hw * 0.28, dx)) * 0.45);
    dirt += lin(vec3(0.25, 0.24, 0.24)) * step(0.82, noise(gp.xz * 7.0)) * 0.5 * detail;
    vec3 c = mix(grass, dirt, onPath);

    // Warm light from the gate spilling down the trail, in the last 25 metres.
    float spill = exp(-max(D_CASTLE - s, 0.0) / 16.0) * step(s, D_CASTLE + 6.0);
    c += lin(vec3(1.0, 0.55, 0.25)) * 0.5 * spill * exp(-dx * dx / (hw * hw * 5.0));

    // A soft dark spot under the knight, so he stands on the ground instead of floating.
    vec2 k = (gp.xz - knightPos.xz) * vec2(1.0, 1.4);
    c *= 1.0 - 0.6 * exp(-dot(k, k) / 0.16);
    return c;
}

// ---------------------------------------------------------------------------
vec3 withFog(vec3 colour, float t) {
    return mix(colour, horizonColour(), 1.0 - exp(-t * FOG));
}

// Where does the ray meet the flat layer at distance D? Everything the derivatives need is
// computed for every pixel (not inside an if), so the pixel size px is always valid.
void layerPoint(float D, vec3 ro, vec3 rd, out float t, out vec2 p, out float px) {
    t = (-D - ro.z) / rd.z;
    p = (ro + rd * t).xy;
    px = max(length(fwidth(p)), 1e-4) * 0.75;
}

// A sheet of low mist standing at distance D. It drifts sideways with time, and because
// it is one more layer at its own distance, it slides against its neighbours when the
// camera moves: the parallax is easier to see.
void paintMist(inout vec3 col, float D, vec3 ro, vec3 rd, float seed, float strength) {
    float t, px; vec2 p;
    layerPoint(D, ro, rd, t, p, px);
    if (t <= 0.0 || p.y < 0.0) return;
    float cloud = fbm(vec2(p.x * 0.035 + time * 0.035 + seed, p.y * 0.16 - time * 0.01));
    float low = 1.0 - smoothstep(0.0, 13.0, p.y);                // only near the ground
    col = mix(col, horizonColour() * 1.5, smoothstep(0.35, 0.85, cloud) * low * strength);
}

// Where the ray meets the knight's plane, and his cut-out painted over the picture.
void paintKnight(inout vec3 col, vec3 ro, vec3 rd) {
    float t, px; vec2 p;
    layerPoint(-knightPos.z, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = knightLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }
}

void main() {
    // The camera: it stands at camPos and looks at camTarget.
    vec3 ro = camPos;
    vec3 forward = normalize(camTarget - ro);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    float aspect = resolution.x / resolution.y;
    vec3 rd = normalize(forward + right * (ndc.x * aspect * FOV_TAN) + up * (ndc.y * FOV_TAN));

    // 1. Sky, then the ground (far to near order starts here).
    vec3 col = skyFull(rd);
    if (rd.y < -0.0005) {
        float t = -ro.y / rd.y;
        vec3 gp = ro + rd * t;
        vec3 g = groundColour(gp, t);
        float fogT = t * (1.0 + 0.6 * fbm(vec2(gp.x * 0.02 + time * 0.01, gp.z * 0.05)) * smoothstep(10.0, 90.0, t));
        col = withFog(g, fogT);
    }

    // 2. The cut-outs, from far to near. A cut-out only shows above the ground (p.y >= 0)
    //    and in front of the camera (t > 0). The knight is painted just before the first
    //    layer that is nearer than he is.
    float kd = -knightPos.z;
    bool knightDone = false;
    float t, px; vec2 p;
    layerPoint(D_MOUNTAIN, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = mountainLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    layerPoint(D_HILLS, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = hillLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    layerPoint(D_CASTLE, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = castleLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    paintMist(col, 112.0, ro, rd, 0.0, 0.55);   // between the castle and the far trees

    // Trees stand to the sides and leave the castle and the trail free.
    if (!knightDone && kd > D_FOREST) { paintKnight(col, ro, rd); knightDone = true; }
    layerPoint(D_FOREST, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 4.5, 8.0, 18.0, 1.0, 0.0, 36.0, 0.95, lin(vec3(0.025, 0.04, 0.06)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    paintMist(col, 56.0, ro, rd, 7.3, 0.40);    // between the far and the middle trees
    if (!knightDone && kd > D_MID) { paintKnight(col, ro, rd); knightDone = true; }
    layerPoint(D_MID, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 7.0, 9.0, 18.0, 2.0, pathX(D_MID), 13.0, 0.8, lin(vec3(0.015, 0.025, 0.04)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    if (!knightDone && kd > D_NEAR) { paintKnight(col, ro, rd); knightDone = true; }
    layerPoint(D_NEAR, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 9.0, 12.0, 22.0, 3.0, pathX(D_NEAR), 5.5, 0.75, lin(vec3(0.008, 0.013, 0.022)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    if (!knightDone) { paintKnight(col, ro, rd); knightDone = true; }
    layerPoint(D_GRASS, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = grassLayer(p, px, pathX(D_GRASS), pathHalfWidth(D_GRASS) + 0.3);
        col = mix(col, l.rgb, l.a);
    }

    FragColor = vec4(col * (1.0 - fade), 1.0);
}
