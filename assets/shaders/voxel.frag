#version 450

layout(location = 0) flat in uint fragmentMaterial;
layout(location = 1) in float fragmentLight;

layout(location = 0) out vec4 outColor;

vec3 materialColor(uint material) {
    switch (material) {
    case 1U:
        return vec3(0.31, 0.215, 0.12);
    case 2U:
        return vec3(0.145, 0.165, 0.18);
    case 3U:
        return vec3(0.085, 0.11, 0.13);
    case 4U:
        return vec3(0.78, 0.028, 0.01);
    case 5U:
        return vec3(0.045, 0.004, 0.009);
    default:
        return vec3(1.0, 0.0, 1.0);
    }
}

float materialDiffuseResponse(uint material) {
    switch (material) {
    case 1U:
        return 0.92;
    case 2U:
        return 0.78;
    case 3U:
        return 0.58;
    case 4U:
        return 0.34;
    case 5U:
        return 0.50;
    default:
        return 1.0;
    }
}

vec3 materialEmission(uint material) {
    if (material == 4U) {
        return vec3(2.55, 0.055, 0.018);
    }
    if (material == 5U) {
        return vec3(0.018, 0.0, 0.002);
    }
    return vec3(0.0);
}

void main() {
    const vec3 baseColor = materialColor(fragmentMaterial);
    const float diffuseResponse = materialDiffuseResponse(fragmentMaterial);
    const float shapedLight = pow(clamp(fragmentLight, 0.0, 1.0), 1.28);
    const float shadowFloor = fragmentMaterial == 5U ? 0.20 : 0.28;
    const float lighting = shadowFloor + shapedLight * diffuseResponse;

    vec3 color = baseColor * lighting + materialEmission(fragmentMaterial);

    if (fragmentMaterial == 2U) {
        color *= vec3(0.92, 0.98, 1.06);
    } else if (fragmentMaterial == 3U) {
        color *= vec3(0.82, 0.94, 1.04);
    } else if (fragmentMaterial == 5U) {
        color += vec3(0.012, 0.0, 0.001) * (1.0 - shapedLight);
    }

    color = max(color, vec3(0.0));
    outColor = vec4(color, 1.0);
}
