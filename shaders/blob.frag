#version 330 core
in vec2 vCorner;
in vec3 worldPos;
uniform vec3 eyePos;
out vec4 fragColor;

// Soft round ground decal. Faded IN by distance (so trees inside the real shadow-map
// range ~60 m keep their dappled shadow and don't double-darken), then faded OUT into
// the distance fog.
void main() {
    float r = length(vCorner);
    float a = (1.0 - smoothstep(0.55, 1.0, r)) * 0.40;   // soft falloff to the rim
    float d = length(worldPos - eyePos);
    a *= smoothstep(45.0, 65.0, d);          // only beyond the real shadow range
    a *= 1.0 - smoothstep(250.0, 340.0, d);  // dissolve into fog
    if (a <= 0.003) discard;
    fragColor = vec4(0.0, 0.0, 0.0, a);
}
