#version 330 core
// Vegetation lighting: same 3-term daylight + shadow + fog + grade as basic.frag,
// minus the splat/triplanar machinery. Adds the screen-door LOD cross-fade and
// two-sided normals (blades and cone skirts are drawn without face culling).
in vec3  worldPos;
in vec3 treeLocal;
in vec3 treeUnit;
uniform int trainingTree;
in vec3  vNormal;
in vec3  vColor;
in vec2  vUV;
in vec4  lightSpacePos;
in float vCutNear;
in float vCutFar;

uniform vec3  eyePos;
uniform float time;
uniform int   bake;         // 1 = impostor bake: output raw albedo, no light/fog

uniform vec3  sunDir;
uniform vec3  sunColor;
uniform vec3  skyZenith;
uniform vec3  skyHorizon;
uniform vec3  groundAmbient;
uniform float fogDist;
uniform float fogHeightAmt;
uniform float cloudAmount;
uniform float exposure;
uniform float saturation;

uniform sampler2D shadowMap;
uniform sampler2D branchTex;   // needle-spray photo, alpha cutout
uniform int       useShadow;
uniform int       alphaToCoverage;

out vec4 fragColor;
#include "atmosphere.glsl"

float bayer(vec2 p) {
    // 4x4 ordered-dither threshold, stable per screen pixel: complementary LOD
    // draws split pixels instead of blending, so no sorting and no double-cover.
    int x = int(mod(p.x, 4.0));
    int y = int(mod(p.y, 4.0));
    int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[y * 4 + x]) + 0.5) / 16.0;
}

#include "shadow_bias.glsl"
float sunVisibility(vec3 n, vec3 L, bool foliage) {
    if (useShadow == 0) return 1.0;
    vec3 p = lightSpacePos.xyz / lightSpacePos.w;
    p = p * 0.5 + 0.5;
    if (p.z > 1.0 || p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
    float bias = shadowBias(n, L, 1.5);
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    if (foliage) {
        float lit = 0.0;
        for (int x = 0; x <= 1; x++)
            for (int y = 0; y <= 1; y++)
                lit += (p.z - bias > texture(shadowMap, p.xy + (vec2(x, y) - 0.5) * texel).r) ? 0.0 : 1.0;
        return lit * 0.25;
    }
    float vis = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            float d = texture(shadowMap, p.xy + vec2(x, y) * texel).r;
            vis += (p.z - bias > d) ? 0.0 : 1.0;
        }
    return vis / 9.0;
}

#include "cloud_shadow.glsl"

vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

// Approximate canopy occlusion in undeformed tree space. Stable through wind,
// instance scaling and impostor baking; no extra shadow pass or texture lookup.
float canopyAccess() {
    if(trainingTree==1) {
        float radius=max(.025,.25*pow(max(0.0,1.0-treeUnit.y),.8));
        return mix(.30,1.0,smoothstep(.12,1.0,length(treeUnit.xz)/radius));
    }
    float width=trainingTree==3 ? .29 : trainingTree==4 ? .48 : .41;
    vec3 q=(treeUnit-vec3(0,.58,0))/vec3(width,.46,width);
    return mix(.20,1.0,smoothstep(.22,1.05,length(q)));
}

uniform float clipWater;
void main() {
    if(clipWater>0.0 && worldPos.y<clipWater) discard;
    // Branch cards: photo albedo, cut out by alpha. Trunks (uv sentinel
    // -1) shade from the vertex color alone. vColor is the card's shade jitter.
    vec3 albedo = vColor;
    float coverage = 1.0;
    if (vUV.x >= 0.0) {
        vec4 t = texture(branchTex, vUV);
        float cut = trainingTree>0 ? 0.30 : 0.42;
        coverage = clamp((t.a - cut) / max(fwidth(t.a), 0.0001) + 0.5, 0.0, 1.0);
        if (coverage < (alphaToCoverage == 1 ? 0.004 : 0.5)) discard;
        albedo *= t.rgb;
    }
    if(trainingTree>0 && vUV.x<0.0) {
        // World-scale bark fissures and small grey lichen patches, filtered out
        // before they become sub-pixel. No displacement of the collision trunk.
        float a=atan(treeLocal.z,treeLocal.x);
        vec2 q=vec2(a*22.0,treeLocal.y*4.0);
        float fine=1.0-smoothstep(.25,1.3,length(fwidth(q)));
        float ridge=vnoise(q+vec2(vnoise(q*.3)*2.0,0));
        float crack=smoothstep(.28,.46,ridge);
        albedo*=mix(1.0,.55+.65*crack,fine);
        float lichen=smoothstep(.65,.82,vnoise(vec2(a*4.0,treeLocal.y*9.0)));
        albedo=mix(albedo,vec3(.30,.315,.255),lichen*.32*fine);
        if(trainingTree==3) {
            // Broken horizontal lenticels and peeling patches on birch bark.
            float scars=smoothstep(.63,.79,vnoise(vec2(a*2.5,treeLocal.y*28.0)));
            albedo=mix(albedo,vec3(.12,.13,.115),scars*.80*fine);
        }
    }

    float access=1.0;
    bool crown=trainingTree>0 && vUV.x>=0.0;
    if(crown) {
        access=canopyAccess();
        albedo*=trainingTree>1 ? vec3(.74,.93,.66) : vec3(.96);
    }
    if (bake == 1) {   // Retain canopy depth in the distant albedo capture.
        fragColor = vec4(albedo*(crown ? mix(.36,.87,access) : 1.0), 1.0);
        return;
    }

    float th = bayer(gl_FragCoord.xy);
    if (th < vCutNear || th >= vCutFar) discard;

    vec3 V = normalize(eyePos - worldPos);
    vec3 n = normalize(vNormal);
    if (vUV.x >= -1.5 && !(trainingTree>0 && vUV.x>=0.0) && dot(n, V) < 0.0) n = -n;
    vec3 L = normalize(sunDir);

    float cloud = cloudShadow(worldPos.xz, time);
    float visible = sunVisibility(n, L, crown);
    vec3 sun     = sunColor * max(dot(n, L), 0.0) * visible * cloud;
    vec3 ambient = mix(groundAmbient, skyZenith, n.y * 0.5 + 0.5);
    vec3 lit3    = albedo * (sun + ambient);
    if(trainingTree>0 && vUV.x>=0.0) {
        // A spray is a volume of needles, not a sheet that turns black from below.
        float diffuse=.55*max(dot(n,L),0.0)+.45*abs(dot(n,L));
        float transmitted=pow(max(dot(-L,V),0.0),3.0)*.10*access;
        vec3 needleSun=sunColor*(diffuse*visible*mix(.35,1.0,access)+transmitted)*cloud;
        // Interior foliage receives less sky fill and less direct/transmitted sun.
        lit3=albedo*(needleSun+mix(skyHorizon,skyZenith,.65)*mix(.32,.72,access));
    }
    float dens = 1.0 + fogHeightAmt * exp(-max(worldPos.y, 0.0) / 12.0);
    float fog  = clamp(length(worldPos - eyePos) * dens / fogDist, 0.0, 1.0);
    fragColor = vec4(grade(mix(lit3, fogColor(worldPos - eyePos), fog * fog)), coverage);
}
