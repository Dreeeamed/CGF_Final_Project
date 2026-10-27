#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 UV;
flat in int Material;
out vec4 FragColor;
uniform sampler2D stoneTexture;
uniform vec3 cameraPos;
uniform float time;
uniform sampler2D heightMap;
uniform bool parallaxOn;

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

float stoneVariation(vec3 p, vec3 n) {
    // World-space triplanar colour; this is not POM.
    vec3 weights = pow(abs(n),vec3(4.0));
    weights /= max(weights.x+weights.y+weights.z,0.0001);
    vec3 colour = texture(stoneTexture,p.yz*0.14).rgb*weights.x
                + texture(stoneTexture,p.xz*0.14).rgb*weights.y
                + texture(stoneTexture,p.xy*0.14).rgb*weights.z;
    return mix(0.86,dot(colour,vec3(0.2126,0.7152,0.0722)),0.32);
}

void main() {
    // Soft stylized shafts: transparent geometry, not volumetric ray marching.
    if (Material == 9) {
        float edge = smoothstep(0.0,0.28,UV.x)*smoothstep(0.0,0.28,1.0-UV.x);
        float ends = smoothstep(0.0,0.12,UV.y)*smoothstep(0.0,0.10,1.0-UV.y);
        float dust = 0.88+0.12*sin(FragPos.y*0.7+time*0.35);
        FragColor = vec4(0.38,0.56,0.72,0.09*edge*ends*dust);
        return;
    }
    if (Material == 2) {
        // Deep recesses stay dark even when the camera moves closer.
        FragColor = vec4(vec3(0.010,0.017,0.031),1.0);
        return;
    }
    vec3 n = normalize(Normal);
    if (dot(n,cameraPos-FragPos)<0.0) n=-n;
    if (Material == 11) {
        // POM step 2: simple parallax - shift the UV along the view ray by the depth under it.
        vec2 uv;
        vec3 T, B;
        wallFrame(FragPos, n, uv, T, B);
        vec3 V = normalize(cameraPos - FragPos);
        vec3 viewTS = vec3(dot(V, T), dot(V, B), dot(V, n));
        const float heightScale = 0.05;  // deepest groove: 0.05 of a 4 m tile = 20 cm
        float depth = 1.0 - texture(heightMap, uv).r;  // 0 = stone face, 1 = deepest groove
        vec2 shiftedUV = uv;
        if(parallaxOn){
            shiftedUV = uv - viewTS.xy/viewTS.z * depth * heightScale ;
        }
        float h = texture(heightMap, shiftedUV).r;
        FragColor = vec4(vec3(h), 1.0);
        return;
    }
    float variation = stoneVariation(FragPos,n);
    vec3 base = vec3(0.31,0.40,0.53)*variation;
    float specularStrength = 0.07;
    if (Material == 1) {
        // Large quartz-grey slabs, with subtle joints in world metres.
        vec2 q = FragPos.xz/3.0;
        vec2 f = fract(q);
        vec2 edge = min(f,1.0-f);
        vec2 aa = max(fwidth(q),vec2(0.0001));
        float joint = min(smoothstep(vec2(0.008),vec2(0.008)+aa,edge).x,
                          smoothstep(vec2(0.008),vec2(0.008)+aa,edge).y);
        float checker = mod(floor(q.x)+floor(q.y),2.0);
        base = mix(vec3(0.12,0.15,0.20),vec3(0.27,0.33,0.41)*(0.96+checker*0.04),joint);
        base *= 0.93+0.07*variation;
        specularStrength = 0.18;
    }
    if (Material == 3) {
        base = vec3(0.28,0.018,0.038)*(0.85+0.15*sin(UV.x*19.0));
        float border = step(UV.x,0.055)+step(0.945,UV.x)+step(UV.y,0.025)+step(0.975,UV.y);
        float crest = abs(UV.x-0.5)*2.5+abs(UV.y-0.67)*5.0;
        float emblem = 1.0-smoothstep(0.04,0.075,abs(crest-0.55));
        base = mix(base,vec3(0.64,0.43,0.18),clamp(border+emblem,0.0,1.0));
        specularStrength = 0.0;
    }
    if (Material == 4) {
        base = vec3(0.36,0.016,0.036);
        float edge = abs(FragPos.z);
        float trim = smoothstep(2.58,2.61,edge)*(1.0-smoothstep(2.78,2.81,edge));
        float pattern = abs(fract(FragPos.x/4.0)-0.5)*2.0 + abs(FragPos.z)*0.7;
        float ornament = (1.0-smoothstep(0.035,0.07,abs(pattern-0.8)))*0.36;
        base = mix(base,vec3(0.60,0.38,0.12),max(trim,ornament));
        specularStrength = 0.0;
    }
    if (Material == 5) {
        base = vec3(0.45,0.29,0.10);
        specularStrength = 0.8;
    }
    if (Material == 6) {
        FragColor = vec4(vec3(1.0,0.72,0.31)*(0.94+0.06*sin(time*4.0+FragPos.x)),1.0);
        return;
    }
    if (Material == 7) {
        FragColor = vec4(0.48,0.63,0.76,1.0);
        return;
    }
    if (Material == 8) {
        float grain = 0.85+0.15*sin(FragPos.x*25.0+sin(FragPos.y));
        base = vec3(0.11,0.085,0.07)*grain;
    }
    if (Material == 10) {
        base = vec3(0.72,0.79,0.83);
        specularStrength = 0.45;
    }
    vec3 keyDir = normalize(vec3(-0.65,0.85,0.6));
    vec3 viewDir = normalize(cameraPos-FragPos);
    float diffuse = max(dot(n,keyDir),0.0);
    float rim = pow(1.0-max(dot(n,viewDir),0.0),3.0)*0.10;
    vec3 colour = base*(vec3(0.33,0.40,0.52)+vec3(0.80,0.84,0.91)*diffuse+rim);
    float spec = pow(max(dot(n,normalize(keyDir+viewDir)),0.0),40.0);
    colour += vec3(0.40,0.52,0.67)*spec*specularStrength;
    // A warm local pool below the chandelier, against the cool high windows.
    vec3 toLamp = vec3(0,31,0)-FragPos;
    float warm = 1.0/(1.0+dot(toLamp,toLamp)*0.0025);
    float lampDiffuse = max(dot(n,normalize(toLamp)),0.0);
    colour += base*vec3(0.95,0.42,0.11)*warm*(0.12+lampDiffuse);
    // Fog depends mostly on depth into the hall; zooming out won't erase the scene.
    float depth = clamp((-FragPos.z+5.0)/60.0,0.0,1.0);
    float lowMist = exp(-max(FragPos.y,0.0)*0.23)*0.13;
    float fog = min(0.28,depth*0.13+lowMist);
    colour = mix(colour,vec3(0.075,0.12,0.18),fog);
    FragColor = vec4(colour,1.0);
}
