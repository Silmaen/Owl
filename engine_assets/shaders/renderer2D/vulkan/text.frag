#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec4 o_Color;
layout (location = 1) out int o_EntityID;

struct VertexOutput {
    vec4 Color;
    vec2 TexCoord;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 2) in flat float i_TexIndex;
layout (location = 3) in flat int i_EntityID;

layout (binding = 1) uniform sampler2D u_Textures[32];

// Branchless sRGB to linear conversion using mix() to avoid warp divergence.
vec3 sRGBToLinear(vec3 srgb) {
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
}

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    int texIdx = int(i_TexIndex);
    vec4 texColor = vec4(sRGBToLinear(i_Vertex.Color.rgb), i_Vertex.Color.a);

    // MSDF text rendering
    vec3 msd = texture(u_Textures[nonuniformEXT(texIdx)], i_Vertex.TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);

    // Screen-space pixel range for anti-aliasing
    const float pxRange = 2.0;
    vec2 texDimensions = vec2(textureSize(u_Textures[nonuniformEXT(texIdx)], 0));
    vec2 unitRange = vec2(pxRange) / texDimensions;
    vec2 screenTexSize = vec2(1.0) / fwidth(i_Vertex.TexCoord);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float screenPxDistance = screenPxRange * (sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    if (opacity < 0.001) discard;

    o_Color = vec4(texColor.rgb, texColor.a * opacity);
    if (o_Color.a < 0.001) discard;

    o_EntityID = i_EntityID;
}
