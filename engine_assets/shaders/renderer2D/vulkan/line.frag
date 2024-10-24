#version 450 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput {
    vec4 Color;
};

layout (location = 0) in VertexOutput i_Vertex;
layout (location = 1) in flat int i_EntityID;

void main() {
    o_Color = i_Vertex.Color;
    o_EntityID = i_EntityID;
}
