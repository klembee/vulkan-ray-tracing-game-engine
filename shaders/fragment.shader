#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(fragment)

layout(binding = 3) uniform sampler2D texSampler[8];

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in int texId;
layout(location = 2) in vec4 color;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = color;//texture(texSampler[texId], fragTexCoord);
}