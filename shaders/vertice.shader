#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

layout(std430, push_constant) uniform CameraMatrices{
    mat4 view;
    mat4 projection;
} cameraMatrices;

layout(binding = 0) uniform ModelMatrix{
    mat4 model;
} modelMatrix;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in int inTexId;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) flat out int texId;

void main() {
    gl_Position = cameraMatrices.projection * cameraMatrices.view * modelMatrix.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    texId = inTexId;
}