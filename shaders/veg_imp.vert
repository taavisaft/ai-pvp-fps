#version 330 core
// Tree impostor: one camera-facing (cylindrical, Y-locked) quad per far tree,
// textured with the spruce baked at startup. Same instance layout as veg.vert.
layout(location = 0) in vec2 aCorner;  // x in [-0.5,0.5], y in [0,1]
layout(location = 1) in vec2 aUV;
layout(location = 4) in vec4 iA;       // world x,y,z + scale (tree height, m)
layout(location = 5) in vec4 iB;       // yaw, phase, brightness, unused

uniform mat4  view;
uniform mat4  proj;
uniform vec3  eyePos;
uniform vec2  impSize;   // world width/height of the baked box at scale 1
uniform vec2  fadeIn;    // dither-in band (complements mesh LOD1's fade-out)
uniform vec2  fadeOut;   // far dither-out band toward the impostor cap (x<y = active)

out vec3  worldPos;
out vec2  vUV;
out float vTint;
out float vCutNear;

void main() {
    vec3 toEye = eyePos - iA.xyz;
    toEye.y = 0.0;
    vec3 fwd   = normalize(toEye);
    vec3 right = vec3(-fwd.z, 0.0, fwd.x);
    vec3 wp = iA.xyz + right * (aCorner.x * impSize.x * iA.w)
                     + vec3(0.0, aCorner.y * impSize.y * iA.w, 0.0);
    worldPos = wp;
    vUV      = aUV;
    vTint    = iB.z;
    float dist = length(iA.xyz - eyePos);
    float cutNear = (fadeIn.y > fadeIn.x) ? 1.0 - smoothstep(fadeIn.x, fadeIn.y, dist) : 0.0;
    float cutFar  = (fadeOut.y > fadeOut.x) ? smoothstep(fadeOut.x, fadeOut.y, dist) : 0.0;
    // Threshold rises at both ends of the impostor's range; more fragments dither
    // away as the tree nears the near hand-off or the far cap. Nothing pops.
    vCutNear = max(cutNear, cutFar);
    gl_Position = proj * view * vec4(wp, 1.0);
}
