#version 450 core

layout (location = 0) in vec3 i_Position;
layout (location = 1) in vec4 i_Color;
layout (location = 2) in vec2 i_TexCoord;
layout (location = 3) in float i_TexIndex;
layout (location = 4) in vec3 i_Normal;
layout (location = 5) in int i_EntityID;

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

struct VertexOutput {
    vec4 Color;
    vec2 TexCoord;
    vec3 Normal;
};

layout (location = 0) out VertexOutput o_Vertex;
layout (location = 3) out flat float o_TexIndex;
layout (location = 4) out flat int o_EntityID;

void main() {
    o_Vertex.Color = i_Color;
    o_Vertex.TexCoord = i_TexCoord;
    o_Vertex.Normal = i_Normal;
    o_TexIndex = i_TexIndex;
    o_EntityID = i_EntityID;

    gl_Position = u_ViewProjection * vec4(i_Position, 1.0);
}
