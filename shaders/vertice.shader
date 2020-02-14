#version 450
#extension GL_ARB_separate_shader_objects : enable
#pragma shader_stage(vertex)

layout(binding = 0) uniform ModelMat{
    mat4 model;
} modelMat;

layout(binding = 1) uniform ViewMat{
    mat4 view;
    mat4 projection;
} viewMats;

layout(binding = 2) uniform Bones{
    mat4 matrices[100];
} bones;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in int inTexId;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) flat out int texId;
layout(location = 2) out vec4 outColor;

void main() {
    mat4 boneTransform = bones.matrices[boneIds[0]] * weights[0];
    boneTransform += bones.matrices[boneIds[1]] * weights[1];
    boneTransform += bones.matrices[boneIds[2]] * weights[2];
    boneTransform += bones.matrices[boneIds[3]] * weights[3];

    gl_Position = viewMats.projection * viewMats.view * modelMat.model * boneTransform * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    texId = inTexId;

    outColor = vec4(boneIds[0]/46.0f, boneIds[1]/46.0f, boneIds[2]/46.0f, 1.0f);
}