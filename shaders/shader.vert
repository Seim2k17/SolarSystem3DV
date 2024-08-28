#version 450

// vertex attributes, properties specified per-vertex in the vertex buffer
// annotation assign indices to the inputs that we can later use to reference them
// some types use multiplpe slots (index must at least be 2 or higher)
//layout(location = 2) in vec3 inColor;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor; // input taken from a vertex buffer
layout(location = 2) in vec2 inTexCoord; // input for uv coordinates taken from a vertex buffer


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

// specify a descriptor set layout for each descriptor when creating the pipeline layout
// shaders reference the specific descriptor set like
layout (set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // 1. approach static positions
    // gl_Position = vec4(inPosition, 0.0, 1.0);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
