#version 450 core
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput {
    vec2 TexCoord;
    vec4 Color;
    vec4 TopColor;
};

layout(location = 0) in VertexOutput i_Vertex;
layout(location = 3) in flat float i_TexIndex;
layout(location = 4) in flat int i_Mode;
layout(location = 5) in flat int i_EntityID;
layout(location = 6) in mat4 i_InvVR;

layout(binding = 1) uniform sampler2D u_Textures[32];

const float PI = 3.14159265359;

// Branchless sRGB to linear conversion using mix() to avoid warp divergence.
vec3 sRGBToLinear(vec3 srgb) {
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
}

void main() {
    o_EntityID = i_EntityID;

    if (i_Mode == 0) {
        // Solid color
        o_Color = vec4(sRGBToLinear(i_Vertex.Color.rgb), i_Vertex.Color.a);
    } else if (i_Mode == 1) {
        // Gradient (bottom to top)
        vec4 mixed = mix(i_Vertex.Color, i_Vertex.TopColor, i_Vertex.TexCoord.y);
        o_Color = vec4(sRGBToLinear(mixed.rgb), mixed.a);
    } else if (i_Mode == 2) {
        // Texture background
        o_Color = texture(u_Textures[nonuniformEXT(int(i_TexIndex))], i_Vertex.TexCoord);
    } else {
        // Skybox equirectangular
        vec2 ndc = i_Vertex.TexCoord * 2.0 - 1.0;
        vec4 worldDir = i_InvVR * vec4(ndc, -1.0, 1.0);
        vec3 dir = normalize(worldDir.xyz);
        vec2 uv = vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5,
                        asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5);
        o_Color = texture(u_Textures[nonuniformEXT(int(i_TexIndex))], uv);
    }
}
