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
// That difference IS the parallax. The camera always looks at the castle.
in vec2 ndc;
out vec4 FragColor;

uniform vec2 resolution;   // window size in pixels
uniform float time;        // seconds
uniform vec3 camPos;       // the eye, in metres. y = height, z gets smaller when we walk forward
uniform float fade;        // 0..1: after START the picture fades to black

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

void main() {
    // The camera: it stands at camPos and always looks at the castle.
    vec3 ro = camPos;
    vec3 forward = normalize(vec3(0.0, 21.0, -D_CASTLE) - ro);
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

    // 2. The cut-outs, from far to near. A cut-out only shows above the ground (p.y >= 0).
    float t, px; vec2 p;
    layerPoint(D_MOUNTAIN, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = mountainLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    layerPoint(D_HILLS, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = hillLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    layerPoint(D_CASTLE, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) { vec4 l = castleLayer(p, px); col = mix(col, withFog(l.rgb, t), l.a); }

    // Trees stand to the sides and leave the castle and the trail free.
    layerPoint(D_FOREST, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 4.5, 8.0, 18.0, 1.0, 0.0, 36.0, 0.95, lin(vec3(0.025, 0.04, 0.06)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    layerPoint(D_MID, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 7.0, 9.0, 18.0, 2.0, pathX(D_MID), 13.0, 0.8, lin(vec3(0.015, 0.025, 0.04)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    layerPoint(D_NEAR, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = pineLayer(p, px, 9.0, 12.0, 22.0, 3.0, pathX(D_NEAR), 5.5, 0.75, lin(vec3(0.008, 0.013, 0.022)));
        col = mix(col, withFog(l.rgb, t), l.a);
    }
    layerPoint(D_GRASS, ro, rd, t, p, px);
    if (t > 0.0 && p.y >= 0.0) {
        vec4 l = grassLayer(p, px, pathX(D_GRASS), pathHalfWidth(D_GRASS) + 0.3);
        col = mix(col, l.rgb, l.a);
    }

    FragColor = vec4(col * (1.0 - fade), 1.0);
}
