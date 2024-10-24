#version 450 core

layout(location = 0) in vec3 i_WorldPosition;
layout(location = 1) in vec3 i_LocalPosition;
layout(location = 2) in vec4 i_Color;
layout(location = 3) in float i_Thickness;
layout(location = 4) in float i_Fade;
layout(location = 5) in int i_EntityID;

layout(std140, binding = 0) uniform Camera {
    mat4 u_ViewProjection;
};

struct VertexOutput {
    vec3 LocalPosition;
    vec4 Color;
    float Thickness;
    float Fade;
};

layout (location = 0) out VertexOutput o_vertex;
layout (location = 4) out flat int o_EntityID;

void main() {
    o_vertex.LocalPosition = i_LocalPosition;
    o_vertex.Color = i_Color;
    o_vertex.Thickness = i_Thickness;
    o_vertex.Fade = i_Fade;

    o_EntityID = i_EntityID;

    gl_Position = u_ViewProjection * vec4(i_WorldPosition, 1.0);
}
