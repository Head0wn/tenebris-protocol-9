#version 450

layout(location = 0) flat in uint fragmentMaterial;
layout(location = 1) in float fragmentLight;

layout(location = 0) out vec4 outColor;

vec3 materialColor(uint material) {
    switch (material) {
    case 1U:
        return vec3(0.34, 0.22, 0.12);
    case 2U:
        return vec3(0.19, 0.20, 0.22);
    case 3U:
        return vec3(0.24, 0.28, 0.31);
    case 4U:
        return vec3(0.62, 0.025, 0.012);
    case 5U:
        return vec3(0.035, 0.008, 0.012);
    default:
        return vec3(1.0, 0.0, 1.0);
    }
}

float materialEmission(uint material) {
    if (material == 4U) {
        return 1.75;
    }
    if (material == 5U) {
        return 0.42;
    }
    return 0.0;
}

void main() {
    const vec3 baseColor = materialColor(fragmentMaterial);
    const float emission = materialEmission(fragmentMaterial);
    const vec3 litColor = baseColor * fragmentLight + baseColor * emission;
    outColor = vec4(litColor, 1.0);
}
