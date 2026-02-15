#version 450 core

layout(location = 0) in vec3 i_Position;
layout(location = 1) in vec2 i_TexCoord;
layout(location = 2) in float i_TexIndex;
layout(location = 3) in vec4 i_Color;
layout(location = 4) in vec4 i_TopColor;
layout(location = 5) in int i_Mode;
layout(location = 6) in int i_EntityID;
layout(location = 7) in vec4 i_InvVR0;
layout(location = 8) in vec4 i_InvVR1;
layout(location = 9) in vec4 i_InvVR2;
layout(location = 10) in vec4 i_InvVR3;

struct VertexOutput {
    vec2 TexCoord;
    vec4 Color;
    vec4 TopColor;
};

layout(location = 0) out VertexOutput o_Vertex;
layout(location = 3) out flat float o_TexIndex;
layout(location = 4) out flat int o_Mode;
layout(location = 5) out flat int o_EntityID;
layout(location = 6) out mat4 o_InvVR;

void main() {
    o_Vertex.TexCoord = i_TexCoord;
    o_Vertex.Color = i_Color;
    o_Vertex.TopColor = i_TopColor;
    o_TexIndex = i_TexIndex;
    o_Mode = i_Mode;
    o_EntityID = i_EntityID;
    o_InvVR = mat4(i_InvVR0, i_InvVR1, i_InvVR2, i_InvVR3);
    gl_Position = vec4(i_Position.xy, 0.0, 1.0);
}
