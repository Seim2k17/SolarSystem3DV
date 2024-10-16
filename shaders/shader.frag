#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler; // acc the combined_image_sampler descriptor type

void main() {
    //outColor = vec4(fragTexCoord, 0.0, 1.0); // this would be printf for shaderprogramming, output texture coordinates as colorvalues
    outColor = texture(texSampler, fragTexCoord );
}
