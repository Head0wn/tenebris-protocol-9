#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in uint inMaterial;

layout(push_constant) uniform VoxelPushConstants {
    mat4 viewProjection;
    vec4 lightDirectionAmbient;
} pushConstants;

layout(location = 0) flat out uint fragmentMaterial;
layout(location = 1) out float fragmentLight;

void main() {
    const vec3 normal = normalize(inNormal.xyz);
    const vec3 lightDirection = normalize(-pushConstants.lightDirectionAmbient.xyz);
    const float diffuse = max(dot(normal, lightDirection), 0.0);

    fragmentMaterial = inMaterial;
    fragmentLight = max(pushConstants.lightDirectionAmbient.w, diffuse);
    gl_Position = pushConstants.viewProjection * vec4(inPosition, 1.0);
}
