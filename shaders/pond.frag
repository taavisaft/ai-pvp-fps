#version 330 core
in vec3 worldPos;
in vec4 clipPos;
uniform vec3 eyePos;
uniform float time;
uniform sampler2D reflectionMap;
uniform float reflectionMix;
uniform vec3 skyZenith;
uniform vec3 skyHorizon;
uniform float exposure;
uniform float saturation;
out vec4 fragColor;
vec3 grade(vec3 c) {
    c *= exposure;
    c = mix(vec3(dot(c, vec3(0.299, 0.587, 0.114))), c, saturation);
    return clamp((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec2 p=worldPos.xz;
    vec3 n=normalize(vec3(.008*sin(p.x*1.3+p.y*.7+time*.8),1,
                           .012*sin(p.x*.8-p.y*1.8+time*.7)));
    vec3 v=normalize(eyePos-worldPos);
    float fresnel=.06+.94*pow(1-max(dot(n,v),0.0),5.0);
    vec2 uv=clipPos.xy/clipPos.w*.5+.5+n.xz*.05;
    vec3 sky=grade(mix(skyHorizon,skyZenith,clamp(v.y,0.0,1.0)));
    vec3 reflected=mix(sky,texture(reflectionMap,clamp(uv,.002,.998)).rgb,reflectionMix);
    vec3 water=vec3(.075,.105,.07);
    fragColor=vec4(mix(water,reflected,.48+.52*fresnel),1);
}
