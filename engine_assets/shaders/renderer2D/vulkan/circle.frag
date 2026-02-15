#version 450 core

layout (location = 0) out vec4 o_Color;
layout (location = 1) out int o_EntityID;

struct VertexOutput {
    vec3 LocalPosition;
    vec4 Color;
    float Thickness;
    float Fade;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 4) in flat int i_EntityID;

// Branchless sRGB to linear conversion using mix() to avoid warp divergence.
vec3 sRGBToLinear(vec3 srgb) {
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
}

void main() {
    float distance = 1.0 - length(i_Vertex.LocalPosition);
    float circle = smoothstep(0.0, i_Vertex.Fade, distance);
    circle *= smoothstep(i_Vertex.Thickness + i_Vertex.Fade, i_Vertex.Thickness, distance);

    if (circle < 0.001) discard;

    o_Color = vec4(sRGBToLinear(i_Vertex.Color.rgb), i_Vertex.Color.a * circle);
    if (o_Color.a < 0.001) discard;

    o_EntityID = i_EntityID;
}
