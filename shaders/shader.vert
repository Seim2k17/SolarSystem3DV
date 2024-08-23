#version 450

layout(location = 0) in vec2 inPosition; // vertex attributes, properties specified per-vertex in the vertex buffer
// annotation assign indices to the inputs that we can later use to reference them
// some types use multiplpe slots (index must at least be 2 or higher)
//layout(location = 2) in vec3 inColor;
layout(location = 1) in vec3 inColor; // input taken from a vertex buffer

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
