#version 330 core
in vec3 worldPos;
uniform vec3 color;
uniform float alpha;
uniform int lit;        // 1 = world pass (lighting/grid/fog), 0 = HUD
uniform vec3 eyePos;

uniform sampler2D diffuseMap;
uniform int useTexture;   // 0 = flat color, 1 = triplanar sample
uniform float tileSize;   // world meters per repeat
uniform float specular;     // view-aligned highlight strength
uniform vec3 tint;

out vec4 fragColor;

vec3 triplanar(vec3 p, float tile) {
    vec3 an = abs(normalize(cross(dFdx(p), dFdy(p))));
    an /= max(an.x + an.y + an.z, 1e-4);
    vec2 uvX = p.yz / tile;
    vec2 uvY = p.xz / tile;
    vec2 uvZ = p.xy / tile;
    return texture(diffuseMap, uvX).rgb * an.x
         + texture(diffuseMap, uvY).rgb * an.y
         + texture(diffuseMap, uvZ).rgb * an.z;
}

void main() {
    vec3 c = color;
    if (lit == 1 && useTexture != 0) {
        c = triplanar(worldPos, tileSize) * tint;
    }

    if (lit == 1) {
        vec3 n = normalize(cross(dFdx(worldPos), dFdy(worldPos)));
        float ndl = max(dot(n, normalize(vec3(0.5, 0.8, 0.3))), 0.0);
        c *= 0.55 + 0.45 * ndl;

        if (specular > 0.0) {
            vec3 V = normalize(eyePos - worldPos);
            float spec = pow(max(dot(n, V), 0.0), 24.0) * specular;
            c += vec3(spec);
        }

        float fog = clamp(length(worldPos - eyePos) / 120.0, 0.0, 1.0);
        c = mix(c, vec3(0.1, 0.1, 0.15), fog * fog);
    }
    fragColor = vec4(c, alpha);
}
