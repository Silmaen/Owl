#version 450 core

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

layout (binding = 0) uniform sampler2D u_Textures[32];

void main() {
    vec4 texColor = i_Vertex.Color;
    texColor *= texture(u_Textures[int(i_TexIndex)], i_Vertex.TexCoord * i_Vertex.TilingFactor);
    if (texColor.a < 0.001) discard;
    o_Color = texColor;
    o_EntityID = i_EntityID;
}
