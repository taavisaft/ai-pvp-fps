#version 330 core
// Shadow pass: render scene depth from the sun's point of view. Position only.
layout(location = 0) in vec3 aPos;
uniform mat4 lightSpace;   // ortho light proj * light view
uniform mat4 model;
void main() {
    gl_Position = lightSpace * model * vec4(aPos, 1.0);
}
