#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) out vec4 o_Color;
layout (location = 1) out int o_EntityID;

struct VertexOutput {
    vec4 Color;
    vec2 TexCoord;
    float TilingFactor;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 3) in flat float i_TexIndex;
layout (location = 4) in flat int i_EntityID;

layout (binding = 1) uniform sampler2D u_Textures[32];

// Branchless sRGB to linear conversion using mix() to avoid warp divergence.
vec3 sRGBToLinear(vec3 srgb) {
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
}

void main() {
    vec4 texColor = vec4(sRGBToLinear(i_Vertex.Color.rgb), i_Vertex.Color.a);
    texColor *= texture(u_Textures[nonuniformEXT(int(i_TexIndex))], i_Vertex.TexCoord * i_Vertex.TilingFactor);
    if (texColor.a < 0.001) discard;
    o_Color = texColor;
    o_EntityID = i_EntityID;
}
